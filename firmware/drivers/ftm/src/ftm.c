/**
 * @file ftm.c
 * @brief FlexTimer Module driver implementation.
 *
 * @details
 * This file implements the register-level FTM driver used by
 * EduFramework.
 *
 * The current implementation focuses on simple educational PWM use:
 * - FTM0, FTM1, and FTM2 instance support.
 * - Eight channel identifiers per instance.
 * - Edge-aligned PWM mode.
 * - Runtime duty-cycle update.
 * - Runtime frequency update by changing MOD.
 *
 * The driver keeps FTM register access inside the low-level driver layer.
 * Arduino-style APIs such as analogWrite() should use this driver instead
 * of accessing FTM registers directly.
 */

#include "ftm.h"

/* ============================================================
 * Local constants
 * ============================================================ */

/**
 * @brief Number of FTM instances handled by this driver.
 */
#define FTM_DRIVER_INSTANCE_COUNT (3U)

/**
 * @brief Number of FTM channels handled by this driver.
 */
#define FTM_DRIVER_CHANNEL_COUNT (8U)

/**
 * @brief Invalid PCC index marker.
 */
#define FTM_INVALID_PCC_INDEX (0xFFFFFFFFUL)

/**
 * @brief PCC PCS value used for the current FTM clock path.
 *
 * @details
 * This value is kept unchanged from the previous implementation.
 * The analog/PWM stack currently uses the same SOSCDIV1-style clock
 * scheme as before.
 */
#define FTM_PCC_CLOCK_SOURCE_SOSCDIV1 (1U)

/**
 * @brief SC[CLKS] value used to keep the FTM counter stopped.
 */
#define FTM_COUNTER_CLOCK_STOPPED (0U)

/**
 * @brief SC[CPWMS] value for edge-aligned PWM.
 */
#define FTM_COUNTER_EDGE_ALIGNED (0U)

/**
 * @brief Maximum number of counts represented by a 16-bit FTM period.
 *
 * @details
 * MOD stores period minus one, therefore 65536 counts is the largest
 * representable PWM period when MOD is 0xFFFF.
 */
#define FTM_MAX_PERIOD_COUNTS (65536UL)

/**
 * @brief Maximum 16-bit modulo value.
 */
#define FTM_MAX_16BIT_VALUE (0xFFFFU)

/**
 * @brief Maximum duty cycle percentage accepted by the driver.
 */
#define FTM_DUTY_PERCENT_MAX (100U)

/**
 * @brief First PWMEN bit position inside FTM SC register.
 *
 * @details
 * The previous implementation enabled channel PWM output by setting
 * bit position 16 + channel in SC. This constant preserves that behavior
 * while avoiding raw magic numbers in code.
 */
#define FTM_PWM_ENABLE_BIT_BASE_SHIFT (16U)

/**
 * @brief CnSC value for edge-aligned high-true PWM.
 *
 * @details
 * This value preserves the previous implementation:
 * MSB:MSA = 10, ELSB:ELSA = 10.
 */
#define FTM_CNSC_EDGE_PWM_HIGH_TRUE_VALUE (0x28U)

/**
 * @brief CnSC value for edge-aligned low-true PWM.
 *
 * @details
 * This value preserves the previous implementation:
 * MSB:MSA = 10, ELSB:ELSA = 01.
 */
#define FTM_CNSC_EDGE_PWM_LOW_TRUE_VALUE (0x24U)

/**
 * @brief Null pointer value for FTM register block pointers.
 */
#define FTM_NULL_BASE ((FTM_Type *)0)

/**
 * @brief Null pointer value for FTM base configuration pointers.
 */
#define FTM_NULL_CONFIG ((const FTM_Config_t *)0)

/**
 * @brief Null pointer value for FTM PWM configuration pointers.
 */
#define FTM_NULL_PWM_CONFIG ((const FTM_PwmConfig_t *)0)

/**
 * @brief Null pointer value for modulo output pointers.
 */
#define FTM_NULL_MODULO_PTR ((uint16_t *)0)

/* ============================================================
 * Internal types
 * ============================================================ */

