/**
 * @file lpit.c
 * @brief Low Power Interrupt Timer driver implementation.
 *
 * @details
 * This file implements basic LPIT services for the NXP S32K144
 * microcontroller.
 *
 * The LPIT driver provides low-level register access for:
 * - LPIT module initialization.
 * - Timer period configuration.
 * - Timer channel start and stop control.
 * - Timeout flag read and clear operations.
 * - Timer interrupt enable and disable control.
 *
 * This module is part of the Driver Layer and must not depend on the
 * Arduino-style API layer.
 */

#include "S32K144.h"
#include "lpit.h"

/**
 * @brief Number of timer channels available in LPIT0.
 */
#define LPIT_CHANNEL_COUNT              (4U)

/**
 * @brief PCC clock source value for SPLL_DIV2_CLK.
 *
 * @details
 * The current project clock configuration uses SPLL_DIV2_CLK as the LPIT
 * functional clock source. With the existing 80 MHz RUN mode setup,
 * SPLL_DIV2_CLK is expected to provide a 40 MHz LPIT clock.
 */
#define LPIT_CLOCK_SOURCE_SPLL_DIV2     (6U)

/**
 * @brief Generate a bit mask for an LPIT channel flag or interrupt bit.
 *
 * @param[in] u8Channel
 * LPIT channel index.
 */
#define LPIT_CHANNEL_MASK(u8Channel)    (1UL << (u8Channel))

/**
 * @brief Check whether an LPIT channel index is valid.
 *
 * @details
 * LPIT0 on S32K144 provides four timer channels, indexed from 0 to 3.
 * This helper prevents invalid array access to LPIT timer registers.
 *
 * @param[in] u8Channel
 * LPIT channel index.
 *
 * @return uint8_t
 *
 * @retval 0U
 * Channel is invalid.
 *
 * @retval 1U
 * Channel is valid.
 */
static uint8_t LPIT_IsValidChannel(uint8_t u8Channel)
{
    uint8_t u8IsValid = 0U;

    if (LPIT_CHANNEL_COUNT > u8Channel)
    {
        u8IsValid = 1U;
    }

    return u8IsValid;
}

/**
 * @brief Initialize the LPIT peripheral.
 *
 * @details
 * This function configures the LPIT peripheral clock source, enables
 * the LPIT module clock, clears pending timeout flags, and resets all
 * timer channels to a disabled state.
 *
 * The LPIT timer channels are not started by this function.
 *
 * @return None.
 */
void LPIT_Init(void)
{
    uint8_t u8Channel = 0U;

    /*
     * Disable the PCC clock gate before changing PCS.
     * On S32K peripherals, the clock source should be configured while
     * the peripheral clock gate is disabled.
     */
    IP_PCC->PCCn[PCC_LPIT_INDEX] &= ~PCC_PCCn_CGC_MASK;

    /*
     * Select SPLL_DIV2_CLK as the LPIT functional clock source.
     * The CGC bit is still disabled at this point.
     */
    IP_PCC->PCCn[PCC_LPIT_INDEX] = PCC_PCCn_PCS(LPIT_CLOCK_SOURCE_SPLL_DIV2);

    /* Enable PCC clock gate so LPIT registers can be accessed. */
    IP_PCC->PCCn[PCC_LPIT_INDEX] |= PCC_PCCn_CGC_MASK;

    /*
     * Enable the LPIT module clock.
     * M_CEN must be set before timer channel registers are used.
     */
    IP_LPIT0->MCR = LPIT_MCR_M_CEN_MASK;

    /*
     * Clear all pending timeout flags.
     * LPIT MSR timeout flags are write-1-to-clear bits.
     */
    IP_LPIT0->MSR = LPIT_MSR_TIF0_MASK
                  | LPIT_MSR_TIF1_MASK
                  | LPIT_MSR_TIF2_MASK
                  | LPIT_MSR_TIF3_MASK;

    /*
     * Reset all timer channels to a known disabled state.
     * TVAL is cleared here so each user must explicitly configure
     * the timer period before starting a channel.
     */
    for (u8Channel = 0U; u8Channel < LPIT_CHANNEL_COUNT; u8Channel++)
    {
        IP_LPIT0->TMR[u8Channel].TCTRL = 0U;
        IP_LPIT0->TMR[u8Channel].TVAL  = 0U;
    }
}

