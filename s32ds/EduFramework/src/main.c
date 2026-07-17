#include "Arduino.h"
#include "ntc_uart.h"

int main(void)
{
    setup();

    NTC_UART_Example();

    while (1)
    {
    }
}