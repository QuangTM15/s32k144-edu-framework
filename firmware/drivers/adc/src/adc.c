/**
 * @file adc.c
 * @brief ADC driver implementation.
 *
 * @details
 * This file implements the register-level ADC driver for EduFramework.
 *
 * The driver currently supports:
 * - ADC0 and ADC1 instance selection.
 * - Software-triggered single conversion mode.
 * - Interrupt-based conversion completion.
 * - Raw ADC result storage inside the driver state.
 * - ADC calibration before normal conversion.
 *
 * Current board-level analog usage is intentionally limited to ADC0_SE12
 * and ADC0_SE13 because these are the analog inputs exposed for the
 * MaaZEDU educational use case.
 */

#include "adc.h"

/* ============================================================
 * Local constants
 * ============================================================ */

/**
 * @brief Number of ADC instances handled by the driver.
 */
#define ADC_DRIVER_INSTANCE_COUNT (2U)

/**
 * @brief Invalid PCC index marker.
 */
#define ADC_INVALID_PCC_INDEX (0xFFFFFFFFUL)

/**
 * @brief Invalid decoded register field marker.
 */
#define ADC_INVALID_FIELD_VALUE (0xFFFFFFFFUL)

/**
 * @brief SC1 ADCH value used to disable ADC conversions.
 */
#define ADC_CONVERSION_DISABLE_CHANNEL (31U)

/**
 * @brief PCC PCS value used to select SOSCDIV2 as ADC functional clock.
 *
 * @details
 * This value is kept from the previous implementation and matches the
 * framework clock setup used by the current analog stack.
 */
#define ADC_PCC_CLOCK_SOURCE_SOSCDIV2 (1U)

/**
 * @brief ADC CFG1 ADICLK field value used by this driver.
 */
#define ADC_CFG1_ADICLK_ALTCLK1 (0U)

/**
 * @brief ADC CFG1 ADIV field value for divide-by-1.
 */
#define ADC_CFG1_ADIV_DIV_1 (0U)

/**
 * @brief ADC SC2 REFSEL field value for default reference pins.
 */
#define ADC_SC2_REFSEL_DEFAULT (0U)

/**
 * @brief ADC SC2 REFSEL field value for alternative reference.
 */
#define ADC_SC2_REFSEL_ALT (1U)

/**
 * @brief CFG1 MODE field value for 8-bit conversion.
 */
#define ADC_MODE_FIELD_8BIT (0U)

/**
 * @brief CFG1 MODE field value for 10-bit conversion.
 */
#define ADC_MODE_FIELD_10BIT (2U)

/**
 * @brief CFG1 MODE field value for 12-bit conversion.
 */
#define ADC_MODE_FIELD_12BIT (1U)

/**
 * @brief SC3 AVGS field value for 4-sample hardware average.
 */
#define ADC_AVGS_FIELD_4_SAMPLES (0U)

/**
 * @brief SC3 AVGS field value for 8-sample hardware average.
 */
#define ADC_AVGS_FIELD_8_SAMPLES (1U)

/**
 * @brief SC3 AVGS field value for 16-sample hardware average.
 */
#define ADC_AVGS_FIELD_16_SAMPLES (2U)

/**
 * @brief SC3 AVGS field value for 32-sample hardware average.
 */
#define ADC_AVGS_FIELD_32_SAMPLES (3U)

/**
 * @brief ADC SC3 CALF bit shift used by the existing implementation.
 */
#define ADC_SC3_CALF_BIT_SHIFT (6U)

/**
 * @brief One-bit field mask.
 */
#define ADC_ONE_BIT_MASK (1U)

/**
 * @brief Mask used to keep the raw ADC result within 16 bits.
 */
#define ADC_RESULT_16BIT_MASK (0xFFFFU)

/* ============================================================
 * Internal types
 * ============================================================ */

/**
 * @brief Runtime state of one ADC instance.
 *
 * @details
 * The ADC driver stores conversion state internally because conversion
 * completion is interrupt-driven. The ISR captures the hardware result
 * and marks the result as available for ADC_GetResult().
 */
typedef struct
{
    uint8_t initialized;
    uint8_t busy;
    uint8_t done;
    uint16_t lastResult;
    ADC_Channel_t currentChannel;
    ADC_Status_t lastStatus;
    ADC_Config_t config;
} ADC_State_t;

