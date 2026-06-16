#ifndef PORT_H
#define PORT_H

/**
 * @file port.h
 * @brief PORT driver interface for NXP S32K144.
 *
 * This file provides low-level PORT configuration services, including
 * PORT clock enable, pin mux selection, internal pull resistor configuration,
 * and passive input filter control.
 */

#include "S32K144.h"

#include <stdint.h>
#include <stdbool.h>

/* ========================================================================= */
/* Type Definitions                                                           */
/* ========================================================================= */

/**
 * @brief PORT name type.
 */
typedef uint8_t port_name_t;

/**
 * @brief PORT mux selection type.
 */
typedef uint8_t port_mux_t;

/**
 * @brief PORT internal pull resistor configuration type.
 */
typedef uint8_t port_pull_t;

/* ========================================================================= */
/* Macro Definitions                                                          */
/* ========================================================================= */

/**
 * @brief PORT A name.
 */
#define PORT_NAME_A             ((port_name_t)0U)

/**
 * @brief PORT B name.
 */
#define PORT_NAME_B             ((port_name_t)1U)

/**
 * @brief PORT C name.
 */
#define PORT_NAME_C             ((port_name_t)2U)

/**
 * @brief PORT D name.
 */
#define PORT_NAME_D             ((port_name_t)3U)

/**
 * @brief PORT E name.
 */
#define PORT_NAME_E             ((port_name_t)4U)

/**
 * @brief Pin mux disabled.
 */
#define PORT_MUX_DISABLED       ((port_mux_t)0U)

/**
 * @brief Pin mux GPIO function.
 */
#define PORT_MUX_GPIO           ((port_mux_t)1U)

/**
 * @brief Pin mux alternate function 2.
 */
#define PORT_MUX_ALT2           ((port_mux_t)2U)

/**
 * @brief Pin mux alternate function 3.
 */
#define PORT_MUX_ALT3           ((port_mux_t)3U)

/**
 * @brief Pin mux alternate function 4.
 */
#define PORT_MUX_ALT4           ((port_mux_t)4U)

/**
 * @brief Pin mux alternate function 5.
 */
#define PORT_MUX_ALT5           ((port_mux_t)5U)

/**
 * @brief Pin mux alternate function 6.
 */
#define PORT_MUX_ALT6           ((port_mux_t)6U)

/**
 * @brief Pin mux alternate function 7.
 */
#define PORT_MUX_ALT7           ((port_mux_t)7U)

/**
 * @brief Disable internal pull resistor.
 */
#define PORT_PULL_DISABLED      ((port_pull_t)0U)

/**
 * @brief Enable internal pull-down resistor.
 */
#define PORT_PULL_DOWN          ((port_pull_t)1U)

/**
 * @brief Enable internal pull-up resistor.
 */
#define PORT_PULL_UP            ((port_pull_t)2U)

/* ========================================================================= */
/* Public API Prototypes                                                      */
/* ========================================================================= */

/**
 * @brief Enable clock for a PORT module.
 *
 * @details
 * This function enables the clock gate of PORTA, PORTB, PORTC, PORTD,
 * or PORTE through the Peripheral Clock Controller (PCC).
 *
 * @param[in] u8PortName
 * PORT name. Use PORT_NAME_A, PORT_NAME_B, PORT_NAME_C,
 * PORT_NAME_D, or PORT_NAME_E.
 *
 * @return None.
 */
void PORT_EnableClock(port_name_t u8PortName);

/**
 * @brief Configure pin mux function.
 *
 * @details
 * This function configures the MUX field in the PORT Pin Control Register
 * (PCR). The PORT clock must be enabled before calling this function.
 *
 * @param[in] pBase
 * Pointer to PORT peripheral base address.
 *
 * @param[in] u8Pin
 * Pin number inside the selected PORT module.
 *
 * @param[in] u8Mux
 * Pin mux selection. Use PORT_MUX_xxx macros.
 *
 * @return None.
 */
void PORT_SetPinMux(PORT_Type *pBase,
                    uint8_t u8Pin,
                    port_mux_t u8Mux);

/**
 * @brief Configure internal pull resistor.
 *
 * @details
 * This function configures the PE and PS bits in the PORT Pin Control
 * Register (PCR).
 *
 * @param[in] pBase
 * Pointer to PORT peripheral base address.
 *
 * @param[in] u8Pin
 * Pin number inside the selected PORT module.
 *
 * @param[in] u8Pull
 * Pull configuration. Use PORT_PULL_DISABLED, PORT_PULL_DOWN,
 * or PORT_PULL_UP.
 *
 * @return None.
 */
void PORT_SetPinPull(PORT_Type *pBase,
                     uint8_t u8Pin,
                     port_pull_t u8Pull);

/**
 * @brief Enable or disable passive input filter.
 *
 * @details
 * This function controls the Passive Filter Enable (PFE) bit in the PORT
 * Pin Control Register (PCR).
 *
 * @param[in] pBase
 * Pointer to PORT peripheral base address.
 *
 * @param[in] u8Pin
 * Pin number inside the selected PORT module.
 *
 * @param[in] bEnable
 * true enables passive filter, false disables passive filter.
 *
 * @return None.
 */
void PORT_SetPassiveFilter(PORT_Type *pBase,
                           uint8_t u8Pin,
                           bool bEnable);

#endif /* PORT_H */
