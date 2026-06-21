/**
 * @file wiring_analog.c
 * @brief Arduino-style analog API implementation.
 *
 * @details
 * This file implements Arduino-style analog input and PWM output APIs.
 *
 * Analog input is implemented using:
 * - ADC driver for software-triggered interrupt-based conversion.
 * - ADC0 as the default analog input instance.
 * - ADC0_SE12 and ADC0_SE13 as the currently supported analog channels.
 *
 * PWM output is implemented using:
 * - Arduino pin mapping metadata.
 * - PORT driver for pin mux configuration.
 * - FTM driver for PWM generation.
 *
 * The implementation keeps hardware-specific register access inside the
 * low-level drivers. This file only coordinates the Arduino-style API
 * behavior.
 */

#include "wiring_analog.h"
#include "arduino_pins.h"
#include "adc.h"
#include "irq.h"
#include "ftm.h"

/* ============================================================
 * Local constants
 * ============================================================ */

/**
 * @brief Generic boolean-like false value used inside this module.
 */
#define ANALOG_FALSE                         (0U)

/**
 * @brief Generic boolean-like true value used inside this module.
 */
#define ANALOG_TRUE                          (1U)

/**
 * @brief Error return value used by int-returning Arduino-style APIs.
 */
#define ANALOG_ERROR_VALUE                   (-1)

/**
 * @brief Default ADC instance used by Arduino-style analog input APIs.
 */
#define ANALOG_ADC_INSTANCE                  (IP_ADC_0)

/**
 * @brief ADC source clock frequency used by the current framework setup.
 */
#define ANALOG_ADC_SRC_CLOCK_HZ              (8000000UL)

/**
 * @brief ADC sample time setting used by the default analog configuration.
 */
#define ANALOG_ADC_SAMPLE_TIME               (12U)

/**
 * @brief ADC interrupt enable state for the analog input path.
 */
#define ANALOG_ADC_INTERRUPT_ENABLE          (1U)

/**
 * @brief Default analog reference voltage in millivolts.
 *
 * @details
 * This value is used only by analogReadMilliVolts(). The value is kept at
 * 5000 mV to preserve the previous cookbook-style scaling behavior.
 */
#define ANALOG_REFERENCE_MV                  (5000UL)

/**
 * @brief Maximum raw value of a 12-bit ADC conversion.
 */
#define ANALOG_MAX_12BIT_VALUE               (4095UL)

/**
 * @brief Default PWM source clock frequency used by analogWrite().
 */
#define ANALOG_PWM_SRC_CLOCK_HZ              (8000000UL)

/**
 * @brief Default PWM frequency used by analogWrite().
 */
#define ANALOG_PWM_DEFAULT_FREQ_HZ           (1000UL)

/**
 * @brief Maximum Arduino-style PWM input value.
 */
#define ANALOG_PWM_MAX_VALUE                 (255U)

/**
 * @brief Maximum PWM duty percent passed to the FTM driver.
 */
#define ANALOG_PWM_MAX_PERCENT               (100UL)

/**
 * @brief Number of FTM instances tracked by the Arduino analog layer.
 */
#define ANALOG_PWM_INSTANCE_COUNT            (3U)

/**
 * @brief Number of FTM channels tracked by the Arduino analog layer.
 */
#define ANALOG_PWM_CHANNEL_COUNT             (8U)

/* ============================================================
 * Internal state
 * ============================================================ */

static uint8_t s_u8AnalogInitialized = ANALOG_FALSE;
static uint8_t s_u8AnalogConversionActive = ANALOG_FALSE;

static uint8_t s_au8PwmInitialized[ANALOG_PWM_INSTANCE_COUNT] = {ANALOG_FALSE};
static uint8_t s_au8PwmChannelConfigured[ANALOG_PWM_INSTANCE_COUNT][ANALOG_PWM_CHANNEL_COUNT] = {{ANALOG_FALSE}};

/* ============================================================
 * Internal helpers
 * ============================================================ */

/**
 * @brief Check whether a pin is supported by the current analog input API.
 *
 * @details
 * The MaaZEDU analog input scope is intentionally limited to ADC0_SE12
 * and ADC0_SE13. Other ADC channels are not exposed through this API in
 * the current version.
 *
 * @param[in] u8Pin
 * Arduino-style pin identifier.
 *
 * @return uint8_t
 *
 * @retval ANALOG_TRUE
 * Pin is supported as analog input.
 *
 * @retval ANALOG_FALSE
 * Pin is not supported as analog input.
 */
