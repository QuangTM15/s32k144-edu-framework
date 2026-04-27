#include "lpi2c.h"
#include "board_pins.h"

/* ============================================================
 * Internal definitions
 * ============================================================ */

/* PCC clock source: SPLL_DIV2_CLK = 40 MHz */
#define LPI2C0_PCC_CLOCK_SOURCE    6U

#define LPI2C_MASTER_CLEAR_FLAGS   (LPI2C_MSR_DMF_MASK  | \
                                    LPI2C_MSR_PLTF_MASK | \
                                    LPI2C_MSR_FEF_MASK  | \
                                    LPI2C_MSR_ALF_MASK  | \
                                    LPI2C_MSR_NDF_MASK  | \
                                    LPI2C_MSR_SDF_MASK  | \
                                    LPI2C_MSR_EPF_MASK)

/* MTDR command values */
#define LPI2C_CMD_TRANSMIT         0U
#define LPI2C_CMD_RECEIVE          1U
#define LPI2C_CMD_STOP             2U
#define LPI2C_CMD_START            4U

/* ============================================================
 * Internal helpers
 * ============================================================ */

static void LPI2C0_PortInit(void)
{
    IP_PCC->PCCn[PCC_PORTA_INDEX] |= PCC_PCCn_CGC_MASK;

    /* PTA3 = LPI2C0_SCL ALT3, pull-up */
    BOARD_I2C0_SCL_PORT->PCR[BOARD_I2C0_SCL_PIN] =
        PORT_PCR_MUX(3U) |
        PORT_PCR_PE_MASK |
        PORT_PCR_PS_MASK;

    /* PTA2 = LPI2C0_SDA ALT3, pull-up */
    BOARD_I2C0_SDA_PORT->PCR[BOARD_I2C0_SDA_PIN] =
        PORT_PCR_MUX(3U) |
        PORT_PCR_PE_MASK |
        PORT_PCR_PS_MASK;
}

static LPI2C_Status_t LPI2C_MasterCheckError(LPI2C_Type *base)
{
    uint32_t status = base->MSR;

    if ((status & LPI2C_MSR_NDF_MASK) != 0U)
    {
        return LPI2C_STATUS_NACK;
    }

    if ((status & LPI2C_MSR_ALF_MASK) != 0U)
    {
        return LPI2C_STATUS_ARBITRATION_LOST;
    }

    if ((status & LPI2C_MSR_FEF_MASK) != 0U)
    {
        return LPI2C_STATUS_FIFO_ERROR;
    }

    if ((status & LPI2C_MSR_PLTF_MASK) != 0U)
    {
        return LPI2C_STATUS_PIN_LOW_TIMEOUT;
    }

    return LPI2C_STATUS_OK;
}

static LPI2C_Status_t LPI2C_MasterWaitFlag(LPI2C_Type *base,
                                           uint32_t flag,
                                           uint32_t timeout)
{
    LPI2C_Status_t status;

    while ((base->MSR & flag) == 0U)
    {
        status = LPI2C_MasterCheckError(base);
        if (status != LPI2C_STATUS_OK)
        {
            return status;
        }

        if (timeout == 0U)
        {
            return LPI2C_STATUS_TIMEOUT;
        }

        timeout--;
    }

    return LPI2C_STATUS_OK;
}

