#include "Arduino.h"
#include "analog_read_blocking.h"

void Demo_AnalogReadBlocking(void)
{
    int value;
    int milliVolts;

    setup();

    Serial1_begin(9600);
    Serial1_println("=== Analog Read Blocking Demo ===");

    while (1)
    {
        value = analogRead(ADC0_SE12);
        milliVolts = analogReadMilliVolts(ADC0_SE12);

        Serial1_print("ADC0_SE12 = ");
        Serial1_printlnInt(value);

        Serial1_print("Voltage = ");
        Serial1_printInt(milliVolts);
        Serial1_println(" mV");

        delay(200);
    }
}