/* ============================================================
 * Internal state
 * ============================================================ */

static ADC_State_t g_adcState[ADC_DRIVER_INSTANCE_COUNT] = {{0U}};

/* ============================================================
 * Internal helpers
 * ============================================================ */

/**
 * @brief Get ADC peripheral base address from ADC instance.
 *
 * @param[in] instance
 * ADC instance identifier.
 *
 * @return ADC_Type*
 * Pointer to ADC register block, or null pointer if the instance is invalid.
 */
static ADC_Type *ADC_GetBase(ADC_Instance_t instance)
{
    ADC_Type *pBase = (ADC_Type *)0;

    switch (instance)
    {
    case IP_ADC_0:
        pBase = IP_ADC0;
        break;

    case IP_ADC_1:
        pBase = IP_ADC1;
        break;

    default:
        pBase = (ADC_Type *)0;
        break;
    }

    return pBase;
}

/**
 * @brief Get PCC index from ADC instance.
 *
 * @param[in] instance
 * ADC instance identifier.
 *
 * @return uint32_t
 * PCC index, or ADC_INVALID_PCC_INDEX if the instance is invalid.
 */
static uint32_t ADC_GetPccIndex(ADC_Instance_t instance)
{
    uint32_t u32Index = ADC_INVALID_PCC_INDEX;

    switch (instance)
    {
    case IP_ADC_0:
        u32Index = PCC_ADC0_INDEX;
        break;

    case IP_ADC_1:
        u32Index = PCC_ADC1_INDEX;
        break;

    default:
        u32Index = ADC_INVALID_PCC_INDEX;
        break;
    }

    return u32Index;
}

/**
 * @brief Check whether an ADC instance identifier is supported.
 *
 * @param[in] instance
 * ADC instance identifier.
 *
 * @return uint8_t
 *
 * @retval 1U Instance is valid.
 * @retval 0U Instance is invalid.
 */
static uint8_t ADC_IsValidInstance(ADC_Instance_t instance)
{
    uint8_t u8IsValid = 0U;

    if ((IP_ADC_0 == instance) || (IP_ADC_1 == instance))
    {
        u8IsValid = 1U;
    }

    return u8IsValid;
}

/**
 * @brief Check whether an ADC channel is supported by this driver setup.
 *
 * @details
 * EduFramework currently exposes only ADC SE12 and SE13 for the
 * board-level analog API. The driver keeps this scope explicit to avoid
 * silently accepting channels that are not part of the current board use.
 *
 * @param[in] channel
 * ADC channel identifier.
 *
 * @return uint8_t
 *
 * @retval 1U Channel is supported.
 * @retval 0U Channel is unsupported.
 */
static uint8_t ADC_IsValidChannel(ADC_Channel_t channel)
{
    uint8_t u8IsValid = 0U;

    if ((ADC_CHANNEL_SE12 == channel) || (ADC_CHANNEL_SE13 == channel))
    {
        u8IsValid = 1U;
    }

    return u8IsValid;
}

/**
 * @brief Convert public resolution value to ADC CFG1 MODE field value.
 *
 * @param[in] resolution
 * Public ADC resolution configuration.
 *
 * @return uint32_t
 * MODE field value, or ADC_INVALID_FIELD_VALUE if unsupported.
 */
static uint32_t ADC_GetModeBits(ADC_Resolution_t resolution)
{
    uint32_t u32ModeBits = ADC_INVALID_FIELD_VALUE;

    switch (resolution)
    {
    case ADC_RESOLUTION_8BIT:
        u32ModeBits = ADC_MODE_FIELD_8BIT;
        break;

    case ADC_RESOLUTION_10BIT:
        u32ModeBits = ADC_MODE_FIELD_10BIT;
        break;

    case ADC_RESOLUTION_12BIT:
        u32ModeBits = ADC_MODE_FIELD_12BIT;
        break;

    default:
        u32ModeBits = ADC_INVALID_FIELD_VALUE;
        break;
    }

    return u32ModeBits;
}

/**
 * @brief Convert public average value to ADC SC3 AVGS field value.
 *
 * @param[in] average
 * Public ADC hardware averaging configuration.
 *
 * @return uint32_t
 * AVGS field value, or ADC_INVALID_FIELD_VALUE if unsupported.
 */
