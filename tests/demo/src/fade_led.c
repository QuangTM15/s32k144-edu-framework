#include "Arduino.h"
#include "fade_led.h"

void Demo_FadeLed(void)
{
    uint16_t value;

    setup();

    while (1)
    {

        for (value = 0U; value <= 255U; value++)
        {
            analogWrite(LED_RED, (uint8_t)value);
            delay(5);
        }

        for (value = 255U; value > 0U; value--)
        {
            analogWrite(LED_RED, (uint8_t)(value - 1U));
            delay(5);
        }

        for (value = 0U; value <= 255U; value++)
        {
            analogWrite(LED_BLUE, (uint8_t)value);
            delay(5);
        }

        for (value = 255U; value > 0U; value--)
        {
            analogWrite(LED_BLUE, (uint8_t)(value - 1U));
            delay(5);
        }

        for (value = 0U; value <= 255U; value++)
        {
            analogWrite(LED_GREEN, (uint8_t)value);
            delay(5);
        }

        for (value = 255U; value > 0U; value--)
        {
            analogWrite(LED_GREEN, (uint8_t)(value - 1U));
            delay(5);
        }


        for (value = 0U; value <= 255U; value++)
        {
            analogWrite(LED_RED,   (uint8_t)value);
            analogWrite(LED_BLUE,  (uint8_t)value);
            analogWrite(LED_GREEN, (uint8_t)value);
            delay(5);
        }

        for (value = 255U; value > 0U; value--)
        {
            analogWrite(LED_RED,   (uint8_t)(value - 1U));
            analogWrite(LED_BLUE,  (uint8_t)(value - 1U));
            analogWrite(LED_GREEN, (uint8_t)(value - 1U));
            delay(5);
        }
    }
}