#include "spi_hw.h"
#include "S32K144.h"

/* =========================================
 * INTERNAL
 * ========================================= */

#define SPI_INSTANCE              IP_LPSPI1
#define SPI_PORT                  IP_PORTB
#define SPI_PCC_PORT_INDEX        PCC_PORTB_INDEX
#define SPI_PCC_LPSPI_INDEX       PCC_LPSPI1_INDEX

#define SPI_SCK_PIN               14U
#define SPI_SIN_PIN               15U
#define SPI_SOUT_PIN              16U
#define SPI_PCS_PIN               17U

#define SPI_PCS_VALUE             3U
#define SPI_MASTER_PCC_VALUE      0xC6000000U   /* SPLL_DIV2 clock source + CGC */
#define SPI_MASTER_CCR_DEFAULT    0x04090808U   /* Cookbook-like 1 MHz timing */
#define SPI_MASTER_FCR_DEFAULT    0x00000003U   /* RXWATER=0, TXWATER=3 */
#define SPI_SLAVE_FCR_DEFAULT     0x00000000U   /* RXWATER=0, TXWATER=0 */

static uint32_t g_spiMasterTcrBase = 0U;
static uint32_t g_spiMasterSrcClockHz = 40000000U;

/* =========================================
 * INTERNAL HELPERS
 * ========================================= */

static void SPI_HW_EnablePortClock(void)
{
    IP_PCC->PCCn[SPI_PCC_PORT_INDEX] |= PCC_PCCn_CGC_MASK;
}

static void SPI_HW_ConfigPins(void)
{
    SPI_PORT->PCR[SPI_SCK_PIN]  = PORT_PCR_MUX(3);
    SPI_PORT->PCR[SPI_SIN_PIN]  = PORT_PCR_MUX(3);
    SPI_PORT->PCR[SPI_SOUT_PIN] = PORT_PCR_MUX(3);
    SPI_PORT->PCR[SPI_PCS_PIN]  = PORT_PCR_MUX(3);
}

static void SPI_HW_EnableModuleClock(void)
{
    IP_PCC->PCCn[SPI_PCC_LPSPI_INDEX] = 0U;
    IP_PCC->PCCn[SPI_PCC_LPSPI_INDEX] = SPI_MASTER_PCC_VALUE;
}

static void SPI_HW_DisableModule(void)
{
    SPI_INSTANCE->CR = 0x00000000U;
}

static void SPI_HW_ResetFifos(void)
{
    SPI_INSTANCE->CR |= (LPSPI_CR_RTF_MASK | LPSPI_CR_RRF_MASK);
}

static uint32_t SPI_HW_BuildTcrCommon(SPI_HW_Mode_t mode, SPI_HW_BitOrder_t bitOrder, uint32_t frameSize)
{
    uint32_t tcr = 0U;

    tcr |= LPSPI_TCR_PCS(SPI_PCS_VALUE);
    tcr |= LPSPI_TCR_FRAMESZ(frameSize);

    if (bitOrder == SPI_HW_LSBFIRST)
    {
        tcr |= LPSPI_TCR_LSBF_MASK;
    }

    switch (mode)
    {
        case SPI_HW_MODE0:
            break;

        case SPI_HW_MODE1:
            tcr |= LPSPI_TCR_CPHA_MASK;
            break;

        case SPI_HW_MODE2:
            tcr |= LPSPI_TCR_CPOL_MASK;
            break;

        case SPI_HW_MODE3:
            tcr |= (LPSPI_TCR_CPOL_MASK | LPSPI_TCR_CPHA_MASK);
            break;

        default:
            break;
    }

    return tcr;
}

static void SPI_HW_MasterSetFrameSize(uint32_t frameSize)
{
    uint32_t tcr = g_spiMasterTcrBase;

    tcr &= ~LPSPI_TCR_FRAMESZ_MASK;
    tcr |= LPSPI_TCR_FRAMESZ(frameSize);

    SPI_INSTANCE->TCR = tcr;
}

static void SPI_HW_ClearStatusFlags(uint32_t flags)
{
    SPI_INSTANCE->SR |= flags;
}

/* =========================================
 * MASTER INIT
 * ========================================= */

