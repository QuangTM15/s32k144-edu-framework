/**
 * @file gpio.c
 * @brief GPIO driver implementation for NXP S32K144.
 *
 * This file implements low-level GPIO operations by accessing the S32K144
 * GPIO register block through the GPIO_Type structure.
 */

#include "gpio.h"

#include <stddef.h>

/* ========================================================================= */
/* Private Macros                                                             */
/* ========================================================================= */

/**
 * @brief Create a bit mask for a GPIO pin.
 */
#define GPIO_PIN_MASK(u8Pin) (1UL << (u8Pin))

/**
 * @brief Single-bit mask used when reading one pin from PDIR.
 */
#define GPIO_PIN_READ_MASK (0x1U)

/* ========================================================================= */
/* Public Functions                                                           */
/* ========================================================================= */

void GPIO_SetPinDirection(GPIO_Type *pBase,
                          uint8_t u8Pin,
                          gpio_direction_t u8Direction)
{
    if (NULL != pBase)
    {
        /*
         * PDDR controls GPIO direction:
         * - bit = 1: output
         * - bit = 0: input
         */
        if (GPIO_DIRECTION_OUTPUT == u8Direction)
        {
            pBase->PDDR |= GPIO_PIN_MASK(u8Pin);
        }
        else
        {
            pBase->PDDR &= ~GPIO_PIN_MASK(u8Pin);
        }
    }
    else
    {
        /* Invalid GPIO base pointer. Keep previous behavior: do nothing. */
    }

    return;
}

void GPIO_WritePin(GPIO_Type *pBase,
                   uint8_t u8Pin,
                   bool bValue)
{
    if (NULL != pBase)
    {
        if (true == bValue)
        {
            /*
             * PSOR is write-one-to-set.
             * Writing 1 sets the selected output bit without affecting others.
             */
            pBase->PSOR = GPIO_PIN_MASK(u8Pin);
        }
        else
        {
            /*
             * PCOR is write-one-to-clear.
             * Writing 1 clears the selected output bit without affecting others.
             */
            pBase->PCOR = GPIO_PIN_MASK(u8Pin);
        }
    }
    else
    {
        /* Invalid GPIO base pointer. Keep previous behavior: do nothing. */
    }

    return;
}

bool GPIO_ReadPin(GPIO_Type *pBase,
                  uint8_t u8Pin)
{
    bool bPinState = false;

    if (NULL != pBase)
    {
        /*
         * PDIR reflects the current physical input level of the pin.
         * The selected bit is shifted to bit 0 before applying the mask.
         */
        if (GPIO_PIN_READ_MASK == ((pBase->PDIR >> u8Pin) & GPIO_PIN_READ_MASK))
        {
            bPinState = true;
        }
        else
        {
            bPinState = false;
        }
    }
    else
    {
        /*
         * Invalid GPIO base pointer. Keep previous behavior:
         * return false as a safe default.
         */
        bPinState = false;
    }

    return bPinState;
}

void GPIO_TogglePin(GPIO_Type *pBase,
                    uint8_t u8Pin)
{
    if (NULL != pBase)
    {
        /*
         * PTOR is write-one-to-toggle.
         * Writing 1 toggles the selected output bit without affecting others.
         */
        pBase->PTOR = GPIO_PIN_MASK(u8Pin);
    }
    else
    {
        /* Invalid GPIO base pointer. Keep previous behavior: do nothing. */
    }

    return;
}