/**
 * @brief Runtime state of one FTM instance.
 *
 * @details
 * The driver stores the selected source clock, counter clock source,
 * prescaler, and modulo value so later APIs can start/stop the counter
 * and update PWM duty/frequency without requiring the application to
 * resend the whole configuration.
 */
typedef struct
{
    uint8_t initialized;
    uint32_t srcClockHz;
    FTM_ClockSource_t clockSource;
    FTM_Prescaler_t prescaler;
    uint16_t modulo;
} FTM_State_t;

/* ============================================================
 * Internal state
 * ============================================================ */

static FTM_State_t s_ftmState[FTM_DRIVER_INSTANCE_COUNT] = {{0U}};

/* ============================================================
 * Internal helpers
 * ============================================================ */

/**
 * @brief Get FTM peripheral base address from instance.
 *
 * @param[in] instance
 * FTM instance identifier.
 *
 * @return FTM_Type*
 * Pointer to FTM register block, or null pointer if invalid.
 */
static FTM_Type *FTM_GetBase(FTM_Instance_t instance)
{
    FTM_Type *pBase = FTM_NULL_BASE;

    switch (instance)
    {
    case IP_FTM_0:
        pBase = IP_FTM0;
        break;

    case IP_FTM_1:
        pBase = IP_FTM1;
        break;

    case IP_FTM_2:
        pBase = IP_FTM2;
        break;

    default:
        pBase = FTM_NULL_BASE;
        break;
    }

    return pBase;
}

/**
 * @brief Get PCC index from FTM instance.
 *
 * @param[in] instance
 * FTM instance identifier.
 *
 * @return uint32_t
 * PCC index, or FTM_INVALID_PCC_INDEX if invalid.
 */
static uint32_t FTM_GetPccIndex(FTM_Instance_t instance)
{
    uint32_t u32PccIndex = FTM_INVALID_PCC_INDEX;

    switch (instance)
    {
    case IP_FTM_0:
        u32PccIndex = PCC_FTM0_INDEX;
        break;

    case IP_FTM_1:
        u32PccIndex = PCC_FTM1_INDEX;
        break;

    case IP_FTM_2:
        u32PccIndex = PCC_FTM2_INDEX;
        break;

    default:
        u32PccIndex = FTM_INVALID_PCC_INDEX;
        break;
    }

    return u32PccIndex;
}

/**
 * @brief Check whether an FTM instance is supported.
 *
 * @param[in] instance
 * FTM instance identifier.
 *
 * @return uint8_t
 *
 * @retval 1U Instance is valid.
 * @retval 0U Instance is invalid.
 */
static uint8_t FTM_IsValidInstance(FTM_Instance_t instance)
{
    uint8_t u8IsValid = 0U;

    if ((IP_FTM_0 == instance) ||
        (IP_FTM_1 == instance) ||
        (IP_FTM_2 == instance))
    {
        u8IsValid = 1U;
    }

    return u8IsValid;
}

/**
 * @brief Check whether an FTM channel is supported.
 *
 * @param[in] channel
 * FTM channel identifier.
 *
 * @return uint8_t
 *
 * @retval 1U Channel is valid.
 * @retval 0U Channel is invalid.
 */
static uint8_t FTM_IsValidChannel(FTM_Channel_t channel)
{
    uint8_t u8IsValid = 0U;

    if ((uint8_t)FTM_DRIVER_CHANNEL_COUNT > (uint8_t)channel)
    {
        u8IsValid = 1U;
    }

    return u8IsValid;
}

/**
 * @brief Check whether an FTM counter clock source value is valid.
 *
 * @param[in] clockSource
 * FTM counter clock source.
 *
 * @return uint8_t
 *
 * @retval 1U Clock source is valid.
 * @retval 0U Clock source is invalid.
 */
static uint8_t FTM_IsValidClockSource(FTM_ClockSource_t clockSource)
{
    uint8_t u8IsValid = 0U;

    if ((FTM_CLOCK_SOURCE_NONE == clockSource) ||
        (FTM_CLOCK_SOURCE_SYSTEM == clockSource) ||
        (FTM_CLOCK_SOURCE_FIXED == clockSource) ||
        (FTM_CLOCK_SOURCE_EXTERNAL == clockSource))
    {
        u8IsValid = 1U;
    }

    return u8IsValid;
}

