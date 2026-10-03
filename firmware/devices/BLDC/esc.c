/**
 * @file esc.c
 * @brief Arduino-style ESC device library implementation.
 *
 * @details
 * This file implements ESC control using the EduFramework FTM driver.
 *
 * Design notes:
 * - One ESC instance is currently supported.
 * - The application uses logical Arduino-style pins.
 * - Hardware FTM instance and channel mapping remain private.
 * - The ESC signal uses 50 Hz RC PWM.
 * - The default throttle pulse range is 1000 us to 2000 us.
 * - No dynamic memory allocation is used.
 */

#include "esc.h"

#include "ftm.h"
#include "port.h"
#include <stddef.h>

/* ========================================================================= */
/* Private Configuration Constants                                           */
/* ========================================================================= */

/**
 * @brief FTM source clock frequency in Normal RUN mode.
 */
#define ESC_FTM_SRC_CLOCK_HZ (80000000UL)

/**
 * @brief Numeric FTM prescaler divisor.
 *
 * @details
 * 80 MHz / 32 = 2.5 MHz timer clock.
 */
#define ESC_FTM_PRESCALER_VALUE (32UL)

/**
 * @brief FTM driver prescaler configuration.
 */
#define ESC_FTM_PRESCALER (FTM_PRESCALER_DIV_32)

/**
 * @brief ESC RC PWM frequency.
 *
 * @details
 * 50 Hz corresponds to a 20 ms PWM period.
 */
#define ESC_PWM_FREQUENCY_HZ (50U)

/**
 * @brief EduFramework default ESC arming duration.
 */
#define ESC_ARM_TIME_MS (3000U)

/**
 * @brief Microseconds per second.
 */
#define ESC_MICROSECONDS_PER_SECOND (1000000UL)

/**
 * @brief Minimum throttle percentage.
 */
#define ESC_MIN_THROTTLE_PERCENT (0U)

/**
 * @brief Maximum throttle percentage.
 */
#define ESC_MAX_THROTTLE_PERCENT (100U)

/**
 * @brief Disabled PWM pulse width.
 *
 * @details
 * This value is used internally to produce zero duty during initialization
 * and shutdown. It is not part of the public ESC pulse range.
 */
#define ESC_DISABLED_PULSE_US (0U)

/* ========================================================================= */
/* Private Types                                                             */
/* ========================================================================= */

/**
 * @brief Internal ESC device context.
 *
 * @details
 * All hardware-specific information and current command state remain private
 * to the ESC device library.
 */
typedef struct
{
    bool initialized;

    uint8_t logicalPin;

    FTM_Instance_t instance;
    FTM_Channel_t channel;

    uint16_t minPulseUs;
    uint16_t maxPulseUs;

    uint16_t currentPulseUs;
    uint8_t currentThrottle;

    uint32_t timerClockHz;

} ESC_Context_t;

/* ========================================================================= */
/* Private Variables                                                          */
/* ========================================================================= */

/**
 * @brief Internal single-instance ESC context.
 */
static ESC_Context_t g_esc;

/* ========================================================================= */
/* Private Function Prototypes                                                */
/* ========================================================================= */

static void ESC_ResetContext(void);

static bool ESC_ConfigureHardware(uint8_t pin);

static uint16_t ESC_ThrottleToMicroseconds(uint8_t percent);

static uint8_t ESC_MicrosecondsToThrottle(uint16_t us);

static uint16_t ESC_ClampPulse(uint16_t us);

static void ESC_WritePulse(uint16_t us);

/* ========================================================================= */
/* Private Functions                                                          */
/* ========================================================================= */

/**
 * @brief Reset the private ESC context.
 */
static void ESC_ResetContext(void)
{
    g_esc.initialized = false;

    g_esc.logicalPin = 0U;

    g_esc.instance = (FTM_Instance_t)0U;
    g_esc.channel = (FTM_Channel_t)0U;

    g_esc.minPulseUs =
        ESC_DEFAULT_MIN_PULSE_US;

    g_esc.maxPulseUs =
        ESC_DEFAULT_MAX_PULSE_US;

    g_esc.currentPulseUs =
        ESC_DISABLED_PULSE_US;

    g_esc.currentThrottle =
        ESC_MIN_THROTTLE_PERCENT;

    g_esc.timerClockHz = 0UL;

    return;
}

/**
 * @brief Configure the hardware resources required by the ESC.
 *
 * @param pin Arduino-style logical PWM pin.
 *
 * @return Configuration state.
 */
