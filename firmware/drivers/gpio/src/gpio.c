#include "gpio.h"

void GPIO_SetPinDirection(GPIO_Type *base, uint8_t pin, gpio_direction_t direction)
{
    if (direction == GPIO_DIRECTION_OUTPUT)
    {
        base->PDDR |= (1UL << pin);
    }
    else
    {
        base->PDDR &= ~(1UL << pin);
    }
}

void GPIO_WritePin(GPIO_Type *base, uint8_t pin, bool value)
{
    if (value == true)
    {
        base->PSOR = (1UL << pin);   /* Set pin = 1 */
    }
    else
    {
        base->PCOR = (1UL << pin);   /* Clear pin = 0 */
    }
}

bool GPIO_ReadPin(GPIO_Type *base, uint8_t pin)
{
    return ( (base->PDIR >> pin) & 0x1U );
}

void GPIO_TogglePin(GPIO_Type *base, uint8_t pin)
{
    base->PTOR = (1UL << pin);
}