SPI_HW_Status_t SPI_HW_MasterInit(const SPI_HW_MasterConfig_t *config)
{
    uint32_t tcr = 0U;

    if (config == 0U)
    {
        return SPI_HW_STATUS_ERROR;
    }

    if ((config->baudRate == 0U) || (config->srcClockHz == 0U))
    {
        return SPI_HW_STATUS_ERROR;
    }

    SPI_HW_EnablePortClock();
    SPI_HW_ConfigPins();
    SPI_HW_EnableModuleClock();

    SPI_HW_DisableModule();

    SPI_INSTANCE->IER   = 0x00000000U;
    SPI_INSTANCE->DER   = 0x00000000U;
    SPI_INSTANCE->CFGR0 = 0x00000000U;
    SPI_INSTANCE->CFGR1 = LPSPI_CFGR1_MASTER_MASK;

    g_spiMasterSrcClockHz = config->srcClockHz;

    tcr = SPI_HW_BuildTcrCommon(config->mode, config->bitOrder, 7U);
    tcr |= LPSPI_TCR_PRESCALE(2U); /* divide by 4 */

    g_spiMasterTcrBase = tcr;
    SPI_INSTANCE->TCR = tcr;

    SPI_INSTANCE->CCR = SPI_MASTER_CCR_DEFAULT;
    SPI_INSTANCE->FCR = SPI_MASTER_FCR_DEFAULT;

    SPI_INSTANCE->CR = 0x00000000U;
    SPI_HW_ResetFifos();
    SPI_HW_ClearStatusFlags(LPSPI_SR_TDF_MASK | LPSPI_SR_RDF_MASK |
                            LPSPI_SR_WCF_MASK | LPSPI_SR_FCF_MASK |
                            LPSPI_SR_TCF_MASK | LPSPI_SR_TEF_MASK |
                            LPSPI_SR_REF_MASK);

    SPI_INSTANCE->CR = 0x00000009U; /* DBGEN=1, MEN=1 */

    return SPI_HW_STATUS_OK;
}

/* =========================================
 * MASTER DEINIT
 * ========================================= */

void SPI_HW_MasterDeinit(void)
{
    SPI_INSTANCE->CR &= ~LPSPI_CR_MEN_MASK;
}

/* =========================================
 * MASTER TRANSFER
 * ========================================= */

SPI_HW_Status_t SPI_HW_MasterTransfer8(uint8_t txData, uint8_t *rxData)
{
    uint8_t data;

    SPI_HW_MasterSetFrameSize(7U);

    while ((SPI_INSTANCE->SR & LPSPI_SR_TDF_MASK) == 0U)
    {
    }

    SPI_INSTANCE->TDR = (uint32_t)txData;
    SPI_HW_ClearStatusFlags(LPSPI_SR_TDF_MASK);

    while ((SPI_INSTANCE->SR & LPSPI_SR_RDF_MASK) == 0U)
    {
    }

    data = (uint8_t)SPI_INSTANCE->RDR;
    SPI_HW_ClearStatusFlags(LPSPI_SR_RDF_MASK);

    if (rxData != 0U)
    {
        *rxData = data;
    }

    return SPI_HW_STATUS_OK;
}

SPI_HW_Status_t SPI_HW_MasterTransfer16(uint16_t txData, uint16_t *rxData)
{
    uint16_t data;

    SPI_HW_MasterSetFrameSize(15U);

    while ((SPI_INSTANCE->SR & LPSPI_SR_TDF_MASK) == 0U)
    {
    }

    SPI_INSTANCE->TDR = (uint32_t)txData;
    SPI_HW_ClearStatusFlags(LPSPI_SR_TDF_MASK);

    while ((SPI_INSTANCE->SR & LPSPI_SR_RDF_MASK) == 0U)
    {
    }

    data = (uint16_t)SPI_INSTANCE->RDR;
    SPI_HW_ClearStatusFlags(LPSPI_SR_RDF_MASK);

    SPI_HW_MasterSetFrameSize(7U);

    if (rxData != 0U)
    {
        *rxData = data;
    }

    return SPI_HW_STATUS_OK;
}

SPI_HW_Status_t SPI_HW_MasterTransferBuffer(const uint8_t *txBuffer, uint8_t *rxBuffer, uint32_t length)
{
    uint32_t i;
    uint8_t rxTemp;

    if (length == 0U)
    {
        return SPI_HW_STATUS_OK;
    }

    for (i = 0U; i < length; i++)
    {
        if (SPI_HW_MasterTransfer8((txBuffer != 0U) ? txBuffer[i] : 0xFFU, &rxTemp) != SPI_HW_STATUS_OK)
        {
            return SPI_HW_STATUS_ERROR;
        }

        if (rxBuffer != 0U)
        {
            rxBuffer[i] = rxTemp;
        }
    }

    return SPI_HW_STATUS_OK;
}

/* =========================================
 * RX UTILITIES
 * ========================================= */

bool SPI_HW_RxAvailable(void)
{
    return ((SPI_INSTANCE->SR & LPSPI_SR_RDF_MASK) != 0U);
}

uint8_t SPI_HW_Read8(void)
{
    uint8_t data = (uint8_t)SPI_INSTANCE->RDR;
    SPI_HW_ClearStatusFlags(LPSPI_SR_RDF_MASK);
    return data;
}

uint16_t SPI_HW_Read16(void)
{
    uint16_t data = (uint16_t)SPI_INSTANCE->RDR;
    SPI_HW_ClearStatusFlags(LPSPI_SR_RDF_MASK);
    return data;
}

/* =========================================
 * RUNTIME CONFIG
 * ========================================= */

