#ifndef GPIO_H
#define GPIO_H

#include "S32K144.h"
#include <stdint.h>
#include <stdbool.h>

/* =========================================
 * GPIO Direction
 * ========================================= */
typedef enum
{
    GPIO_DIRECTION_INPUT = 0U,
    GPIO_DIRECTION_OUTPUT
} gpio_direction_t;

/* =========================================
 * GPIO Driver APIs
 * ========================================= */

/* Configure pin direction */
void GPIO_SetPinDirection(GPIO_Type *base, uint8_t pin, gpio_direction_t direction);

/* Write logic level to output pin */
void GPIO_WritePin(GPIO_Type *base, uint8_t pin, bool value);

/* Read logic level from pin */
bool GPIO_ReadPin(GPIO_Type *base, uint8_t pin);

/* Toggle output pin */
void GPIO_TogglePin(GPIO_Type *base, uint8_t pin);

#endif /* GPIO_H */
