/**
 * @file irq.c
 * @brief Interrupt configuration and ISR bridge implementation.
 *
 * @details
 * This file configures NVIC interrupt lines and implements ISR entry
 * points used by the startup vector table.
 */

#include "S32K144.h"
#include "irq.h"
#include "lpit.h"
#include "lpuart.h"
#include "adc.h"
#include "lpi2c.h"

#include <stddef.h>

/* ========================================================================= */
/* IRQ Numbers and Priorities                                                */
/* ========================================================================= */

#define LPIT0_CH0_IRQ_NUMBER        (48U)
#define LPIT0_CH0_PRIORITY          (10U)

#define LPUART1_RXTX_IRQ_NUMBER     (33U)
#define LPUART1_RXTX_PRIORITY       (10U)

#define LPUART2_RXTX_IRQ_NUMBER     (35U)
#define LPUART2_RXTX_PRIORITY       (10U)

#define ADC0_IRQ_NUMBER             (39U)
#define ADC0_IRQ_PRIORITY           (0xA0U)

#define LPI2C0_MASTER_IRQ_NUMBER    (24U)
#define LPI2C0_MASTER_PRIORITY      (10U)

#define LPI2C0_SLAVE_IRQ_NUMBER     (25U)
#define LPI2C0_SLAVE_PRIORITY       (10U)

/**
 * @brief NVIC interrupt number for PORTD.
 */
#define PORTD_IRQ_NUMBER            (62U)


/**
 * @brief NVIC priority for PORTD interrupt.
 */
#define PORTD_IRQ_PRIORITY          (10U)

/**
 * @brief NVIC interrupt number for PORTE.
 */
#define PORTE_IRQ_NUMBER            (63U)

/**
 * @brief NVIC priority for PORTE interrupt.
 */
#define PORTE_IRQ_PRIORITY          (10U)

/* ========================================================================= */
/* NVIC Register Access                                                      */
/* ========================================================================= */

#define NVIC_ISER_BASE              ((volatile uint32_t *)0xE000E100UL)
#define NVIC_ICPR_BASE              ((volatile uint32_t *)0xE000E280UL)
#define NVIC_IPR_BASE               ((volatile uint8_t *)0xE000E400UL)

#define IRQ_NVIC_REG_INDEX(u8IrqNumber) \
    ((u8IrqNumber) / 32U)

#define IRQ_NVIC_BIT_MASK(u8IrqNumber) \
    (1UL << ((u8IrqNumber) % 32U))

/* ========================================================================= */
/* Callback Storage                                                          */
/* ========================================================================= */

/**
 * @brief Callback executed from LPIT0 channel 0 interrupt context.
 */
static irq_callback_t s_pfLpit0Ch0Callback = NULL;

/**
 * @brief Callback executed from PORTD interrupt context.
 */
static irq_callback_t s_pfPortDCallback = NULL;

/**
 * @brief Callback executed from PORTE interrupt context.
 */
static irq_callback_t s_pfPortECallback = NULL;

/* ========================================================================= */
/* LPIT IRQ Configuration                                                    */
/* ========================================================================= */