/**
 * @brief Convert FTM prescaler field value to divisor.
 *
 * @param[in] prescaler
 * FTM prescaler configuration.
 *
 * @return uint32_t
 * Prescaler divisor. Returns 0U if prescaler is invalid.
 */
static uint32_t FTM_GetPrescalerDivisor(FTM_Prescaler_t prescaler)
{
    uint32_t u32Divisor = 0UL;

    switch (prescaler)
    {
    case FTM_PRESCALER_DIV_1:
        u32Divisor = 1UL;
        break;

    case FTM_PRESCALER_DIV_2:
        u32Divisor = 2UL;
        break;

    case FTM_PRESCALER_DIV_4:
        u32Divisor = 4UL;
        break;

    case FTM_PRESCALER_DIV_8:
        u32Divisor = 8UL;
        break;

    case FTM_PRESCALER_DIV_16:
        u32Divisor = 16UL;
        break;

    case FTM_PRESCALER_DIV_32:
        u32Divisor = 32UL;
        break;

    case FTM_PRESCALER_DIV_64:
        u32Divisor = 64UL;
        break;

    case FTM_PRESCALER_DIV_128:
        u32Divisor = 128UL;
        break;

    default:
        u32Divisor = 0UL;
        break;
    }

    return u32Divisor;
}

/**
 * @brief Validate base FTM configuration.
 *
 * @param[in] pConfig
 * Pointer to FTM base configuration.
 *
 * @return FTM_Status_t
 *
 * @retval FTM_STATUS_OK Configuration is valid.
 * @retval FTM_STATUS_INVALID_CONFIG Configuration is invalid.
 */
static FTM_Status_t FTM_ValidateBaseConfig(const FTM_Config_t *pConfig)
{
    FTM_Status_t RetVal = FTM_STATUS_OK;

    if (FTM_NULL_CONFIG == pConfig)
    {
        RetVal = FTM_STATUS_INVALID_CONFIG;
    }
    else if (0UL == pConfig->srcClockHz)
    {
        RetVal = FTM_STATUS_INVALID_CONFIG;
    }
    else if (0U == FTM_IsValidClockSource(pConfig->clockSource))
    {
        RetVal = FTM_STATUS_INVALID_CONFIG;
    }
    else if (0UL == FTM_GetPrescalerDivisor(pConfig->prescaler))
    {
        RetVal = FTM_STATUS_INVALID_CONFIG;
    }
    else
    {
        RetVal = FTM_STATUS_OK;
    }

    return RetVal;
}

/**
 * @brief Calculate MOD value from source clock, prescaler, and PWM frequency.
 *
 * @details
 * FTM PWM period count is calculated as:
 *
 * periodCounts = FTM clock / PWM frequency
 *
 * MOD stores periodCounts - 1.
 *
 * @param[in] u32SrcClockHz
 * FTM source clock frequency in Hz.
 *
 * @param[in] prescaler
 * FTM prescaler configuration.
 *
 * @param[in] u32PwmFreqHz
 * Target PWM frequency in Hz.
 *
 * @param[out] pModulo
 * Pointer used to receive calculated MOD value.
 *
 * @return FTM_Status_t
 *
 * @retval FTM_STATUS_OK MOD calculated successfully.
 * @retval FTM_STATUS_INVALID_CONFIG Invalid input or unsupported MOD value.
 */