/**
 * @brief Set the period value for an LPIT timer channel.
 *
 * @details
 * This function writes the timer value register of the selected LPIT
 * channel. Invalid channels and zero tick values are ignored.
 *
 * @param[in] u8Channel
 * LPIT channel index.
 *
 * @param[in] u32Ticks
 * Timer period value in LPIT clock ticks.
 *
 * @return None.
 */
void LPIT_SetTimerPeriod(uint8_t u8Channel, uint32_t u32Ticks)
{
    if ((1U == LPIT_IsValidChannel(u8Channel)) && (0U != u32Ticks))
    {
        IP_LPIT0->TMR[u8Channel].TVAL = u32Ticks;
    }
}

/**
 * @brief Start an LPIT timer channel.
 *
 * @details
 * This function sets the timer enable bit for the selected LPIT channel.
 * Invalid channels are ignored.
 *
 * @param[in] u8Channel
 * LPIT channel index.
 *
 * @return None.
 */
void LPIT_StartTimer(uint8_t u8Channel)
{
    if (1U == LPIT_IsValidChannel(u8Channel))
    {
        IP_LPIT0->TMR[u8Channel].TCTRL |= LPIT_TMR_TCTRL_T_EN_MASK;
    }
}

/**
 * @brief Stop an LPIT timer channel.
 *
 * @details
 * This function clears the timer enable bit for the selected LPIT channel.
 * Invalid channels are ignored.
 *
 * @param[in] u8Channel
 * LPIT channel index.
 *
 * @return None.
 */
void LPIT_StopTimer(uint8_t u8Channel)
{
    if (1U == LPIT_IsValidChannel(u8Channel))
    {
        IP_LPIT0->TMR[u8Channel].TCTRL &= ~LPIT_TMR_TCTRL_T_EN_MASK;
    }
}

/**
 * @brief Get timeout flag status of an LPIT timer channel.
 *
 * @details
 * This function reads the LPIT Module Status Register and returns whether
 * the timeout flag of the selected channel is set.
 *
 * Invalid channels return 0U.
 *
 * @param[in] u8Channel
 * LPIT channel index.
 *
 * @return uint8_t
 *
 * @retval 0U
 * Timeout flag is not set or channel is invalid.
 *
 * @retval 1U
 * Timeout flag is set.
 */
uint8_t LPIT_GetFlag(uint8_t u8Channel)
{
    uint8_t u8FlagStatus = 0U;

    if (1U == LPIT_IsValidChannel(u8Channel))
    {
        if (0U != (IP_LPIT0->MSR & LPIT_CHANNEL_MASK(u8Channel)))
        {
            u8FlagStatus = 1U;
        }
    }

    return u8FlagStatus;
}

/**
 * @brief Clear timeout flag of an LPIT timer channel.
 *
 * @details
 * LPIT timeout flags are write-1-to-clear. Therefore this function writes
 * a 1 only to the selected channel flag bit.
 *
 * Invalid channels are ignored.
 *
 * @param[in] u8Channel
 * LPIT channel index.
 *
 * @return None.
 */
void LPIT_ClearFlag(uint8_t u8Channel)
{
    if (1U == LPIT_IsValidChannel(u8Channel))
    {
        IP_LPIT0->MSR = LPIT_CHANNEL_MASK(u8Channel);
    }
}

/**
 * @brief Enable interrupt for an LPIT timer channel.
 *
 * @details
 * This function enables the LPIT channel interrupt at the peripheral
 * level by setting the corresponding bit in MIER.
 *
 * The NVIC interrupt line is configured separately by the IRQ module.
 *
 * Invalid channels are ignored.
 *
 * @param[in] u8Channel
 * LPIT channel index.
 *
 * @return None.
 */
void LPIT_EnableInterrupt(uint8_t u8Channel)
{
    if (1U == LPIT_IsValidChannel(u8Channel))
    {
        IP_LPIT0->MIER |= LPIT_CHANNEL_MASK(u8Channel);
    }
}

/**
 * @brief Disable interrupt for an LPIT timer channel.
 *
 * @details
 * This function disables the LPIT channel interrupt at the peripheral
 * level by clearing the corresponding bit in MIER.
 *
 * Invalid channels are ignored.
 *
 * @param[in] u8Channel
 * LPIT channel index.
 *
 * @return None.
 */
void LPIT_DisableInterrupt(uint8_t u8Channel)
{
    if (1U == LPIT_IsValidChannel(u8Channel))
    {
        IP_LPIT0->MIER &= ~LPIT_CHANNEL_MASK(u8Channel);
    }
}