void IRQ_LPIT0_Ch0_Init(void)
{
    NVIC_ICPR_BASE[IRQ_NVIC_REG_INDEX(LPIT0_CH0_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(LPIT0_CH0_IRQ_NUMBER);

    NVIC_IPR_BASE[LPIT0_CH0_IRQ_NUMBER] =
        LPIT0_CH0_PRIORITY;

    NVIC_ISER_BASE[IRQ_NVIC_REG_INDEX(LPIT0_CH0_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(LPIT0_CH0_IRQ_NUMBER);

    return;
}

void IRQ_LPIT0_Ch0_SetCallback(irq_callback_t pfCallback)
{
    s_pfLpit0Ch0Callback = pfCallback;

    return;
}

/* ========================================================================= */
/* LPUART IRQ Configuration                                                  */
/* ========================================================================= */

void IRQ_LPUART1_RxTx_Init(void)
{
    NVIC_ICPR_BASE[IRQ_NVIC_REG_INDEX(LPUART1_RXTX_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(LPUART1_RXTX_IRQ_NUMBER);

    NVIC_IPR_BASE[LPUART1_RXTX_IRQ_NUMBER] =
        LPUART1_RXTX_PRIORITY;

    NVIC_ISER_BASE[IRQ_NVIC_REG_INDEX(LPUART1_RXTX_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(LPUART1_RXTX_IRQ_NUMBER);

    return;
}

void IRQ_LPUART2_RxTx_Init(void)
{
    NVIC_ICPR_BASE[IRQ_NVIC_REG_INDEX(LPUART2_RXTX_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(LPUART2_RXTX_IRQ_NUMBER);

    NVIC_IPR_BASE[LPUART2_RXTX_IRQ_NUMBER] =
        LPUART2_RXTX_PRIORITY;

    NVIC_ISER_BASE[IRQ_NVIC_REG_INDEX(LPUART2_RXTX_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(LPUART2_RXTX_IRQ_NUMBER);

    return;
}

/* ========================================================================= */
/* ADC IRQ Configuration                                                     */
/* ========================================================================= */

void IRQ_ADC0_Init(void)
{
    NVIC_ICPR_BASE[IRQ_NVIC_REG_INDEX(ADC0_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(ADC0_IRQ_NUMBER);

    NVIC_IPR_BASE[ADC0_IRQ_NUMBER] =
        ADC0_IRQ_PRIORITY;

    NVIC_ISER_BASE[IRQ_NVIC_REG_INDEX(ADC0_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(ADC0_IRQ_NUMBER);

    return;
}

/* ========================================================================= */
/* LPI2C IRQ Configuration                                                   */
/* ========================================================================= */

void IRQ_LPI2C0_Master_Init(void)
{
    NVIC_ICPR_BASE[IRQ_NVIC_REG_INDEX(LPI2C0_MASTER_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(LPI2C0_MASTER_IRQ_NUMBER);

    NVIC_IPR_BASE[LPI2C0_MASTER_IRQ_NUMBER] =
        LPI2C0_MASTER_PRIORITY;

    NVIC_ISER_BASE[IRQ_NVIC_REG_INDEX(LPI2C0_MASTER_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(LPI2C0_MASTER_IRQ_NUMBER);

    return;
}

void IRQ_LPI2C0_Slave_Init(void)
{
    NVIC_ICPR_BASE[IRQ_NVIC_REG_INDEX(LPI2C0_SLAVE_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(LPI2C0_SLAVE_IRQ_NUMBER);

    NVIC_IPR_BASE[LPI2C0_SLAVE_IRQ_NUMBER] =
        LPI2C0_SLAVE_PRIORITY;

    NVIC_ISER_BASE[IRQ_NVIC_REG_INDEX(LPI2C0_SLAVE_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(LPI2C0_SLAVE_IRQ_NUMBER);

    return;
}

/* ========================================================================= */
/* PORT IRQ Configuration                                                    */
/* ========================================================================= */

void IRQ_PORTD_Init(void)
{
    /*
     * Clear any stale pending PORTD request before enabling the interrupt.
     */
    NVIC_ICPR_BASE[IRQ_NVIC_REG_INDEX(PORTD_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(PORTD_IRQ_NUMBER);

    /*
     * Configure PORTD interrupt priority.
     */
    NVIC_IPR_BASE[PORTD_IRQ_NUMBER] =
        PORTD_IRQ_PRIORITY;

    /*
     * Enable PORTD interrupt in NVIC.
     */
    NVIC_ISER_BASE[IRQ_NVIC_REG_INDEX(PORTD_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(PORTD_IRQ_NUMBER);

    return;
}

void IRQ_PORTD_SetCallback(irq_callback_t pfCallback)
{
    s_pfPortDCallback = pfCallback;

    return;
}

void IRQ_PORTE_Init(void)
{
    /*
     * Clear any stale pending PORTE request before enabling the interrupt.
     */
    NVIC_ICPR_BASE[IRQ_NVIC_REG_INDEX(PORTE_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(PORTE_IRQ_NUMBER);

    /*
     * Configure PORTE interrupt priority.
     */
    NVIC_IPR_BASE[PORTE_IRQ_NUMBER] =
        PORTE_IRQ_PRIORITY;

    /*
     * Enable PORTE interrupt in NVIC.
     */
    NVIC_ISER_BASE[IRQ_NVIC_REG_INDEX(PORTE_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(PORTE_IRQ_NUMBER);

    return;
}

void IRQ_PORTE_SetCallback(irq_callback_t pfCallback)
{
    s_pfPortECallback = pfCallback;

    return;
}

/* ========================================================================= */
/* ISR Implementations                                                       */
/* ========================================================================= */

void LPIT0_Ch0_IRQHandler(void)
{
    LPIT_ClearFlag(0U);

    if (NULL != s_pfLpit0Ch0Callback)
    {
        s_pfLpit0Ch0Callback();
    }

    return;
}

void LPUART1_RxTx_IRQHandler(void)
{
    LPUART_IRQHandler(IP_LPUART1);

    return;
}

void LPUART2_RxTx_IRQHandler(void)
{
    LPUART_IRQHandler(IP_LPUART2);

    return;
}

void ADC0_IRQHandler(void)
{
    ADC_IRQHandler(IP_ADC_0);

    return;
}

void LPI2C0_Master_IRQHandler(void)
{
    LPI2C_MasterIRQHandler(IP_LPI2C0);

    return;
}

void LPI2C0_Slave_IRQHandler(void)
{
    LPI2C_SlaveIRQHandler(IP_LPI2C0);

    return;
}

void PORTD_IRQHandler(void)
{
    /*
     * PORT-specific interrupt flag handling belongs to the registered
     * callback because multiple pins share this interrupt vector.
     */
    if (NULL != s_pfPortDCallback)
    {
        s_pfPortDCallback();
    }

    return;
}

void PORTE_IRQHandler(void)
{
    /*
     * PORT-specific interrupt flag handling belongs to the registered
     * callback because multiple pins share this interrupt vector.
     */
    if (NULL != s_pfPortECallback)
    {
        s_pfPortECallback();
    }

    return;
}
