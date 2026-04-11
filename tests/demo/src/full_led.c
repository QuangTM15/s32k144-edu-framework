#include "Arduino.h"
#include "full_led.h"

static void FullLed_UpdateState(unsigned int step)
{
    switch (step)
    {
        case 1U:
            /* Red ON */
            digitalWrite(LED_RED, LOW);
            digitalWrite(LED_BLUE, HIGH);
            digitalWrite(LED_GREEN, HIGH);
            break;

        case 2U:
            /* Red + Blue ON */
            digitalWrite(LED_RED, LOW);
            digitalWrite(LED_BLUE, LOW);
            digitalWrite(LED_GREEN, HIGH);
            break;

        case 3U:
            /* Red + Blue + Green ON */
            digitalWrite(LED_RED, LOW);
            digitalWrite(LED_BLUE, LOW);
            digitalWrite(LED_GREEN, LOW);
            break;

        case 4U:
        default:
            /* All OFF */
            digitalWrite(LED_RED, HIGH);
            digitalWrite(LED_BLUE, HIGH);
            digitalWrite(LED_GREEN, HIGH);
            break;
    }
}

void Demo_FullLed(void)
{
    setup();
    bool lastBtn0 = true;
    unsigned int step = 4U; /* start with all OFF */

    pinMode(BTN0, INPUT_PULLUP);

    pinMode(LED_RED, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);

    /* Initial state: all OFF */
    FullLed_UpdateState(step);

    while (1)
    {
        bool btn0 = digitalRead(BTN0);

        /* Detect falling edge: button pressed */
        if ((lastBtn0 == true) && (btn0 == false))
        {
            step++;

            if (step > 4U)
            {
                step = 1U;
            }

            FullLed_UpdateState(step);
        }

        lastBtn0 = btn0;
    }
}
