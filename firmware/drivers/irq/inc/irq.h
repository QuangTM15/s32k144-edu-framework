#ifndef IRQ_H
#define IRQ_H

#include <stdint.h>


typedef void (*irq_callback_t)(void);

void IRQ_LPIT0_Ch0_Init(void);
void IRQ_LPIT0_Ch0_SetCallback(irq_callback_t cb);

/* ============================================================
 * LPUART
 * ============================================================ */

void IRQ_LPUART1_RxTx_Init(void);
void IRQ_LPUART2_RxTx_Init(void);

/* ============================================================
 * ISR declarations
 * ============================================================ */

void LPIT0_Ch0_IRQHandler(void);
void LPUART1_RxTx_IRQHandler(void);
void LPUART2_RxTx_IRQHandler(void);

void IRQ_ADC0_Init(void);
void ADC0_IRQHandler(void);
#endif /* IRQ_H */
