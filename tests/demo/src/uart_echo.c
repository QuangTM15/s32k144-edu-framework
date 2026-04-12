#include "Arduino.h"
#include "uart_echo.h"

#define RX_BUFFER_SIZE (64U)

void Demo_UartEcho(void)
{
    char rxBuffer[RX_BUFFER_SIZE];
    uint32_t index = 0U;
    char ch;

    setup();

    Serial1_begin(9600);

    Serial1_print("=== UART1 ECHO DEMO ===\n");
    Serial1_println("Type something and press Enter:");

    while (1)
    {
        if (Serial1_available())
        {
            ch = Serial1_read();

            if (ch == '\r')
            {
                continue;
            }

            if (ch == '\n')
            {
                rxBuffer[index] = '\0';

                Serial1_print("Echo: ");
                Serial1_println(rxBuffer);

                index = 0U;
            }
            else
            {
                if (index < (RX_BUFFER_SIZE - 1U))
                {
                    rxBuffer[index] = ch;
                    index++;
                }
                else
                {
                    /* overflow -> reset */
                    index = 0U;
                }
            }
        }
    }
}
