#include "ftm.h"

typedef struct
{
    uint8_t initialized;
    uint32_t srcClockHz;
    FTM_ClockSource_t clockSource;
    FTM_Prescaler_t prescaler;
    uint16_t modulo;
} FTM_State_t;

static FTM_State_t g_ftmState[3] = {0};

static FTM_Type *FTM_GetBase(FTM_Instance_t instance)
{
    switch (instance)
    {
    case IP_FTM_0:
        return IP_FTM0;
    case IP_FTM_1:
        return IP_FTM1;
    case IP_FTM_2:
        return IP_FTM2;
    default:
        return (FTM_Type *)0;
    }
}

static uint32_t FTM_GetPccIndex(FTM_Instance_t instance)
{
    switch (instance)
    {
    case IP_FTM_0:
        return PCC_FTM0_INDEX;
    case IP_FTM_1:
        return PCC_FTM1_INDEX;
    case IP_FTM_2:
        return PCC_FTM2_INDEX;
    default:
        return 0xFFFFFFFFUL;
    }
}

static uint32_t FTM_GetPrescalerDivisor(FTM_Prescaler_t prescaler)
{
    switch (prescaler)
    {
    case FTM_PRESCALER_DIV_1:
        return 1UL;
    case FTM_PRESCALER_DIV_2:
        return 2UL;
    case FTM_PRESCALER_DIV_4:
        return 4UL;
    case FTM_PRESCALER_DIV_8:
        return 8UL;
    case FTM_PRESCALER_DIV_16:
        return 16UL;
    case FTM_PRESCALER_DIV_32:
        return 32UL;
    case FTM_PRESCALER_DIV_64:
        return 64UL;
    case FTM_PRESCALER_DIV_128:
        return 128UL;
    default:
        return 0UL;
    }
}

FTM_Status_t FTM_Init(FTM_Instance_t instance, const FTM_Config_t *config)
{
    FTM_Type *base;
    uint32_t pccIndex;
    FTM_State_t *state;

    base = FTM_GetBase(instance);
    if (base == (FTM_Type *)0)
    {
        return FTM_STATUS_INVALID_INSTANCE;
    }

    if (config == (const FTM_Config_t *)0)
    {
        return FTM_STATUS_INVALID_CONFIG;
    }

    if (config->srcClockHz == 0UL)
    {
        return FTM_STATUS_INVALID_CONFIG;
    }

    if ((uint32_t)config->clockSource > 3U)
    {
        return FTM_STATUS_INVALID_CONFIG;
    }

    if (FTM_GetPrescalerDivisor(config->prescaler) == 0UL)
    {
        return FTM_STATUS_INVALID_CONFIG;
    }

    pccIndex = FTM_GetPccIndex(instance);
    if (pccIndex == 0xFFFFFFFFUL)
    {
        return FTM_STATUS_INVALID_INSTANCE;
    }

    /* Configure PCC for FTM:
     * PCS = 1 -> same scheme you are using now (SOSCDIV1 clock path)
     */
    IP_PCC->PCCn[pccIndex] &= ~PCC_PCCn_CGC_MASK;
    IP_PCC->PCCn[pccIndex] = PCC_PCCn_PCS(1U);
    IP_PCC->PCCn[pccIndex] |= PCC_PCCn_CGC_MASK;

    /* Stop counter before config */
    base->SC = 0U;

    /* Disable write protection.
     * Keep simple PWM behavior for phase 1, do not use enhanced sync flow.
     */
    base->MODE = FTM_MODE_WPDIS_MASK;

    /* Independent channels, no polarity inversion, no masking */
    base->COMBINE = 0U;
    base->POL = 0U;
    base->OUTMASK = 0U;
    base->OUTINIT = 0U;
    base->CONF = 0U;

    /* Counter base */
    base->CNTIN = 0U;
    base->CNT = 0U;                 /* writing CNT loads CNTIN */
    base->MOD = FTM_MOD_MOD(config->modulo);

    /* Prescaler set, counter still stopped, edge-aligned */
    base->SC = FTM_SC_PS((uint32_t)config->prescaler) |
               FTM_SC_CLKS(0U) |
               FTM_SC_CPWMS(0U);

    state = &g_ftmState[(uint8_t)instance];
    state->initialized = 1U;
    state->srcClockHz = config->srcClockHz;
    state->clockSource = config->clockSource;
    state->prescaler = config->prescaler;
    state->modulo = config->modulo;

    return FTM_STATUS_OK;
}

