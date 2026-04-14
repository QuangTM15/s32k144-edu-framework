#include "adc.h"

/* ===================== INTERNAL TYPES ===================== */
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

/* ===================== INTERNAL STATE ===================== */
static ADC_State_t g_adcState[2] = {0};

/* ===================== INTERNAL HELPERS ===================== */
static ADC_Type *ADC_GetBase(ADC_Instance_t instance)
{
    ADC_Type *base = (ADC_Type *)0;

    switch (instance)
    {
    case IP_ADC_0:
        base = IP_ADC0;
        break;

    case IP_ADC_1:
        base = IP_ADC1;
        break;

    default:
        base = (ADC_Type *)0;
        break;
    }

    return base;
}

static uint32_t ADC_GetPccIndex(ADC_Instance_t instance)
{
    uint32_t index = 0xFFFFFFFFUL;

    switch (instance)
    {
    case IP_ADC_0:
        index = PCC_ADC0_INDEX;
        break;

    case IP_ADC_1:
        index = PCC_ADC1_INDEX;
        break;

    default:
        index = 0xFFFFFFFFUL;
        break;
    }

    return index;
}

static uint8_t ADC_IsValidInstance(ADC_Instance_t instance)
{
    return ((instance == IP_ADC_0) || (instance == IP_ADC_1)) ? 1U : 0U;
}

static uint8_t ADC_IsValidChannel(ADC_Channel_t channel)
{
    return ((channel == ADC_CHANNEL_SE12) || (channel == ADC_CHANNEL_SE13)) ? 1U : 0U;
}

static uint32_t ADC_GetModeBits(ADC_Resolution_t resolution)
{
    uint32_t modeBits = 0U;

    switch (resolution)
    {
    case ADC_RESOLUTION_8BIT:
        modeBits = 0U;
        break;

    case ADC_RESOLUTION_10BIT:
        modeBits = 2U;
        break;

    case ADC_RESOLUTION_12BIT:
        modeBits = 1U;
        break;

    default:
        modeBits = 0xFFFFFFFFUL;
        break;
    }

    return modeBits;
}

static uint32_t ADC_GetAverageBits(ADC_Average_t average)
{
    uint32_t avgsBits = 0U;

    switch (average)
    {
    case ADC_AVERAGE_DISABLED:
        avgsBits = 0U;
        break;

    case ADC_AVERAGE_4:
        avgsBits = 0U;
        break;

    case ADC_AVERAGE_8:
        avgsBits = 1U;
        break;

    case ADC_AVERAGE_16:
        avgsBits = 2U;
        break;

    case ADC_AVERAGE_32:
        avgsBits = 3U;
        break;

    default:
        avgsBits = 0xFFFFFFFFUL;
        break;
    }

    return avgsBits;
}

static ADC_Status_t ADC_ValidateConfig(const ADC_Config_t *config)
{
    if (config == (const ADC_Config_t *)0)
    {
        return ADC_STATUS_INVALID_CONFIG;
    }

    if (config->srcClockHz == 0UL)
    {
        return ADC_STATUS_INVALID_CONFIG;
    }

    if (ADC_GetModeBits(config->resolution) == 0xFFFFFFFFUL)
    {
        return ADC_STATUS_INVALID_CONFIG;
    }

    if (ADC_GetAverageBits(config->average) == 0xFFFFFFFFUL)
    {
        return ADC_STATUS_INVALID_CONFIG;
    }

    if ((config->reference != ADC_REF_DEFAULT) &&
        (config->reference != ADC_REF_ALT))
    {
        return ADC_STATUS_INVALID_CONFIG;
    }

    /* Phase 1 driver is interrupt-driven by design */
    if (config->enableInterrupt == 0U)
    {
        return ADC_STATUS_INVALID_CONFIG;
    }

    return ADC_STATUS_OK;
}

