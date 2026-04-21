#include "lpspi.h"

#define LPSPI   LPSPI_INSTANCE

static lpspi_config_t g_lpspiConfig;

static uint32_t LPSPI_BuildTCR(const lpspi_config_t *config)
{
    uint32_t tcr = 0U;

    switch (config->clockMode)
    {
        case LPSPI_MODE0:
            tcr |= LPSPI_TCR_CPOL(0U);
            tcr |= LPSPI_TCR_CPHA(0U);
            break;

        case LPSPI_MODE1:
            tcr |= LPSPI_TCR_CPOL(0U);
            tcr |= LPSPI_TCR_CPHA(1U);
            break;

        case LPSPI_MODE2:
            tcr |= LPSPI_TCR_CPOL(1U);
            tcr |= LPSPI_TCR_CPHA(0U);
            break;

        case LPSPI_MODE3:
            tcr |= LPSPI_TCR_CPOL(1U);
            tcr |= LPSPI_TCR_CPHA(1U);
            break;

        default:
            break;
    }

    /* PTB0 = PCS0 */
    tcr |= LPSPI_TCR_PCS(0U);

    if (config->bitOrder == LPSPI_LSB_FIRST)
    {
        tcr |= LPSPI_TCR_LSBF(1U);
    }
    else
    {
        tcr |= LPSPI_TCR_LSBF(0U);
    }

    if (config->frameSize == LPSPI_FRAME_SIZE_16)
    {
        tcr |= LPSPI_TCR_FRAMESZ(15U);
    }
    else
    {
        tcr |= LPSPI_TCR_FRAMESZ(7U);
    }

    return tcr;
}

bool LPSPI_IsTxReady(void)
{
    return ((LPSPI->SR & LPSPI_SR_TDF_MASK) != 0U);
}

void LPSPI_SetFrameSize(lpspi_frameSize_t frameSize)
{
    uint32_t tcr;

    while (LPSPI->SR & LPSPI_SR_MBF_MASK)
    {
    }

    tcr = LPSPI->TCR;

    tcr &= ~LPSPI_TCR_FRAMESZ_MASK;

    tcr |= LPSPI_TCR_FRAMESZ((uint32_t)frameSize - 1U);

    LPSPI->TCR = tcr;
}

bool LPSPI_IsRxReady(void)
{
    return ((LPSPI->SR & LPSPI_SR_RDF_MASK) != 0U);
}

bool LPSPI_IsTransferComplete(void)
{
    return ((LPSPI->SR & LPSPI_SR_TCF_MASK) != 0U);
}

void LPSPI_Enable(void)
{
    LPSPI->CR |= LPSPI_CR_MEN_MASK;
}

void LPSPI_Disable(void)
{
    LPSPI->CR &= ~LPSPI_CR_MEN_MASK;
}

lpspi_status_t LPSPI_SetMode(lpspi_clock_mode_t mode)
{
    uint32_t tcr;

    switch (mode)
    {
        case LPSPI_MODE0:
        case LPSPI_MODE1:
        case LPSPI_MODE2:
        case LPSPI_MODE3:
            g_lpspiConfig.clockMode = mode;
            break;

        default:
            return LPSPI_STATUS_INVALID_ARG;
    }

    while (LPSPI->SR & LPSPI_SR_MBF_MASK)
    {
    }

    tcr = LPSPI->TCR;
    tcr &= ~(LPSPI_TCR_CPOL_MASK | LPSPI_TCR_CPHA_MASK);

    switch (mode)
    {
        case LPSPI_MODE0:
            tcr |= LPSPI_TCR_CPOL(0U) | LPSPI_TCR_CPHA(0U);
            break;

        case LPSPI_MODE1:
            tcr |= LPSPI_TCR_CPOL(0U) | LPSPI_TCR_CPHA(1U);
            break;

        case LPSPI_MODE2:
            tcr |= LPSPI_TCR_CPOL(1U) | LPSPI_TCR_CPHA(0U);
            break;

        case LPSPI_MODE3:
            tcr |= LPSPI_TCR_CPOL(1U) | LPSPI_TCR_CPHA(1U);
            break;

        default:
            break;
    }

    LPSPI->TCR = tcr;

    return LPSPI_STATUS_OK;
}

