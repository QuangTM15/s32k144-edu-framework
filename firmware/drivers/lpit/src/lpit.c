#include "S32K144.h"
#include "lpit.h"

#define LPIT_CHANNEL_COUNT          (4U)
#define LPIT_CLOCK_SOURCE_SPLL_DIV2 (6U)

static uint8_t LPIT_IsValidChannel(uint8_t channel)
{
    return (channel < LPIT_CHANNEL_COUNT) ? 1U : 0U;
}

void LPIT_Init(void)
{
    uint8_t channel;

    /* Disable PCC clock gate before changing clock source */
    IP_PCC->PCCn[PCC_LPIT_INDEX] &= ~PCC_PCCn_CGC_MASK;

    /* Select LPIT functional clock source = SPLL_DIV2_CLK */
    IP_PCC->PCCn[PCC_LPIT_INDEX] = PCC_PCCn_PCS(LPIT_CLOCK_SOURCE_SPLL_DIV2);

    /* Enable PCC clock gate for LPIT */
    IP_PCC->PCCn[PCC_LPIT_INDEX] |= PCC_PCCn_CGC_MASK;

    /* Enable LPIT module clock */
    IP_LPIT0->MCR = LPIT_MCR_M_CEN_MASK;

    /* Clear all timeout flags */
    IP_LPIT0->MSR = LPIT_MSR_TIF0_MASK
                  | LPIT_MSR_TIF1_MASK
                  | LPIT_MSR_TIF2_MASK
                  | LPIT_MSR_TIF3_MASK;

    /* Reset all channels */
    for (channel = 0U; channel < LPIT_CHANNEL_COUNT; channel++)
    {
        IP_LPIT0->TMR[channel].TCTRL = 0U;
        IP_LPIT0->TMR[channel].TVAL  = 0U;
    }
}

void LPIT_SetTimerPeriod(uint8_t channel, uint32_t ticks)
{
    if (LPIT_IsValidChannel(channel) == 0U)
    {
        return;
    }

    if (ticks == 0U)
    {
        return;
    }

    IP_LPIT0->TMR[channel].TVAL = ticks;
}

void LPIT_StartTimer(uint8_t channel)
{
    if (LPIT_IsValidChannel(channel) == 0U)
    {
        return;
    }

    IP_LPIT0->TMR[channel].TCTRL |= LPIT_TMR_TCTRL_T_EN_MASK;
}

void LPIT_StopTimer(uint8_t channel)
{
    if (LPIT_IsValidChannel(channel) == 0U)
    {
        return;
    }

    IP_LPIT0->TMR[channel].TCTRL &= ~LPIT_TMR_TCTRL_T_EN_MASK;
}

uint8_t LPIT_GetFlag(uint8_t channel)
{
    if (LPIT_IsValidChannel(channel) == 0U)
    {
        return 0U;
    }

    return ((IP_LPIT0->MSR & (1UL << channel)) != 0U) ? 1U : 0U;
}

void LPIT_ClearFlag(uint8_t channel)
{
    if (LPIT_IsValidChannel(channel) == 0U)
    {
        return;
    }

    /* MSR is write-1-to-clear */
    IP_LPIT0->MSR = (1UL << channel);
}