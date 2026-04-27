#include "S32K144.h"
#include "irq.h"
#include "lpit.h"
#include "lpuart.h"
#include "adc.h"
#include "lpi2c.h"

/* ============================================================
 * IRQ numbers (from RM / NVIC table)
 * ============================================================ */

#define LPIT0_CH0_IRQ_NUMBER        (48U)
#define LPIT0_CH0_PRIORITY          (10U)

#define LPUART1_RXTX_IRQ_NUMBER     (33U)
#define LPUART1_RXTX_PRIORITY       (10U)

#define LPUART2_RXTX_IRQ_NUMBER     (35U)
#define LPUART2_RXTX_PRIORITY       (10U)

#define ADC0_IRQ_NUMBER      (39U)
#define ADC0_IRQ_PRIORITY    (0xA0U)

#define LPI2C0_MASTER_IRQ_NUMBER       (24U)
#define LPI2C0_MASTER_PRIORITY         (10U)

#define LPI2C0_SLAVE_IRQ_NUMBER        (25U)
#define LPI2C0_SLAVE_PRIORITY          (10U)

/* ============================================================
 * NVIC registers
 * ============================================================ */

#define NVIC_ISER_BASE   ((volatile uint32_t *)0xE000E100UL)
#define NVIC_ICPR_BASE   ((volatile uint32_t *)0xE000E280UL)
#define NVIC_IPR_BASE    ((volatile uint8_t  *)0xE000E400UL)

/* ============================================================
 * LPIT callback
 * ============================================================ */

static irq_callback_t s_lpit0Ch0Callback = (irq_callback_t)0;

/* ============================================================
 * LPIT IRQ config
 * ============================================================ */

void IRQ_LPIT0_Ch0_Init(void)
{
    NVIC_ICPR_BASE[LPIT0_CH0_IRQ_NUMBER / 32U] =
        (1UL << (LPIT0_CH0_IRQ_NUMBER % 32U));

    NVIC_IPR_BASE[LPIT0_CH0_IRQ_NUMBER] = LPIT0_CH0_PRIORITY;

    NVIC_ISER_BASE[LPIT0_CH0_IRQ_NUMBER / 32U] =
        (1UL << (LPIT0_CH0_IRQ_NUMBER % 32U));
}

void IRQ_LPIT0_Ch0_SetCallback(irq_callback_t cb)
{
    s_lpit0Ch0Callback = cb;
}

/* ============================================================
 * LPUART IRQ config
 * ============================================================ */

void IRQ_LPUART1_RxTx_Init(void)
{
    NVIC_ICPR_BASE[LPUART1_RXTX_IRQ_NUMBER / 32U] =
        (1UL << (LPUART1_RXTX_IRQ_NUMBER % 32U));

    NVIC_IPR_BASE[LPUART1_RXTX_IRQ_NUMBER] = LPUART1_RXTX_PRIORITY;

    NVIC_ISER_BASE[LPUART1_RXTX_IRQ_NUMBER / 32U] =
        (1UL << (LPUART1_RXTX_IRQ_NUMBER % 32U));
}

void IRQ_LPUART2_RxTx_Init(void)
{
    NVIC_ICPR_BASE[LPUART2_RXTX_IRQ_NUMBER / 32U] =
        (1UL << (LPUART2_RXTX_IRQ_NUMBER % 32U));

    NVIC_IPR_BASE[LPUART2_RXTX_IRQ_NUMBER] = LPUART2_RXTX_PRIORITY;

    NVIC_ISER_BASE[LPUART2_RXTX_IRQ_NUMBER / 32U] =
        (1UL << (LPUART2_RXTX_IRQ_NUMBER % 32U));
}

/* ============================================================
 * ISR implementations
 * ============================================================ */

void LPIT0_Ch0_IRQHandler(void)
{
    LPIT_ClearFlag(0U);

    if (s_lpit0Ch0Callback != (irq_callback_t)0)
    {
        s_lpit0Ch0Callback();
    }
}

void LPUART1_RxTx_IRQHandler(void)
{
    /* Delegate to driver */
    LPUART_IRQHandler(IP_LPUART1);
}

void LPUART2_RxTx_IRQHandler(void)
{
    /* Delegate to driver */
    LPUART_IRQHandler(IP_LPUART2);
}

void IRQ_ADC0_Init(void)
{
    NVIC_ICPR_BASE[ADC0_IRQ_NUMBER / 32U] =
        (1UL << (ADC0_IRQ_NUMBER % 32U));

    NVIC_IPR_BASE[ADC0_IRQ_NUMBER] = ADC0_IRQ_PRIORITY;

    NVIC_ISER_BASE[ADC0_IRQ_NUMBER / 32U] =
        (1UL << (ADC0_IRQ_NUMBER % 32U));
}

void ADC0_IRQHandler(void)
{
    ADC_IRQHandler(IP_ADC_0);
}

/* ============================================================
 * LPI2C IRQ config
 * ============================================================ */

void IRQ_LPI2C0_Master_Init(void)
{
    NVIC_ICPR_BASE[LPI2C0_MASTER_IRQ_NUMBER / 32U] =
        (1UL << (LPI2C0_MASTER_IRQ_NUMBER % 32U));

    NVIC_IPR_BASE[LPI2C0_MASTER_IRQ_NUMBER] = LPI2C0_MASTER_PRIORITY;

    NVIC_ISER_BASE[LPI2C0_MASTER_IRQ_NUMBER / 32U] =
        (1UL << (LPI2C0_MASTER_IRQ_NUMBER % 32U));
}

void IRQ_LPI2C0_Slave_Init(void)
{
    NVIC_ICPR_BASE[LPI2C0_SLAVE_IRQ_NUMBER / 32U] =
        (1UL << (LPI2C0_SLAVE_IRQ_NUMBER % 32U));

    NVIC_IPR_BASE[LPI2C0_SLAVE_IRQ_NUMBER] = LPI2C0_SLAVE_PRIORITY;

    NVIC_ISER_BASE[LPI2C0_SLAVE_IRQ_NUMBER / 32U] =
        (1UL << (LPI2C0_SLAVE_IRQ_NUMBER % 32U));
}

void LPI2C0_Master_IRQHandler(void)
{
    LPI2C_MasterIRQHandler(IP_LPI2C0);
}

//void LPI2C0_Slave_IRQHandler(void)
//{
//    LPI2C_SlaveIRQHandler(IP_LPI2C0);
//}