static bool ESC_ConfigureHardware(uint8_t pin)
{
    ArduinoPwmMap_t PwmMap;
    FTM_PwmConfig_t PwmConfig;
    const ArduinoPinMap_t *pPinData = NULL;
    bool bConfigured = false;

    if (ARDUINO_VALID_FALSE !=
        Arduino_HasPwmCapability(pin))
    {
        (void)Arduino_GetPwmMap(
            pin,
            &PwmMap);

        g_esc.logicalPin =
            pin;

        g_esc.instance =
            PwmMap.instance;

        g_esc.channel =
            PwmMap.channel;

        g_esc.timerClockHz =
            ESC_FTM_SRC_CLOCK_HZ /
            ESC_FTM_PRESCALER_VALUE;

        /*
         * Enable the logical pin and its PORT clock.
         */
        pinMode(
            pin,
            OUTPUT);

        /*
         * Route the selected FTM channel to the physical pin.
         */
        pPinData =
            &g_arduinoPinMap[pin];

        PORT_SetPinMux(
            pPinData->portBase,
            pPinData->pinNumber,
            PwmMap.mux);

        /*
         * Configure FTM for 50 Hz RC PWM.
         */
        PwmConfig.srcClockHz =
            ESC_FTM_SRC_CLOCK_HZ;

        PwmConfig.pwmFreqHz =
            ESC_PWM_FREQUENCY_HZ;

        PwmConfig.clockSource =
            FTM_CLOCK_SOURCE_SYSTEM;

        PwmConfig.prescaler =
            ESC_FTM_PRESCALER;

        if (FTM_STATUS_OK ==
            FTM_InitPwm(
                g_esc.instance,
                &PwmConfig))
        {
            if (FTM_STATUS_OK ==
                FTM_SetChannelModePwm(
                    g_esc.instance,
                    g_esc.channel,
                    FTM_PWM_EDGE_ALIGNED_HIGH_TRUE))
            {
                if (FTM_STATUS_OK ==
                    FTM_StartCounter(
                        g_esc.instance))
                {
                    bConfigured = true;
                }
            }
        }
    }

    return bConfigured;
}

/**
 * @brief Convert throttle percentage to pulse width.
 *
 * @param percent Throttle percentage from 0 to 100.
 *
 * @return Pulse width in microseconds.
 */
static uint16_t ESC_ThrottleToMicroseconds(uint8_t percent)
{
    uint32_t u32PulseRange = 0UL;
    uint32_t u32PulseUs = 0UL;

    if (ESC_MAX_THROTTLE_PERCENT < percent)
    {
        percent =
            ESC_MAX_THROTTLE_PERCENT;
    }

    u32PulseRange =
        (uint32_t)g_esc.maxPulseUs -
        (uint32_t)g_esc.minPulseUs;

    u32PulseUs =
        (uint32_t)g_esc.minPulseUs +
        ((u32PulseRange *
          (uint32_t)percent) /
         (uint32_t)ESC_MAX_THROTTLE_PERCENT);

    return (uint16_t)u32PulseUs;
}

/**
 * @brief Convert pulse width to an approximate throttle percentage.
 *
 * @param us Pulse width in microseconds.
 *
 * @return Corresponding throttle percentage.
 */
static uint8_t ESC_MicrosecondsToThrottle(uint16_t us)
{
    uint32_t u32PulseRange = 0UL;
    uint32_t u32PulseOffset = 0UL;
    uint32_t u32Throttle = 0UL;

    if (g_esc.minPulseUs >= us)
    {
        u32Throttle =
            ESC_MIN_THROTTLE_PERCENT;
    }
    else if (g_esc.maxPulseUs <= us)
    {
        u32Throttle =
            ESC_MAX_THROTTLE_PERCENT;
    }
    else
    {
        u32PulseRange =
            (uint32_t)g_esc.maxPulseUs -
            (uint32_t)g_esc.minPulseUs;

        u32PulseOffset =
            (uint32_t)us -
            (uint32_t)g_esc.minPulseUs;

        u32Throttle =
            (u32PulseOffset *
             (uint32_t)ESC_MAX_THROTTLE_PERCENT) /
            u32PulseRange;
    }

    return (uint8_t)u32Throttle;
}

/**
 * @brief Clamp a pulse width to the configured ESC range.
 *
 * @param us Requested pulse width.
 *
 * @return Clamped pulse width.
 */
static uint16_t ESC_ClampPulse(uint16_t us)
{
    uint16_t u16PulseUs = us;

    if (g_esc.minPulseUs > u16PulseUs)
    {
        u16PulseUs =
            g_esc.minPulseUs;
    }
    else if (g_esc.maxPulseUs < u16PulseUs)
    {
        u16PulseUs =
            g_esc.maxPulseUs;
    }
    else
    {
        /* Pulse already lies inside the configured range. */
    }

    return u16PulseUs;
}

/**
 * @brief Write a raw PWM pulse to the configured FTM channel.
 *
 * @details
 * This private function intentionally does not clamp the pulse width.
 * It is therefore also used to generate zero duty during initialization
 * and shutdown.
 *
 * @param us Pulse width in microseconds.
 */