static void ADC_EnablePeripheralClock(ADC_Instance_t instance)
{
    uint32_t pccIndex = ADC_GetPccIndex(instance);

    if (pccIndex == 0xFFFFFFFFUL)
    {
        return;
    }

    /* Disable clock before changing PCS */
    IP_PCC->PCCn[pccIndex] &= ~PCC_PCCn_CGC_MASK;

    /*
     * Use SOSCDIV2 as ADC functional clock source.
     * This matches the shared framework clock setup you already use.
     */
    IP_PCC->PCCn[pccIndex] = PCC_PCCn_PCS(1U);

    /* Enable clock to ADC peripheral */
    IP_PCC->PCCn[pccIndex] |= PCC_PCCn_CGC_MASK;
}

static void ADC_DisableConversions(ADC_Type *base)
{
    /* ADCH = 0x1F disables module for further conversions */
    base->SC1[0] = ADC_SC1_ADCH(0x1FU);
}

/* ===================== API IMPLEMENTATION ===================== */
ADC_Status_t ADC_Init(ADC_Instance_t instance, const ADC_Config_t *config)
{
    ADC_Type *base;
    ADC_State_t *state;
    ADC_Status_t status;
    uint32_t modeBits;
    uint32_t sc2Value = 0U;
    uint32_t sc3Value = 0U;

    if (ADC_IsValidInstance(instance) == 0U)
    {
        return ADC_STATUS_INVALID_INSTANCE;
    }

    status = ADC_ValidateConfig(config);
    if (status != ADC_STATUS_OK)
    {
        return status;
    }

    base = ADC_GetBase(instance);
    state = &g_adcState[(uint8_t)instance];
    modeBits = ADC_GetModeBits(config->resolution);

    ADC_EnablePeripheralClock(instance);

    /* Disable conversion before reconfiguration */
    ADC_DisableConversions(base);

    /*
     * CFG1
     * ADICLK = 0 : input clock = ALTCLK1 (selected by PCC PCS)
     * ADIV   = 0 : divide by 1
     * MODE   : resolution selection
     */
    base->CFG1 = ADC_CFG1_ADICLK(0U) |
                 ADC_CFG1_ADIV(0U) |
                 ADC_CFG1_MODE(modeBits);

    /*
     * CFG2
     * sampleTime is used as raw SMPLTS field value
     * Example: 12 => sample time = 13 ADC clocks (cookbook default)
     */
    base->CFG2 = ADC_CFG2_SMPLTS(config->sampleTime);

    /*
     * SC2
     * ADTRG = 0 : software trigger
     * DMAEN = 0 : DMA disabled
     * Compare disabled
     * REFSEL configurable
     */
    if (config->reference == ADC_REF_ALT)
    {
        sc2Value |= ADC_SC2_REFSEL(1U);
    }
    else
    {
        sc2Value |= ADC_SC2_REFSEL(0U);
    }

    base->SC2 = sc2Value;

    /*
     * SC3
     * ADCO = 0 : single conversion
     * AVGE/AVGS according to config
     */
    if (config->average != ADC_AVERAGE_DISABLED)
    {
        sc3Value |= ADC_SC3_AVGE_MASK;
        sc3Value |= ADC_SC3_AVGS(ADC_GetAverageBits(config->average));
    }

    base->SC3 = sc3Value;

    state->initialized = 1U;
    state->busy = 0U;
    state->done = 0U;
    state->lastResult = 0U;
    state->currentChannel = ADC_CHANNEL_SE12;
    state->lastStatus = ADC_STATUS_OK;
    state->config = *config;

    return ADC_STATUS_OK;
}

