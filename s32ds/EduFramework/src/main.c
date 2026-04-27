#include "S32K144.h"
#include "clock.h"
#include "lpuart.h"
#include "lpi2c.h"
#include "irq.h"
#include <stdio.h>

#define MAX30102_ADDR       0x57U
#define MAX30102_REV_ID     0xFEU
#define MAX30102_PART_ID    0xFFU

static void WaitI2CDone(LPI2C_MasterHandle_t *handle)
{
    while ((LPI2C_MasterGetState(handle) != LPI2C_MASTER_STATE_DONE) &&
           (LPI2C_MasterGetState(handle) != LPI2C_MASTER_STATE_ERROR))
    {
    }
}

static void TestReadRegIT(uint8_t reg, const char *name)
{
    LPI2C_MasterHandle_t handle;
    LPI2C_MasterTransfer_t transfer;
    LPI2C_Status_t status;
    uint8_t value = 0U;
    char msg[100];

    transfer.slaveAddress = MAX30102_ADDR;
    transfer.txData = &reg;
    transfer.txSize = 1U;
    transfer.rxData = &value;
    transfer.rxSize = 1U;
    transfer.type = LPI2C_TRANSFER_WRITE_READ;
    transfer.sendStop = true;

    status = LPI2C_MasterTransferIT(IP_LPI2C0, &handle, &transfer);

    sprintf(msg, "Start IT read %s status=%d\r\n", name, status);
    LPUART_WriteString(IP_LPUART1, msg);

    if (status == LPI2C_STATUS_OK)
    {
        WaitI2CDone(&handle);

        sprintf(msg,
                "%s state=%d status=%d value=0x%02X\r\n",
                name,
                LPI2C_MasterGetState(&handle),
                LPI2C_MasterGetStatus(&handle),
                value);

        LPUART_WriteString(IP_LPUART1, msg);
    }
}

int main(void)
{
    LPUART_Config_t uartConfig;
    LPI2C_MasterConfig_t i2cConfig;

    SOSC_init_8MHz();
    SPLL_init_160MHz();
    NormalRUNmode_80MHz();

    uartConfig.baudRate = 9600U;
    uartConfig.srcClockHz = 8000000U;
    LPUART_Init(IP_LPUART1, &uartConfig);

    LPI2C_MasterGetDefaultConfig(&i2cConfig);
    i2cConfig.baudRate = 100000U;
    LPI2C_MasterInit(IP_LPI2C0, &i2cConfig);

    IRQ_LPI2C0_Master_Init();

    LPUART_WriteString(IP_LPUART1, "\r\nLPI2C MASTER IT WRITEREAD TEST\r\n");

    while (1)
    {
        TestReadRegIT(MAX30102_REV_ID, "REV_ID");
        TestReadRegIT(MAX30102_PART_ID, "PART_ID");

        for (volatile uint32_t i = 0; i < 5000000U; i++)
        {
        }
    }
}
