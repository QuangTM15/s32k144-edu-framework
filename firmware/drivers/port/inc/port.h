#ifndef PORT_H
#define PORT_H

#include "S32K144.h"
#include <stdint.h>
#include <stdbool.h>

/* =========================================
 * PORT Name
 * ========================================= */
typedef enum
{
    PORT_NAME_A = 0U,
    PORT_NAME_B,
    PORT_NAME_C,
    PORT_NAME_D,
    PORT_NAME_E
} port_name_t;

/* =========================================
 * PORT MUX Selection
 * ========================================= */
typedef enum
{
    PORT_MUX_DISABLED = 0U,
    PORT_MUX_GPIO     = 1U,
    PORT_MUX_ALT2     = 2U,
    PORT_MUX_ALT3     = 3U,
    PORT_MUX_ALT4     = 4U,
    PORT_MUX_ALT5     = 5U,
    PORT_MUX_ALT6     = 6U,
    PORT_MUX_ALT7     = 7U
} port_mux_t;

/* =========================================
 * PORT Pull Configuration
 * ========================================= */
typedef enum
{
    PORT_PULL_DISABLED = 0U,
    PORT_PULL_DOWN,
    PORT_PULL_UP
} port_pull_t;

/* =========================================
 * PORT Driver APIs
 * ========================================= */

/* Enable clock for PORTA/PORTB/PORTC/PORTD/PORTE */
void PORT_EnableClock(port_name_t portName);

/* Configure pin mux function */
void PORT_SetPinMux(PORT_Type *base, uint8_t pin, port_mux_t mux);

/* Configure internal pull resistor */
void PORT_SetPinPull(PORT_Type *base, uint8_t pin, port_pull_t pull);

/* Enable or disable passive input filter */
void PORT_SetPassiveFilter(PORT_Type *base, uint8_t pin, bool enable);

#endif /* PORT_H */
