#include "S32K144.h"
#include "clock.h"
#include "lpuart.h"
#include "irq.h"

/*
 * ============================================================
 * Main Application - UART Interrupt Test
 *
 * Flow:
 *  - Init clock
 *  - Init UART1
 *  - Enable interrupt
 *  - Poll driver for received data
 * ============================================================
 */

int main(void)
{
    LPUART_Config_t uart1Config;
    char ch;

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
            /* Initialization failed */
        }
    }

    /* ============================================================
     * Enable UART1 Interrupt
     * ============================================================ */
    IRQ_LPUART1_RxTx_Init();
    LPUART_EnableRxInterrupt(IP_LPUART1);

    /* ============================================================
     * Welcome Message
     * ============================================================ */
    LPUART_WriteString(IP_LPUART1, "\r\n=== UART1 INTERRUPT TEST ===\r\n");
    LPUART_WriteString(IP_LPUART1, "Type any key...\r\n");

    /* ============================================================
     * Main Loop
     * ============================================================ */
    while (1)
    {
        /* Check if new data is available (from interrupt) */
        if (LPUART_IsDataAvailable(IP_LPUART1))
        {
            ch = LPUART_GetChar(IP_LPUART1);

            /* Echo back */
            LPUART_WriteString(IP_LPUART1, "\r\nInterrupt received: ");
            LPUART_WriteChar(IP_LPUART1, ch);
        }
    }
}