static FTM_Status_t FTM_CalculateModulo(uint32_t u32SrcClockHz,
                                        FTM_Prescaler_t prescaler,
                                        uint32_t u32PwmFreqHz,
                                        uint16_t *pModulo)
{
    FTM_Status_t RetVal = FTM_STATUS_OK;
    uint32_t u32PrescalerDiv = 0UL;
    uint32_t u32FtmClockHz = 0UL;
    uint32_t u32PeriodCounts = 0UL;

    if ((0UL == u32SrcClockHz) ||
        (0UL == u32PwmFreqHz) ||
        (FTM_NULL_MODULO_PTR == pModulo))
    {
        RetVal = FTM_STATUS_INVALID_CONFIG;
    }
    else
    {
        u32PrescalerDiv = FTM_GetPrescalerDivisor(prescaler);

        if (0UL == u32PrescalerDiv)
        {
            RetVal = FTM_STATUS_INVALID_CONFIG;
        }
        else
        {
            u32FtmClockHz = u32SrcClockHz / u32PrescalerDiv;

            if (0UL == u32FtmClockHz)
            {
                RetVal = FTM_STATUS_INVALID_CONFIG;
            }
            else
            {
                u32PeriodCounts = u32FtmClockHz / u32PwmFreqHz;

                if ((0UL == u32PeriodCounts) ||
                    (FTM_MAX_PERIOD_COUNTS < u32PeriodCounts))
                {
                    RetVal = FTM_STATUS_INVALID_CONFIG;
                }
                else
                {
                    *pModulo = (uint16_t)(u32PeriodCounts - 1UL);
                    RetVal = FTM_STATUS_OK;
                }
            }
        }
    }

    return RetVal;
}

/**
 * @brief Enable peripheral clock for selected FTM instance.
 *
 * @details
 * PCC clock is disabled before changing PCS, then re-enabled after the
 * clock source field has been configured.
 *
 * @param[in] instance
 * FTM instance identifier.
 *
 * @return FTM_Status_t
 *
 * @retval FTM_STATUS_OK Peripheral clock configured successfully.
 * @retval FTM_STATUS_INVALID_INSTANCE Invalid FTM instance.
 */
static FTM_Status_t FTM_EnablePeripheralClock(FTM_Instance_t instance)
{
    FTM_Status_t RetVal = FTM_STATUS_OK;
    uint32_t u32PccIndex = FTM_GetPccIndex(instance);

    if (FTM_INVALID_PCC_INDEX == u32PccIndex)
    {
        RetVal = FTM_STATUS_INVALID_INSTANCE;
    }
    else
    {
        IP_PCC->PCCn[u32PccIndex] &= ~PCC_PCCn_CGC_MASK;
        IP_PCC->PCCn[u32PccIndex] = PCC_PCCn_PCS(FTM_PCC_CLOCK_SOURCE_SOSCDIV1);
        IP_PCC->PCCn[u32PccIndex] |= PCC_PCCn_CGC_MASK;

        RetVal = FTM_STATUS_OK;
    }

    return RetVal;
}

/**
 * @brief Get channel CnSC value for requested PWM mode.
 *
 * @param[in] pwmMode
 * PWM output polarity mode.
 *
 * @param[out] pCnscValue
 * Pointer used to receive CnSC register value.
 *
 * @return FTM_Status_t
 *
 * @retval FTM_STATUS_OK PWM mode decoded successfully.
 * @retval FTM_STATUS_INVALID_CONFIG Unsupported PWM mode.
 */
static FTM_Status_t FTM_GetPwmCnscValue(FTM_PwmMode_t pwmMode, uint32_t *pCnscValue)
{
    FTM_Status_t RetVal = FTM_STATUS_OK;

    if ((uint32_t *)0 == pCnscValue)
    {
        RetVal = FTM_STATUS_INVALID_CONFIG;
    }
    else
    {
        switch (pwmMode)
        {
        case FTM_PWM_EDGE_ALIGNED_HIGH_TRUE:
            *pCnscValue = FTM_CNSC_EDGE_PWM_HIGH_TRUE_VALUE;
            RetVal = FTM_STATUS_OK;
            break;

        case FTM_PWM_EDGE_ALIGNED_LOW_TRUE:
            *pCnscValue = FTM_CNSC_EDGE_PWM_LOW_TRUE_VALUE;
            RetVal = FTM_STATUS_OK;
            break;

        default:
            RetVal = FTM_STATUS_INVALID_CONFIG;
            break;
        }
    }

    return RetVal;
}

/* ============================================================
 * API implementation
 * ============================================================ */

/**
 * @copydoc FTM_Init
 */
