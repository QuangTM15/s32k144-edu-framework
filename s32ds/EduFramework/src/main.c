#include "S32K144.h"
#include "clock.h"
#include "lpuart.h"
#include "spi_hw.h"
#include "stdio.h"

/* =========================
 * CHON ROLE
 * 1 = MASTER
 * 0 = SLAVE
 * ========================= */
#define SPI_TEST_ROLE   0

static void delay_simple(volatile uint32_t count)
{
    while (count--)
    {
    }
}

static void UART_DebugInit(void)
{
    LPUART_Config_t uartCfg = {
        .baudRate = 9600U,
        .srcClockHz = 8000000U
    };

    LPUART_Init(IP_LPUART1, &uartCfg);
}

int main(void)
{
    char msg[64];
    uint8_t rxData = 0U;

    SOSC_init_8MHz();
    SPLL_init_160MHz();
    NormalRUNmode_80MHz();

    UART_DebugInit();

#if (SPI_TEST_ROLE == 1)

    SPI_HW_MasterConfig_t spiCfg = {
        .srcClockHz = 40000000U,
        .baudRate = 1000000U,
        .mode = SPI_HW_MODE0,
        .bitOrder = SPI_HW_MSBFIRST
    };

    LPUART_WriteString(IP_LPUART1, "SPI MASTER start\r\n");

    if (SPI_HW_MasterInit(&spiCfg) != SPI_HW_STATUS_OK)
    {
        LPUART_WriteString(IP_LPUART1, "Master init failed\r\n");
        while (1)
        {
        }
    }

    while (1)
    {
        /* Master gui 0x55, ky vong doc ve 0xA5 */
        if (SPI_HW_MasterTransfer8(0x55U, &rxData) == SPI_HW_STATUS_OK)
        {
            sprintf(msg, "MASTER RX = 0x%02X\r\n", rxData);
            LPUART_WriteString(IP_LPUART1, msg);
        }
        else
        {
            LPUART_WriteString(IP_LPUART1, "MASTER transfer failed\r\n");
        }

        delay_simple(1000000U);
    }

#else

    SPI_HW_SlaveConfig_t spiCfg = {
        .mode = SPI_HW_MODE0,
        .bitOrder = SPI_HW_MSBFIRST
    };

    LPUART_WriteString(IP_LPUART1, "SPI SLAVE start\r\n");

    if (SPI_HW_SlaveInit(&spiCfg) != SPI_HW_STATUS_OK)
    {
        LPUART_WriteString(IP_LPUART1, "Slave init failed\r\n");
        while (1)
        {
        }
    }

    /* Preload byte dau tien de master doc ve */
    SPI_HW_SlaveWrite8(0xA5U);
    LPUART_WriteString(IP_LPUART1, "SLAVE preload = 0xA5\r\n");

    while (1)
    {
        if (SPI_HW_SlaveRead8(&rxData) == SPI_HW_STATUS_OK)
        {
            sprintf(msg, "SLAVE RX = 0x%02X\r\n", rxData);
            LPUART_WriteString(IP_LPUART1, msg);

            /* Nap lai byte cho lan transfer tiep theo */
            SPI_HW_SlaveWrite8(0xA5U);
        }
        else
        {
            LPUART_WriteString(IP_LPUART1, "SLAVE read failed\r\n");
        }

        delay_simple(100000U);
    }

#endif
}
