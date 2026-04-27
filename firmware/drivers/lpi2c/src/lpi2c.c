#include "lpi2c.h"
#include "board_pins.h"

/* ============================================================
 * Internal definitions
 * ============================================================ */

/* Current EduFramework clock setup: SPLL_DIV2_CLK = 40 MHz */
#define LPI2C0_PCC_CLOCK_SOURCE        6U

#define LPI2C_CMD_TRANSMIT             0U
#define LPI2C_CMD_RECEIVE              1U
#define LPI2C_CMD_STOP                 2U
#define LPI2C_CMD_START                4U

#define LPI2C_MAX_READ_SIZE            256U

#define LPI2C_MASTER_CLEAR_FLAGS       (LPI2C_MSR_DMF_MASK  | \
                                        LPI2C_MSR_PLTF_MASK | \
                                        LPI2C_MSR_FEF_MASK  | \
                                        LPI2C_MSR_ALF_MASK  | \
                                        LPI2C_MSR_NDF_MASK  | \
                                        LPI2C_MSR_SDF_MASK  | \
                                        LPI2C_MSR_EPF_MASK)

#define LPI2C_MASTER_IT_ERROR_INTERRUPTS    (LPI2C_MIER_NDIE_MASK  | \
                                             LPI2C_MIER_ALIE_MASK  | \
                                             LPI2C_MIER_FEIE_MASK  | \
                                             LPI2C_MIER_PLTIE_MASK)

#define LPI2C_MASTER_IT_TRANSFER_INTERRUPTS (LPI2C_MIER_TDIE_MASK | \
                                             LPI2C_MIER_RDIE_MASK | \
                                             LPI2C_MIER_SDIE_MASK)

/* ============================================================
 * Internal state
 * ============================================================ */

static LPI2C_MasterHandle_t *s_lpi2c0MasterHandle = (LPI2C_MasterHandle_t *)0;

/* ============================================================
 * Internal helpers - common
 * ============================================================ */

static bool LPI2C_IsValid7BitAddress(uint8_t address)
{
    return (address <= 0x7FU);
}

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

static void LPI2C_MasterClearStatus(LPI2C_Type *base)
{
    base->MSR = LPI2C_MASTER_CLEAR_FLAGS;
}

static void LPI2C_MasterResetFifoInternal(LPI2C_Type *base)
{
    base->MCR |= LPI2C_MCR_RTF_MASK | LPI2C_MCR_RRF_MASK;
}

static void LPI2C_MasterPrepareTransfer(LPI2C_Type *base)
{
    LPI2C_MasterClearStatus(base);
    LPI2C_MasterResetFifoInternal(base);
}

static void LPI2C_MasterSendStart(LPI2C_Type *base,
                                  uint8_t slaveAddress,
                                  bool read)
{
    uint8_t addressByte;

    addressByte = (uint8_t)(slaveAddress << 1U);

    if (read)
    {
        addressByte |= 1U;
    }

    base->MTDR = LPI2C_MTDR_CMD(LPI2C_CMD_START) |
                 LPI2C_MTDR_DATA(addressByte);
}

static void LPI2C_MasterSendStop(LPI2C_Type *base)
{
    base->MTDR = LPI2C_MTDR_CMD(LPI2C_CMD_STOP);
}

static void LPI2C_MasterSendData(LPI2C_Type *base, uint8_t data)
{
    base->MTDR = LPI2C_MTDR_CMD(LPI2C_CMD_TRANSMIT) |
                 LPI2C_MTDR_DATA(data);
}

static void LPI2C_MasterReceiveCommand(LPI2C_Type *base, uint16_t size)
{
    base->MTDR = LPI2C_MTDR_CMD(LPI2C_CMD_RECEIVE) |
                 LPI2C_MTDR_DATA((uint8_t)(size - 1U));
}

/* ============================================================
 * Internal helpers - status / wait
 * ============================================================ */

