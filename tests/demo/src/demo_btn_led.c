#include <stdbool.h>
#include "wiring_digital.h"
#include "demo_btn_led.h"
#include "arduino_pins.h"

void Demo_ButtonLed(void)
{
    bool lastBtn0 = true;
    bool lastBtn1 = true;
    bool redState = false;

    pinMode(BTN0, INPUT_PULLUP);
    pinMode(BTN1, INPUT_PULLUP);

    pinMode(LED_BLUE, OUTPUT);
    pinMode(LED_RED, OUTPUT);

    digitalWrite(LED_BLUE, LOW);
    digitalWrite(LED_RED, LOW);

    while (1)
    {
        bool btn0 = digitalRead(BTN0);
        bool btn1 = digitalRead(BTN1);

        if ((lastBtn0 == true) && (btn0 == false))
        {
            digitalToggle(LED_BLUE);
        }

        if ((lastBtn1 == true) && (btn1 == false))
        {
            redState = !redState;
            digitalWrite(LED_RED, redState ? HIGH : LOW);
        }

        lastBtn0 = btn0;
        lastBtn1 = btn1;
    }
}