static uint8_t Analog_IsValidAnalogPin(uint8_t u8Pin)
{
    uint8_t u8IsValid = ANALOG_FALSE;

    if ((ADC0_SE12 == u8Pin) || (ADC0_SE13 == u8Pin))
    {
        u8IsValid = ANALOG_TRUE;
    }

    return u8IsValid;
}

/**
 * @brief Convert Arduino-style analog pin identifier to ADC channel.
 *
 * @details
 * The default channel is ADC_CHANNEL_SE12. The caller is expected to
 * validate the pin before using this helper.
 *
 * @param[in] u8Pin
 * Arduino-style analog pin identifier.
 *
 * @return ADC_Channel_t
 * ADC channel associated with the given pin.
 */
static ADC_Channel_t Analog_PinToChannel(uint8_t u8Pin)
{
    ADC_Channel_t Channel = ADC_CHANNEL_SE12;

    switch (u8Pin)
    {
    case ADC0_SE12:
        Channel = ADC_CHANNEL_SE12;
        break;

    case ADC0_SE13:
        Channel = ADC_CHANNEL_SE13;
        break;

    default:
        Channel = ADC_CHANNEL_SE12;
        break;
    }

    return Channel;
}

/**
 * @brief Enable PORT clock for a PORT register base pointer.
 *
 * @details
 * PWM output requires the corresponding pin mux to be configured before
 * the FTM signal can appear on the physical pin. The PORT clock must be
 * enabled before writing PCR registers.
 *
 * @param[in] pBase
 * Pointer to PORT register block.
 *
 * @return None.
 */
static void Analog_EnablePortClock(PORT_Type *pBase)
{
    if (IP_PORTA == pBase)
    {
        PORT_EnableClock(PORT_NAME_A);
    }
    else if (IP_PORTB == pBase)
    {
        PORT_EnableClock(PORT_NAME_B);
    }
    else if (IP_PORTC == pBase)
    {
        PORT_EnableClock(PORT_NAME_C);
    }
    else if (IP_PORTD == pBase)
    {
        PORT_EnableClock(PORT_NAME_D);
    }
    else if (IP_PORTE == pBase)
    {
        PORT_EnableClock(PORT_NAME_E);
    }
    else
    {
        /* Invalid PORT pointer: no clock can be enabled. */
    }
}

/**
 * @brief Ensure an FTM instance has been initialized for PWM.
 *
 * @details
 * analogWrite() lazily initializes each FTM instance the first time a PWM
 * pin mapped to that instance is used. This avoids enabling all PWM
 * timers during startup.
 *
 * @param[in] Instance
 * FTM instance identifier.
 *
 * @return uint8_t
 *
 * @retval ANALOG_TRUE
 * PWM instance is initialized or was already initialized.
 *
 * @retval ANALOG_FALSE
 * PWM instance initialization failed.
 */
static uint8_t Analog_EnsurePwmInitialized(FTM_Instance_t Instance)
{
    FTM_PwmConfig_t PwmConfig = {0U};
    uint8_t u8Result = ANALOG_FALSE;

    if ((uint8_t)ANALOG_PWM_INSTANCE_COUNT <= (uint8_t)Instance)
    {
        u8Result = ANALOG_FALSE;
    }
    else if (ANALOG_FALSE != s_au8PwmInitialized[(uint8_t)Instance])
    {
        u8Result = ANALOG_TRUE;
    }
    else
    {
        PwmConfig.srcClockHz = ANALOG_PWM_SRC_CLOCK_HZ;
        PwmConfig.pwmFreqHz = ANALOG_PWM_DEFAULT_FREQ_HZ;
        PwmConfig.clockSource = FTM_CLOCK_SOURCE_EXTERNAL;
        PwmConfig.prescaler = FTM_PRESCALER_DIV_1;

        if (FTM_STATUS_OK == FTM_InitPwm(Instance, &PwmConfig))
        {
            s_au8PwmInitialized[(uint8_t)Instance] = ANALOG_TRUE;
            u8Result = ANALOG_TRUE;
        }
        else
        {
            u8Result = ANALOG_FALSE;
        }
    }

    return u8Result;
}

