#ifndef PORT_H
#define PORT_H

/**
 * @file port.h
 * @brief PORT driver interface for NXP S32K144.
 *
 * This file provides low-level PORT configuration services, including
 * PORT clock enable, pin mux selection, internal pull resistor configuration,
 * passive input filter control, and pin interrupt configuration.
 */

#include "S32K144.h"

#include <stdint.h>
#include <stdbool.h>

/* ========================================================================= */
/* Type Definitions                                                          */
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

/**
 * @brief PORT pin interrupt configuration type.
 *
 * @details
 * Values correspond directly to the IRQC field of the PORT PCR register.
 */
typedef uint8_t port_interrupt_config_t;

/* ========================================================================= */
/* Macro Definitions                                                         */
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

/* ========================================================================= */
/* Pin Mux                                                                   */
/* ========================================================================= */

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

/* ========================================================================= */
/* Pull Configuration                                                        */
/* ========================================================================= */

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
/* Interrupt Configuration                                                   */
/* ========================================================================= */

/**
 * @brief Disable pin interrupt and DMA request.
 *
 * @details
 * PORT PCR IRQC = 0000.
 */
#define PORT_INTERRUPT_DISABLED       ((port_interrupt_config_t)0x0U)

/**
 * @brief Generate interrupt on rising edge.
 *
 * @details
 * PORT PCR IRQC = 1001.
 */
#define PORT_INTERRUPT_RISING_EDGE    ((port_interrupt_config_t)0x9U)

/**
 * @brief Generate interrupt on falling edge.
 *
 * @details
 * PORT PCR IRQC = 1010.
 */
#define PORT_INTERRUPT_FALLING_EDGE   ((port_interrupt_config_t)0xAU)

/**
 * @brief Generate interrupt on either edge.
 *
 * @details
 * PORT PCR IRQC = 1011.
 */
#define PORT_INTERRUPT_EITHER_EDGE    ((port_interrupt_config_t)0xBU)

/* ========================================================================= */
/* Public API Prototypes                                                     */
/* ========================================================================= */

/**
 * @brief Enable clock for a PORT module.
 *
 * @param[in] u8PortName
 * PORT name. Use PORT_NAME_A through PORT_NAME_E.
 *
 * @return None.
 */
void PORT_EnableClock(port_name_t u8PortName);

/**
 * @brief Configure pin mux function.
 *
 * @param[in] pBase
 * Pointer to PORT peripheral base address.
 *
 * @param[in] u8Pin
 * Pin number inside the selected PORT module.
 *
 * @param[in] u8Mux
 * Pin mux selection.
 *
 * @return None.
 */
void PORT_SetPinMux(PORT_Type *pBase,
                    uint8_t u8Pin,
                    port_mux_t u8Mux);

/**
 * @brief Configure internal pull resistor.
 *
 * @param[in] pBase
 * Pointer to PORT peripheral base address.
 *
 * @param[in] u8Pin
 * Pin number inside the selected PORT module.
 *
 * @param[in] u8Pull
 * Pull configuration.
 *
 * @return None.
 */
void PORT_SetPinPull(PORT_Type *pBase,
                     uint8_t u8Pin,
                     port_pull_t u8Pull);

/**
 * @brief Enable or disable passive input filter.
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

/**
 * @brief Configure interrupt generation for one PORT pin.
 *
 * @details
 * This function programs the IRQC field in the selected pin PCR.
 *
 * Supported configurations:
 * - PORT_INTERRUPT_DISABLED
 * - PORT_INTERRUPT_RISING_EDGE
 * - PORT_INTERRUPT_FALLING_EDGE
 * - PORT_INTERRUPT_EITHER_EDGE
 *
 * This function configures only the PORT interrupt source. The corresponding
 * NVIC interrupt line must be enabled separately through the IRQ module.
 *
 * @param[in] pBase
 * Pointer to PORT peripheral base address.
 *
 * @param[in] u8Pin
 * Pin number inside the selected PORT module.
 *
 * @param[in] u8Config
 * Interrupt configuration.
 *
 * @return None.
 */
void PORT_SetPinInterruptConfig(PORT_Type *pBase,
                                uint8_t u8Pin,
                                port_interrupt_config_t u8Config);

/**
 * @brief Get all pending interrupt flags for a PORT module.
 *
 * @details
 * Each set bit in ISFR indicates that the corresponding PORT pin has an
 * active interrupt status flag.
 *
 * @param[in] pBase
 * Pointer to PORT peripheral base address.
 *
 * @return Current PORT ISFR value.
 * @retval 0U No interrupt flags are active or pBase is invalid.
 */
uint32_t PORT_GetInterruptFlags(PORT_Type *pBase);

/**
 * @brief Check the interrupt flag of one PORT pin.
 *
 * @param[in] pBase
 * Pointer to PORT peripheral base address.
 *
 * @param[in] u8Pin
 * Pin number inside the selected PORT module.
 *
 * @return uint8_t
 * @retval 1U Interrupt flag is set.
 * @retval 0U Interrupt flag is clear or pBase is invalid.
 */
uint8_t PORT_GetPinInterruptFlag(PORT_Type *pBase,
                                 uint8_t u8Pin);

/**
 * @brief Clear the interrupt flag of one PORT pin.
 *
 * @details
 * PORT ISFR flags are write-one-to-clear. Writing one to the selected bit
 * clears that pin interrupt flag without clearing unrelated flags.
 *
 * @param[in] pBase
 * Pointer to PORT peripheral base address.
 *
 * @param[in] u8Pin
 * Pin number inside the selected PORT module.
 *
 * @return None.
 */
void PORT_ClearPinInterruptFlag(PORT_Type *pBase,
                                uint8_t u8Pin);

/**
 * @brief Clear selected interrupt flags of a PORT module.
 *
 * @details
 * Every set bit in u32Mask is written to ISFR. Because ISFR is
 * write-one-to-clear, only the selected flags are cleared.
 *
 * @param[in] pBase
 * Pointer to PORT peripheral base address.
 *
 * @param[in] u32Mask
 * Bit mask of interrupt flags to clear.
 *
 * @return None.
 */
void PORT_ClearInterruptFlags(PORT_Type *pBase,
                              uint32_t u32Mask);

#endif /* PORT_H */