static uint32_t ADC_GetAverageBits(ADC_Average_t average)
{
    uint32_t u32AverageBits = ADC_INVALID_FIELD_VALUE;

    switch (average)
    {
    case ADC_AVERAGE_DISABLED:
        u32AverageBits = ADC_AVGS_FIELD_4_SAMPLES;
        break;

    case ADC_AVERAGE_4:
        u32AverageBits = ADC_AVGS_FIELD_4_SAMPLES;
        break;

    case ADC_AVERAGE_8:
        u32AverageBits = ADC_AVGS_FIELD_8_SAMPLES;
        break;

    case ADC_AVERAGE_16:
        u32AverageBits = ADC_AVGS_FIELD_16_SAMPLES;
        break;

    case ADC_AVERAGE_32:
        u32AverageBits = ADC_AVGS_FIELD_32_SAMPLES;
        break;

    default:
        u32AverageBits = ADC_INVALID_FIELD_VALUE;
        break;
    }

    return u32AverageBits;
}

/**
 * @brief Validate ADC initialization configuration.
 *
 * @param[in] pConfig
 * Pointer to ADC configuration.
 *
 * @return ADC_Status_t
 *
 * @retval ADC_STATUS_OK Configuration is valid.
 * @retval ADC_STATUS_INVALID_CONFIG Configuration is invalid.
 */
static ADC_Status_t ADC_ValidateConfig(const ADC_Config_t *pConfig)
{
    ADC_Status_t RetVal = ADC_STATUS_OK;

    if ((const ADC_Config_t *)0 == pConfig)
    {
        RetVal = ADC_STATUS_INVALID_CONFIG;
    }
    else if (0UL == pConfig->srcClockHz)
    {
        RetVal = ADC_STATUS_INVALID_CONFIG;
    }
    else if (ADC_INVALID_FIELD_VALUE == ADC_GetModeBits(pConfig->resolution))
    {
        RetVal = ADC_STATUS_INVALID_CONFIG;
    }
    else if (ADC_INVALID_FIELD_VALUE == ADC_GetAverageBits(pConfig->average))
    {
        RetVal = ADC_STATUS_INVALID_CONFIG;
    }
    else if ((ADC_REF_DEFAULT != pConfig->reference) &&
             (ADC_REF_ALT != pConfig->reference))
    {
        RetVal = ADC_STATUS_INVALID_CONFIG;
    }
    else if (0U == pConfig->enableInterrupt)
    {
        /*
         * The current analog stack uses interrupt-based completion.
         * Disabling ADC interrupt would break analogStart()/analogAvailable().
         */
        RetVal = ADC_STATUS_INVALID_CONFIG;
    }
    else
    {
        RetVal = ADC_STATUS_OK;
    }

    return RetVal;
}

/**
 * @brief Enable peripheral clock for the selected ADC instance.
 *
 * @details
 * The PCC clock gate is disabled before changing PCS because the clock
 * source field must be configured while the peripheral clock is gated.
 *
 * @param[in] instance
 * ADC instance identifier.
 *
 * @return None.
 */
static void ADC_EnablePeripheralClock(ADC_Instance_t instance)
{
    uint32_t u32PccIndex = ADC_GetPccIndex(instance);

    if (ADC_INVALID_PCC_INDEX != u32PccIndex)
    {
        IP_PCC->PCCn[u32PccIndex] &= ~PCC_PCCn_CGC_MASK;

        IP_PCC->PCCn[u32PccIndex] = PCC_PCCn_PCS(ADC_PCC_CLOCK_SOURCE_SOSCDIV2);

        IP_PCC->PCCn[u32PccIndex] |= PCC_PCCn_CGC_MASK;
    }
}

/**
 * @brief Disable ADC conversions through SC1 ADCH field.
 *
 * @details
 * Writing ADCH with the disable-channel value prevents unintended
 * conversions while the ADC is being reconfigured.
 *
 * @param[in] pBase
 * Pointer to ADC register block.
 *
 * @return None.
 */
static void ADC_DisableConversions(ADC_Type *pBase)
{
    if ((ADC_Type *)0 != pBase)
    {
        pBase->SC1[0] = ADC_SC1_ADCH(ADC_CONVERSION_DISABLE_CHANNEL);
    }
}

/* ============================================================
 * API implementation
 * ============================================================ */

/**
 * @copydoc ADC_Init
 */
