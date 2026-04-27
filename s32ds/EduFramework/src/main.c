#include "S32K144.h"
#include "clock.h"
#include "lpuart.h"
#include "lpi2c.h"
#include "irq.h"

#include "port.h"
#include "gpio.h"

#include <stdio.h>
#include <stdbool.h>

/* ============================================================
 * Select role
 * ============================================================ */

#define IS_MASTER      0

/* ============================================================
 * I2C config
 * ============================================================ */

#define SLAVE_ADDR     0x12U
#define I2C_TIMEOUT    100000U

#define CMD_TOGGLE_LED 0xA5U

/* ============================================================
 * Board pins
 * ============================================================ */

/*
 * Sửa nếu board_pins.h của bạn đã có define LED/BTN riêng.
 * Tạm dùng LED RED / BTN0 phổ biến theo MaaZEDU.
 */
#define LED_PORT_NAME  PORT_NAME_D
#define LED_PORT       IP_PORTD
#define LED_GPIO       IP_PTD
#define LED_PIN        15U

#define BTN_PORT_NAME  PORT_NAME_C
#define BTN_PORT       IP_PORTC
#define BTN_GPIO       IP_PTC
#define BTN_PIN        12U

/* ============================================================
 * Delay
 * ============================================================ */

static void delay_loop(uint32_t count)
{
    while (count--)
    {
        __asm("nop");
    }
}

/* ============================================================
 * GPIO init
 * ============================================================ */

static void LED_Init(void)
{
    PORT_EnableClock(LED_PORT_NAME);
    PORT_SetPinMux(LED_PORT, LED_PIN, PORT_MUX_GPIO);

    GPIO_SetPinDirection(LED_GPIO, LED_PIN, GPIO_DIRECTION_OUTPUT);

    /* LED active-low: OFF */
    GPIO_WritePin(LED_GPIO, LED_PIN, true);
}

static void LED_Toggle(void)
{
    GPIO_TogglePin(LED_GPIO, LED_PIN);
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
    /* Pull-up button: pressed = 0 */
    return (GPIO_ReadPin(BTN_GPIO, BTN_PIN) == false);
}

/* ============================================================
 * Slave callback
 * ============================================================ */

#if (IS_MASTER == 0)

static volatile uint8_t g_slaveLastRx = 0U;
static volatile bool g_slaveRxFlag = false;

static void SlaveCallback(LPI2C_Type *base,
                          LPI2C_SlaveEvent_t event,
                          void *userData)
{
    uint8_t data;

    (void)userData;

    if (event == LPI2C_SLAVE_EVENT_RX_DATA)
    {
        if (LPI2C_SlaveReadByte(base, &data) == LPI2C_STATUS_OK)
        {
            g_slaveLastRx = data;
            g_slaveRxFlag = true;
        }
    }
}

#endif

/* ============================================================
 * Main
 * ============================================================ */

int main(void)
{
    LPUART_Config_t uartConfig;
    LPI2C_MasterConfig_t masterConfig;
    LPI2C_SlaveConfig_t slaveConfig;

#if (IS_MASTER == 1)
    LPI2C_Status_t status;
    uint8_t cmd;
    bool lastPressed = false;
    bool nowPressed;
    char msg[80];
#endif

    SOSC_init_8MHz();
    SPLL_init_160MHz();
    NormalRUNmode_80MHz();

    uartConfig.baudRate = 9600U;
    uartConfig.srcClockHz = 8000000U;
    LPUART_Init(IP_LPUART1, &uartConfig);

#if (IS_MASTER == 1)

    BTN_Init();

    LPI2C_MasterGetDefaultConfig(&masterConfig);
    masterConfig.baudRate = 100000U;

    status = LPI2C_MasterInit(IP_LPI2C0, &masterConfig);

    sprintf(msg, "\r\nI2C MASTER START init=%d\r\n", status);
    LPUART_WriteString(IP_LPUART1, msg);

    while (1)
    {
        nowPressed = BTN_IsPressed();

        if ((nowPressed == true) && (lastPressed == false))
        {
            delay_loop(300000U);

            if (BTN_IsPressed() == true)
            {
                cmd = CMD_TOGGLE_LED;

                status = LPI2C_MasterWriteBlocking(IP_LPI2C0,
                                                   SLAVE_ADDR,
                                                   &cmd,
                                                   1U,
                                                   I2C_TIMEOUT);

                sprintf(msg, "MASTER send 0x%02X status=%d\r\n", cmd, status);
                LPUART_WriteString(IP_LPUART1, msg);
            }
        }

        lastPressed = nowPressed;
    }

#else

    LED_Init();

    LPI2C_SlaveGetDefaultConfig(&slaveConfig);
    slaveConfig.slaveAddress = SLAVE_ADDR;
    slaveConfig.enableClockStretching = true;
    slaveConfig.enableFilter = true;
    slaveConfig.callback = SlaveCallback;
    slaveConfig.userData = 0;

    LPI2C_SlaveInit(IP_LPI2C0, &slaveConfig);
    IRQ_LPI2C0_Slave_Init();

    LPUART_WriteString(IP_LPUART1, "\r\nI2C SLAVE START\r\n");

    while (1)
    {
        if (g_slaveRxFlag == true)
        {
            g_slaveRxFlag = false;

            if (g_slaveLastRx == CMD_TOGGLE_LED)
            {
                LED_Toggle();
                LPUART_WriteString(IP_LPUART1, "SLAVE RX 0xA5 -> toggle LED\r\n");
            }
        }
    }

#endif
}