SPI_HW_Status_t SPI_HW_SetMode(SPI_HW_Mode_t mode)
{
    uint32_t tcr;

    tcr = g_spiMasterTcrBase;
    tcr &= ~(LPSPI_TCR_CPOL_MASK | LPSPI_TCR_CPHA_MASK);

    switch (mode)
    {
        case SPI_HW_MODE0:
            break;

        case SPI_HW_MODE1:
            tcr |= LPSPI_TCR_CPHA_MASK;
            break;

        case SPI_HW_MODE2:
            tcr |= LPSPI_TCR_CPOL_MASK;
            break;

        case SPI_HW_MODE3:
            tcr |= (LPSPI_TCR_CPOL_MASK | LPSPI_TCR_CPHA_MASK);
            break;

        default:
            return SPI_HW_STATUS_ERROR;
    }

    g_spiMasterTcrBase = tcr;
    SPI_HW_MasterSetFrameSize(7U);

    return SPI_HW_STATUS_OK;
}

SPI_HW_Status_t SPI_HW_SetBitOrder(SPI_HW_BitOrder_t bitOrder)
{
    uint32_t tcr = g_spiMasterTcrBase;

    if (bitOrder == SPI_HW_LSBFIRST)
    {
        tcr |= LPSPI_TCR_LSBF_MASK;
    }
    else
    {
        tcr &= ~LPSPI_TCR_LSBF_MASK;
    }

    g_spiMasterTcrBase = tcr;
    SPI_HW_MasterSetFrameSize(7U);

    return SPI_HW_STATUS_OK;
}

SPI_HW_Status_t SPI_HW_SetBaudRate(uint32_t baudRate)
{
    uint32_t sckdiv;

    if (baudRate == 0U)
    {
        return SPI_HW_STATUS_ERROR;
    }

    sckdiv = (g_spiMasterSrcClockHz / baudRate) - 2U;
    SPI_INSTANCE->CCR = LPSPI_CCR_SCKDIV(sckdiv);

    return SPI_HW_STATUS_OK;
}

/* =========================================
 * SLAVE INIT
 * ========================================= */

SPI_HW_Status_t SPI_HW_SlaveInit(const SPI_HW_SlaveConfig_t *config)
{
    uint32_t tcr;

    if (config == 0U)
    {
        return SPI_HW_STATUS_ERROR;
    }

    SPI_HW_EnablePortClock();
    SPI_HW_ConfigPins();
    SPI_HW_EnableModuleClock();

    SPI_HW_DisableModule();

    SPI_INSTANCE->IER   = 0x00000000U;
    SPI_INSTANCE->DER   = 0x00000000U;
    SPI_INSTANCE->CFGR0 = 0x00000000U;
    SPI_INSTANCE->CFGR1 = 0x00000000U; /* slave mode */

    tcr = SPI_HW_BuildTcrCommon(config->mode, config->bitOrder, 7U);
    SPI_INSTANCE->TCR = tcr;

    SPI_INSTANCE->FCR = SPI_SLAVE_FCR_DEFAULT;

    SPI_INSTANCE->CR = 0x00000000U;
    SPI_HW_ResetFifos();
    SPI_HW_ClearStatusFlags(LPSPI_SR_TDF_MASK | LPSPI_SR_RDF_MASK |
                            LPSPI_SR_WCF_MASK | LPSPI_SR_FCF_MASK |
                            LPSPI_SR_TCF_MASK | LPSPI_SR_TEF_MASK |
                            LPSPI_SR_REF_MASK);

    SPI_INSTANCE->CR = LPSPI_CR_MEN_MASK;

    return SPI_HW_STATUS_OK;
}

/* =========================================
 * SLAVE DEINIT
 * ========================================= */

void SPI_HW_SlaveDeinit(void)
{
    SPI_INSTANCE->CR &= ~LPSPI_CR_MEN_MASK;
}

/* =========================================
 * SLAVE DATA
 * ========================================= */

bool SPI_HW_SlaveRxAvailable(void)
{
    return ((SPI_INSTANCE->SR & LPSPI_SR_RDF_MASK) != 0U);
}

SPI_HW_Status_t SPI_HW_SlaveRead8(uint8_t *rxData)
{
    if (rxData == 0U)
    {
        return SPI_HW_STATUS_ERROR;
    }

    while ((SPI_INSTANCE->SR & LPSPI_SR_RDF_MASK) == 0U)
    {
    }

    *rxData = (uint8_t)SPI_INSTANCE->RDR;
    SPI_HW_ClearStatusFlags(LPSPI_SR_RDF_MASK);

    return SPI_HW_STATUS_OK;
}

SPI_HW_Status_t SPI_HW_SlaveWrite8(uint8_t txData)
{
    /* Preload data into TX FIFO before master clocks */
    SPI_INSTANCE->TDR = (uint32_t)txData;
    return SPI_HW_STATUS_OK;
}