/**
 * @brief Ensure a PWM-capable pin is muxed and configured for PWM output.
 *
 * @details
 * This helper performs all one-time configuration needed before updating
 * PWM duty cycle:
 * - Resolve Arduino PWM mapping.
 * - Enable PORT clock.
 * - Configure pin mux to FTM function.
 * - Initialize the mapped FTM instance if needed.
 * - Configure the mapped FTM channel for edge-aligned low-true PWM.
 * - Start the FTM counter.
 *
 * @param[in] u8Pin
 * Arduino-style pin identifier.
 *
 * @param[out] pPwmMap
 * Pointer used to receive the PWM mapping.
 *
 * @return uint8_t
 *
 * @retval ANALOG_TRUE
 * Pin is configured and ready for PWM duty update.
 *
 * @retval ANALOG_FALSE
 * Pin configuration failed.
 */
static uint8_t Analog_EnsurePwmPinConfigured(uint8_t u8Pin, ArduinoPwmMap_t *pPwmMap)
{
    const ArduinoPinMap_t *pPinMap = (const ArduinoPinMap_t *)0;
    uint8_t u8Result = ANALOG_FALSE;

    if ((ArduinoPwmMap_t *)0 == pPwmMap)
    {
        u8Result = ANALOG_FALSE;
    }
    else if (ANALOG_FALSE == Arduino_GetPwmMap(u8Pin, pPwmMap))
    {
        u8Result = ANALOG_FALSE;
    }
    else
    {
        pPinMap = &g_arduinoPinMap[u8Pin];

        Analog_EnablePortClock(pPinMap->portBase);
        PORT_SetPinMux(pPinMap->portBase, pPinMap->pinNumber, pPwmMap->mux);

        if (ANALOG_FALSE == Analog_EnsurePwmInitialized(pPwmMap->instance))
        {
            u8Result = ANALOG_FALSE;
        }
        else
        {
            if (ANALOG_FALSE == s_au8PwmChannelConfigured[(uint8_t)pPwmMap->instance][(uint8_t)pPwmMap->channel])
            {
                if (FTM_STATUS_OK == FTM_SetChannelModePwm(pPwmMap->instance,
                                                           pPwmMap->channel,
                                                           FTM_PWM_EDGE_ALIGNED_LOW_TRUE))
                {
                    s_au8PwmChannelConfigured[(uint8_t)pPwmMap->instance][(uint8_t)pPwmMap->channel] = ANALOG_TRUE;
                    u8Result = ANALOG_TRUE;
                }
                else
                {
                    u8Result = ANALOG_FALSE;
                }
            }
            else
            {
                u8Result = ANALOG_TRUE;
            }

            if (ANALOG_TRUE == u8Result)
            {
                if (FTM_STATUS_OK != FTM_StartCounter(pPwmMap->instance))
                {
                    u8Result = ANALOG_FALSE;
                }
            }
        }
    }

    return u8Result;
}

/* ============================================================
 * Public API
 * ============================================================ */

/**
 * @copydoc analogInit
 */
void analogInit(void)
{
    ADC_Config_t AdcConfig = {0U};

    if (ANALOG_FALSE == s_u8AnalogInitialized)
    {
        AdcConfig.srcClockHz = ANALOG_ADC_SRC_CLOCK_HZ;
        AdcConfig.resolution = ADC_RESOLUTION_12BIT;
        AdcConfig.reference = ADC_REF_DEFAULT;
        AdcConfig.sampleTime = ANALOG_ADC_SAMPLE_TIME;
        AdcConfig.average = ADC_AVERAGE_DISABLED;
        AdcConfig.enableInterrupt = ANALOG_ADC_INTERRUPT_ENABLE;

        if (ADC_STATUS_OK == ADC_Init(ANALOG_ADC_INSTANCE, &AdcConfig))
        {
            if (ADC_STATUS_OK == ADC_Calibrate(ANALOG_ADC_INSTANCE))
            {
                IRQ_ADC0_Init();

                s_u8AnalogInitialized = ANALOG_TRUE;
                s_u8AnalogConversionActive = ANALOG_FALSE;
            }
        }
    }
}

/**
 * @copydoc analogRead
 */
