#include "Arduino.h"
#include "analog_full_demo.h"

void Demo_AnalogFull(void)
{
    int adcValue;
    uint8_t pwmValue;

    setup();
    while (1)
    {
        adcValue = analogRead(ADC0_SE12);
        if (adcValue < 0)
        {
            continue;
        }

        /* Convert ADC 12-bit (0..4095) to PWM 8-bit (0..255) */
        pwmValue = (uint8_t)(((uint32_t)adcValue * 255U) / 4095U);

        /* LED brightness follows potentiometer */
        analogWrite(LED_RED, pwmValue);
    }
}