static LPI2C_Status_t LPI2C_MasterSetTiming(LPI2C_Type *base,
                                            uint32_t baudRate)
{
    if (baudRate == 100000U)
    {
        base->MCFGR1 = LPI2C_MCFGR1_PINCFG(0U) |
                       LPI2C_MCFGR1_PRESCALE(3U);

        base->MCFGR2 = LPI2C_MCFGR2_FILTSCL(1U) |
                       LPI2C_MCFGR2_FILTSDA(1U);

        base->MCCR0 = LPI2C_MCCR0_CLKLO(28U)   |
                      LPI2C_MCCR0_CLKHI(20U)   |
                      LPI2C_MCCR0_SETHOLD(20U) |
                      LPI2C_MCCR0_DATAVD(4U);

        return LPI2C_STATUS_OK;
    }

    if (baudRate == 400000U)
    {
        base->MCFGR1 = LPI2C_MCFGR1_PINCFG(0U) |
                       LPI2C_MCFGR1_PRESCALE(1U);

        base->MCFGR2 = LPI2C_MCFGR2_FILTSCL(1U) |
                       LPI2C_MCFGR2_FILTSDA(1U);

        base->MCCR0 = LPI2C_MCCR0_CLKLO(31U)   |
                      LPI2C_MCCR0_CLKHI(21U)   |
                      LPI2C_MCCR0_SETHOLD(15U) |
                      LPI2C_MCCR0_DATAVD(4U);

        return LPI2C_STATUS_OK;
    }

    return LPI2C_STATUS_INVALID_ARGUMENT;
}

LPI2C_Status_t LPI2C_MasterTransferBlocking(LPI2C_Type *base,
                                            const LPI2C_MasterTransfer_t *transfer,
                                            uint32_t timeout)
{
    if ((base == 0) || (transfer == 0))
    {
        return LPI2C_STATUS_INVALID_ARGUMENT;
    }

    if (transfer->type == LPI2C_TRANSFER_WRITE)
    {
        return LPI2C_MasterWriteBlocking(base,
                                         transfer->slaveAddress,
                                         transfer->txData,
                                         transfer->txSize,
                                         timeout);
    }

    if (transfer->type == LPI2C_TRANSFER_READ)
    {
        return LPI2C_MasterReadBlocking(base,
                                        transfer->slaveAddress,
                                        transfer->rxData,
                                        transfer->rxSize,
                                        timeout);
    }

    if (transfer->type == LPI2C_TRANSFER_WRITE_READ)
    {
        return LPI2C_MasterWriteReadBlocking(base,
                                             transfer->slaveAddress,
                                             transfer->txData,
                                             transfer->txSize,
                                             transfer->rxData,
                                             transfer->rxSize,
                                             timeout);
    }

    return LPI2C_STATUS_INVALID_ARGUMENT;
}

/* ============================================================
 * Public API - Phase 1
 * ============================================================ */

void LPI2C_MasterGetDefaultConfig(LPI2C_MasterConfig_t *config)
{
    if (config == 0)
    {
        return;
    }

    config->srcClockHz  = 40000000U;
    config->baudRate    = LPI2C_SPEED_STANDARD;
    config->enableDebug = true;
    config->enableDoze  = false;
    config->timeout     = 1000000U;
}

LPI2C_Status_t LPI2C_MasterInit(LPI2C_Type *base,
                                const LPI2C_MasterConfig_t *config)
{
    uint32_t mcr = 0U;

    if ((base == 0) || (config == 0))
    {
        return LPI2C_STATUS_INVALID_ARGUMENT;
    }

    if (base != IP_LPI2C0)
    {
        return LPI2C_STATUS_INVALID_ARGUMENT;
    }

    LPI2C0_PortInit();

    IP_PCC->PCCn[PCC_LPI2C0_INDEX] = 0U;
    IP_PCC->PCCn[PCC_LPI2C0_INDEX] =
        PCC_PCCn_PCS(LPI2C0_PCC_CLOCK_SOURCE) |
        PCC_PCCn_CGC_MASK;

    /* Reset master logic */
    base->MCR = LPI2C_MCR_RST_MASK;
    base->MCR = 0U;

    /* Polling phase: no interrupt, no DMA */
    base->MIER = 0U;
    base->MDER = 0U;

    /* Clear old flags */
    base->MSR = LPI2C_MASTER_CLEAR_FLAGS;

    /* Reset TX/RX FIFO */
    base->MCR = LPI2C_MCR_RTF_MASK | LPI2C_MCR_RRF_MASK;

    /* TXWATER = 0, RXWATER = 0 */
    base->MFCR = LPI2C_MFCR_TXWATER(0U) |
                 LPI2C_MFCR_RXWATER(0U);

    if (LPI2C_MasterSetTiming(base, config->baudRate) != LPI2C_STATUS_OK)
    {
        return LPI2C_STATUS_INVALID_ARGUMENT;
    }

    if (config->enableDebug)
    {
        mcr |= LPI2C_MCR_DBGEN_MASK;
    }

    if (config->enableDoze)
    {
        mcr |= LPI2C_MCR_DOZEN_MASK;
    }

    base->MCR = mcr | LPI2C_MCR_MEN_MASK;

    return LPI2C_STATUS_OK;
}