int analogRead(uint8_t u8Pin)
{
    int s32Result = ANALOG_ERROR_VALUE;

    if (ANALOG_FALSE == s_u8AnalogInitialized)
    {
        analogInit();
    }

    if ((ANALOG_FALSE != s_u8AnalogInitialized) &&
        (ANALOG_FALSE != Analog_IsValidAnalogPin(u8Pin)))
    {
        analogStart(u8Pin);

        if (ANALOG_FALSE != s_u8AnalogConversionActive)
        {
            while (ANALOG_FALSE == analogAvailable())
            {
            }

            s32Result = analogGetResult();
        }
    }

    return s32Result;
}

/**
 * @copydoc analogStart
 */
void analogStart(uint8_t u8Pin)
{
    ADC_Channel_t Channel = ADC_CHANNEL_SE12;

    if (ANALOG_FALSE == s_u8AnalogInitialized)
    {
        analogInit();
    }

    if ((ANALOG_FALSE != s_u8AnalogInitialized) &&
        (ANALOG_FALSE != Analog_IsValidAnalogPin(u8Pin)) &&
        (ANALOG_FALSE == s_u8AnalogConversionActive))
    {
        Channel = Analog_PinToChannel(u8Pin);

        if (ADC_STATUS_OK == ADC_StartConversion_IT(ANALOG_ADC_INSTANCE, Channel))
        {
            s_u8AnalogConversionActive = ANALOG_TRUE;
        }
    }
}

/**
 * @copydoc analogAvailable
 */
uint8_t analogAvailable(void)
{
    uint8_t u8Available = ANALOG_FALSE;

    if ((ANALOG_FALSE != s_u8AnalogInitialized) &&
        (ANALOG_FALSE != s_u8AnalogConversionActive))
    {
        u8Available = ADC_IsDone(ANALOG_ADC_INSTANCE);
    }

    return u8Available;
}

/**
 * @copydoc analogGetResult
 */
int analogGetResult(void)
{
    uint16_t u16Result = 0U;
    int s32Result = ANALOG_ERROR_VALUE;

    if ((ANALOG_FALSE != s_u8AnalogInitialized) &&
        (ANALOG_FALSE != s_u8AnalogConversionActive))
    {
        if (ADC_STATUS_OK == ADC_GetResult(ANALOG_ADC_INSTANCE, &u16Result))
        {
            s_u8AnalogConversionActive = ANALOG_FALSE;
            s32Result = (int)u16Result;
        }
    }

    return s32Result;
}

/**
 * @copydoc analogReadMilliVolts
 */
int analogReadMilliVolts(uint8_t u8Pin)
{
    int s32RawValue = ANALOG_ERROR_VALUE;
    int s32MilliVolts = ANALOG_ERROR_VALUE;
    uint32_t u32MilliVolts = 0UL;

    s32RawValue = analogRead(u8Pin);

    if (ANALOG_ERROR_VALUE != s32RawValue)
    {
        u32MilliVolts = ((uint32_t)s32RawValue * ANALOG_REFERENCE_MV) /
                        ANALOG_MAX_12BIT_VALUE;

        s32MilliVolts = (int)u32MilliVolts;
    }

    return s32MilliVolts;
}

/**
 * @copydoc analogWrite
 */
void analogWrite(uint8_t u8Pin, uint8_t u8Value)
{
    ArduinoPwmMap_t PwmMap = {0U};
    uint32_t u32DutyPercent = 0UL;

    if ((ANALOG_FALSE != Arduino_IsValidPin(u8Pin)) &&
        (ANALOG_FALSE != Arduino_HasPwmCapability(u8Pin)))
    {
        if (ANALOG_FALSE != Analog_EnsurePwmPinConfigured(u8Pin, &PwmMap))
        {
            u32DutyPercent = ((uint32_t)u8Value * ANALOG_PWM_MAX_PERCENT) /
                             (uint32_t)ANALOG_PWM_MAX_VALUE;

            if (ANALOG_PWM_MAX_PERCENT < u32DutyPercent)
            {
                u32DutyPercent = ANALOG_PWM_MAX_PERCENT;
            }

            (void)FTM_SetPwmDutyPercent(PwmMap.instance,
                                        PwmMap.channel,
                                        (uint8_t)u32DutyPercent);
        }
    }
}