#include "wiring_analog.h"
#include "arduino_pins.h"
#include "adc.h"
#include "irq.h"

/* ============================================================
 * Default analog configuration
 * ============================================================ */
#define ANALOG_ADC_INSTANCE          (IP_ADC_0)
#define ANALOG_ADC_SRC_CLOCK_HZ      (8000000U)
#define ANALOG_ADC_SAMPLE_TIME       (12U)

/*
 * NOTE:
 * Set this to the actual ADC reference voltage used by your board.
 * Current default is 5000 mV to match the cookbook-style scaling.
 */
#define ANALOG_REFERENCE_MV          (5000U)
#define ANALOG_MAX_12BIT_VALUE       (4095U)

/* ============================================================
 * Internal state
 * ============================================================ */
static uint8_t g_analogInitialized = 0U;
static uint8_t g_analogConversionActive = 0U;

/* ============================================================
 * Internal helpers
 * ============================================================ */
static uint8_t Analog_IsValidAnalogPin(uint8_t pin)
{
    if ((pin == ADC0_SE12) || (pin == ADC0_SE13))
    {
        return 1U;
    }

    return 0U;
}

static ADC_Channel_t Analog_PinToChannel(uint8_t pin)
{
    ADC_Channel_t channel = ADC_CHANNEL_SE12;

    switch (pin)
    {
    case ADC0_SE12:
        channel = ADC_CHANNEL_SE12;
        break;

    case ADC0_SE13:
        channel = ADC_CHANNEL_SE13;
        break;

    default:
        channel = ADC_CHANNEL_SE12;
        break;
    }

    return channel;
}

/* ============================================================
 * Public API
 * ============================================================ */
void analogInit(void)
{
    ADC_Config_t adcConfig;

    if (g_analogInitialized != 0U)
    {
        return;
    }

    adcConfig.srcClockHz = ANALOG_ADC_SRC_CLOCK_HZ;
    adcConfig.resolution = ADC_RESOLUTION_12BIT;
    adcConfig.reference = ADC_REF_DEFAULT;
    adcConfig.sampleTime = ANALOG_ADC_SAMPLE_TIME;
    adcConfig.average = ADC_AVERAGE_DISABLED;
    adcConfig.enableInterrupt = 1U;

    if (ADC_Init(ANALOG_ADC_INSTANCE, &adcConfig) != ADC_STATUS_OK)
    {
        return;
    }

    if (ADC_Calibrate(ANALOG_ADC_INSTANCE) != ADC_STATUS_OK)
    {
        return;
    }

    IRQ_ADC0_Init();

    g_analogInitialized = 1U;
    g_analogConversionActive = 0U;
}

int analogRead(uint8_t pin)
{
    if (g_analogInitialized == 0U)
    {
        analogInit();
    }

    if (g_analogInitialized == 0U)
    {
        return -1;
    }

    if (Analog_IsValidAnalogPin(pin) == 0U)
    {
        return -1;
    }

    analogStart(pin);

    if (g_analogConversionActive == 0U)
    {
        return -1;
    }

    while (analogAvailable() == 0U)
    {
    }

    return analogGetResult();
}

void analogStart(uint8_t pin)
{
    ADC_Channel_t channel;

    if (g_analogInitialized == 0U)
    {
        analogInit();
    }

    if (g_analogInitialized == 0U)
    {
        return;
    }

    if (Analog_IsValidAnalogPin(pin) == 0U)
    {
        return;
    }

    if (g_analogConversionActive != 0U)
    {
        return;
    }

    channel = Analog_PinToChannel(pin);

    if (ADC_StartConversion_IT(ANALOG_ADC_INSTANCE, channel) == ADC_STATUS_OK)
    {
        g_analogConversionActive = 1U;
    }
}

uint8_t analogAvailable(void)
{
    if (g_analogInitialized == 0U)
    {
        return 0U;
    }

    if (g_analogConversionActive == 0U)
    {
        return 0U;
    }

    return ADC_IsDone(ANALOG_ADC_INSTANCE);
}

int analogGetResult(void)
{
    uint16_t result = 0U;

    if (g_analogInitialized == 0U)
    {
        return -1;
    }

    if (g_analogConversionActive == 0U)
    {
        return -1;
    }

    if (ADC_GetResult(ANALOG_ADC_INSTANCE, &result) != ADC_STATUS_OK)
    {
        return -1;
    }

    g_analogConversionActive = 0U;

    return (int)result;
}

int analogReadMilliVolts(uint8_t pin)
{
    int rawValue;
    uint32_t milliVolts;

    rawValue = analogRead(pin);
    if (rawValue < 0)
    {
        return -1;
    }

    milliVolts = ((uint32_t)rawValue * ANALOG_REFERENCE_MV) / ANALOG_MAX_12BIT_VALUE;

    return (int)milliVolts;
}
