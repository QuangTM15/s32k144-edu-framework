#ifndef LPIT_H
#define LPIT_H

/**
 * @file lpit.h
 * @brief Low Power Interrupt Timer driver interface.
 *
 * @details
 * This file declares the public APIs for controlling the LPIT peripheral
 * on the NXP S32K144 microcontroller.
 *
 * The LPIT driver is a low-level register driver. It is responsible for:
 * - Initializing the LPIT module clock.
 * - Configuring timer period values.
 * - Starting and stopping timer channels.
 * - Reading and clearing timer timeout flags.
 * - Enabling and disabling LPIT channel interrupts.
 *
 * This driver does not depend on the Arduino-style API layer.
 */

#include <stdint.h>

/**
 * @brief Initialize the LPIT peripheral.
 *
 * @details
 * This function enables the LPIT peripheral clock, selects the configured
 * LPIT functional clock source, enables the LPIT module, clears pending
 * timeout flags, and resets all LPIT timer channels.
 *
 * This function must be called before using any LPIT timer channel.
 *
 * @return None.
 */
void LPIT_Init(void);

/**
 * @brief Set the period value for an LPIT timer channel.
 *
 * @details
 * This function writes the timer value register of the selected LPIT
 * channel. The timer period depends on the LPIT functional clock source.
 *
 * For example, if LPIT uses a 40 MHz clock, a value of 40000 ticks
 * corresponds to approximately 1 ms.
 *
 * Invalid channels or zero tick values are ignored by the implementation.
 *
 * @param[in] u8Channel
 * LPIT channel index.
 *
 * @param[in] u32Ticks
 * Timer period value in LPIT clock ticks.
 *
 * @return None.
 */
void LPIT_SetTimerPeriod(uint8_t u8Channel, uint32_t u32Ticks);

/**
 * @brief Start an LPIT timer channel.
 *
 * @details
 * This function enables counting for the selected LPIT channel.
 * The channel period should be configured before starting the timer.
 *
 * Invalid channels are ignored by the implementation.
 *
 * @param[in] u8Channel
 * LPIT channel index.
 *
 * @return None.
 */
void LPIT_StartTimer(uint8_t u8Channel);

/**
 * @brief Stop an LPIT timer channel.
 *
 * @details
 * This function disables counting for the selected LPIT channel.
 * The timer configuration is preserved.
 *
 * Invalid channels are ignored by the implementation.
 *
 * @param[in] u8Channel
 * LPIT channel index.
 *
 * @return None.
 */
void LPIT_StopTimer(uint8_t u8Channel);

/**
 * @brief Get timeout flag status of an LPIT timer channel.
 *
 * @details
 * This function checks whether the selected LPIT channel timeout flag
 * is set in the LPIT Module Status Register.
 *
 * Invalid channels return 0U.
 *
 * @param[in] u8Channel
 * LPIT channel index.
 *
 * @return uint8_t
 *
 * @retval 0U
 * Timeout flag is not set or the channel is invalid.
 *
 * @retval 1U
 * Timeout flag is set.
 */
uint8_t LPIT_GetFlag(uint8_t u8Channel);

/**
 * @brief Clear timeout flag of an LPIT timer channel.
 *
 * @details
 * The LPIT timeout flag is cleared by writing 1 to the corresponding
 * flag bit in the Module Status Register.
 *
 * Invalid channels are ignored by the implementation.
 *
 * @param[in] u8Channel
 * LPIT channel index.
 *
 * @return None.
 */
void LPIT_ClearFlag(uint8_t u8Channel);

/**
 * @brief Enable interrupt for an LPIT timer channel.
 *
 * @details
 * This function enables the LPIT timer interrupt for the selected channel
 * at the LPIT peripheral level. The NVIC interrupt must be configured
 * separately by the IRQ module.
 *
 * Invalid channels are ignored by the implementation.
 *
 * @param[in] u8Channel
 * LPIT channel index.
 *
 * @return None.
 */
void LPIT_EnableInterrupt(uint8_t u8Channel);

/**
 * @brief Disable interrupt for an LPIT timer channel.
 *
 * @details
 * This function disables the LPIT timer interrupt for the selected channel
 * at the LPIT peripheral level.
 *
 * Invalid channels are ignored by the implementation.
 *
 * @param[in] u8Channel
 * LPIT channel index.
 *
 * @return None.
 */
void LPIT_DisableInterrupt(uint8_t u8Channel);

/**
 * @brief Get current counter value of an LPIT timer channel.
 *
 * @param[in] u8Channel
 * LPIT channel index.
 *
 * @return uint32_t
 * Current counter value.
 *
 * @retval 0U
 * Invalid channel.
 */
uint32_t LPIT_GetCurrentValue(uint8_t u8Channel);


#endif /* LPIT_H */
