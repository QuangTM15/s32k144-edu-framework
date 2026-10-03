#ifndef IRQ_H
#define IRQ_H

/**
 * @file irq.h
 * @brief Interrupt configuration and ISR declaration interface.
 *
 * @details
 * The IRQ module configures NVIC interrupt lines and bridges startup-vector
 * handlers to low-level EduFramework drivers.
 */

#include <stdint.h>

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
/* FlexCAN0                                                                  */
/* ========================================================================= */

/**
 * @brief Initialize NVIC routing for FlexCAN0 Message Buffer interrupts.
 *
 * @details
 * S32K144 FlexCAN0 uses two OR'ed Message Buffer vectors:
 * - IRQ81 for MB0 through MB15.
 * - IRQ82 for MB16 through MB31.
 *
 * Individual Message Buffer interrupt generation remains controlled by
 * FlexCAN IMASK1 through the FlexCAN driver.
 */
void IRQ_FLEXCAN0_MB_Init(void);

/* ========================================================================= */
/* LPI2C                                                                     */
/* ========================================================================= */

void IRQ_LPI2C0_Master_Init(void);
void IRQ_LPI2C0_Slave_Init(void);

/* ========================================================================= */
/* PORT                                                                      */
/* ========================================================================= */

void IRQ_PORTD_Init(void);
void IRQ_PORTD_SetCallback(irq_callback_t pfCallback);
void IRQ_PORTE_Init(void);
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
void PORTD_IRQHandler(void);
void PORTE_IRQHandler(void);
void CAN0_ORed_0_15_MB_IRQHandler(void);
void CAN0_ORed_16_31_MB_IRQHandler(void);

#endif /* IRQ_H */
