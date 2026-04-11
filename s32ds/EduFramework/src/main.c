#include "S32K144.h"
#include "port.h"
#include "gpio.h"
#include "clock.h"
#include "lpit.h"
#include "irq.h"

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
    Board_InitLed();

    SOSC_init_8MHz();
    SPLL_init_160MHz();
    NormalRUNmode_80MHz();

    LPIT_Init();
    LPIT_SetTimerPeriod(0U, 40000000U);

    LPIT_EnableInterrupt(0U);
    IRQ_LPIT0_Ch0_Init();

    LPIT_StartTimer(0U);

    while (1)
    {
    }
}
