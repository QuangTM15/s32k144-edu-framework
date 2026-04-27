#include "S32K144.h"
#include "clock.h"
#include "lpuart.h"
#include "lpi2c.h"
#include "irq.h"

#include "port.h"
#include "gpio.h"

#include <stdio.h>
#include <stdbool.h>

#define IS_MASTER       0

#define SLAVE_ADDR      0x12U
#define I2C_TIMEOUT     100000U

#define SLAVE_REPLY     0x5AU

#define BTN_PORT_NAME   PORT_NAME_C
#define BTN_PORT        IP_PORTC
#define BTN_GPIO        IP_PTC
#define BTN_PIN         12U

#define LED_PORT_NAME   PORT_NAME_D
#define LED_PORT        IP_PORTD
#define LED_GPIO        IP_PTD
#define LED_PIN         15U

static void delay_loop(uint32_t count)
{
    while (count--)
    {
        __asm("nop");
    }
}

static void BTN_Init(void)
{
    PORT_EnableClock(BTN_PORT_NAME);
    PORT_SetPinMux(BTN_PORT, BTN_PIN, PORT_MUX_GPIO);
    PORT_SetPinPull(BTN_PORT, BTN_PIN, PORT_PULL_UP);
    PORT_SetPassiveFilter(BTN_PORT, BTN_PIN, true);
    GPIO_SetPinDirection(BTN_GPIO, BTN_PIN, GPIO_DIRECTION_INPUT);
}

static bool BTN_IsPressed(void)
{
    return (GPIO_ReadPin(BTN_GPIO, BTN_PIN) == false);
}

static bool BTN_WaitPress(void)
{
    if (BTN_IsPressed())
    {
        delay_loop(300000U);

        if (BTN_IsPressed())
        {
            while (BTN_IsPressed())
            {
            }

            delay_loop(300000U);
            return true;
        }
    }

    return false;
}

static void LED_Init(void)
{
    PORT_EnableClock(LED_PORT_NAME);
    PORT_SetPinMux(LED_PORT, LED_PIN, PORT_MUX_GPIO);
    GPIO_SetPinDirection(LED_GPIO, LED_PIN, GPIO_DIRECTION_OUTPUT);

    /* active-low LED off */
    GPIO_WritePin(LED_GPIO, LED_PIN, true);
}

#if (IS_MASTER == 0)

static void SlaveCallback(LPI2C_Type *base,
                          LPI2C_SlaveEvent_t event,
                          void *userData)
{
    (void)userData;

    if (event == LPI2C_SLAVE_EVENT_TX_REQUEST)
    {
        (void)LPI2C_SlaveWriteByte(base, SLAVE_REPLY);
    }
}

#endif

int main(void)
{
    LPUART_Config_t uartConfig;

#if (IS_MASTER == 1)
    LPI2C_MasterConfig_t masterConfig;
    LPI2C_Status_t status;
    uint8_t rxData;
    char msg[80];
#else
    LPI2C_SlaveConfig_t slaveConfig;
#endif

    SOSC_init_8MHz();
    SPLL_init_160MHz();
    NormalRUNmode_80MHz();

    uartConfig.baudRate = 9600U;
    uartConfig.srcClockHz = 8000000U;
    LPUART_Init(IP_LPUART1, &uartConfig);

#if (IS_MASTER == 1)

    BTN_Init();
    LED_Init();

    LPI2C_MasterGetDefaultConfig(&masterConfig);
    masterConfig.baudRate = 100000U;
    LPI2C_MasterInit(IP_LPI2C0, &masterConfig);

    LPUART_WriteString(IP_LPUART1, "\r\nI2C MASTER READ FROM SLAVE TEST\r\n");

    while (1)
    {
        if (BTN_WaitPress())
        {
            rxData = 0U;

            status = LPI2C_MasterReadBlocking(IP_LPI2C0,
                                              SLAVE_ADDR,
                                              &rxData,
                                              1U,
                                              I2C_TIMEOUT);

            sprintf(msg, "MASTER read status=%d data=0x%02X\r\n",
                    status,
                    rxData);
            LPUART_WriteString(IP_LPUART1, msg);

            if ((status == LPI2C_STATUS_OK) && (rxData == SLAVE_REPLY))
            {
                GPIO_TogglePin(LED_GPIO, LED_PIN);
            }
        }
    }

#else

    LPI2C_SlaveGetDefaultConfig(&slaveConfig);
    slaveConfig.slaveAddress = SLAVE_ADDR;
    slaveConfig.enableClockStretching = true;
    slaveConfig.enableFilter = true;
    slaveConfig.callback = SlaveCallback;
    slaveConfig.userData = 0;

    LPI2C_SlaveInit(IP_LPI2C0, &slaveConfig);
    IRQ_LPI2C0_Slave_Init();

    LPUART_WriteString(IP_LPUART1, "\r\nI2C SLAVE TX TEST\r\n");

    while (1)
    {
    }

#endif
}
