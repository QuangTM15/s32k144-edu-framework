#include "S32K144.h"
#include "clock.h"
#include "port.h"
#include "gpio.h"
#include "lpspi.h"

#define IS_MASTER   0   /* 1 = master, 0 = slave */

#define BTN_PORT    IP_PTC
#define BTN_PIN     12U

#define LED_PORT    IP_PTD
#define LED_PIN     0U

#define TEST_BYTE   0xA5U

static void DelayLoop(volatile uint32_t count)
{
    while (count--)
    {
        __asm("nop");
    }
}

static void SPI_PinInit(void)
{
    PORT_EnableClock(PORT_NAME_B);

    /* LPSPI0 / ALT3 */
    PORT_SetPinMux(IP_PORTB, 0U, PORT_MUX_ALT3); /* PCS0 */
    PORT_SetPinMux(IP_PORTB, 1U, PORT_MUX_ALT3); /* SOUT */
    PORT_SetPinMux(IP_PORTB, 2U, PORT_MUX_ALT3); /* SCK  */
    PORT_SetPinMux(IP_PORTB, 3U, PORT_MUX_ALT3); /* SIN  */
}

static void Button_Init(void)
{
    PORT_EnableClock(PORT_NAME_C);
    PORT_SetPinMux(IP_PORTC, BTN_PIN, PORT_MUX_GPIO);
    PORT_SetPinPull(IP_PORTC, BTN_PIN, PORT_PULL_UP);
    GPIO_SetPinDirection(BTN_PORT, BTN_PIN, GPIO_DIRECTION_INPUT);
}

static void LED_Init(void)
{
    PORT_EnableClock(PORT_NAME_D);
    PORT_SetPinMux(IP_PORTD, LED_PIN, PORT_MUX_GPIO);
    GPIO_SetPinDirection(LED_PORT, LED_PIN, GPIO_DIRECTION_OUTPUT);

    /* Blue LED active low -> OFF */
    GPIO_WritePin(LED_PORT, LED_PIN, true);
}

int main(void)
{
    lpspi_config_t spiConfig;
    lpspi_status_t status;

    SOSC_init_8MHz();
    SPLL_init_160MHz();
    NormalRUNmode_80MHz();

    SPI_PinInit();
    LED_Init();

#if IS_MASTER
    Button_Init();
#endif

    spiConfig.mode      = (IS_MASTER == 1) ? LPSPI_MODE_MASTER : LPSPI_MODE_SLAVE;
    spiConfig.clockMode = LPSPI_MODE0;
    spiConfig.bitOrder  = LPSPI_MSB_FIRST;
    spiConfig.dataSize  = LPSPI_DATASIZE_8BIT;
    spiConfig.baudrate  = 1000U;

    status = LPSPI_Init(&spiConfig);
    if (status != LPSPI_STATUS_OK)
    {
        while (1)
        {
        }
    }

    while (1)
    {
#if IS_MASTER
        static uint8_t lastButtonState = 1U;
        uint8_t currentButtonState;
        uint8_t rxDummy;

        currentButtonState = GPIO_ReadPin(BTN_PORT, BTN_PIN);

        /* Button active low: send on falling edge */
        if ((currentButtonState == 0U) && (lastButtonState == 1U))
        {
            status = LPSPI_Transfer8(TEST_BYTE, &rxDummy);
            if (status == LPSPI_STATUS_OK)
            {
                /* optional local indication */
                GPIO_TogglePin(LED_PORT, LED_PIN);
            }

            DelayLoop(200000U);   /* simple debounce */
        }

        lastButtonState = currentButtonState;

#else
        uint16_t rxData;

        if (LPSPI_IsTxReady())
        {
            (void)LPSPI_Write(0x00U);
        }

        if (LPSPI_IsRxReady())
        {
            status = LPSPI_Read(&rxData);
            if (status == LPSPI_STATUS_OK)
            {
                if ((uint8_t)rxData == TEST_BYTE)
                {
                    GPIO_TogglePin(LED_PORT, LED_PIN);
                    DelayLoop(200000U);
                }
            }
        }
#endif
    }
}