FTM_Status_t FTM_StartCounter(FTM_Instance_t instance)
{
    FTM_Type *base;
    FTM_State_t *state;

    base = FTM_GetBase(instance);
    if (base == (FTM_Type *)0)
    {
        return FTM_STATUS_INVALID_INSTANCE;
    }

    state = &g_ftmState[(uint8_t)instance];
    if (state->initialized == 0U)
    {
        return FTM_STATUS_NOT_INIT;
    }

    base->SC &= ~FTM_SC_CLKS_MASK;
    base->SC |= FTM_SC_CLKS((uint32_t)state->clockSource);

    return FTM_STATUS_OK;
}

FTM_Status_t FTM_StopCounter(FTM_Instance_t instance)
{
    FTM_Type *base;
    FTM_State_t *state;

    base = FTM_GetBase(instance);
    if (base == (FTM_Type *)0)
    {
        return FTM_STATUS_INVALID_INSTANCE;
    }

    state = &g_ftmState[(uint8_t)instance];
    if (state->initialized == 0U)
    {
        return FTM_STATUS_NOT_INIT;
    }

    base->SC &= ~FTM_SC_CLKS_MASK;

    return FTM_STATUS_OK;
}

FTM_Status_t FTM_InitPwm(FTM_Instance_t instance, const FTM_PwmConfig_t *config)
{
    FTM_Config_t baseConfig;
    uint32_t prescalerDiv;
    uint32_t ftmClockHz;
    uint32_t modulo;

    if (config == (const FTM_PwmConfig_t *)0)
    {
        return FTM_STATUS_INVALID_CONFIG;
    }

    if ((config->srcClockHz == 0UL) || (config->pwmFreqHz == 0UL))
    {
        return FTM_STATUS_INVALID_CONFIG;
    }

    prescalerDiv = FTM_GetPrescalerDivisor(config->prescaler);
    if (prescalerDiv == 0UL)
    {
        return FTM_STATUS_INVALID_CONFIG;
    }

    ftmClockHz = config->srcClockHz / prescalerDiv;
    if (ftmClockHz == 0UL)
    {
        return FTM_STATUS_INVALID_CONFIG;
    }

    modulo = ftmClockHz / config->pwmFreqHz;
    if ((modulo == 0UL) || (modulo > 65536UL))
    {
        return FTM_STATUS_INVALID_CONFIG;
    }

    modulo -= 1UL;

    baseConfig.srcClockHz = config->srcClockHz;
    baseConfig.clockSource = config->clockSource;
    baseConfig.prescaler = config->prescaler;
    baseConfig.modulo = (uint16_t)modulo;

    return FTM_Init(instance, &baseConfig);
}

FTM_Status_t FTM_SetChannelModePwm(FTM_Instance_t instance,
                                   FTM_Channel_t channel,
                                   FTM_PwmMode_t pwmMode)
{
    FTM_Type *base;
    FTM_State_t *state;
    uint32_t cnscValue;

    base = FTM_GetBase(instance);
    if (base == (FTM_Type *)0)
    {
        return FTM_STATUS_INVALID_INSTANCE;
    }

    if ((uint8_t)channel > 7U)
    {
        return FTM_STATUS_INVALID_CHANNEL;
    }

    state = &g_ftmState[(uint8_t)instance];
    if (state->initialized == 0U)
    {
        return FTM_STATUS_NOT_INIT;
    }

    switch (pwmMode)
    {
    case FTM_PWM_EDGE_ALIGNED_HIGH_TRUE:
        /* MSB:MSA = 10, ELSB:ELSA = 10 */
        cnscValue = 0x28U;
        break;

    case FTM_PWM_EDGE_ALIGNED_LOW_TRUE:
        /* MSB:MSA = 10, ELSB:ELSA = 01 */
        cnscValue = 0x24U;
        break;

    default:
        return FTM_STATUS_INVALID_CONFIG;
    }

    base->CONTROLS[(uint8_t)channel].CnSC = cnscValue;
    base->SC |= (1UL << (16U + (uint32_t)channel));

    return FTM_STATUS_OK;
}

