#ifndef IRQ_H
#define IRQ_H

#include <stdint.h>

/* Init NVIC cho LPIT channel */
void IRQ_LPIT0_Ch0_Init(void);

/* Handler (ISR) */
void LPIT0_Ch0_IRQHandler(void);

#endif /* IRQ_H */
