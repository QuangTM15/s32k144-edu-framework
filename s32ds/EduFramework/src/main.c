#include "port.h"
#include "gpio.h"
#include "board_pins.h"

int main(void)
{
    /* Enable clock for PORTC and PORTD */
    PORT_EnableClock(PORT_NAME_C);
    PORT_EnableClock(PORT_NAME_D);

    /* Configure Button 0 as GPIO input */
    PORT_SetPinMux(BOARD_BUTTON0_PORT, BOARD_BUTTON0_PIN, PORT_MUX_GPIO);
    PORT_SetPassiveFilter(BOARD_BUTTON0_PORT, BOARD_BUTTON0_PIN, true);
    GPIO_SetPinDirection(BOARD_BUTTON0_GPIO, BOARD_BUTTON0_PIN, GPIO_DIRECTION_INPUT);

    /* Configure Blue LED as GPIO output */
    PORT_SetPinMux(BOARD_LED_BLUE_PORT, BOARD_LED_BLUE_PIN, PORT_MUX_GPIO);
    GPIO_SetPinDirection(BOARD_LED_BLUE_GPIO, BOARD_LED_BLUE_PIN, GPIO_DIRECTION_OUTPUT);

    for (;;)
    {
        if (GPIO_ReadPin(BOARD_BUTTON0_GPIO, BOARD_BUTTON0_PIN) == true)
        {
            GPIO_WritePin(BOARD_LED_BLUE_GPIO, BOARD_LED_BLUE_PIN, false);
        }
        else
        {
            GPIO_WritePin(BOARD_LED_BLUE_GPIO, BOARD_LED_BLUE_PIN, true);
        }
    }
}
