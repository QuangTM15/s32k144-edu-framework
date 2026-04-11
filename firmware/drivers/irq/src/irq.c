#include "S32K144.h"
#include "irq.h"
#include "lpit.h"
#include "gpio.h"

#define LPIT0_CH0_IRQ_NUMBER    (48U)
#define LPIT0_CH0_PRIORITY      (10U)

#define NVIC_ISER_BASE   ((volatile uint32_t *)0xE000E100UL)
#define NVIC_ICPR_BASE   ((volatile uint32_t *)0xE000E280UL)
#define NVIC_IPR_BASE    ((volatile uint8_t  *)0xE000E400UL)

void IRQ_LPIT0_Ch0_Init(void)
{
    NVIC_ICPR_BASE[LPIT0_CH0_IRQ_NUMBER / 32U] = (1UL << (LPIT0_CH0_IRQ_NUMBER % 32U));
    NVIC_IPR_BASE[LPIT0_CH0_IRQ_NUMBER] = LPIT0_CH0_PRIORITY;
    NVIC_ISER_BASE[LPIT0_CH0_IRQ_NUMBER / 32U] = (1UL << (LPIT0_CH0_IRQ_NUMBER % 32U));
}

void LPIT0_Ch0_IRQHandler(void)
{
    LPIT_ClearFlag(0U);
    GPIO_TogglePin(IP_PTD, 0U);
}
