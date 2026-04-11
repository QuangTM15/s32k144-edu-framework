#ifndef IRQ_H
#define IRQ_H

#include <stdint.h>

typedef void (*irq_callback_t)(void);

void IRQ_LPIT0_Ch0_Init(void);
void IRQ_LPIT0_Ch0_SetCallback(irq_callback_t cb);

void LPIT0_Ch0_IRQHandler(void);

#endif /* IRQ_H */
