#include "S32K144.h"
#include "clock.h"
#include "lpuart.h"
#include "irq.h"

#define APP_RX_LINE_BUFFER_SIZE    (64U)

int main(void)
{
    LPUART_Config_t uart1Config;
    char ch;
    char rxLine[APP_RX_LINE_BUFFER_SIZE];
    uint32_t rxIndex = 0U;

    /* ============================================================
     * System Clock Initialization
     * ============================================================ */
    SOSC_init_8MHz();
    SPLL_init_160MHz();
    NormalRUNmode_80MHz();

    /* ============================================================
     * UART1 Configuration
     * ============================================================ */
    uart1Config.baudRate = 9600U;
    uart1Config.srcClockHz = 8000000U;

    if (LPUART_Init(IP_LPUART1, &uart1Config) != LPUART_STATUS_OK)
    {
        while (1)
        {
            /* UART init failed */
        }
    }

    /* ============================================================
     * Enable UART1 RX Interrupt
     * ============================================================ */
    IRQ_LPUART1_RxTx_Init();
    LPUART_EnableRxInterrupt(IP_LPUART1);

    /* ============================================================
     * Welcome Message
     * ============================================================ */
    LPUART_WriteString(IP_LPUART1, "\r\n=== UART1 STRING ECHO TEST ===\r\n");
    LPUART_WriteString(IP_LPUART1, "Type a line and press Enter:\r\n");

    /* ============================================================
     * Main Loop
     * ============================================================ */
    while (1)
    {
        while (LPUART_IsDataAvailable(IP_LPUART1))
        {
            ch = LPUART_GetChar(IP_LPUART1);

            /* Ignore '\r', use '\n' as end of line */
            if (ch == '\r')
            {
                continue;
            }

            if (ch == '\n')
            {
                rxLine[rxIndex] = '\0';

                LPUART_WriteString(IP_LPUART1, "\r\nEcho string: ");
                LPUART_WriteString(IP_LPUART1, rxLine);
                LPUART_WriteString(IP_LPUART1, "\r\n");

                rxIndex = 0U;
            }
            else
            {
                if (rxIndex < (APP_RX_LINE_BUFFER_SIZE - 1U))
                {
                    rxLine[rxIndex] = ch;
                    rxIndex++;
                }
                else
                {
                    /* Buffer full: terminate current string and report */
                    rxLine[rxIndex] = '\0';

                    LPUART_WriteString(IP_LPUART1, "\r\nBuffer full: ");
                    LPUART_WriteString(IP_LPUART1, rxLine);
                    LPUART_WriteString(IP_LPUART1, "\r\n");

                    rxIndex = 0U;
                }
            }
        }
    }
}