FTM_Status_t FTM_Init(FTM_Instance_t instance, const FTM_Config_t *pConfig)
{
    FTM_Status_t RetVal = FTM_STATUS_OK;
    FTM_Type *pBase = FTM_NULL_BASE;
    FTM_State_t *pState = (FTM_State_t *)0;

    if (0U == FTM_IsValidInstance(instance))
    {
        RetVal = FTM_STATUS_INVALID_INSTANCE;
    }
    else
    {
        RetVal = FTM_ValidateBaseConfig(pConfig);
    }

    if (FTM_STATUS_OK == RetVal)
    {
        RetVal = FTM_EnablePeripheralClock(instance);
    }

    if (FTM_STATUS_OK == RetVal)
    {
        pBase = FTM_GetBase(instance);
        pState = &s_ftmState[(uint8_t)instance];

        /*
         * Stop the counter before changing mode, modulo, and channel
         * behavior. This avoids updating active timer configuration while
         * the counter is running.
         */
        pBase->SC = 0U;

        /*
         * Disable write protection so MOD, channel control, and mode
         * registers can be configured by the driver.
         */
        pBase->MODE = FTM_MODE_WPDIS_MASK;

        /*
         * Keep the first PWM implementation simple:
         * - Independent channels.
         * - Default polarity.
         * - No output mask.
         * - No enhanced synchronization flow.
         */
        pBase->COMBINE = 0U;
        pBase->POL = 0U;
        pBase->OUTMASK = 0U;
        pBase->OUTINIT = 0U;
        pBase->CONF = 0U;

        /*
         * Counter starts from CNTIN. Writing CNT forces the counter to
         * load CNTIN on S32K FTM hardware.
         */
        pBase->CNTIN = 0U;
        pBase->CNT = 0U;
        pBase->MOD = FTM_MOD_MOD(pConfig->modulo);

        /*
         * Configure prescaler and edge-aligned mode, but keep CLKS at
         * zero so the counter remains stopped after initialization.
         */
        pBase->SC = FTM_SC_PS((uint32_t)pConfig->prescaler) |
                    FTM_SC_CLKS(FTM_COUNTER_CLOCK_STOPPED) |
                    FTM_SC_CPWMS(FTM_COUNTER_EDGE_ALIGNED);

        pState->initialized = 1U;
        pState->srcClockHz = pConfig->srcClockHz;
        pState->clockSource = pConfig->clockSource;
        pState->prescaler = pConfig->prescaler;
        pState->modulo = pConfig->modulo;
    }

    return RetVal;
}

/**
 * @copydoc FTM_StartCounter
 */
FTM_Status_t FTM_StartCounter(FTM_Instance_t instance)
{
    FTM_Status_t RetVal = FTM_STATUS_OK;
    FTM_Type *pBase = FTM_NULL_BASE;
    FTM_State_t *pState = (FTM_State_t *)0;

    if (0U == FTM_IsValidInstance(instance))
    {
        RetVal = FTM_STATUS_INVALID_INSTANCE;
    }
    else
    {
        pState = &s_ftmState[(uint8_t)instance];

        if (0U == pState->initialized)
        {
            RetVal = FTM_STATUS_NOT_INIT;
        }
        else
        {
            pBase = FTM_GetBase(instance);

            pBase->SC &= ~FTM_SC_CLKS_MASK;
            pBase->SC |= FTM_SC_CLKS((uint32_t)pState->clockSource);

            RetVal = FTM_STATUS_OK;
        }
    }

    return RetVal;
}

/**
 * @copydoc FTM_StopCounter
 */
FTM_Status_t FTM_StopCounter(FTM_Instance_t instance)
{
    FTM_Status_t RetVal = FTM_STATUS_OK;
    FTM_Type *pBase = FTM_NULL_BASE;
    FTM_State_t *pState = (FTM_State_t *)0;

    if (0U == FTM_IsValidInstance(instance))
    {
        RetVal = FTM_STATUS_INVALID_INSTANCE;
    }
    else
    {
        pState = &s_ftmState[(uint8_t)instance];

        if (0U == pState->initialized)
        {
            RetVal = FTM_STATUS_NOT_INIT;
        }
        else
        {
            pBase = FTM_GetBase(instance);
            pBase->SC &= ~FTM_SC_CLKS_MASK;

            RetVal = FTM_STATUS_OK;
        }
    }

    return RetVal;
}

/**
 * @copydoc FTM_InitPwm
 */