void LPI2C_MasterDeinit(LPI2C_Type *base)
{
    if (base == 0)
    {
        return;
    }

    base->MCR = 0U;

    if (base == IP_LPI2C0)
    {
        IP_PCC->PCCn[PCC_LPI2C0_INDEX] = 0U;
    }
}

LPI2C_Status_t LPI2C_MasterWriteBlocking(LPI2C_Type *base,
                                         uint8_t slaveAddress,
                                         const uint8_t *data,
                                         uint16_t size,
                                         uint32_t timeout)
{
    uint16_t i;
    uint8_t addressByte;
    LPI2C_Status_t status;

    if (base == 0)
    {
        return LPI2C_STATUS_INVALID_ARGUMENT;
    }

    if ((data == 0) && (size > 0U))
    {
        return LPI2C_STATUS_INVALID_ARGUMENT;
    }

    if (slaveAddress > 0x7FU)
    {
        return LPI2C_STATUS_INVALID_ARGUMENT;
    }

    if ((base->MSR & LPI2C_MSR_MBF_MASK) != 0U)
    {
        return LPI2C_STATUS_BUSY;
    }

    base->MSR = LPI2C_MASTER_CLEAR_FLAGS;
    base->MCR |= LPI2C_MCR_RTF_MASK | LPI2C_MCR_RRF_MASK;

    addressByte = (uint8_t)(slaveAddress << 1U); /* Write bit = 0 */

    status = LPI2C_MasterWaitFlag(base, LPI2C_MSR_TDF_MASK, timeout);
    if (status != LPI2C_STATUS_OK)
    {
        base->MSR = LPI2C_MASTER_CLEAR_FLAGS;
        return status;
    }

    /* START + address */
    base->MTDR = LPI2C_MTDR_CMD(LPI2C_CMD_START) |
                 LPI2C_MTDR_DATA(addressByte);

    for (i = 0U; i < size; i++)
    {
        status = LPI2C_MasterWaitFlag(base, LPI2C_MSR_TDF_MASK, timeout);
        if (status != LPI2C_STATUS_OK)
        {
            base->MTDR = LPI2C_MTDR_CMD(LPI2C_CMD_STOP);
            base->MSR = LPI2C_MASTER_CLEAR_FLAGS;
            return status;
        }

        base->MTDR = LPI2C_MTDR_CMD(LPI2C_CMD_TRANSMIT) |
                     LPI2C_MTDR_DATA(data[i]);
    }

    status = LPI2C_MasterWaitFlag(base, LPI2C_MSR_TDF_MASK, timeout);
    if (status != LPI2C_STATUS_OK)
    {
        base->MSR = LPI2C_MASTER_CLEAR_FLAGS;
        return status;
    }

    /* STOP */
    base->MTDR = LPI2C_MTDR_CMD(LPI2C_CMD_STOP);

    status = LPI2C_MasterWaitFlag(base, LPI2C_MSR_SDF_MASK, timeout);
    if (status != LPI2C_STATUS_OK)
    {
        base->MSR = LPI2C_MASTER_CLEAR_FLAGS;
        return status;
    }

    status = LPI2C_MasterCheckError(base);

    base->MSR = LPI2C_MASTER_CLEAR_FLAGS;

    return status;
}