lpspi_status_t LPSPI_SetBitOrder(lpspi_bit_order_t bitOrder)
{
    uint32_t tcr;

    if ((bitOrder != LPSPI_MSB_FIRST) &&
        (bitOrder != LPSPI_LSB_FIRST))
    {
        return LPSPI_STATUS_INVALID_ARG;
    }

    g_lpspiConfig.bitOrder = bitOrder;

    while (LPSPI->SR & LPSPI_SR_MBF_MASK)
    {
    }

    tcr = LPSPI->TCR;
    tcr &= ~LPSPI_TCR_LSBF_MASK;

    if (bitOrder == LPSPI_LSB_FIRST)
    {
        tcr |= LPSPI_TCR_LSBF(1U);
    }
    else
    {
        tcr |= LPSPI_TCR_LSBF(0U);
    }

    LPSPI->TCR = tcr;

    return LPSPI_STATUS_OK;
}

lpspi_status_t LPSPI_SetBaudRate(uint32_t baudrate)
{
    if (baudrate == 0U)
    {
        return LPSPI_STATUS_INVALID_ARG;
    }

    g_lpspiConfig.baudrate = baudrate;

    while (LPSPI->SR & LPSPI_SR_MBF_MASK)
    {
    }

    LPSPI->CCR =
        LPSPI_CCR_SCKPCS(0U) |
        LPSPI_CCR_PCSSCK(0U) |
        LPSPI_CCR_DBT(0U)    |
        LPSPI_CCR_SCKDIV(10U);

    LPSPI->TCR &= ~LPSPI_TCR_PRESCALE_MASK;
    LPSPI->TCR |= LPSPI_TCR_PRESCALE(0U);

    return LPSPI_STATUS_OK;
}

lpspi_status_t LPSPI_Init(const lpspi_config_t *config)
{
    uint32_t cfgr1;
    uint32_t tcr;

    if (config == (const lpspi_config_t *)0)
    {
        return LPSPI_STATUS_INVALID_ARG;
    }

    g_lpspiConfig = *config;

    /* LPSPI0 clock source = SPLL_DIV2_CLK = 40 MHz */
    IP_PCC->PCCn[PCC_LPSPI0_INDEX] = 0U;
    IP_PCC->PCCn[PCC_LPSPI0_INDEX] = PCC_PCCn_PCS(6) | PCC_PCCn_CGC_MASK;

    LPSPI_Disable();

    /* Reset FIFOs */
    LPSPI->CR = LPSPI_CR_RTF_MASK | LPSPI_CR_RRF_MASK;

    /* Clear all status flags */
    LPSPI->SR = 0xFFFFFFFFUL;

    cfgr1 = 0U;

    if (config->mode == LPSPI_MODE_MASTER)
    {
        cfgr1 |= LPSPI_CFGR1_MASTER(1U);
    }
    else
    {
        cfgr1 |= LPSPI_CFGR1_MASTER(0U);
    }

    /* Normal single-bit SPI: SIN=input, SOUT=output */
    cfgr1 |= LPSPI_CFGR1_PINCFG(0U);

    /* Active low PCS */
    cfgr1 |= LPSPI_CFGR1_PCSPOL(0U);

    LPSPI->CFGR1 = cfgr1;

    tcr = LPSPI_BuildTCR(config);
    LPSPI->TCR = tcr;

    if (config->mode == LPSPI_MODE_MASTER)
    {
        LPSPI->CCR =
            LPSPI_CCR_SCKPCS(0U) |
            LPSPI_CCR_PCSSCK(0U) |
            LPSPI_CCR_DBT(0U)    |
            LPSPI_CCR_SCKDIV(10U);

        LPSPI->TCR &= ~LPSPI_TCR_PRESCALE_MASK;
        LPSPI->TCR |= LPSPI_TCR_PRESCALE(0U);
    }

    LPSPI->FCR = LPSPI_FCR_TXWATER(0U) | LPSPI_FCR_RXWATER(0U);

    LPSPI_Enable();

    return LPSPI_STATUS_OK;
}


