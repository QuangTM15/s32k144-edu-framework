#include "Arduino.h"
#include "analog_read_nonblocking.h"

void Demo_AnalogReadNonBlocking(void)
{
    int value;
    uint8_t started = 0U;

    setup();

    Serial1_begin(9600);
    Serial1_println("=== Analog Read Non-Blocking Demo ===");

    while (1)
    {
        if (started == 0U)
        {
            analogStart(ADC0_SE12);
            started = 1U;
        }
        else
        {
            if (analogAvailable() != 0U)
            {
                value = analogGetResult();

                Serial1_print("ADC0_SE12 = ");
                Serial1_printlnInt(value);

                started = 0U;
                delay(200);
            }
        }

        delay(10);
    }
}