FTM_Status_t FTM_InitPwm(FTM_Instance_t instance, const FTM_PwmConfig_t *pConfig)
{
    FTM_Status_t RetVal = FTM_STATUS_OK;
    FTM_Config_t BaseConfig = {0U};
    uint16_t u16Modulo = 0U;

    if (FTM_NULL_PWM_CONFIG == pConfig)
    {
        RetVal = FTM_STATUS_INVALID_CONFIG;
    }
    else if (0U == FTM_IsValidClockSource(pConfig->clockSource))
    {
        RetVal = FTM_STATUS_INVALID_CONFIG;
    }
    else
    {
        RetVal = FTM_CalculateModulo(pConfig->srcClockHz,
                                     pConfig->prescaler,
                                     pConfig->pwmFreqHz,
                                     &u16Modulo);
    }

    if (FTM_STATUS_OK == RetVal)
    {
        BaseConfig.srcClockHz = pConfig->srcClockHz;
        BaseConfig.clockSource = pConfig->clockSource;
        BaseConfig.prescaler = pConfig->prescaler;
        BaseConfig.modulo = u16Modulo;

        RetVal = FTM_Init(instance, &BaseConfig);
    }

    return RetVal;
}

/**
 * @copydoc FTM_SetChannelModePwm
 */
FTM_Status_t FTM_SetChannelModePwm(FTM_Instance_t instance,
                                   FTM_Channel_t channel,
                                   FTM_PwmMode_t pwmMode)
{
    FTM_Status_t RetVal = FTM_STATUS_OK;
    FTM_Type *pBase = FTM_NULL_BASE;
    FTM_State_t *pState = (FTM_State_t *)0;
    uint32_t u32CnscValue = 0U;
    uint32_t u32PwmEnableMask = 0U;

    if (0U == FTM_IsValidInstance(instance))
    {
        RetVal = FTM_STATUS_INVALID_INSTANCE;
    }
    else if (0U == FTM_IsValidChannel(channel))
    {
        RetVal = FTM_STATUS_INVALID_CHANNEL;
    }
    else
    {
        pState = &s_ftmState[(uint8_t)instance];

        if (0U == pState->initialized)
        {
            RetVal = FTM_STATUS_NOT_INIT;
        }
        else
        {
            RetVal = FTM_GetPwmCnscValue(pwmMode, &u32CnscValue);
        }
    }

    if (FTM_STATUS_OK == RetVal)
    {
        pBase = FTM_GetBase(instance);

        pBase->CONTROLS[(uint8_t)channel].CnSC = u32CnscValue;

        /*
         * Enable PWM output control for the selected channel.
         * This preserves the previous implementation behavior that used
         * SC bit position 16 + channel.
         */
        u32PwmEnableMask = (uint32_t)(1UL << (FTM_PWM_ENABLE_BIT_BASE_SHIFT +
                                              (uint32_t)channel));
        pBase->SC |= u32PwmEnableMask;
    }

    return RetVal;
}

/**
 * @copydoc FTM_SetPwmDuty
 */
FTM_Status_t FTM_SetPwmDuty(FTM_Instance_t instance,
                            FTM_Channel_t channel,
                            uint16_t dutyCounts)
{
    FTM_Status_t RetVal = FTM_STATUS_OK;
    FTM_Type *pBase = FTM_NULL_BASE;
    FTM_State_t *pState = (FTM_State_t *)0;
    uint32_t u32MaxDutyCounts = 0UL;
    uint16_t u16DutyCounts = dutyCounts;

    if (0U == FTM_IsValidInstance(instance))
    {
        RetVal = FTM_STATUS_INVALID_INSTANCE;
    }
    else if (0U == FTM_IsValidChannel(channel))
    {
        RetVal = FTM_STATUS_INVALID_CHANNEL;
    }
    else
    {
        pState = &s_ftmState[(uint8_t)instance];

        if (0U == pState->initialized)
        {
            RetVal = FTM_STATUS_NOT_INIT;
        }
        else
        {
            /*
             * For edge-aligned PWM with CNTIN = 0, the effective period
             * count is MOD + 1. Duty input is clamped instead of rejected
             * to preserve the previous driver behavior.
             */
            u32MaxDutyCounts = (uint32_t)pState->modulo + 1UL;

            if ((uint32_t)u16DutyCounts > u32MaxDutyCounts)
            {
                u16DutyCounts = (uint16_t)u32MaxDutyCounts;
            }

            pBase = FTM_GetBase(instance);
            pBase->CONTROLS[(uint8_t)channel].CnV = FTM_CnV_VAL(u16DutyCounts);

            RetVal = FTM_STATUS_OK;
        }
    }

    return RetVal;
}