ADC_Status_t ADC_Init(ADC_Instance_t instance, const ADC_Config_t *pConfig)
{
    ADC_Type *pBase = (ADC_Type *)0;
    ADC_State_t *pState = (ADC_State_t *)0;
    ADC_Status_t RetVal = ADC_STATUS_OK;
    uint32_t u32ModeBits = 0U;
    uint32_t u32Sc2Value = 0U;
    uint32_t u32Sc3Value = 0U;

    if (0U == ADC_IsValidInstance(instance))
    {
        RetVal = ADC_STATUS_INVALID_INSTANCE;
    }
    else
    {
        RetVal = ADC_ValidateConfig(pConfig);
    }

    if (ADC_STATUS_OK == RetVal)
    {
        pBase = ADC_GetBase(instance);
        pState = &g_adcState[(uint8_t)instance];
        u32ModeBits = ADC_GetModeBits(pConfig->resolution);

        ADC_EnablePeripheralClock(instance);

        ADC_DisableConversions(pBase);

        /*
         * CFG1:
         * - ADICLK selects the ADC input clock path.
         * - ADIV keeps the selected ADC clock undivided.
         * - MODE selects the conversion resolution.
         */
        pBase->CFG1 = ADC_CFG1_ADICLK(ADC_CFG1_ADICLK_ALTCLK1) |
                      ADC_CFG1_ADIV(ADC_CFG1_ADIV_DIV_1) |
                      ADC_CFG1_MODE(u32ModeBits);

        /*
         * CFG2:
         * SMPLTS is intentionally passed from configuration because the
         * analog layer owns the educational default sample-time policy.
         */
        pBase->CFG2 = ADC_CFG2_SMPLTS(pConfig->sampleTime);

        /*
         * SC2:
         * The current driver uses software trigger and disables DMA and
         * compare function by leaving the remaining SC2 fields cleared.
         */
        if (ADC_REF_ALT == pConfig->reference)
        {
            u32Sc2Value |= ADC_SC2_REFSEL(ADC_SC2_REFSEL_ALT);
        }
        else
        {
            u32Sc2Value |= ADC_SC2_REFSEL(ADC_SC2_REFSEL_DEFAULT);
        }

        pBase->SC2 = u32Sc2Value;

        /*
         * SC3:
         * ADCO remains cleared for single conversion mode. Hardware
         * averaging is enabled only when requested by configuration.
         */
        if (ADC_AVERAGE_DISABLED != pConfig->average)
        {
            u32Sc3Value |= ADC_SC3_AVGE_MASK;
            u32Sc3Value |= ADC_SC3_AVGS(ADC_GetAverageBits(pConfig->average));
        }

        pBase->SC3 = u32Sc3Value;

        pState->initialized = 1U;
        pState->busy = 0U;
        pState->done = 0U;
        pState->lastResult = 0U;
        pState->currentChannel = ADC_CHANNEL_SE12;
        pState->lastStatus = ADC_STATUS_OK;
        pState->config = *pConfig;
    }

    return RetVal;
}

/**
 * @copydoc ADC_Calibrate
 */
ADC_Status_t ADC_Calibrate(ADC_Instance_t instance)
{
    ADC_Type *pBase = (ADC_Type *)0;
    ADC_State_t *pState = (ADC_State_t *)0;
    ADC_Status_t RetVal = ADC_STATUS_OK;
    uint32_t u32Sc3Value = 0U;

    if (0U == ADC_IsValidInstance(instance))
    {
        RetVal = ADC_STATUS_INVALID_INSTANCE;
    }
    else
    {
        pState = &g_adcState[(uint8_t)instance];

        if (0U == pState->initialized)
        {
            RetVal = ADC_STATUS_NOT_INIT;
        }
        else if (0U != pState->busy)
        {
            RetVal = ADC_STATUS_BUSY;
        }
        else
        {
            pBase = ADC_GetBase(instance);

            /*
             * Calibration is executed after initialization and before
             * normal conversion to improve ADC conversion accuracy.
             */
            u32Sc3Value = pBase->SC3;

            pBase->SC3 = u32Sc3Value | ADC_SC3_CAL_MASK;

            while (0U != (pBase->SC3 & ADC_SC3_CAL_MASK))
            {
            }

            if (0U != ((pBase->SC3 >> ADC_SC3_CALF_BIT_SHIFT) & ADC_ONE_BIT_MASK))
            {
                pState->lastStatus = ADC_STATUS_CALIB_FAIL;
                RetVal = ADC_STATUS_CALIB_FAIL;
            }
            else
            {
                pState->lastStatus = ADC_STATUS_OK;
                RetVal = ADC_STATUS_OK;
            }
        }
    }

    return RetVal;
}

