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
 * - Registering simple interrupt callbacks where required.
 * - Declaring ISR entry points used by the startup vector table.
 *
 * This module acts as a bridge between hardware interrupt vectors and
 * low-level drivers or device handlers.
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

/* ========================================================================= */
/* LPIT0                                                                     */
/* ========================================================================= */

void IRQ_LPIT0_Ch0_Init(void);

void IRQ_LPIT0_Ch0_SetCallback(irq_callback_t pfCallback);

/* ========================================================================= */
/* LPUART                                                                    */
/* ========================================================================= */

void IRQ_LPUART1_RxTx_Init(void);

void IRQ_LPUART2_RxTx_Init(void);

/* ========================================================================= */
/* ADC                                                                       */
/* ========================================================================= */

void IRQ_ADC0_Init(void);

/* ========================================================================= */
/* LPI2C                                                                     */
/* ========================================================================= */

void IRQ_LPI2C0_Master_Init(void);

void IRQ_LPI2C0_Slave_Init(void);

/* ========================================================================= */
/* PORT                                                                      */
/* ========================================================================= */

/**
 * @brief Initialize NVIC configuration for PORTD interrupt.
 *
 * @details
 * This function clears any pending PORTD interrupt request, configures
 * the interrupt priority, and enables the PORTD interrupt line in NVIC.
 *
 * Individual PORTD pin interrupt sources are configured separately by
 * the PORT driver through PCR[IRQC].
 *
 * @return None.
 */
void IRQ_PORTD_Init(void);

/**
 * @brief Register callback for PORTD interrupt.
 *
 * @details
 * The callback is executed directly from PORTD_IRQHandler().
 *
 * The callback is responsible for reading and clearing the relevant PORTD
 * interrupt flags through the PORT driver.
 *
 * Passing a null callback disables callback execution.
 *
 * @param[in] pfCallback
 * Pointer to callback function.
 *
 * @return None.
 */
void IRQ_PORTD_SetCallback(irq_callback_t pfCallback);

/**
 * @brief Initialize NVIC configuration for PORTE interrupt.
 *
 * @details
 * This function clears any pending PORTE interrupt request, configures
 * the interrupt priority, and enables the PORTE interrupt line in NVIC.
 *
 * Individual PORTE pin interrupt sources are configured separately by
 * the PORT driver through PCR[IRQC].
 *
 * @return None.
 */
void IRQ_PORTE_Init(void);

/**
 * @brief Register callback for PORTE interrupt.
 *
 * @details
 * The callback is executed directly from PORTE_IRQHandler().
 *
 * The callback is responsible for reading and clearing the relevant PORTE
 * interrupt flags through the PORT driver.
 *
 * Passing a null callback disables callback execution.
 *
 * @param[in] pfCallback
 * Pointer to callback function.
 *
 * @return None.
 */
void IRQ_PORTE_SetCallback(irq_callback_t pfCallback);

/* ========================================================================= */
/* ISR Declarations                                                          */
/* ========================================================================= */

void LPIT0_Ch0_IRQHandler(void);

void LPUART1_RxTx_IRQHandler(void);

void LPUART2_RxTx_IRQHandler(void);

void ADC0_IRQHandler(void);

void LPI2C0_Master_IRQHandler(void);

void LPI2C0_Slave_IRQHandler(void);

/**
 * @brief PORTD interrupt service routine.
 *
 * @details
 * This ISR executes the registered PORTD callback.
 *
 * @return None.
 */
void PORTD_IRQHandler(void);

/**
 * @brief PORTE interrupt service routine.
 *
 * @details
 * This ISR executes the registered PORTE callback.
 *
 * @return None.
 */
void PORTE_IRQHandler(void);

#endif /* IRQ_H */
