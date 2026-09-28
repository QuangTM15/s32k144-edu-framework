/**
 * @file esc.c
 * @brief Arduino-style ESC driver implementation.
 */

#include "esc.h"
#include "ftm.h"
#include "port.h"

/* ========================================================================= */
/* Private Configuration Constants                                           */
/* ========================================================================= */

/**
 * Assume S32K144 Normal RUN Mode with 80 MHz SPLL for FTM.
 *
 * FTM prescaler:
 * 80 MHz / 32 = 2.5 MHz
 *
 * PWM period:
 * 2.5 MHz / 50 Hz = 50,000 counts
 *
 * This fits within the 16-bit FTM counter range.
 */
#define ESC_FTM_SRC_CLOCK_HZ (80000000UL)
#define ESC_FTM_PRESCALER_VAL (32UL)
#define ESC_FTM_PRESCALER_ENUM (FTM_PRESCALER_DIV_32)
#define ESC_PWM_FREQ_HZ (50U)

#define ESC_ARM_TIME_MS (3000U)

/* ========================================================================= */
/* Private Types                                                             */
/* ========================================================================= */

/**
 * @brief Internal ESC device context.
 *
 * @details
 * Stores all hardware mapping and pulse configuration required by the ESC
 * device library. This structure is private and is not exposed to the
 * application layer.
 */
typedef struct
{
    bool initialized;

    uint8_t logicalPin;

    FTM_Instance_t instance;
    FTM_Channel_t channel;

    uint16_t minPulseUs;
    uint16_t maxPulseUs;

    uint32_t timerClockHz;
} ESC_Context_t;

/* ========================================================================= */
/* Private Variables                                                         */
/* ========================================================================= */

/**
 * @brief Internal ESC context.
 *
 * @details
 * EduFramework currently supports one ESC instance through this device API.
 */
static ESC_Context_t g_esc;

/* ========================================================================= */
/* Public API Implementation                                                 */
/* ========================================================================= */

/**
 * @copydoc ESC_Init
 */
bool ESC_Init(uint8_t pin)
{
    bool status = false;

    if (Arduino_HasPwmCapability(pin) != ARDUINO_VALID_FALSE)
    {
        ArduinoPwmMap_t pwmMap;
        FTM_PwmConfig_t pwmConfig;
        const ArduinoPinMap_t *pinData;

        /* Get PWM hardware mapping for the selected logical pin. */
        (void)Arduino_GetPwmMap(pin, &pwmMap);

        /* Configure internal ESC context. */
        g_esc.initialized = false;

        g_esc.logicalPin = pin;

        g_esc.instance = pwmMap.instance;
        g_esc.channel = pwmMap.channel;

        g_esc.minPulseUs = ESC_DEFAULT_MIN_PULSE_US;
        g_esc.maxPulseUs = ESC_DEFAULT_MAX_PULSE_US;

        g_esc.timerClockHz =
            ESC_FTM_SRC_CLOCK_HZ / ESC_FTM_PRESCALER_VAL;

        /* Enable the port clock. */
        pinMode(pin, OUTPUT);

        /* Route the FTM signal to the selected pin. */
        pinData = &g_arduinoPinMap[pin];

        PORT_SetPinMux(
            pinData->portBase,
            pinData->pinNumber,
            pwmMap.mux);

        /* Configure FTM for 50 Hz RC PWM. */
        pwmConfig.srcClockHz = ESC_FTM_SRC_CLOCK_HZ;
        pwmConfig.pwmFreqHz = ESC_PWM_FREQ_HZ;
        pwmConfig.clockSource = FTM_CLOCK_SOURCE_SYSTEM;
        pwmConfig.prescaler = ESC_FTM_PRESCALER_ENUM;

        (void)FTM_InitPwm(
            g_esc.instance,
            &pwmConfig);

        (void)FTM_SetChannelModePwm(
            g_esc.instance,
            g_esc.channel,
            FTM_PWM_EDGE_ALIGNED_HIGH_TRUE);

        (void)FTM_StartCounter(
            g_esc.instance);

        g_esc.initialized = true;

        /*
         * Preserve the startup behavior of the previously verified
         * ESC implementation.
         */
        ESC_SetMicroseconds(0U);

        status = true;
    }

    return status;
}

/**
 * @copydoc ESC_SetPulseRange
 */
bool ESC_SetPulseRange(uint16_t minUs, uint16_t maxUs)
{
    bool status = false;

    if ((g_esc.initialized) && (minUs < maxUs))
    {
        g_esc.minPulseUs = minUs;
        g_esc.maxPulseUs = maxUs;

        status = true;
    }

    return status;
}

/**
 * @copydoc ESC_Arm
 */
void ESC_Arm(void)
{
    if (g_esc.initialized)
    {
        /* Send the configured minimum throttle pulse. */
        ESC_SetMicroseconds(g_esc.minPulseUs);

        /* Hold minimum throttle while the ESC performs its arming process. */
        delay(ESC_ARM_TIME_MS);
    }
}

/**
 * @copydoc ESC_SetThrottle
 */
void ESC_SetThrottle(uint8_t percent)
{
    if (g_esc.initialized)
    {
        uint16_t pulseRange;
        uint16_t targetUs;

        if (percent > 100U)
        {
            percent = 100U;
        }

        pulseRange =
            g_esc.maxPulseUs -
            g_esc.minPulseUs;

        targetUs =
            g_esc.minPulseUs +
            ((pulseRange * (uint16_t)percent) / 100U);

        ESC_SetMicroseconds(targetUs);
    }
}

/**
 * @copydoc ESC_SetMicroseconds
 */
void ESC_SetMicroseconds(uint16_t us)
{
    if (g_esc.initialized)
    {
        uint32_t counts32;
        uint16_t dutyCounts;

        /*
         * Convert pulse width in microseconds to FTM compare counts:
         *
         * dutyCounts =
         *     (pulseUs * timerClockHz) / 1,000,000
         */
        counts32 =
            ((uint32_t)us * g_esc.timerClockHz) /
            1000000UL;

        dutyCounts = (uint16_t)counts32;

        (void)FTM_SetPwmDuty(
            g_esc.instance,
            g_esc.channel,
            dutyCounts);
    }
}