/**
 * @file ntc_uart.c
 * @brief NTC temperature sensor UART example.
 *
 * @details
 * Demonstrates the simplest way to use the EduFramework
 * NTC device library with Serial1.
 *
 * Hardware connection:
 *
 * - NTC module VCC  -> 3.3V
 * - NTC module GND  -> GND
 * - NTC module AO   -> ADC0_SE13
 *
 * Every second the example prints:
 *
 * - ADC voltage (mV)
 * - Resistance (Ohm)
 * - Temperature (°C)
 */

#include "Arduino.h"
#include "ntc.h"
#include "ntc_uart.h"

void NTC_UART_Example(void)
{
    float temperature = 0.0F;
    float resistance = 0.0F;
    int millivolts = 0;

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