#include "Arduino.h"
#include "uart_led.h"

void Demo_UartLedCommand(void)
{
    char ch;
    float redState = 0.0f;
    float greenState = 0.0f;
    float blueState = 0.0f;

    setup();

    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);

    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, HIGH);

    Serial1_begin(9600);

    Serial1_println("UART LED SIMPLE DEMO");
    Serial1_println("Send: r / g / b");

    while (1)
    {
        if (Serial1_available())
        {
            ch = Serial1_read();

            if ((ch == 'r') || (ch == 'R'))
            {
                digitalToggle(LED_RED);
                redState = (redState == 0.0f) ? 1.0f : 0.0f;

                Serial1_print("RED: ");
                Serial1_printlnFloat(redState);
            }
            else if ((ch == 'g') || (ch == 'G'))
            {
                digitalToggle(LED_GREEN);
                greenState = (greenState == 0.0f) ? 1.0f : 0.0f;

                Serial1_print("GREEN: ");
                Serial1_printlnFloat(greenState);
            }
            else if ((ch == 'b') || (ch == 'B'))
            {
                digitalToggle(LED_BLUE);
                blueState = (blueState == 0.0f) ? 1.0f : 0.0f;

                Serial1_print("BLUE: ");
                Serial1_printlnFloat(blueState);
            }
        }
    }
}
