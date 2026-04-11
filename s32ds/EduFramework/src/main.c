#include "S32K144.h"
#include "port.h"
#include "gpio.h"
#include "clock.h"

#define LED_PORT    IP_PORTD
#define LED_GPIO    IP_PTD
#define LED_PIN     (0U)

static void Board_InitLed(void)
{
    PORT_EnableClock(PORT_NAME_D);
    PORT_SetPinMux(LED_PORT, LED_PIN, 1U);
    GPIO_SetPinDirection(LED_GPIO, LED_PIN, GPIO_DIRECTION_OUTPUT);

    /* active-low LED: off */
    GPIO_WritePin(LED_GPIO, LED_PIN, true);
}

int main(void)
{
    volatile uint32_t delay;

    Board_InitLed();

    SOSC_init_8MHz();
    SPLL_init_160MHz();
    NormalRUNmode_80MHz();

    while (1)
    {
        GPIO_TogglePin(LED_GPIO, LED_PIN);

        for (delay = 0U; delay < 500000U; delay++)
        {
        }
    }
}
