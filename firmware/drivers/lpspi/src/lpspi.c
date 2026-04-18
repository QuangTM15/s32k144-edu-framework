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

    if (config->dataSize == LPSPI_DATASIZE_16BIT)
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
    g_lpspiConfig.clockMode = mode;

    LPSPI->TCR &= ~(LPSPI_TCR_CPOL_MASK | LPSPI_TCR_CPHA_MASK);

    switch (mode)
    {
        case LPSPI_MODE0:
            LPSPI->TCR |= LPSPI_TCR_CPOL(0U) | LPSPI_TCR_CPHA(0U);
            break;

        case LPSPI_MODE1:
            LPSPI->TCR |= LPSPI_TCR_CPOL(0U) | LPSPI_TCR_CPHA(1U);
            break;

        case LPSPI_MODE2:
            LPSPI->TCR |= LPSPI_TCR_CPOL(1U) | LPSPI_TCR_CPHA(0U);
            break;

        case LPSPI_MODE3:
            LPSPI->TCR |= LPSPI_TCR_CPOL(1U) | LPSPI_TCR_CPHA(1U);
            break;

        default:
            return LPSPI_STATUS_INVALID_ARG;
    }

    return LPSPI_STATUS_OK;
}

lpspi_status_t LPSPI_SetBitOrder(lpspi_bit_order_t bitOrder)
{
    g_lpspiConfig.bitOrder = bitOrder;

    LPSPI->TCR &= ~LPSPI_TCR_LSBF_MASK;

    if (bitOrder == LPSPI_LSB_FIRST)
    {
        LPSPI->TCR |= LPSPI_TCR_LSBF(1U);
    }
    else if (bitOrder == LPSPI_MSB_FIRST)
    {
        LPSPI->TCR |= LPSPI_TCR_LSBF(0U);
    }
    else
    {
        return LPSPI_STATUS_INVALID_ARG;
    }

    return LPSPI_STATUS_OK;
}

lpspi_status_t LPSPI_SetBaudRate(uint32_t baudrate)
{
    if (baudrate == 0U)
    {
        return LPSPI_STATUS_INVALID_ARG;
    }

    /* Keep old simple logic for now */
    g_lpspiConfig.baudrate = baudrate;

    /* Fixed simple timing, same logic as before */
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
        LPSPI_SetBaudRate(config->baudrate);
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

    LPSPI->TDR = LPSPI_TDR_DATA(txData);

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

    while (!LPSPI_IsTxReady())
    {
    }

    LPSPI->TDR = LPSPI_TDR_DATA(txData);

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
    uint8_t rx;

    if ((txBuf == (const uint8_t *)0) && (rxBuf == (uint8_t *)0))
    {
        return LPSPI_STATUS_INVALID_ARG;
    }

    for (i = 0U; i < length; i++)
    {
        tx = (txBuf != (const uint8_t *)0) ? txBuf[i] : 0xFFU;

        if (LPSPI_Transfer8(tx, &rx) != LPSPI_STATUS_OK)
        {
            return LPSPI_STATUS_ERROR;
        }

        if (rxBuf != (uint8_t *)0)
        {
            rxBuf[i] = rx;
        }
    }

    return LPSPI_STATUS_OK;
}

lpspi_status_t LPSPI_Write(uint16_t data)
{
    while (!LPSPI_IsTxReady())
    {
    }

    LPSPI->TDR = LPSPI_TDR_DATA(data);

    return LPSPI_STATUS_OK;
}

lpspi_status_t LPSPI_Read(uint16_t *data)
{
    if (data == (uint16_t *)0)
    {
        return LPSPI_STATUS_INVALID_ARG;
    }

    while (!LPSPI_IsRxReady())
    {
    }

    *data = (uint16_t)(LPSPI->RDR & LPSPI_RDR_DATA_MASK);

    return LPSPI_STATUS_OK;
}