/**
 * @copydoc FTM_SetPwmDutyPercent
 */
FTM_Status_t FTM_SetPwmDutyPercent(FTM_Instance_t instance,
                                   FTM_Channel_t channel,
                                   uint8_t dutyPercent)
{
    FTM_Status_t RetVal = FTM_STATUS_OK;
    FTM_State_t *pState = (FTM_State_t *)0;
    uint32_t u32DutyCounts = 0UL;

    if (0U == FTM_IsValidInstance(instance))
    {
        RetVal = FTM_STATUS_INVALID_INSTANCE;
    }
    else if (0U == FTM_IsValidChannel(channel))
    {
        RetVal = FTM_STATUS_INVALID_CHANNEL;
    }
    else if (FTM_DUTY_PERCENT_MAX < dutyPercent)
    {
        RetVal = FTM_STATUS_INVALID_CONFIG;
    }
    else
    {
        pState = &s_ftmState[(uint8_t)instance];

        if (0U == pState->initialized)
        {
            RetVal = FTM_STATUS_NOT_INIT;
        }
        else
        {
            /*
             * 100% duty needs special handling. If MOD is not the maximum
             * 16-bit value, MOD + 1 produces a fully active output. If MOD
             * already equals 0xFFFF, adding one cannot be represented by
             * the 16-bit CnV register, so the previous behavior is kept by
             * using MOD.
             */
            if (FTM_DUTY_PERCENT_MAX == dutyPercent)
            {
                if (FTM_MAX_16BIT_VALUE > pState->modulo)
                {
                    u32DutyCounts = (uint32_t)pState->modulo + 1UL;
                }
                else
                {
                    u32DutyCounts = (uint32_t)pState->modulo;
                }
            }
            else
            {
                u32DutyCounts = (((uint32_t)pState->modulo + 1UL) *
                                 (uint32_t)dutyPercent) /
                                (uint32_t)FTM_DUTY_PERCENT_MAX;
            }

            RetVal = FTM_SetPwmDuty(instance, channel, (uint16_t)u32DutyCounts);
        }
    }

    return RetVal;
}

/**
 * @copydoc FTM_SetPwmFrequency
 */
FTM_Status_t FTM_SetPwmFrequency(FTM_Instance_t instance, uint32_t pwmFreqHz)
{
    FTM_Status_t RetVal = FTM_STATUS_OK;
    FTM_Type *pBase = FTM_NULL_BASE;
    FTM_State_t *pState = (FTM_State_t *)0;
    uint16_t u16Modulo = 0U;
    uint32_t u32SavedClks = 0U;

    if (0U == FTM_IsValidInstance(instance))
    {
        RetVal = FTM_STATUS_INVALID_INSTANCE;
    }
    else
    {
        pState = &s_ftmState[(uint8_t)instance];

        if (0U == pState->initialized)
        {
            RetVal = FTM_STATUS_NOT_INIT;
        }
        else
        {
            RetVal = FTM_CalculateModulo(pState->srcClockHz,
                                         pState->prescaler,
                                         pwmFreqHz,
                                         &u16Modulo);
        }
    }

    if (FTM_STATUS_OK == RetVal)
    {
        pBase = FTM_GetBase(instance);

        /*
         * Stop temporarily while MOD is updated, then restore the previous
         * clock selection. This avoids changing period while the counter is
         * actively running.
         */
        u32SavedClks = pBase->SC & FTM_SC_CLKS_MASK;
        pBase->SC &= ~FTM_SC_CLKS_MASK;

        pBase->MOD = FTM_MOD_MOD(u16Modulo);
        pState->modulo = u16Modulo;

        pBase->SC |= u32SavedClks;
    }

    return RetVal;
}