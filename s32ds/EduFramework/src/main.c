#include "S32K144.h"
#include "port.h"
#include "gpio.h"
#include "clock.h"
#include "lpit.h"

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

    /* Clock test baseline */
    SOSC_init_8MHz();
    SPLL_init_160MHz();
    NormalRUNmode_80MHz();

    /* Step 1: test only LPIT_Init() */
    LPIT_Init();

    LPIT_SetTimerPeriod(0U, 40000000U);


    LPIT_StartTimer(0U);

    while (1)
    {
        if (LPIT_GetFlag(0U) != 0U)
        {
            LPIT_ClearFlag(0U);
            GPIO_TogglePin(LED_GPIO, LED_PIN);
        }
    }
}
