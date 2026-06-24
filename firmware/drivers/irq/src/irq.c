/**
 * @file irq.c
 * @brief Interrupt configuration and ISR bridge implementation.
 *
 * @details
 * This file configures NVIC interrupt lines and implements ISR entry
 * points used by the startup vector table.
 *
 * Clean status:
 * - LPIT0 channel 0 interrupt path has been cleaned.
 * - LPUART1/LPUART2 RxTx interrupt path has been cleaned.
 * - ADC0 interrupt path has been cleaned for analog stack support.
 * - LPI2C interrupt paths are intentionally left mostly unchanged and
 *   should be cleaned in later module-specific cleanup steps.
 *
 * LPUART clean scope:
 * - Added documentation for LPUART interrupt configuration and ISR.
 * - Reused NVIC helper macros for register index and interrupt bit mask.
 * - Kept interrupt numbers, priority values, public API, and runtime behavior
 *   unchanged.
 */

#include "S32K144.h"
#include "irq.h"
#include "lpit.h"
#include "lpuart.h"
#include "adc.h"
#include "lpi2c.h"
#include <stddef.h>

/* ============================================================
 * IRQ numbers and priorities
 * ============================================================ */

/**
 * @brief NVIC interrupt number for LPIT0 channel 0.
 */
#define LPIT0_CH0_IRQ_NUMBER        (48U)

/**
 * @brief NVIC priority value for LPIT0 channel 0.
 *
 * @details
 * The value is kept unchanged from the previous implementation.
 */
#define LPIT0_CH0_PRIORITY          (10U)

/**
 * @brief NVIC interrupt number for LPUART1 Rx/Tx interrupt.
 */
#define LPUART1_RXTX_IRQ_NUMBER     (33U)

/**
 * @brief NVIC priority value for LPUART1 Rx/Tx interrupt.
 *
 * @details
 * The value is kept unchanged from the previous implementation.
 */
#define LPUART1_RXTX_PRIORITY       (10U)

/**
 * @brief NVIC interrupt number for LPUART2 Rx/Tx interrupt.
 */
#define LPUART2_RXTX_IRQ_NUMBER     (35U)

/**
 * @brief NVIC priority value for LPUART2 Rx/Tx interrupt.
 *
 * @details
 * The value is kept unchanged from the previous implementation.
 */
#define LPUART2_RXTX_PRIORITY       (10U)

/**
 * @brief NVIC interrupt number for ADC0 interrupt.
 */
#define ADC0_IRQ_NUMBER             (39U)

/**
 * @brief NVIC priority value for ADC0 interrupt.
 *
 * @details
 * The value is kept unchanged from the previous implementation to avoid
 * changing interrupt scheduling behavior during analog stack cleanup.
 */
#define ADC0_IRQ_PRIORITY           (0xA0U)

/**
 * @brief NVIC interrupt number for LPI2C0 master interrupt.
 */
#define LPI2C0_MASTER_IRQ_NUMBER    (24U)

/**
 * @brief NVIC priority value for LPI2C0 master interrupt.
 */
#define LPI2C0_MASTER_PRIORITY      (10U)

/**
 * @brief NVIC interrupt number for LPI2C0 slave interrupt.
 */
#define LPI2C0_SLAVE_IRQ_NUMBER     (25U)
#define LPI2C0_SLAVE_PRIORITY       (10U)

/* ============================================================
 * NVIC register access
 * ============================================================ */

/**
 * @brief Cortex-M NVIC Interrupt Set-Enable Register base address.
 */
#define NVIC_ISER_BASE              ((volatile uint32_t *)0xE000E100UL)

/**
 * @brief Cortex-M NVIC Interrupt Clear-Pending Register base address.
 */
#define NVIC_ICPR_BASE              ((volatile uint32_t *)0xE000E280UL)

/**
 * @brief Cortex-M NVIC Interrupt Priority Register base address.
 */
#define NVIC_IPR_BASE               ((volatile uint8_t  *)0xE000E400UL)