LPI2C_Status_t LPI2C_MasterReadBlocking(LPI2C_Type *base,
                                        uint8_t slaveAddress,
                                        uint8_t *data,
                                        uint16_t size,
                                        uint32_t timeout)
{
    uint16_t i;
    uint32_t rxWord;
    uint8_t addressByte;
    LPI2C_Status_t status;

    if ((base == 0) || (data == 0) || (size == 0U))
    {
        return LPI2C_STATUS_INVALID_ARGUMENT;
    }

    if (slaveAddress > 0x7FU)
    {
        return LPI2C_STATUS_INVALID_ARGUMENT;
    }

    if ((base->MSR & LPI2C_MSR_MBF_MASK) != 0U)
    {
        return LPI2C_STATUS_BUSY;
    }

    base->MSR = LPI2C_MASTER_CLEAR_FLAGS;
    base->MCR |= LPI2C_MCR_RTF_MASK | LPI2C_MCR_RRF_MASK;

    addressByte = (uint8_t)((slaveAddress << 1U) | 1U);

    status = LPI2C_MasterWaitFlag(base, LPI2C_MSR_TDF_MASK, timeout);
    if (status != LPI2C_STATUS_OK)
    {
        base->MSR = LPI2C_MASTER_CLEAR_FLAGS;
        return status;
    }

    base->MTDR = LPI2C_MTDR_CMD(LPI2C_CMD_START) |
                 LPI2C_MTDR_DATA(addressByte);

    status = LPI2C_MasterWaitFlag(base, LPI2C_MSR_TDF_MASK, timeout);
    if (status != LPI2C_STATUS_OK)
    {
        base->MTDR = LPI2C_MTDR_CMD(LPI2C_CMD_STOP);
        base->MSR = LPI2C_MASTER_CLEAR_FLAGS;
        return status;
    }

    base->MTDR = LPI2C_MTDR_CMD(LPI2C_CMD_RECEIVE) |
                 LPI2C_MTDR_DATA((uint8_t)(size - 1U));

    for (i = 0U; i < size; i++)
    {
        status = LPI2C_MasterWaitFlag(base, LPI2C_MSR_RDF_MASK, timeout);
        if (status != LPI2C_STATUS_OK)
        {
            base->MTDR = LPI2C_MTDR_CMD(LPI2C_CMD_STOP);
            base->MSR = LPI2C_MASTER_CLEAR_FLAGS;
            return status;
        }

        rxWord = base->MRDR;

        if ((rxWord & LPI2C_MRDR_RXEMPTY_MASK) != 0U)
        {
            return LPI2C_STATUS_ERROR;
        }

        data[i] = (uint8_t)(rxWord & LPI2C_MRDR_DATA_MASK);
    }

    status = LPI2C_MasterWaitFlag(base, LPI2C_MSR_TDF_MASK, timeout);
    if (status != LPI2C_STATUS_OK)
    {
        base->MSR = LPI2C_MASTER_CLEAR_FLAGS;
        return status;
    }

    base->MTDR = LPI2C_MTDR_CMD(LPI2C_CMD_STOP);

    status = LPI2C_MasterWaitFlag(base, LPI2C_MSR_SDF_MASK, timeout);

    base->MSR = LPI2C_MASTER_CLEAR_FLAGS;

    return status;
}

