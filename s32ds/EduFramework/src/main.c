#include "Arduino.h"
#include "ntc.h"
int main () {
    float temperature = 0.0F;
    float resistance = 0.0F;
    int millivolts = 0;
    setup();
    Serial1_begin(9600);
    NTC_Init();

    while (1)
    {
        millivolts = NTC_ReadMilliVolts(ADC0_SE13);
        resistance = NTC_ReadResistance(ADC0_SE13);
        temperature = NTC_ReadCelsius(ADC0_SE13);

        Serial1_print("NTC mV: ");
        Serial1_printlnInt(millivolts);

        Serial1_print("NTC Ohm: ");
        Serial1_printlnFloat(resistance);

        Serial1_print("Temp C: ");
        Serial1_printlnFloat(temperature);

        Serial1_println("----------------");

        delay(1000);
    }
}