ADC_Status_t ADC_Calibrate(ADC_Instance_t instance)
{
    ADC_Type *base;
    ADC_State_t *state;
    uint32_t sc3Value;

    if (ADC_IsValidInstance(instance) == 0U)
    {
        return ADC_STATUS_INVALID_INSTANCE;
    }

    state = &g_adcState[(uint8_t)instance];
    if (state->initialized == 0U)
    {
        return ADC_STATUS_NOT_INIT;
    }

    if (state->busy != 0U)
    {
        return ADC_STATUS_BUSY;
    }

    base = ADC_GetBase(instance);

    /*
     * Calibration must be run after reset and before normal conversion
     * to achieve specified accuracy.
     */
    sc3Value = base->SC3;

    /* Start calibration sequence */
    base->SC3 = sc3Value | ADC_SC3_CAL_MASK;

    /* Wait until calibration completes */
    while ((base->SC3 & ADC_SC3_CAL_MASK) != 0U)
    {
    }

    /* Check calibration failure flag */
    if (((base->SC3 >> 6U) & 0x1U) != 0U)
    {
        state->lastStatus = ADC_STATUS_CALIB_FAIL;
        return ADC_STATUS_CALIB_FAIL;
    }

    state->lastStatus = ADC_STATUS_OK;
    return ADC_STATUS_OK;
}

ADC_Status_t ADC_StartConversion_IT(ADC_Instance_t instance, ADC_Channel_t channel)
{
    ADC_Type *base;
    ADC_State_t *state;
    uint32_t sc1Value = 0U;

    if (ADC_IsValidInstance(instance) == 0U)
    {
        return ADC_STATUS_INVALID_INSTANCE;
    }

    if (ADC_IsValidChannel(channel) == 0U)
    {
        return ADC_STATUS_INVALID_CHANNEL;
    }

    state = &g_adcState[(uint8_t)instance];
    if (state->initialized == 0U)
    {
        return ADC_STATUS_NOT_INIT;
    }

    if (state->busy != 0U)
    {
        return ADC_STATUS_BUSY;
    }

    base = ADC_GetBase(instance);

    state->busy = 1U;
    state->done = 0U;
    state->currentChannel = channel;
    state->lastStatus = ADC_STATUS_BUSY;

    /*
     * In software trigger mode:
     * write SC1A with channel to start conversion.
     * AIEN must be written in the same SC1A word for interrupt completion.
     */
    sc1Value = ADC_SC1_ADCH((uint32_t)channel);

    if (state->config.enableInterrupt != 0U)
    {
        sc1Value |= ADC_SC1_AIEN_MASK;
    }

    base->SC1[0] = sc1Value;

    return ADC_STATUS_OK;
}

uint8_t ADC_IsDone(ADC_Instance_t instance)
{
    if (ADC_IsValidInstance(instance) == 0U)
    {
        return 0U;
    }

    return g_adcState[(uint8_t)instance].done;
}

ADC_Status_t ADC_GetResult(ADC_Instance_t instance, uint16_t *result)
{
    ADC_State_t *state;

    if (ADC_IsValidInstance(instance) == 0U)
    {
        return ADC_STATUS_INVALID_INSTANCE;
    }

    if (result == (uint16_t *)0)
    {
        return ADC_STATUS_INVALID_CONFIG;
    }

    state = &g_adcState[(uint8_t)instance];

    if (state->initialized == 0U)
    {
        return ADC_STATUS_NOT_INIT;
    }

    if (state->done == 0U)
    {
        return ADC_STATUS_NO_RESULT;
    }

    *result = state->lastResult;
    state->done = 0U;
    state->lastStatus = ADC_STATUS_OK;

    return ADC_STATUS_OK;
}

void ADC_IRQHandler(ADC_Instance_t instance)
{
    ADC_Type *base;
    ADC_State_t *state;

    if (ADC_IsValidInstance(instance) == 0U)
    {
        return;
    }

    base = ADC_GetBase(instance);
    state = &g_adcState[(uint8_t)instance];

    /*
     * COCO indicates conversion complete.
     * Reading R[0] captures the result for SC1A / software trigger flow.
     */
    if ((base->SC1[0] & ADC_SC1_COCO_MASK) != 0U)
    {
        state->lastResult = (uint16_t)(base->R[0] & 0xFFFFU);
        state->busy = 0U;
        state->done = 1U;
        state->lastStatus = ADC_STATUS_OK;
    }
}
