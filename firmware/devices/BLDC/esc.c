/**
 * @file esc.c
 * @brief Arduino-style ESC driver implementation.
 */

#include "esc.h"
#include "port.h"
#include <stddef.h>

/* ========================================================================= */
/* Private Configuration Constants                                           */
/* ========================================================================= */

/**
 * Assume S32K144 Normal RUN Mode with 80MHz SPLL for FTM.
 * FTM Prescaler is set to 32 -> Timer Clock = 2.5 MHz.
 * Period for 50Hz = 50,000 counts (fits in 16-bit register).
 */
#define ESC_FTM_SRC_CLOCK_HZ    (80000000UL)
#define ESC_FTM_PRESCALER_VAL   (32UL)
#define ESC_FTM_PRESCALER_ENUM  (FTM_PRESCALER_DIV_32)
#define ESC_PWM_FREQ_HZ         (50U)

/* ========================================================================= */
/* Public API Implementation                                                 */
/* ========================================================================= */

/**
 * @copydoc ESC_Init
 */
bool ESC_Init(ESC_t *esc, uint8_t pin)
{
    if ((esc == NULL) || (Arduino_HasPwmCapability(pin) == ARDUINO_VALID_FALSE)) //[cite: 4]
    {
        return false;
    }

    ArduinoPwmMap_t pwmMap;
    (void)Arduino_GetPwmMap(pin, &pwmMap); //[cite: 4]

    /* 1. Setup object context */
    esc->logicalPin = pin;
    esc->instance = pwmMap.instance;
    esc->channel = pwmMap.channel;
    esc->minPulseUs = ESC_DEFAULT_MIN_PULSE_US;
    esc->maxPulseUs = ESC_DEFAULT_MAX_PULSE_US;
    esc->timerClockHz = ESC_FTM_SRC_CLOCK_HZ / ESC_FTM_PRESCALER_VAL;

    /* 2. Enable Port Clock by calling standard pinMode[cite: 4] */
    pinMode(pin, OUTPUT); //[cite: 4]

    /* 3. Override pin mux to route FTM signal to the pin[cite: 4] */
    const ArduinoPinMap_t *pinData = &g_arduinoPinMap[pin]; //[cite: 4]
    PORT_SetPinMux(pinData->portBase, pinData->pinNumber, pwmMap.mux); //[cite: 4]

    /* 4. Configure FTM Timer for 50Hz[cite: 4] */
    FTM_PwmConfig_t pwmConfig;
    pwmConfig.srcClockHz = ESC_FTM_SRC_CLOCK_HZ;
    pwmConfig.pwmFreqHz = ESC_PWM_FREQ_HZ;
    pwmConfig.clockSource = FTM_CLOCK_SOURCE_SYSTEM; //[cite: 4]
    pwmConfig.prescaler = ESC_FTM_PRESCALER_ENUM;

    (void)FTM_InitPwm(esc->instance, &pwmConfig); //[cite: 4]
    (void)FTM_SetChannelModePwm(esc->instance, esc->channel, FTM_PWM_EDGE_ALIGNED_HIGH_TRUE); //[cite: 4]
    (void)FTM_StartCounter(esc->instance); //[cite: 4]

    /* 5. Start with minimum pulse to avoid accidental spin */
    ESC_SetMicroseconds(esc, 0U);

    return true;
}

/**
 * @copydoc ESC_SetPulseRange
 */
void ESC_SetPulseRange(ESC_t *esc, uint16_t minUs, uint16_t maxUs)
{
    if (esc != NULL)
    {
        esc->minPulseUs = minUs;
        esc->maxPulseUs = maxUs;
    }
}

/**
 * @copydoc ESC_Arm
 */
void ESC_Arm(ESC_t *esc)
{
    if (esc != NULL)
    {
        /* Send 1000us minimum pulse */
        ESC_SetMicroseconds(esc, esc->minPulseUs);

        /* Hold this signal for 3 seconds to arm the ESC[cite: 4] */
        delay(3000); //[cite: 4]
    }
}

/**
 * @copydoc ESC_SetThrottle
 */
void ESC_SetThrottle(ESC_t *esc, uint8_t percent)
{
    if (esc != NULL)
    {
        if (percent > 100U)
        {
            percent = 100U;
        }

        uint16_t pulseRange = esc->maxPulseUs - esc->minPulseUs;
        uint16_t targetUs = esc->minPulseUs + ((pulseRange * (uint16_t)percent) / 100U);

        ESC_SetMicroseconds(esc, targetUs);
    }
}

/**
 * @copydoc ESC_SetMicroseconds
 */
void ESC_SetMicroseconds(ESC_t *esc, uint16_t us)
{
    if (esc != NULL)
    {
        /* Calculate FTM compare counts:
         * dutyCounts = (us * timerClockHz) / 1,000,000
         */
        uint32_t counts32 = ((uint32_t)us * esc->timerClockHz) / 1000000UL;
        uint16_t dutyCounts = (uint16_t)counts32;

        (void)FTM_SetPwmDuty(esc->instance, esc->channel, dutyCounts); //[cite: 4]
    }
}