/**
 * @brief Get NVIC register array index from interrupt number.
 *
 * @param[in] u8IrqNumber
 * NVIC interrupt number.
 */
#define IRQ_NVIC_REG_INDEX(u8IrqNumber)    ((u8IrqNumber) / 32U)

/**
 * @brief Get NVIC interrupt bit mask from interrupt number.
 *
 * @param[in] u8IrqNumber
 * NVIC interrupt number.
 */
#define IRQ_NVIC_BIT_MASK(u8IrqNumber)     (1UL << ((u8IrqNumber) % 32U))

/* ============================================================
 * LPIT callback storage
 * ============================================================ */

/**
 * @brief Callback executed from LPIT0 channel 0 interrupt context.
 */
static irq_callback_t s_pfLpit0Ch0Callback = NULL;

/* ============================================================
 * LPIT IRQ configuration
 * ============================================================ */

/**
 * @brief Initialize NVIC configuration for LPIT0 channel 0 interrupt.
 *
 * @details
 * This function clears any pending LPIT0 channel 0 interrupt request,
 * sets the NVIC priority, and enables the interrupt line in NVIC.
 *
 * The LPIT peripheral interrupt enable bit is configured separately by
 * LPIT_EnableInterrupt().
 *
 * @return None.
 */
void IRQ_LPIT0_Ch0_Init(void)
{
    /*
     * Clear any pending interrupt before enabling NVIC.
     * This prevents entering the ISR immediately because of a stale
     * pending interrupt flag.
     */
    NVIC_ICPR_BASE[IRQ_NVIC_REG_INDEX(LPIT0_CH0_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(LPIT0_CH0_IRQ_NUMBER);

    /*
     * Configure LPIT interrupt priority.
     * The priority value is kept unchanged to avoid behavior changes.
     */
    NVIC_IPR_BASE[LPIT0_CH0_IRQ_NUMBER] = LPIT0_CH0_PRIORITY;

    /* Enable LPIT0 channel 0 interrupt in NVIC. */
    NVIC_ISER_BASE[IRQ_NVIC_REG_INDEX(LPIT0_CH0_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(LPIT0_CH0_IRQ_NUMBER);
}

/**
 * @brief Register callback for LPIT0 channel 0 interrupt.
 *
 * @details
 * The callback is executed by LPIT0_Ch0_IRQHandler() after the LPIT
 * timeout flag is cleared.
 *
 * Passing a null callback disables callback execution.
 *
 * @param[in] pfCallback
 * Pointer to callback function.
 *
 * @return None.
 */
void IRQ_LPIT0_Ch0_SetCallback(irq_callback_t pfCallback)
{
    s_pfLpit0Ch0Callback = pfCallback;
}

/* ============================================================
 * LPUART IRQ configuration
 * ============================================================ */

/**
 * @brief Initialize NVIC configuration for LPUART1 Rx/Tx interrupt.
 *
 * @details
 * This function clears any pending LPUART1 Rx/Tx interrupt request,
 * sets the NVIC priority, and enables the interrupt line in NVIC.
 *
 * The LPUART peripheral interrupt enable bit is configured separately by
 * the LPUART driver.
 *
 * @return None.
 */
void IRQ_LPUART1_RxTx_Init(void)
{
    /*
     * Clear any pending interrupt before enabling NVIC.
     * This avoids entering the ISR immediately because of a stale
     * pending interrupt request.
     */
    NVIC_ICPR_BASE[IRQ_NVIC_REG_INDEX(LPUART1_RXTX_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(LPUART1_RXTX_IRQ_NUMBER);

    /*
     * Configure LPUART1 Rx/Tx interrupt priority.
     * The priority value is kept unchanged to avoid behavior changes.
     */
    NVIC_IPR_BASE[LPUART1_RXTX_IRQ_NUMBER] = LPUART1_RXTX_PRIORITY;

    /* Enable LPUART1 Rx/Tx interrupt in NVIC. */
    NVIC_ISER_BASE[IRQ_NVIC_REG_INDEX(LPUART1_RXTX_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(LPUART1_RXTX_IRQ_NUMBER);
}

/**
 * @brief Initialize NVIC configuration for LPUART2 Rx/Tx interrupt.
 *
 * @details
 * This function clears any pending LPUART2 Rx/Tx interrupt request,
 * sets the NVIC priority, and enables the interrupt line in NVIC.
 *
 * The LPUART peripheral interrupt enable bit is configured separately by
 * the LPUART driver.
 *
 * @return None.
 */
void IRQ_LPUART2_RxTx_Init(void)
{
    /*
     * Clear any pending interrupt before enabling NVIC.
     * This avoids entering the ISR immediately because of a stale
     * pending interrupt request.
     */
    NVIC_ICPR_BASE[IRQ_NVIC_REG_INDEX(LPUART2_RXTX_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(LPUART2_RXTX_IRQ_NUMBER);

    /*
     * Configure LPUART2 Rx/Tx interrupt priority.
     * The priority value is kept unchanged to avoid behavior changes.
     */
    NVIC_IPR_BASE[LPUART2_RXTX_IRQ_NUMBER] = LPUART2_RXTX_PRIORITY;

    /* Enable LPUART2 Rx/Tx interrupt in NVIC. */
    NVIC_ISER_BASE[IRQ_NVIC_REG_INDEX(LPUART2_RXTX_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(LPUART2_RXTX_IRQ_NUMBER);
}

/* ============================================================
 * ISR implementations
 * ============================================================ */

/**
 * @brief LPIT0 channel 0 interrupt service routine.
 *
 * @details
 * This ISR clears the LPIT channel timeout flag first, then executes
 * the registered callback if one is available.
 *
 * The callback is expected to be short because it runs in interrupt
 * context. In the current time module, this callback increments the
 * millisecond system tick.
 *
 * @return None.
 */
void LPIT0_Ch0_IRQHandler(void)
{
    /*
     * Clear the LPIT timeout flag before executing user logic.
     * This prevents the same interrupt event from being handled again
     * after exiting the ISR.
     */
    LPIT_ClearFlag(0U);

    if (NULL != s_pfLpit0Ch0Callback)
    {
        s_pfLpit0Ch0Callback();
    }
}

/**
 * @brief LPUART1 Rx/Tx interrupt service routine.
 *
 * @details
 * This ISR delegates interrupt processing to the LPUART driver.
 * The driver is responsible for checking interrupt flags, reading
 * received data, and storing it into the RX software buffer.
 *
 * @return None.
 */
void LPUART1_RxTx_IRQHandler(void)
{
    LPUART_IRQHandler(IP_LPUART1);
}

/**
 * @brief LPUART2 Rx/Tx interrupt service routine.
 *
 * @details
 * This ISR delegates interrupt processing to the LPUART driver.
 * The driver is responsible for checking interrupt flags, reading
 * received data, and storing it into the RX software buffer.
 *
 * @return None.
 */
void LPUART2_RxTx_IRQHandler(void)
{
    LPUART_IRQHandler(IP_LPUART2);
}
/**
 * @brief Initialize NVIC configuration for ADC0 interrupt.
 *
 * @details
 * This function clears any pending ADC0 interrupt request, sets the NVIC
 * priority, and enables the ADC0 interrupt line in NVIC.
 *
 * The ADC peripheral interrupt enable bit is configured separately by
 * the ADC driver when a conversion is started.
 *
 * @return None.
 */
void IRQ_ADC0_Init(void)
{
    /*
     * Clear any pending ADC0 interrupt before enabling NVIC.
     * This prevents a stale ADC conversion-complete request from causing
     * an unexpected ISR entry immediately after initialization.
     */
    NVIC_ICPR_BASE[IRQ_NVIC_REG_INDEX(ADC0_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(ADC0_IRQ_NUMBER);

    /*
     * Configure ADC0 interrupt priority.
     * The priority value is kept unchanged to avoid behavior changes.
     */
    NVIC_IPR_BASE[ADC0_IRQ_NUMBER] = ADC0_IRQ_PRIORITY;

    /* Enable ADC0 interrupt in NVIC. */
    NVIC_ISER_BASE[IRQ_NVIC_REG_INDEX(ADC0_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(ADC0_IRQ_NUMBER);
}

/**
 * @brief ADC0 interrupt service routine.
 *
 * @details
 * This ISR delegates ADC0 interrupt processing to the ADC driver.
 * The ADC driver checks the conversion-complete flag, reads the result
 * register, and updates its internal conversion state.
 *
 * @return None.
 */
void ADC0_IRQHandler(void)
{
    ADC_IRQHandler(IP_ADC_0);
}

/* ============================================================
 * LPI2C IRQ config
 * ============================================================ */

/* ============================================================
 * LPI2C IRQ configuration
 * ============================================================ */

/**
 * @brief Initialize NVIC configuration for LPI2C0 master interrupt.
 *
 * @details
 * This function clears any pending LPI2C0 master interrupt request,
 * configures the interrupt priority, and enables the interrupt line
 * in the NVIC.
 *
 * The LPI2C master interrupt sources are configured separately by
 * the LPI2C driver.
 *
 * @return None.
 */
void IRQ_LPI2C0_Master_Init(void)
{
    NVIC_ICPR_BASE[IRQ_NVIC_REG_INDEX(LPI2C0_MASTER_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(LPI2C0_MASTER_IRQ_NUMBER);

    NVIC_IPR_BASE[LPI2C0_MASTER_IRQ_NUMBER] =
        LPI2C0_MASTER_PRIORITY;

    NVIC_ISER_BASE[IRQ_NVIC_REG_INDEX(LPI2C0_MASTER_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(LPI2C0_MASTER_IRQ_NUMBER);
}

/**
 * @brief LPI2C0 master interrupt service routine.
 *
 * @details
 * This ISR delegates interrupt processing to the LPI2C driver.
 *
 * The driver handles:
 * - Master transmit state machine
 * - Master receive state machine
 * - Transfer completion
 * - Error handling
 *
 * @return None.
 */
void LPI2C0_Master_IRQHandler(void)
{
    LPI2C_MasterIRQHandler(IP_LPI2C0);
}

/**
 * @brief Initialize NVIC configuration for LPI2C0 slave interrupt.
 *
 * @details
 * This function clears any pending LPI2C0 slave interrupt request,
 * configures the interrupt priority, and enables the interrupt line
 * in the NVIC.
 *
 * The LPI2C slave interrupt sources are configured separately by
 * the LPI2C driver.
 *
 * @return None.
 */
void IRQ_LPI2C0_Slave_Init(void)
{
    NVIC_ICPR_BASE[IRQ_NVIC_REG_INDEX(LPI2C0_SLAVE_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(LPI2C0_SLAVE_IRQ_NUMBER);

    NVIC_IPR_BASE[LPI2C0_SLAVE_IRQ_NUMBER] =
        LPI2C0_SLAVE_PRIORITY;

    NVIC_ISER_BASE[IRQ_NVIC_REG_INDEX(LPI2C0_SLAVE_IRQ_NUMBER)] =
        IRQ_NVIC_BIT_MASK(LPI2C0_SLAVE_IRQ_NUMBER);
}

/**
 * @brief LPI2C0 slave interrupt service routine.
 *
 * @details
 * This ISR delegates interrupt processing to the LPI2C slave driver.
 *
 * The driver handles:
 * - Address match detection
 * - Slave receive events
 * - Slave transmit requests
 * - STOP detection
 * - Error conditions
 *
 * @return None.
 */
void LPI2C0_Slave_IRQHandler(void)
{
    LPI2C_SlaveIRQHandler(IP_LPI2C0);
}