/**
 * @copydoc ADC_StartConversion_IT
 */
ADC_Status_t ADC_StartConversion_IT(ADC_Instance_t instance, ADC_Channel_t channel)
{
    ADC_Type *pBase = (ADC_Type *)0;
    ADC_State_t *pState = (ADC_State_t *)0;
    ADC_Status_t RetVal = ADC_STATUS_OK;
    uint32_t u32Sc1Value = 0U;

    if (0U == ADC_IsValidInstance(instance))
    {
        RetVal = ADC_STATUS_INVALID_INSTANCE;
    }
    else if (0U == ADC_IsValidChannel(channel))
    {
        RetVal = ADC_STATUS_INVALID_CHANNEL;
    }
    else
    {
        pState = &g_adcState[(uint8_t)instance];

        if (0U == pState->initialized)
        {
            RetVal = ADC_STATUS_NOT_INIT;
        }
        else if (0U != pState->busy)
        {
            RetVal = ADC_STATUS_BUSY;
        }
        else
        {
            pBase = ADC_GetBase(instance);

            pState->busy = 1U;
            pState->done = 0U;
            pState->currentChannel = channel;
            pState->lastStatus = ADC_STATUS_BUSY;

            /*
             * In software-triggered mode, writing SC1A with ADCH starts
             * one conversion. AIEN is set in the same SC1A write so the
             * ADC interrupt can latch the result when conversion completes.
             */
            u32Sc1Value = ADC_SC1_ADCH((uint32_t)channel);

            if (0U != pState->config.enableInterrupt)
            {
                u32Sc1Value |= ADC_SC1_AIEN_MASK;
            }

            pBase->SC1[0] = u32Sc1Value;
            RetVal = ADC_STATUS_OK;
        }
    }

    return RetVal;
}

/**
 * @copydoc ADC_IsDone
 */
uint8_t ADC_IsDone(ADC_Instance_t instance)
{
    uint8_t u8IsDone = 0U;

    if (0U != ADC_IsValidInstance(instance))
    {
        u8IsDone = g_adcState[(uint8_t)instance].done;
    }

    return u8IsDone;
}

/**
 * @copydoc ADC_GetResult
 */
ADC_Status_t ADC_GetResult(ADC_Instance_t instance, uint16_t *pResult)
{
    ADC_State_t *pState = (ADC_State_t *)0;
    ADC_Status_t RetVal = ADC_STATUS_OK;

    if (0U == ADC_IsValidInstance(instance))
    {
        RetVal = ADC_STATUS_INVALID_INSTANCE;
    }
    else if ((uint16_t *)0 == pResult)
    {
        RetVal = ADC_STATUS_INVALID_CONFIG;
    }
    else
    {
        pState = &g_adcState[(uint8_t)instance];

        if (0U == pState->initialized)
        {
            RetVal = ADC_STATUS_NOT_INIT;
        }
        else if (0U == pState->done)
        {
            RetVal = ADC_STATUS_NO_RESULT;
        }
        else
        {
            *pResult = pState->lastResult;
            pState->done = 0U;
            pState->lastStatus = ADC_STATUS_OK;
            RetVal = ADC_STATUS_OK;
        }
    }

    return RetVal;
}

/**
 * @copydoc ADC_IRQHandler
 */
void ADC_IRQHandler(ADC_Instance_t instance)
{
    ADC_Type *pBase = (ADC_Type *)0;
    ADC_State_t *pState = (ADC_State_t *)0;

    if (0U != ADC_IsValidInstance(instance))
    {
        pBase = ADC_GetBase(instance);
        pState = &g_adcState[(uint8_t)instance];

        /*
         * COCO is set when the conversion selected by SC1A is complete.
         * Reading R[0] retrieves the result for the software-triggered
         * conversion flow used by this driver.
         */
        if (0U != (pBase->SC1[0] & ADC_SC1_COCO_MASK))
        {
            pState->lastResult = (uint16_t)(pBase->R[0] & ADC_RESULT_16BIT_MASK);
            pState->busy = 0U;
            pState->done = 1U;
            pState->lastStatus = ADC_STATUS_OK;
        }
    }
}