LPI2C_Status_t LPI2C_MasterWriteReadBlocking(LPI2C_Type *base,
                                             uint8_t slaveAddress,
                                             const uint8_t *txData,
                                             uint16_t txSize,
                                             uint8_t *rxData,
                                             uint16_t rxSize,
                                             uint32_t timeout)
{
    uint16_t i;
    uint32_t rxWord;
    uint8_t addressByte;
    LPI2C_Status_t status;

    if ((base == 0) || (txData == 0) || (rxData == 0) ||
        (txSize == 0U) || (rxSize == 0U))
    {
        return LPI2C_STATUS_INVALID_ARGUMENT;
    }

    if (slaveAddress > 0x7FU)
    {
        return LPI2C_STATUS_INVALID_ARGUMENT;
    }

    if ((base->MSR & LPI2C_MSR_MBF_MASK) != 0U)
    {
        return LPI2C_STATUS_BUSY;
    }

    base->MSR = LPI2C_MASTER_CLEAR_FLAGS;
    base->MCR |= LPI2C_MCR_RTF_MASK | LPI2C_MCR_RRF_MASK;

    addressByte = (uint8_t)(slaveAddress << 1U);

    status = LPI2C_MasterWaitFlag(base, LPI2C_MSR_TDF_MASK, timeout);
    if (status != LPI2C_STATUS_OK)
    {
        return status;
    }

    base->MTDR = LPI2C_MTDR_CMD(LPI2C_CMD_START) |
                 LPI2C_MTDR_DATA(addressByte);

    for (i = 0U; i < txSize; i++)
    {
        status = LPI2C_MasterWaitFlag(base, LPI2C_MSR_TDF_MASK, timeout);
        if (status != LPI2C_STATUS_OK)
        {
            base->MTDR = LPI2C_MTDR_CMD(LPI2C_CMD_STOP);
            return status;
        }

        base->MTDR = LPI2C_MTDR_CMD(LPI2C_CMD_TRANSMIT) |
                     LPI2C_MTDR_DATA(txData[i]);
    }

    addressByte = (uint8_t)((slaveAddress << 1U) | 1U);

    status = LPI2C_MasterWaitFlag(base, LPI2C_MSR_TDF_MASK, timeout);
    if (status != LPI2C_STATUS_OK)
    {
        base->MTDR = LPI2C_MTDR_CMD(LPI2C_CMD_STOP);
        return status;
    }

    base->MTDR = LPI2C_MTDR_CMD(LPI2C_CMD_START) |
                 LPI2C_MTDR_DATA(addressByte);

    status = LPI2C_MasterWaitFlag(base, LPI2C_MSR_TDF_MASK, timeout);
    if (status != LPI2C_STATUS_OK)
    {
        base->MTDR = LPI2C_MTDR_CMD(LPI2C_CMD_STOP);
        return status;
    }

    base->MTDR = LPI2C_MTDR_CMD(LPI2C_CMD_RECEIVE) |
                 LPI2C_MTDR_DATA((uint8_t)(rxSize - 1U));

    for (i = 0U; i < rxSize; i++)
    {
        status = LPI2C_MasterWaitFlag(base, LPI2C_MSR_RDF_MASK, timeout);
        if (status != LPI2C_STATUS_OK)
        {
            base->MTDR = LPI2C_MTDR_CMD(LPI2C_CMD_STOP);
            return status;
        }

        rxWord = base->MRDR;

        if ((rxWord & LPI2C_MRDR_RXEMPTY_MASK) != 0U)
        {
            return LPI2C_STATUS_ERROR;
        }

        rxData[i] = (uint8_t)(rxWord & LPI2C_MRDR_DATA_MASK);
    }

    status = LPI2C_MasterWaitFlag(base, LPI2C_MSR_TDF_MASK, timeout);
    if (status != LPI2C_STATUS_OK)
    {
        return status;
    }

    base->MTDR = LPI2C_MTDR_CMD(LPI2C_CMD_STOP);

    status = LPI2C_MasterWaitFlag(base, LPI2C_MSR_SDF_MASK, timeout);

    base->MSR = LPI2C_MASTER_CLEAR_FLAGS;

    return status;
}

/* ============================================================
 * Helpers
 * ============================================================ */

void LPI2C_MasterResetFIFO(LPI2C_Type *base)
{
    if (base == 0)
    {
        return;
    }

    base->MCR |= LPI2C_MCR_RTF_MASK | LPI2C_MCR_RRF_MASK;
}

void LPI2C_MasterClearFlags(LPI2C_Type *base)
{
    if (base == 0)
    {
        return;
    }

    base->MSR = LPI2C_MASTER_CLEAR_FLAGS;
}

uint32_t LPI2C_MasterGetStatusFlags(LPI2C_Type *base)
{
    if (base == 0)
    {
        return 0U;
    }

    return base->MSR;
}

bool LPI2C_MasterIsBusy(LPI2C_Type *base)
{
    if (base == 0)
    {
        return false;
    }

    return ((base->MSR & LPI2C_MSR_MBF_MASK) != 0U);
}
