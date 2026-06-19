#ifndef IRQ_H
#define IRQ_H

/**
 * @file irq.h
 * @brief Interrupt configuration and ISR declaration interface.
 *
 * @details
 * This file declares interrupt initialization APIs and interrupt service
 * routines used by EduFramework.
 *
 * The IRQ module is responsible for:
 * - Configuring NVIC interrupt lines.
 * - Registering simple interrupt callbacks.
 * - Declaring ISR entry points used by the startup vector table.
 *
 * This module acts as a bridge between hardware interrupt vectors and
 * low-level driver handlers.
 */

#include <stdint.h>

/**
 * @brief Generic interrupt callback function type.
 *
 * @details
 * A callback registered through the IRQ module is executed from interrupt
 * context. Callback functions must be short and must not perform blocking
 * operations.
 */
typedef void (*irq_callback_t)(void);

/**
 * @brief Initialize NVIC configuration for LPIT0 channel 0 interrupt.
 *
 * @details
 * This function clears any pending LPIT0 channel 0 interrupt request,
 * configures the interrupt priority, and enables the interrupt in NVIC.
 *
 * The LPIT peripheral interrupt enable bit is configured separately by
 * the LPIT driver.
 *
 * @return None.
 */
void IRQ_LPIT0_Ch0_Init(void);

/**
 * @brief Register callback for LPIT0 channel 0 interrupt.
 *
 * @details
 * The registered callback is executed from LPIT0_Ch0_IRQHandler()
 * after the LPIT timeout flag is cleared.
 *
 * Passing a null callback disables the callback.
 *
 * @param[in] pfCallback
 * Pointer to callback function.
 *
 * @return None.
 */
void IRQ_LPIT0_Ch0_SetCallback(irq_callback_t pfCallback);

/**
 * @brief Initialize NVIC configuration for LPUART1 Rx/Tx interrupt.
 *
 * @details
 * This function enables the LPUART1 Rx/Tx interrupt line in NVIC.
 *
 * @return None.
 */
void IRQ_LPUART1_RxTx_Init(void);

/**
 * @brief Initialize NVIC configuration for LPUART2 Rx/Tx interrupt.
 *
 * @details
 * This function enables the LPUART2 Rx/Tx interrupt line in NVIC.
 *
 * @return None.
 */
void IRQ_LPUART2_RxTx_Init(void);

/**
 * @brief Initialize NVIC configuration for ADC0 interrupt.
 *
 * @details
 * This function enables the ADC0 interrupt line in NVIC.
 *
 * @return None.
 */
void IRQ_ADC0_Init(void);

/**
 * @brief Initialize NVIC configuration for LPI2C0 master interrupt.
 *
 * @details
 * This function enables the LPI2C0 master interrupt line in NVIC.
 *
 * @return None.
 */
void IRQ_LPI2C0_Master_Init(void);

/**
 * @brief Initialize NVIC configuration for LPI2C0 slave interrupt.
 *
 * @details
 * This function enables the LPI2C0 slave interrupt line in NVIC.
 *
 * @return None.
 */
void IRQ_LPI2C0_Slave_Init(void);

/**
 * @brief LPIT0 channel 0 interrupt service routine.
 *
 * @details
 * This ISR clears the LPIT0 channel 0 timeout flag and executes the
 * registered LPIT callback if available.
 *
 * @return None.
 */
void LPIT0_Ch0_IRQHandler(void);

/**
 * @brief LPUART1 Rx/Tx interrupt service routine.
 *
 * @details
 * This ISR delegates interrupt processing to the LPUART driver.
 *
 * @return None.
 */
void LPUART1_RxTx_IRQHandler(void);

/**
 * @brief LPUART2 Rx/Tx interrupt service routine.
 *
 * @details
 * This ISR delegates interrupt processing to the LPUART driver.
 *
 * @return None.
 */
void LPUART2_RxTx_IRQHandler(void);

/**
 * @brief ADC0 interrupt service routine.
 *
 * @details
 * This ISR delegates interrupt processing to the ADC driver.
 *
 * @return None.
 */
void ADC0_IRQHandler(void);

/**
 * @brief LPI2C0 master interrupt service routine.
 *
 * @details
 * This ISR delegates interrupt processing to the LPI2C master driver.
 *
 * @return None.
 */
void LPI2C0_Master_IRQHandler(void);

/**
 * @brief LPI2C0 slave interrupt service routine.
 *
 * @details
 * This ISR delegates interrupt processing to the LPI2C slave driver.
 *
 * @return None.
 */
void LPI2C0_Slave_IRQHandler(void);

#endif /* IRQ_H */