#include "S32K144.h"
#include "clock.h"
#include "lpuart.h"

int main(void)
{
    LPUART_Config_t uart1Config;
    char ch;

    SOSC_init_8MHz();
    SPLL_init_160MHz();
    NormalRUNmode_80MHz();

    /* UART1 config for polling test */
    uart1Config.baudRate = 9600U;
    uart1Config.srcClockHz = 8000000U;

    if (LPUART_Init(IP_LPUART1, &uart1Config) != LPUART_STATUS_OK)
    {
        while (1)
        {
            /* Init fail: stay here */
        }
    }

    LPUART_WriteString(IP_LPUART1, "\r\n==============================\r\n");
    LPUART_WriteString(IP_LPUART1, "EduFramework UART1 Polling Test\r\n");
    LPUART_WriteString(IP_LPUART1, "Baud: 9600, 8N1\r\n");
    LPUART_WriteString(IP_LPUART1, "Type any character to echo...\r\n");
    LPUART_WriteString(IP_LPUART1, "==============================\r\n");

    while (1)
    {
        LPUART_WriteString(IP_LPUART1, "\r\n> ");
        ch = LPUART_ReadChar(IP_LPUART1);

        LPUART_WriteString(IP_LPUART1, "You typed: ");
        LPUART_WriteChar(IP_LPUART1, ch);
    }
}