FTM_Status_t FTM_SetPwmDuty(FTM_Instance_t instance,
                            FTM_Channel_t channel,
                            uint16_t dutyCounts)
{
    FTM_Type *base;
    FTM_State_t *state;

    base = FTM_GetBase(instance);
    if (base == (FTM_Type *)0)
    {
        return FTM_STATUS_INVALID_INSTANCE;
    }

    if ((uint8_t)channel > 7U)
    {
        return FTM_STATUS_INVALID_CHANNEL;
    }

    state = &g_ftmState[(uint8_t)instance];
    if (state->initialized == 0U)
    {
        return FTM_STATUS_NOT_INIT;
    }

    /* For EPWM with CNTIN = 0:
     * 0%  -> CnV = 0
     * 100% -> CnV > MOD
     */
    if (dutyCounts > (uint16_t)(state->modulo + 1U))
    {
        dutyCounts = (uint16_t)(state->modulo + 1U);
    }

    base->CONTROLS[(uint8_t)channel].CnV = FTM_CnV_VAL(dutyCounts);

    return FTM_STATUS_OK;
}

FTM_Status_t FTM_SetPwmDutyPercent(FTM_Instance_t instance,
                                   FTM_Channel_t channel,
                                   uint8_t dutyPercent)
{
    FTM_State_t *state;
    uint32_t dutyCounts;

    if (FTM_GetBase(instance) == (FTM_Type *)0)
    {
        return FTM_STATUS_INVALID_INSTANCE;
    }

    if ((uint8_t)channel > 7U)
    {
        return FTM_STATUS_INVALID_CHANNEL;
    }

    if (dutyPercent > 100U)
    {
        return FTM_STATUS_INVALID_CONFIG;
    }

    state = &g_ftmState[(uint8_t)instance];
    if (state->initialized == 0U)
    {
        return FTM_STATUS_NOT_INIT;
    }

    if (dutyPercent == 100U)
    {
        if (state->modulo < 0xFFFFU)
        {
            dutyCounts = (uint32_t)state->modulo + 1UL;
        }
        else
        {
            dutyCounts = (uint32_t)state->modulo;
        }
    }
    else
    {
        dutyCounts = (((uint32_t)state->modulo + 1UL) * (uint32_t)dutyPercent) / 100UL;
    }

    return FTM_SetPwmDuty(instance, channel, (uint16_t)dutyCounts);
}

FTM_Status_t FTM_SetPwmFrequency(FTM_Instance_t instance, uint32_t pwmFreqHz)
{
    FTM_Type *base;
    FTM_State_t *state;
    uint32_t prescalerDiv;
    uint32_t ftmClockHz;
    uint32_t modulo;
    uint32_t savedClks;

    base = FTM_GetBase(instance);
    if (base == (FTM_Type *)0)
    {
        return FTM_STATUS_INVALID_INSTANCE;
    }

    if (pwmFreqHz == 0UL)
    {
        return FTM_STATUS_INVALID_CONFIG;
    }

    state = &g_ftmState[(uint8_t)instance];
    if (state->initialized == 0U)
    {
        return FTM_STATUS_NOT_INIT;
    }

    prescalerDiv = FTM_GetPrescalerDivisor(state->prescaler);
    if (prescalerDiv == 0UL)
    {
        return FTM_STATUS_INVALID_CONFIG;
    }

    ftmClockHz = state->srcClockHz / prescalerDiv;
    if (ftmClockHz == 0UL)
    {
        return FTM_STATUS_INVALID_CONFIG;
    }

    modulo = ftmClockHz / pwmFreqHz;
    if ((modulo == 0UL) || (modulo > 65536UL))
    {
        return FTM_STATUS_INVALID_CONFIG;
    }

    modulo -= 1UL;

    /* Stop temporarily, update MOD, then restore CLKS */
    savedClks = base->SC & FTM_SC_CLKS_MASK;
    base->SC &= ~FTM_SC_CLKS_MASK;

    base->MOD = FTM_MOD_MOD((uint16_t)modulo);
    state->modulo = (uint16_t)modulo;

    base->SC |= savedClks;

    return FTM_STATUS_OK;
}