lpspi_status_t LPSPI_Transfer8(uint8_t txData, uint8_t *rxData)
{
    if (rxData == (uint8_t *)0)
    {
        return LPSPI_STATUS_INVALID_ARG;
    }

    while (!LPSPI_IsTxReady())
    {
    }

    LPSPI->TDR = LPSPI_TDR_DATA((uint32_t)txData);

    while (!LPSPI_IsRxReady())
    {
    }

    *rxData = (uint8_t)(LPSPI->RDR & LPSPI_RDR_DATA_MASK);

    return LPSPI_STATUS_OK;
}

lpspi_status_t LPSPI_Transfer16(uint16_t txData, uint16_t *rxData)
{
    if (rxData == (uint16_t *)0)
    {
        return LPSPI_STATUS_INVALID_ARG;
    }

    LPSPI_SetFrameSize(LPSPI_FRAME_SIZE_16);

    while (!LPSPI_IsTxReady())
    {
    }

    LPSPI->TDR = LPSPI_TDR_DATA((uint32_t)txData);

    while (!LPSPI_IsRxReady())
    {
    }

    *rxData = (uint16_t)(LPSPI->RDR & LPSPI_RDR_DATA_MASK);

    return LPSPI_STATUS_OK;
}

lpspi_status_t LPSPI_TransferBuffer(const uint8_t *txBuf,
                                    uint8_t *rxBuf,
                                    uint32_t length)
{
    uint32_t i;
    uint8_t tx;

    if ((txBuf == (const uint8_t *)0) && (rxBuf == (uint8_t *)0))
    {
        return LPSPI_STATUS_INVALID_ARG;
    }

    if (length == 0U)
    {
        return LPSPI_STATUS_OK;
    }

    LPSPI_SetFrameSize(LPSPI_FRAME_SIZE_8);

    for (i = 0U; i < length; i++)
    {
        tx = (txBuf != (const uint8_t *)0) ? txBuf[i] : 0xFFU;

        while (!LPSPI_IsTxReady())
        {
        }

        LPSPI->TDR = LPSPI_TDR_DATA((uint32_t)tx);

        while (!LPSPI_IsRxReady())
        {
        }

        if (rxBuf != (uint8_t *)0)
        {
            rxBuf[i] = (uint8_t)(LPSPI->RDR & LPSPI_RDR_DATA_MASK);
        }
        else
        {
            (void)LPSPI->RDR;
        }
    }

    return LPSPI_STATUS_OK;
}

lpspi_status_t LPSPI_Write8(uint8_t data)
{
    while (!LPSPI_IsTxReady())
    {
    }

    LPSPI->TDR = LPSPI_TDR_DATA((uint32_t)data);

    return LPSPI_STATUS_OK;
}

lpspi_status_t LPSPI_Write16(uint16_t data)
{
    while (!LPSPI_IsTxReady())
    {
    }

    LPSPI_SetFrameSize(LPSPI_FRAME_SIZE_16);
    LPSPI->TDR = LPSPI_TDR_DATA((uint32_t)data);

    return LPSPI_STATUS_OK;
}

lpspi_status_t LPSPI_Read8(uint8_t *data)
{
    if (data == (uint8_t *)0)
    {
        return LPSPI_STATUS_INVALID_ARG;
    }

    while (!LPSPI_IsRxReady())
    {
    }

    *data = (uint8_t)(LPSPI->RDR & LPSPI_RDR_DATA_MASK);

    return LPSPI_STATUS_OK;
}

lpspi_status_t LPSPI_Read16(uint16_t *data)
{
    if (data == (uint16_t *)0)
    {
        return LPSPI_STATUS_INVALID_ARG;
    }

    LPSPI_SetFrameSize(LPSPI_FRAME_SIZE_16);

    while (!LPSPI_IsRxReady())
    {
    }

    *data = (uint16_t)(LPSPI->RDR & LPSPI_RDR_DATA_MASK);

    return LPSPI_STATUS_OK;
}



