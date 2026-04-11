#include "Arduino.h"
#include "blink_led.h"

void Demo_BlinkLed(void)
{
    uint32_t lastGreen = 0U;
    uint32_t lastBlue  = 0U;
    uint8_t redTick = 0U;

    setup();

    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);

    /* All OFF initially
       Note: board LED is active-low */
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, HIGH);

    while (1)
    {
        /* Base tick using delay(): 100 ms */
        delay(100);
        redTick++;

        /* RED LED: 500 ms using delay-based tick */
        if (redTick >= 5U)
        {
            redTick = 0U;
            digitalToggle(LED_RED);
        }

        /* GREEN LED: 700 ms using millis() */
        if ((millis() - lastGreen) >= 700U)
        {
            lastGreen = millis();
            digitalToggle(LED_GREEN);
        }

        /* BLUE LED: 1100 ms using millis() */
        if ((millis() - lastBlue) >= 1100U)
        {
            lastBlue = millis();
            digitalToggle(LED_BLUE);
        }
    }
}
