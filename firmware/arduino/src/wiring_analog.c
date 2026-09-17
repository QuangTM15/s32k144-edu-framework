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
 * PWM output is implemented through the internal wiring_pwm module.
 * This file is responsible only for Arduino-style value scaling and
 * delegates PWM hardware configuration to the shared PWM service.
 *
 * The implementation keeps hardware-specific register access inside the
 * low-level drivers.
 */

#include "wiring_analog.h"
#include "wiring_pwm.h"
#include "arduino_pins.h"
#include "adc.h"
#include "irq.h"

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
 * @brief Maximum Arduino-style PWM input value.
 */
#define ANALOG_PWM_MAX_VALUE                 (255U)

/**
 * @brief Maximum PWM duty percentage used by analogWrite().
 */
#define ANALOG_PWM_MAX_PERCENT               (100UL)

/* ============================================================
 * Internal state
 * ============================================================ */

static uint8_t s_u8AnalogInitialized = ANALOG_FALSE;
static uint8_t s_u8AnalogConversionActive = ANALOG_FALSE;

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
    uint32_t u32DutyPercent = 0UL;

    if ((ANALOG_FALSE != Arduino_IsValidPin(u8Pin)) &&
        (ANALOG_FALSE != Arduino_HasPwmCapability(u8Pin)))
    {
        u32DutyPercent = ((uint32_t)u8Value * ANALOG_PWM_MAX_PERCENT) /
                         (uint32_t)ANALOG_PWM_MAX_VALUE;

        if (ANALOG_PWM_MAX_PERCENT < u32DutyPercent)
        {
            u32DutyPercent = ANALOG_PWM_MAX_PERCENT;
        }

        (void)WiringPwm_SetDutyPercent(
            u8Pin,
            (uint8_t)u32DutyPercent);
    }
}
