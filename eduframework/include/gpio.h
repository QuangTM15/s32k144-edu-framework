#ifndef GPIO_H
#define GPIO_H

/**
 * @file gpio.h
 * @brief GPIO driver interface for NXP S32K144.
 *
 * This file provides low-level GPIO services for configuring pin direction,
 * writing output level, reading input level, and toggling output level.
 */

#include "S32K144.h"

#include <stdint.h>
#include <stdbool.h>

/* ========================================================================= */
/* Macro Definitions                                                          */
/* ========================================================================= */

/**
 * @brief GPIO pin direction type.
 */
typedef uint8_t gpio_direction_t;
/**
 * @brief Configure GPIO pin as input.
 */
#define GPIO_DIRECTION_INPUT ((gpio_direction_t)0U)

/**
 * @brief Configure GPIO pin as output.
 */
#define GPIO_DIRECTION_OUTPUT ((gpio_direction_t)1U)

/* ========================================================================= */
/* Public API Prototypes                                                      */
/* ========================================================================= */

/**
 * @brief Configure the direction of a GPIO pin.
 *
 * @details
 * This function updates the GPIO Port Data Direction Register (PDDR).
 * A pin configured as output can drive logic high or low. A pin configured
 * as input is used for reading external logic level.
 *
 * @param[in] pBase
 * Pointer to GPIO peripheral base address.
 *
 * @param[in] u8Pin
 * Pin number inside the selected GPIO port.
 *
 * @param[in] u8Direction
 * Direction configuration.
 * Use GPIO_DIRECTION_INPUT or GPIO_DIRECTION_OUTPUT.
 *
 * @return None.
 */
void GPIO_SetPinDirection(GPIO_Type *pBase,
                          uint8_t u8Pin,
                          gpio_direction_t u8Direction);

/**
 * @brief Write a logic level to a GPIO output pin.
 *
 * @details
 * This function writes to PSOR or PCOR register instead of directly modifying
 * PDOR. This avoids read-modify-write behavior on the output data register.
 *
 * @param[in] pBase
 * Pointer to GPIO peripheral base address.
 *
 * @param[in] u8Pin
 * Pin number inside the selected GPIO port.
 *
 * @param[in] bValue
 * Output logic level.
 * true sets the pin high, false clears the pin low.
 *
 * @return None.
 */
void GPIO_WritePin(GPIO_Type *pBase,
                   uint8_t u8Pin,
                   bool bValue);

/**
 * @brief Read the current logic level of a GPIO pin.
 *
 * @details
 * This function reads the GPIO Port Data Input Register (PDIR).
 *
 * @param[in] pBase
 * Pointer to GPIO peripheral base address.
 *
 * @param[in] u8Pin
 * Pin number inside the selected GPIO port.
 *
 * @return Pin logic level.
 * @retval true   Pin input level is high.
 * @retval false  Pin input level is low or GPIO base pointer is invalid.
 */
bool GPIO_ReadPin(GPIO_Type *pBase,
                  uint8_t u8Pin);

/**
 * @brief Toggle a GPIO output pin.
 *
 * @details
 * This function writes to the GPIO Port Toggle Output Register (PTOR).
 *
 * @param[in] pBase
 * Pointer to GPIO peripheral base address.
 *
 * @param[in] u8Pin
 * Pin number inside the selected GPIO port.
 *
 * @return None.
 */
void GPIO_TogglePin(GPIO_Type *pBase,
                    uint8_t u8Pin);

#endif /* GPIO_H */