static void ESC_WritePulse(uint16_t us)
{
    uint32_t u32Counts = 0UL;
    uint16_t u16DutyCounts = 0U;

    u32Counts =
        ((uint32_t)us *
         g_esc.timerClockHz) /
        ESC_MICROSECONDS_PER_SECOND;

    u16DutyCounts =
        (uint16_t)u32Counts;

    (void)FTM_SetPwmDuty(
        g_esc.instance,
        g_esc.channel,
        u16DutyCounts);

    return;
}

/* ========================================================================= */
/* Public API Implementation                                                 */
/* ========================================================================= */

/**
 * @copydoc ESC_Init
 */
bool ESC_Init(uint8_t pin)
{
    bool bInitialized = false;

    if (true == ESC_IsInitialized())
    {
        ESC_End();
    }

    ESC_ResetContext();

    if (true ==
        ESC_ConfigureHardware(pin))
    {
        /*
         * Hardware is ready. Keep the PWM output at zero duty until the
         * application explicitly arms or commands the ESC.
         */
        ESC_WritePulse(
            ESC_DISABLED_PULSE_US);

        g_esc.currentPulseUs =
            ESC_DISABLED_PULSE_US;

        g_esc.currentThrottle =
            ESC_MIN_THROTTLE_PERCENT;

        g_esc.initialized =
            true;

        bInitialized =
            true;
    }
    else
    {
        ESC_ResetContext();
    }

    return bInitialized;
}

/**
 * @copydoc ESC_End
 */
void ESC_End(void)
{
    if (true == g_esc.initialized)
    {
        /*
         * Disable the ESC signal before releasing the software context.
         *
         * The FTM instance may be shared with other PWM channels, so this
         * device layer does not stop or deinitialize the complete FTM module.
         */
        ESC_WritePulse(
            ESC_DISABLED_PULSE_US);
    }

    ESC_ResetContext();

    return;
}

/**
 * @copydoc ESC_IsInitialized
 */
bool ESC_IsInitialized(void)
{
    bool bInitialized = false;

    if (true == g_esc.initialized)
    {
        bInitialized = true;
    }
    else
    {
        bInitialized = false;
    }

    return bInitialized;
}

/**
 * @copydoc ESC_SetPulseRange
 */
bool ESC_SetPulseRange(uint16_t minUs,
                       uint16_t maxUs)
{
    bool bSuccess = false;

    if ((true == g_esc.initialized) &&
        (0U < minUs) &&
        (minUs < maxUs))
    {
        g_esc.minPulseUs =
            minUs;

        g_esc.maxPulseUs =
            maxUs;

        bSuccess =
            true;
    }

    return bSuccess;
}

/**
 * @copydoc ESC_Arm
 */
void ESC_Arm(void)
{
    if (true == g_esc.initialized)
    {
        /*
         * Send minimum throttle during the arming interval.
         */
        ESC_SetThrottle(
            ESC_MIN_THROTTLE_PERCENT);

        delay(
            ESC_ARM_TIME_MS);
    }

    return;
}

/**
 * @copydoc ESC_SetThrottle
 */
void ESC_SetThrottle(uint8_t percent)
{
    uint16_t u16PulseUs = 0U;

    if (true == g_esc.initialized)
    {
        if (ESC_MAX_THROTTLE_PERCENT < percent)
        {
            percent =
                ESC_MAX_THROTTLE_PERCENT;
        }

        u16PulseUs =
            ESC_ThrottleToMicroseconds(
                percent);

        ESC_WritePulse(
            u16PulseUs);

        g_esc.currentPulseUs =
            u16PulseUs;

        g_esc.currentThrottle =
            percent;
    }

    return;
}

/**
 * @copydoc ESC_GetThrottle
 */
uint8_t ESC_GetThrottle(void)
{
    uint8_t u8Throttle =
        ESC_MIN_THROTTLE_PERCENT;

    if (true == g_esc.initialized)
    {
        u8Throttle =
            g_esc.currentThrottle;
    }

    return u8Throttle;
}

/**
 * @copydoc ESC_SetMicroseconds
 */
void ESC_SetMicroseconds(uint16_t us)
{
    uint16_t u16PulseUs = 0U;

    if (true == g_esc.initialized)
    {
        u16PulseUs =
            ESC_ClampPulse(us);

        ESC_WritePulse(
            u16PulseUs);

        g_esc.currentPulseUs =
            u16PulseUs;

        g_esc.currentThrottle =
            ESC_MicrosecondsToThrottle(
                u16PulseUs);
    }

    return;
}

/**
 * @copydoc ESC_GetMicroseconds
 */
uint16_t ESC_GetMicroseconds(void)
{
    uint16_t u16PulseUs =
        ESC_DISABLED_PULSE_US;

    if (true == g_esc.initialized)
    {
        u16PulseUs =
            g_esc.currentPulseUs;
    }

    return u16PulseUs;
}