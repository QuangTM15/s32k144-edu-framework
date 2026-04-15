#include "wiring_analog.h"
#include "arduino_pins.h"
#include "adc.h"
#include "irq.h"
#include "ftm.h"


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

#define ANALOG_PWM_DEFAULT_FREQ_HZ   (1000U)
#define ANALOG_PWM_MAX_VALUE         (255U)

/* ============================================================
 * Internal state
 * ============================================================ */
static uint8_t g_analogInitialized = 0U;
static uint8_t g_analogConversionActive = 0U;

static uint8_t g_pwmInitialized[3] = {0U, 0U, 0U};
static uint8_t g_pwmChannelConfigured[3][8] = {{0U}};

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

static void Analog_EnablePortClock(PORT_Type *base)
{
    if (base == IP_PORTA)
    {
        PORT_EnableClock(PORT_NAME_A);
    }
    else if (base == IP_PORTB)
    {
        PORT_EnableClock(PORT_NAME_B);
    }
    else if (base == IP_PORTC)
    {
        PORT_EnableClock(PORT_NAME_C);
    }
    else if (base == IP_PORTD)
    {
        PORT_EnableClock(PORT_NAME_D);
    }
    else if (base == IP_PORTE)
    {
        PORT_EnableClock(PORT_NAME_E);
    }
    else
    {
    }
}

static uint8_t Analog_EnsurePwmInitialized(FTM_Instance_t instance)
{
    FTM_PwmConfig_t pwmConfig;

    if ((uint8_t)instance > 2U)
    {
        return 0U;
    }

    if (g_pwmInitialized[(uint8_t)instance] != 0U)
    {
        return 1U;
    }

    pwmConfig.srcClockHz = 8000000U;
    pwmConfig.pwmFreqHz = ANALOG_PWM_DEFAULT_FREQ_HZ;
    pwmConfig.clockSource = FTM_CLOCK_SOURCE_EXTERNAL;
    pwmConfig.prescaler = FTM_PRESCALER_DIV_1;

    if (FTM_InitPwm(instance, &pwmConfig) != FTM_STATUS_OK)
    {
        return 0U;
    }

    g_pwmInitialized[(uint8_t)instance] = 1U;
    return 1U;
}

static uint8_t Analog_EnsurePwmPinConfigured(uint8_t pin, ArduinoPwmMap_t *pwmMap)
{
    const ArduinoPinMap_t *pinMap;

    if (Arduino_GetPwmMap(pin, pwmMap) == 0U)
    {
        return 0U;
    }

    pinMap = &g_arduinoPinMap[pin];

    Analog_EnablePortClock(pinMap->portBase);
    PORT_SetPinMux(pinMap->portBase, pinMap->pinNumber, pwmMap->mux);

    if (Analog_EnsurePwmInitialized(pwmMap->instance) == 0U)
    {
        return 0U;
    }

    if (g_pwmChannelConfigured[(uint8_t)pwmMap->instance][(uint8_t)pwmMap->channel] == 0U)
    {
        if (FTM_SetChannelModePwm(pwmMap->instance,
                                  pwmMap->channel,
                                  FTM_PWM_EDGE_ALIGNED_LOW_TRUE) != FTM_STATUS_OK)
        {
            return 0U;
        }

        g_pwmChannelConfigured[(uint8_t)pwmMap->instance][(uint8_t)pwmMap->channel] = 1U;
    }

    if (FTM_StartCounter(pwmMap->instance) != FTM_STATUS_OK)
    {
        return 0U;
    }

    return 1U;
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

void analogWrite(uint8_t pin, uint8_t value)
{
    ArduinoPwmMap_t pwmMap;
    uint32_t dutyPercent;

    if (Arduino_IsValidPin(pin) == 0U)
    {
        return;
    }

    if (Arduino_HasPwmCapability(pin) == 0U)
    {
        return;
    }

    if (Analog_EnsurePwmPinConfigured(pin, &pwmMap) == 0U)
    {
        return;
    }

    dutyPercent = ((uint32_t)value * 100U) / ANALOG_PWM_MAX_VALUE;

    if (dutyPercent > 100U)
    {
        dutyPercent = 100U;
    }

    (void)FTM_SetPwmDutyPercent(pwmMap.instance,
                                pwmMap.channel,
                                (uint8_t)dutyPercent);
}