static LPI2C_Status_t LPI2C_MasterCheckError(LPI2C_Type *base)
{
    uint32_t status;

    status = base->MSR;

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

static LPI2C_Status_t LPI2C_MasterWaitTxReady(LPI2C_Type *base,
                                              uint32_t timeout)
{
    return LPI2C_MasterWaitFlag(base, LPI2C_MSR_TDF_MASK, timeout);
}

static LPI2C_Status_t LPI2C_MasterWaitRxReady(LPI2C_Type *base,
                                              uint32_t timeout)
{
    return LPI2C_MasterWaitFlag(base, LPI2C_MSR_RDF_MASK, timeout);
}

static LPI2C_Status_t LPI2C_MasterWaitStop(LPI2C_Type *base,
                                           uint32_t timeout)
{
    return LPI2C_MasterWaitFlag(base, LPI2C_MSR_SDF_MASK, timeout);
}

/* ============================================================
 * Internal helpers - timing
 * ============================================================ */

static LPI2C_Status_t LPI2C_MasterSetTiming(LPI2C_Type *base,
                                            uint32_t baudRate)
{
    /*
     * Current project clock:
     * LPI2C functional clock = SPLL_DIV2_CLK = 40 MHz
     */

    if (baudRate == LPI2C_SPEED_STANDARD)
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

    if (baudRate == LPI2C_SPEED_FAST)
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

/* ============================================================
 * Internal helpers - validation
 * ============================================================ */

static LPI2C_Status_t LPI2C_MasterValidateCommon(LPI2C_Type *base,
                                                 uint8_t slaveAddress)
{
    if (base == 0)
    {
        return LPI2C_STATUS_INVALID_ARGUMENT;
    }

    if (!LPI2C_IsValid7BitAddress(slaveAddress))
    {
        return LPI2C_STATUS_INVALID_ARGUMENT;
    }

    if (LPI2C_MasterIsBusy(base))
    {
        return LPI2C_STATUS_BUSY;
    }

    return LPI2C_STATUS_OK;
}

static LPI2C_Status_t LPI2C_MasterValidateTransfer(const LPI2C_MasterTransfer_t *transfer)
{
    if (transfer == 0)
    {
        return LPI2C_STATUS_INVALID_ARGUMENT;
    }

    if (!LPI2C_IsValid7BitAddress(transfer->slaveAddress))
    {
        return LPI2C_STATUS_INVALID_ARGUMENT;
    }

    if (transfer->type == LPI2C_TRANSFER_WRITE)
    {
        if ((transfer->txData == 0) && (transfer->txSize > 0U))
        {
            return LPI2C_STATUS_INVALID_ARGUMENT;
        }

        return LPI2C_STATUS_OK;
    }

    if (transfer->type == LPI2C_TRANSFER_READ)
    {
        if ((transfer->rxData == 0) ||
            (transfer->rxSize == 0U) ||
            (transfer->rxSize > LPI2C_MAX_READ_SIZE))
        {
            return LPI2C_STATUS_INVALID_ARGUMENT;
        }

        return LPI2C_STATUS_OK;
    }

    if (transfer->type == LPI2C_TRANSFER_WRITE_READ)
    {
        if ((transfer->txData == 0) ||
            (transfer->txSize == 0U) ||
            (transfer->rxData == 0) ||
            (transfer->rxSize == 0U) ||
            (transfer->rxSize > LPI2C_MAX_READ_SIZE))
        {
            return LPI2C_STATUS_INVALID_ARGUMENT;
        }

        return LPI2C_STATUS_OK;
    }

    return LPI2C_STATUS_INVALID_ARGUMENT;
}

/* ============================================================
 * Master API - config
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
    uint32_t mcr;

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

    base->MCR = LPI2C_MCR_RST_MASK;
    base->MCR = 0U;

    base->MIER = 0U;
    base->MDER = 0U;

    LPI2C_MasterClearStatus(base);
    LPI2C_MasterResetFifoInternal(base);

    base->MFCR = LPI2C_MFCR_TXWATER(0U) |
                 LPI2C_MFCR_RXWATER(0U);

    if (LPI2C_MasterSetTiming(base, config->baudRate) != LPI2C_STATUS_OK)
    {
        return LPI2C_STATUS_INVALID_ARGUMENT;
    }

    mcr = 0U;

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

    base->MIER = 0U;
    base->MDER = 0U;
    base->MCR = 0U;

    if (base == IP_LPI2C0)
    {
        IP_PCC->PCCn[PCC_LPI2C0_INDEX] = 0U;
    }
}

/* ============================================================
 * Master API - blocking
 * ============================================================ */

LPI2C_Status_t LPI2C_MasterWriteBlocking(LPI2C_Type *base,
                                         uint8_t slaveAddress,
                                         const uint8_t *data,
                                         uint16_t size,
                                         uint32_t timeout)
{
    uint16_t i;
    LPI2C_Status_t status;

    status = LPI2C_MasterValidateCommon(base, slaveAddress);
    if (status != LPI2C_STATUS_OK)
    {
        return status;
    }

    if ((data == 0) && (size > 0U))
    {
        return LPI2C_STATUS_INVALID_ARGUMENT;
    }

    LPI2C_MasterPrepareTransfer(base);

    status = LPI2C_MasterWaitTxReady(base, timeout);
    if (status != LPI2C_STATUS_OK)
    {
        LPI2C_MasterClearStatus(base);
        return status;
    }

    LPI2C_MasterSendStart(base, slaveAddress, false);

    for (i = 0U; i < size; i++)
    {
        status = LPI2C_MasterWaitTxReady(base, timeout);
        if (status != LPI2C_STATUS_OK)
        {
            LPI2C_MasterSendStop(base);
            LPI2C_MasterClearStatus(base);
            return status;
        }

        LPI2C_MasterSendData(base, data[i]);
    }

    status = LPI2C_MasterWaitTxReady(base, timeout);
    if (status != LPI2C_STATUS_OK)
    {
        LPI2C_MasterClearStatus(base);
        return status;
    }

    LPI2C_MasterSendStop(base);

    status = LPI2C_MasterWaitStop(base, timeout);
    if (status != LPI2C_STATUS_OK)
    {
        LPI2C_MasterClearStatus(base);
        return status;
    }

    status = LPI2C_MasterCheckError(base);

    LPI2C_MasterClearStatus(base);

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
    LPI2C_Status_t status;

    status = LPI2C_MasterValidateCommon(base, slaveAddress);
    if (status != LPI2C_STATUS_OK)
    {
        return status;
    }

    if ((data == 0) || (size == 0U) || (size > LPI2C_MAX_READ_SIZE))
    {
        return LPI2C_STATUS_INVALID_ARGUMENT;
    }

    LPI2C_MasterPrepareTransfer(base);

    status = LPI2C_MasterWaitTxReady(base, timeout);
    if (status != LPI2C_STATUS_OK)
    {
        LPI2C_MasterClearStatus(base);
        return status;
    }

    LPI2C_MasterSendStart(base, slaveAddress, true);

    status = LPI2C_MasterWaitTxReady(base, timeout);
    if (status != LPI2C_STATUS_OK)
    {
        LPI2C_MasterSendStop(base);
        LPI2C_MasterClearStatus(base);
        return status;
    }

    LPI2C_MasterReceiveCommand(base, size);

    for (i = 0U; i < size; i++)
    {
        status = LPI2C_MasterWaitRxReady(base, timeout);
        if (status != LPI2C_STATUS_OK)
        {
            LPI2C_MasterSendStop(base);
            LPI2C_MasterClearStatus(base);
            return status;
        }

        rxWord = base->MRDR;

        if ((rxWord & LPI2C_MRDR_RXEMPTY_MASK) != 0U)
        {
            LPI2C_MasterClearStatus(base);
            return LPI2C_STATUS_ERROR;
        }

        data[i] = (uint8_t)(rxWord & LPI2C_MRDR_DATA_MASK);
    }

    status = LPI2C_MasterWaitTxReady(base, timeout);
    if (status != LPI2C_STATUS_OK)
    {
        LPI2C_MasterClearStatus(base);
        return status;
    }

    LPI2C_MasterSendStop(base);

    status = LPI2C_MasterWaitStop(base, timeout);

    LPI2C_MasterClearStatus(base);

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
    LPI2C_Status_t status;

    status = LPI2C_MasterValidateCommon(base, slaveAddress);
    if (status != LPI2C_STATUS_OK)
    {
        return status;
    }

    if ((txData == 0) ||
        (txSize == 0U) ||
        (rxData == 0) ||
        (rxSize == 0U) ||
        (rxSize > LPI2C_MAX_READ_SIZE))
    {
        return LPI2C_STATUS_INVALID_ARGUMENT;
    }

    LPI2C_MasterPrepareTransfer(base);

    status = LPI2C_MasterWaitTxReady(base, timeout);
    if (status != LPI2C_STATUS_OK)
    {
        LPI2C_MasterClearStatus(base);
        return status;
    }

    LPI2C_MasterSendStart(base, slaveAddress, false);

    for (i = 0U; i < txSize; i++)
    {
        status = LPI2C_MasterWaitTxReady(base, timeout);
        if (status != LPI2C_STATUS_OK)
        {
            LPI2C_MasterSendStop(base);
            LPI2C_MasterClearStatus(base);
            return status;
        }

        LPI2C_MasterSendData(base, txData[i]);
    }

    status = LPI2C_MasterWaitTxReady(base, timeout);
    if (status != LPI2C_STATUS_OK)
    {
        LPI2C_MasterSendStop(base);
        LPI2C_MasterClearStatus(base);
        return status;
    }

    LPI2C_MasterSendStart(base, slaveAddress, true);

    status = LPI2C_MasterWaitTxReady(base, timeout);
    if (status != LPI2C_STATUS_OK)
    {
        LPI2C_MasterSendStop(base);
        LPI2C_MasterClearStatus(base);
        return status;
    }

    LPI2C_MasterReceiveCommand(base, rxSize);

    for (i = 0U; i < rxSize; i++)
    {
        status = LPI2C_MasterWaitRxReady(base, timeout);
        if (status != LPI2C_STATUS_OK)
        {
            LPI2C_MasterSendStop(base);
            LPI2C_MasterClearStatus(base);
            return status;
        }

        rxWord = base->MRDR;

        if ((rxWord & LPI2C_MRDR_RXEMPTY_MASK) != 0U)
        {
            LPI2C_MasterClearStatus(base);
            return LPI2C_STATUS_ERROR;
        }

        rxData[i] = (uint8_t)(rxWord & LPI2C_MRDR_DATA_MASK);
    }

    status = LPI2C_MasterWaitTxReady(base, timeout);
    if (status != LPI2C_STATUS_OK)
    {
        LPI2C_MasterClearStatus(base);
        return status;
    }

    LPI2C_MasterSendStop(base);

    status = LPI2C_MasterWaitStop(base, timeout);

    LPI2C_MasterClearStatus(base);

    return status;
}

LPI2C_Status_t LPI2C_MasterTransferBlocking(LPI2C_Type *base,
                                            const LPI2C_MasterTransfer_t *transfer,
                                            uint32_t timeout)
{
    LPI2C_Status_t status;

    if (base == 0)
    {
        return LPI2C_STATUS_INVALID_ARGUMENT;
    }

    status = LPI2C_MasterValidateTransfer(transfer);
    if (status != LPI2C_STATUS_OK)
    {
        return status;
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
 * Master API - interrupt
 * ============================================================ */

static void LPI2C_MasterIT_Complete(LPI2C_Type *base,
                                    LPI2C_MasterHandle_t *handle)
{
    base->MIER = 0U;

    LPI2C_MasterClearStatus(base);

    handle->status = LPI2C_STATUS_OK;
    handle->state = LPI2C_MASTER_STATE_DONE;
}

static void LPI2C_MasterIT_Error(LPI2C_Type *base,
                                 LPI2C_MasterHandle_t *handle,
                                 LPI2C_Status_t status)
{
    base->MIER = 0U;

    LPI2C_MasterSendStop(base);
    LPI2C_MasterClearStatus(base);

    handle->status = status;
    handle->state = LPI2C_MASTER_STATE_ERROR;
}

LPI2C_Status_t LPI2C_MasterTransferIT(LPI2C_Type *base,
                                      LPI2C_MasterHandle_t *handle,
                                      const LPI2C_MasterTransfer_t *transfer)
{
    LPI2C_Status_t status;

    if ((base == 0) || (handle == 0))
    {
        return LPI2C_STATUS_INVALID_ARGUMENT;
    }

    if (base != IP_LPI2C0)
    {
        return LPI2C_STATUS_INVALID_ARGUMENT;
    }

    if (LPI2C_MasterIsBusy(base))
    {
        return LPI2C_STATUS_BUSY;
    }

    status = LPI2C_MasterValidateTransfer(transfer);
    if (status != LPI2C_STATUS_OK)
    {
        return status;
    }

    handle->transfer = *transfer;
    handle->state = LPI2C_MASTER_STATE_START;
    handle->status = LPI2C_STATUS_BUSY;
    handle->txCount = 0U;
    handle->rxCount = 0U;
    handle->rxCommandSent = false;

    s_lpi2c0MasterHandle = handle;

    LPI2C_MasterPrepareTransfer(base);

    base->MIER = LPI2C_MASTER_IT_ERROR_INTERRUPTS |
                 LPI2C_MASTER_IT_TRANSFER_INTERRUPTS;

    return LPI2C_STATUS_OK;
}

void LPI2C_MasterIRQHandler(LPI2C_Type *base)
{
    LPI2C_MasterHandle_t *handle;
    LPI2C_Status_t errorStatus;
    uint32_t status;
    uint32_t rxWord;

    if (base != IP_LPI2C0)
    {
        return;
    }

    handle = s_lpi2c0MasterHandle;

    if (handle == 0)
    {
        base->MIER = 0U;
        LPI2C_MasterClearStatus(base);
        return;
    }

    status = base->MSR;

    errorStatus = LPI2C_MasterCheckError(base);
    if (errorStatus != LPI2C_STATUS_OK)
    {
        LPI2C_MasterIT_Error(base, handle, errorStatus);
        return;
    }

    if ((status & LPI2C_MSR_RDF_MASK) != 0U)
    {
        if (handle->state == LPI2C_MASTER_STATE_RECEIVE)
        {
            rxWord = base->MRDR;

            if ((rxWord & LPI2C_MRDR_RXEMPTY_MASK) == 0U)
            {
                if (handle->rxCount < handle->transfer.rxSize)
                {
                    handle->transfer.rxData[handle->rxCount] =
                        (uint8_t)(rxWord & LPI2C_MRDR_DATA_MASK);

                    handle->rxCount++;
                }
            }

            if (handle->rxCount >= handle->transfer.rxSize)
            {
                handle->state = LPI2C_MASTER_STATE_STOP;
                LPI2C_MasterSendStop(base);
            }
        }
    }

    if ((status & LPI2C_MSR_TDF_MASK) != 0U)
    {
        switch (handle->state)
        {
            case LPI2C_MASTER_STATE_START:
            {
                if (handle->transfer.type == LPI2C_TRANSFER_READ)
                {
                    LPI2C_MasterSendStart(base,
                                          handle->transfer.slaveAddress,
                                          true);

                    handle->state = LPI2C_MASTER_STATE_RECEIVE;
                    handle->rxCommandSent = false;
                }
                else
                {
                    LPI2C_MasterSendStart(base,
                                          handle->transfer.slaveAddress,
                                          false);

                    handle->state = LPI2C_MASTER_STATE_SEND;
                }

                break;
            }

            case LPI2C_MASTER_STATE_SEND:
            {
                if (handle->txCount < handle->transfer.txSize)
                {
                    LPI2C_MasterSendData(base,
                                         handle->transfer.txData[handle->txCount]);

                    handle->txCount++;
                }
                else
                {
                    if (handle->transfer.type == LPI2C_TRANSFER_WRITE)
                    {
                        handle->state = LPI2C_MASTER_STATE_STOP;
                        LPI2C_MasterSendStop(base);
                    }
                    else
                    {
                        LPI2C_MasterSendStart(base,
                                              handle->transfer.slaveAddress,
                                              true);

                        handle->state = LPI2C_MASTER_STATE_RECEIVE;
                        handle->rxCommandSent = false;
                    }
                }

                break;
            }

            case LPI2C_MASTER_STATE_RECEIVE:
            {
                if (handle->rxCommandSent == false)
                {
                    LPI2C_MasterReceiveCommand(base, handle->transfer.rxSize);
                    handle->rxCommandSent = true;
                }

                break;
            }

            case LPI2C_MASTER_STATE_STOP:
            case LPI2C_MASTER_STATE_DONE:
            case LPI2C_MASTER_STATE_ERROR:
            case LPI2C_MASTER_STATE_IDLE:
            default:
            {
                break;
            }
        }
    }

    if ((base->MSR & LPI2C_MSR_SDF_MASK) != 0U)
    {
        if (handle->state == LPI2C_MASTER_STATE_STOP)
        {
            LPI2C_MasterIT_Complete(base, handle);
        }
        else
        {
            base->MSR = LPI2C_MSR_SDF_MASK;
        }
    }
}

LPI2C_MasterState_t LPI2C_MasterGetState(const LPI2C_MasterHandle_t *handle)
{
    if (handle == 0)
    {
        return LPI2C_MASTER_STATE_ERROR;
    }

    return handle->state;
}

LPI2C_Status_t LPI2C_MasterGetStatus(const LPI2C_MasterHandle_t *handle)
{
    if (handle == 0)
    {
        return LPI2C_STATUS_INVALID_ARGUMENT;
    }

    return handle->status;
}

/* ============================================================
 * Master helper API
 * ============================================================ */

void LPI2C_MasterResetFIFO(LPI2C_Type *base)
{
    if (base == 0)
    {
        return;
    }

    LPI2C_MasterResetFifoInternal(base);
}

void LPI2C_MasterClearFlags(LPI2C_Type *base)
{
    if (base == 0)
    {
        return;
    }

    LPI2C_MasterClearStatus(base);
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

/* ============================================================
 * Slave API - placeholder for Phase 4
 * ============================================================ */

void LPI2C_SlaveGetDefaultConfig(LPI2C_SlaveConfig_t *config)
{
    if (config == 0)
    {
        return;
    }

    config->slaveAddress = 0x10U;
    config->enableGeneralCall = false;
    config->enableClockStretching = true;
    config->enableFilter = true;
    config->callback = (LPI2C_SlaveCallback_t)0;
    config->userData = 0;
}

LPI2C_Status_t LPI2C_SlaveInit(LPI2C_Type *base,
                               const LPI2C_SlaveConfig_t *config)
{
    (void)base;
    (void)config;

    return LPI2C_STATUS_ERROR;
}

void LPI2C_SlaveDeinit(LPI2C_Type *base)
{
    (void)base;
}

void LPI2C_SlaveIRQHandler(LPI2C_Type *base)
{
    (void)base;
}

LPI2C_Status_t LPI2C_SlaveWriteByte(LPI2C_Type *base,
                                    uint8_t data)
{
    (void)base;
    (void)data;

    return LPI2C_STATUS_ERROR;
}

LPI2C_Status_t LPI2C_SlaveReadByte(LPI2C_Type *base,
                                   uint8_t *data)
{
    (void)base;
    (void)data;

    return LPI2C_STATUS_ERROR;
}
