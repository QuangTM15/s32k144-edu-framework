#include "lpi2c.h"
#include <stddef.h>

/**
 * @file lpi2c.c
 * @brief Low-level LPI2C driver implementation for EduFramework_v2.
 *
 * @details
 * This driver configures and controls only the S32K144 LPI2C peripheral.
 *
 * Important design rule:
 * - This file must not configure board pins.
 * - This file must not include board_pins.h.
 * - SDA/SCL mux configuration must be done by the board layer or Wire layer.
 */

/* ============================================================
 * Internal Definitions
 * ============================================================ */

#define LPI2C0_PCC_CLOCK_SOURCE                 6U

#define LPI2C_DEFAULT_SOURCE_CLOCK_HZ           40000000U
#define LPI2C_DEFAULT_TIMEOUT                   1000000U

#define LPI2C_CMD_TRANSMIT                      0U
#define LPI2C_CMD_RECEIVE                       1U
#define LPI2C_CMD_STOP                          2U
#define LPI2C_CMD_START                         4U

#define LPI2C_MAX_READ_SIZE                     256U
#define LPI2C_7BIT_ADDRESS_MAX                  0x7FU

#define LPI2C_MASTER_CLEAR_FLAGS                (LPI2C_MSR_DMF_MASK  | \
                                                 LPI2C_MSR_PLTF_MASK | \
                                                 LPI2C_MSR_FEF_MASK  | \
                                                 LPI2C_MSR_ALF_MASK  | \
                                                 LPI2C_MSR_NDF_MASK  | \
                                                 LPI2C_MSR_SDF_MASK  | \
                                                 LPI2C_MSR_EPF_MASK)

#define LPI2C_MASTER_IT_ERROR_INTERRUPTS        (LPI2C_MIER_NDIE_MASK  | \
                                                 LPI2C_MIER_ALIE_MASK  | \
                                                 LPI2C_MIER_FEIE_MASK  | \
                                                 LPI2C_MIER_PLTIE_MASK)

#define LPI2C_MASTER_IT_TRANSFER_INTERRUPTS     (LPI2C_MIER_TDIE_MASK | \
                                                 LPI2C_MIER_RDIE_MASK | \
                                                 LPI2C_MIER_SDIE_MASK)

#define LPI2C_SLAVE_CLEAR_FLAGS                 (LPI2C_SSR_SARF_MASK | \
                                                 LPI2C_SSR_GCF_MASK  | \
                                                 LPI2C_SSR_AM1F_MASK | \
                                                 LPI2C_SSR_AM0F_MASK | \
                                                 LPI2C_SSR_FEF_MASK  | \
                                                 LPI2C_SSR_BEF_MASK  | \
                                                 LPI2C_SSR_SDF_MASK  | \
                                                 LPI2C_SSR_RSF_MASK)

#define LPI2C_SLAVE_IT_INTERRUPTS               (LPI2C_SIER_AVIE_MASK  | \
                                                 LPI2C_SIER_RDIE_MASK  | \
                                                 LPI2C_SIER_TDIE_MASK  | \
                                                 LPI2C_SIER_SDIE_MASK  | \
                                                 LPI2C_SIER_RSIE_MASK  | \
                                                 LPI2C_SIER_BEIE_MASK  | \
                                                 LPI2C_SIER_FEIE_MASK  | \
                                                 LPI2C_SIER_AM0IE_MASK)

/* ============================================================
 * Internal State
 * ============================================================ */

static LPI2C_MasterHandle_t *s_pLpi2c0MasterHandle = NULL;
static LPI2C_SlaveConfig_t s_Lpi2c0SlaveConfig;

/* ============================================================
 * Internal Helpers - Common
 * ============================================================ */

/**
 * @brief Check whether the given LPI2C base is supported by this driver.
 *
 * @details
 * Current EduFramework_v2 only enables LPI2C0.
 *
 * @param base LPI2C peripheral base address.
 *
 * @retval true Base address is supported.
 * @retval false Base address is invalid or unsupported.
 */
static bool LPI2C_IsValidBase(const LPI2C_Type *base)
{
    bool isValid = false;

    if (IP_LPI2C0 == base)
    {
        isValid = true;
    }

    return isValid;
}

/**
 * @brief Check whether an address is valid for 7-bit I2C addressing.
 *
 * @param address I2C slave address.
 *
 * @retval true Address is within 7-bit range.
 * @retval false Address is outside 7-bit range.
 */
static bool LPI2C_IsValid7BitAddress(uint8_t address)
{
    bool isValid = false;

    if (address <= LPI2C_7BIT_ADDRESS_MAX)
    {
        isValid = true;
    }

    return isValid;
}

/**
 * @brief Enable PCC clock for the selected LPI2C instance.
 *
 * @details
 * This function only enables the peripheral clock. It does not configure
 * SDA/SCL pins.
 *
 * @param base LPI2C peripheral base address.
 */
static void LPI2C_EnableClock(LPI2C_Type *base)
{
    if (IP_LPI2C0 == base)
    {
        IP_PCC->PCCn[PCC_LPI2C0_INDEX] = 0U;
        IP_PCC->PCCn[PCC_LPI2C0_INDEX] =
            PCC_PCCn_PCS(LPI2C0_PCC_CLOCK_SOURCE) |
            PCC_PCCn_CGC_MASK;
    }
}

/**
 * @brief Disable PCC clock for the selected LPI2C instance.
 *
 * @param base LPI2C peripheral base address.
 */
static void LPI2C_DisableClock(LPI2C_Type *base)
{
    if (IP_LPI2C0 == base)
    {
        IP_PCC->PCCn[PCC_LPI2C0_INDEX] = 0U;
    }
}

/* ============================================================
 * Internal Helpers - Master
 * ============================================================ */

/**
 * @brief Clear master status flags.
 *
 * @param base LPI2C peripheral base address.
 */
static void LPI2C_MasterClearStatus(LPI2C_Type *base)
{
    base->MSR = LPI2C_MASTER_CLEAR_FLAGS;
}

/**
 * @brief Reset master TX and RX FIFOs.
 *
 * @param base LPI2C peripheral base address.
 */
static void LPI2C_MasterResetFifoInternal(LPI2C_Type *base)
{
    base->MCR |= LPI2C_MCR_RTF_MASK | LPI2C_MCR_RRF_MASK;
}

/**
 * @brief Prepare master peripheral before starting a transfer.
 *
 * @details
 * This helper clears old status flags and flushes FIFOs to avoid stale data
 * from a previous transaction.
 *
 * @param base LPI2C peripheral base address.
 */
static void LPI2C_MasterPrepareTransfer(LPI2C_Type *base)
{
    LPI2C_MasterClearStatus(base);
    LPI2C_MasterResetFifoInternal(base);
}

/**
 * @brief Push START command and address byte to master command FIFO.
 *
 * @param base LPI2C peripheral base address.
 * @param slaveAddress 7-bit slave address.
 * @param read true for read direction, false for write direction.
 */
static void LPI2C_MasterSendStart(LPI2C_Type *base,
                                  uint8_t slaveAddress,
                                  bool read)
{
    uint8_t addressByte = 0U;

    addressByte = (uint8_t)(slaveAddress << 1U);

    if (true == read)
    {
        addressByte |= 1U;
    }

    base->MTDR = LPI2C_MTDR_CMD(LPI2C_CMD_START) |
                 LPI2C_MTDR_DATA(addressByte);
}

/**
 * @brief Push STOP command to master command FIFO.
 *
 * @param base LPI2C peripheral base address.
 */
static void LPI2C_MasterSendStop(LPI2C_Type *base)
{
    base->MTDR = LPI2C_MTDR_CMD(LPI2C_CMD_STOP);
}

/**
 * @brief Push one transmit byte to master command FIFO.
 *
 * @param base LPI2C peripheral base address.
 * @param data Data byte.
 */
static void LPI2C_MasterSendData(LPI2C_Type *base,
                                 uint8_t data)
{
    base->MTDR = LPI2C_MTDR_CMD(LPI2C_CMD_TRANSMIT) |
                 LPI2C_MTDR_DATA(data);
}

/**
 * @brief Push receive command to master command FIFO.
 *
 * @details
 * LPI2C receive command encodes number of bytes as size - 1.
 *
 * @param base LPI2C peripheral base address.
 * @param size Number of bytes to receive.
 */
static void LPI2C_MasterReceiveCommand(LPI2C_Type *base,
                                       uint16_t size)
{
    base->MTDR = LPI2C_MTDR_CMD(LPI2C_CMD_RECEIVE) |
                 LPI2C_MTDR_DATA((uint8_t)(size - 1U));
}

/**
 * @brief Check master error flags.
 *
 * @param base LPI2C peripheral base address.
 *
 * @return LPI2C status code.
 */
static LPI2C_Status_t LPI2C_MasterCheckError(LPI2C_Type *base)
{
    LPI2C_Status_t result = LPI2C_STATUS_OK;
    uint32_t status = 0U;

    status = base->MSR;

    if (0U != (status & LPI2C_MSR_NDF_MASK))
    {
        result = LPI2C_STATUS_NACK;
    }
    else if (0U != (status & LPI2C_MSR_ALF_MASK))
    {
        result = LPI2C_STATUS_ARBITRATION_LOST;
    }
    else if (0U != (status & LPI2C_MSR_FEF_MASK))
    {
        result = LPI2C_STATUS_FIFO_ERROR;
    }
    else if (0U != (status & LPI2C_MSR_PLTF_MASK))
    {
        result = LPI2C_STATUS_PIN_LOW_TIMEOUT;
    }
    else
    {
        result = LPI2C_STATUS_OK;
    }

    return result;
}

/**
 * @brief Wait until a master status flag is set.
 *
 * @details
 * The loop exits when:
 * - Requested flag is set
 * - An error flag is detected
 * - Timeout reaches zero
 *
 * @param base LPI2C peripheral base address.
 * @param flag Status flag mask to wait for.
 * @param timeout Timeout counter.
 *
 * @return LPI2C status code.
 */
static LPI2C_Status_t LPI2C_MasterWaitFlag(LPI2C_Type *base,
                                           uint32_t flag,
                                           uint32_t timeout)
{
    LPI2C_Status_t result = LPI2C_STATUS_OK;
    LPI2C_Status_t errorStatus = LPI2C_STATUS_OK;
    bool done = false;

    while (false == done)
    {
        if (0U != (base->MSR & flag))
        {
            result = LPI2C_STATUS_OK;
            done = true;
        }
        else
        {
            errorStatus = LPI2C_MasterCheckError(base);

            if (LPI2C_STATUS_OK != errorStatus)
            {
                result = errorStatus;
                done = true;
            }
            else if (0U == timeout)
            {
                result = LPI2C_STATUS_TIMEOUT;
                done = true;
            }
            else
            {
                timeout--;
            }
        }
    }

    return result;
}

/**
 * @brief Wait until master TX FIFO can accept a command.
 *
 * @param base LPI2C peripheral base address.
 * @param timeout Timeout counter.
 *
 * @return LPI2C status code.
 */
static LPI2C_Status_t LPI2C_MasterWaitTxReady(LPI2C_Type *base,
                                              uint32_t timeout)
{
    return LPI2C_MasterWaitFlag(base, LPI2C_MSR_TDF_MASK, timeout);
}

/**
 * @brief Wait until master RX FIFO has data.
 *
 * @param base LPI2C peripheral base address.
 * @param timeout Timeout counter.
 *
 * @return LPI2C status code.
 */
static LPI2C_Status_t LPI2C_MasterWaitRxReady(LPI2C_Type *base,
                                              uint32_t timeout)
{
    return LPI2C_MasterWaitFlag(base, LPI2C_MSR_RDF_MASK, timeout);
}

/**
 * @brief Wait until STOP condition is detected.
 *
 * @param base LPI2C peripheral base address.
 * @param timeout Timeout counter.
 *
 * @return LPI2C status code.
 */
static LPI2C_Status_t LPI2C_MasterWaitStop(LPI2C_Type *base,
                                           uint32_t timeout)
{
    return LPI2C_MasterWaitFlag(base, LPI2C_MSR_SDF_MASK, timeout);
}

/**
 * @brief Configure LPI2C master timing.
 *
 * @details
 * Current project setup assumes:
 * - LPI2C functional clock = SPLL_DIV2_CLK = 40 MHz
 *
 * Supported speeds:
 * - 100 kHz
 * - 400 kHz
 *
 * @param base LPI2C peripheral base address.
 * @param baudRate Target I2C baud rate.
 *
 * @return LPI2C status code.
 */
static LPI2C_Status_t LPI2C_MasterSetTiming(LPI2C_Type *base,
                                            uint32_t baudRate)
{
    LPI2C_Status_t result = LPI2C_STATUS_OK;

    if (LPI2C_SPEED_STANDARD == baudRate)
    {
        base->MCFGR1 = LPI2C_MCFGR1_PINCFG(0U) |
                       LPI2C_MCFGR1_PRESCALE(3U);

        base->MCFGR2 = LPI2C_MCFGR2_FILTSCL(1U) |
                       LPI2C_MCFGR2_FILTSDA(1U);

        base->MCCR0 = LPI2C_MCCR0_CLKLO(28U)   |
                      LPI2C_MCCR0_CLKHI(20U)   |
                      LPI2C_MCCR0_SETHOLD(20U) |
                      LPI2C_MCCR0_DATAVD(4U);

        result = LPI2C_STATUS_OK;
    }
    else if (LPI2C_SPEED_FAST == baudRate)
    {
        base->MCFGR1 = LPI2C_MCFGR1_PINCFG(0U) |
                       LPI2C_MCFGR1_PRESCALE(1U);

        base->MCFGR2 = LPI2C_MCFGR2_FILTSCL(1U) |
                       LPI2C_MCFGR2_FILTSDA(1U);

        base->MCCR0 = LPI2C_MCCR0_CLKLO(31U)   |
                      LPI2C_MCCR0_CLKHI(21U)   |
                      LPI2C_MCCR0_SETHOLD(15U) |
                      LPI2C_MCCR0_DATAVD(4U);

        result = LPI2C_STATUS_OK;
    }
    else
    {
        result = LPI2C_STATUS_INVALID_ARGUMENT;
    }

    return result;
}

/**
 * @brief Validate common master transfer parameters.
 *
 * @param base LPI2C peripheral base address.
 * @param slaveAddress 7-bit slave address.
 *
 * @return LPI2C status code.
 */
static LPI2C_Status_t LPI2C_MasterValidateCommon(LPI2C_Type *base,
                                                 uint8_t slaveAddress)
{
    LPI2C_Status_t result = LPI2C_STATUS_OK;

    if (false == LPI2C_IsValidBase(base))
    {
        result = LPI2C_STATUS_INVALID_ARGUMENT;
    }
    else if (false == LPI2C_IsValid7BitAddress(slaveAddress))
    {
        result = LPI2C_STATUS_INVALID_ARGUMENT;
    }
    else if (true == LPI2C_MasterIsBusy(base))
    {
        result = LPI2C_STATUS_BUSY;
    }
    else
    {
        result = LPI2C_STATUS_OK;
    }

    return result;
}

/**
 * @brief Validate a master transfer descriptor.
 *
 * @param transfer Pointer to transfer descriptor.
 *
 * @return LPI2C status code.
 */
static LPI2C_Status_t LPI2C_MasterValidateTransfer(const LPI2C_MasterTransfer_t *transfer)
{
    LPI2C_Status_t result = LPI2C_STATUS_OK;

    if (NULL == transfer)
    {
        result = LPI2C_STATUS_INVALID_ARGUMENT;
    }
    else if (false == LPI2C_IsValid7BitAddress(transfer->slaveAddress))
    {
        result = LPI2C_STATUS_INVALID_ARGUMENT;
    }
    else if (LPI2C_TRANSFER_WRITE == transfer->type)
    {
        if ((NULL == transfer->txData) && (0U < transfer->txSize))
        {
            result = LPI2C_STATUS_INVALID_ARGUMENT;
        }
    }
    else if (LPI2C_TRANSFER_READ == transfer->type)
    {
        if ((NULL == transfer->rxData) ||
            (0U == transfer->rxSize) ||
            (LPI2C_MAX_READ_SIZE < transfer->rxSize))
        {
            result = LPI2C_STATUS_INVALID_ARGUMENT;
        }
    }
    else if (LPI2C_TRANSFER_WRITE_READ == transfer->type)
    {
        if ((NULL == transfer->txData) ||
            (0U == transfer->txSize) ||
            (NULL == transfer->rxData) ||
            (0U == transfer->rxSize) ||
            (LPI2C_MAX_READ_SIZE < transfer->rxSize))
        {
            result = LPI2C_STATUS_INVALID_ARGUMENT;
        }
    }
    else
    {
        result = LPI2C_STATUS_INVALID_ARGUMENT;
    }

    return result;
}

/* ============================================================
 * Master API - Config
 * ============================================================ */

/**
 * @brief Load default master configuration.
 *
 * @details
 * Default configuration:
 * - Source clock: 40 MHz
 * - Baud rate: 100 kHz
 * - Debug mode enabled
 * - Doze mode disabled
 * - Timeout: 1000000
 *
 * @param config Pointer to master configuration structure.
 */
void LPI2C_MasterGetDefaultConfig(LPI2C_MasterConfig_t *config)
{
    if (NULL != config)
    {
        config->srcClockHz  = LPI2C_DEFAULT_SOURCE_CLOCK_HZ;
        config->baudRate    = LPI2C_SPEED_STANDARD;
        config->enableDebug = true;
        config->enableDoze  = false;
        config->timeout     = LPI2C_DEFAULT_TIMEOUT;
    }
}

/**
 * @brief Initialize LPI2C peripheral in master mode.
 *
 * @details
 * This function:
 * - Enables LPI2C peripheral clock
 * - Resets master logic
 * - Clears status flags
 * - Resets TX/RX FIFOs
 * - Configures FIFO watermark
 * - Configures master timing
 * - Enables master mode
 *
 * Pin muxing is not configured here.
 *
 * @param base LPI2C peripheral base address.
 * @param config Pointer to master configuration.
 *
 * @return LPI2C status code.
 */
LPI2C_Status_t LPI2C_MasterInit(LPI2C_Type *base,
                                const LPI2C_MasterConfig_t *config)
{
    LPI2C_Status_t result = LPI2C_STATUS_OK;
    uint32_t mcr = 0U;

    if ((false == LPI2C_IsValidBase(base)) || (NULL == config))
    {
        result = LPI2C_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        LPI2C_EnableClock(base);

        base->MCR = LPI2C_MCR_RST_MASK;
        base->MCR = 0U;

        base->MIER = 0U;
        base->MDER = 0U;

        LPI2C_MasterClearStatus(base);
        LPI2C_MasterResetFifoInternal(base);

        base->MFCR = LPI2C_MFCR_TXWATER(0U) |
                     LPI2C_MFCR_RXWATER(0U);

        result = LPI2C_MasterSetTiming(base, config->baudRate);

        if (LPI2C_STATUS_OK == result)
        {
            mcr = 0U;

            if (true == config->enableDebug)
            {
                mcr |= LPI2C_MCR_DBGEN_MASK;
            }

            if (true == config->enableDoze)
            {
                mcr |= LPI2C_MCR_DOZEN_MASK;
            }

            base->MCR = mcr | LPI2C_MCR_MEN_MASK;
        }
    }

    return result;
}

/**
 * @brief Deinitialize LPI2C master mode.
 *
 * @param base LPI2C peripheral base address.
 */
void LPI2C_MasterDeinit(LPI2C_Type *base)
{
    if (true == LPI2C_IsValidBase(base))
    {
        base->MIER = 0U;
        base->MDER = 0U;
        base->MCR = 0U;

        LPI2C_DisableClock(base);

        if (IP_LPI2C0 == base)
        {
            s_pLpi2c0MasterHandle = NULL;
        }
    }
}

/* ============================================================
 * Master API - Blocking
 * ============================================================ */

/**
 * @brief Write data to an I2C slave using blocking transfer.
 *
 * @details
 * Bus sequence:
 * START -> ADDRESS + W -> DATA bytes -> STOP
 *
 * @param base LPI2C peripheral base address.
 * @param slaveAddress 7-bit slave address.
 * @param data Pointer to transmit buffer.
 * @param size Number of bytes to transmit.
 * @param timeout Timeout counter.
 *
 * @return LPI2C status code.
 */
LPI2C_Status_t LPI2C_MasterWriteBlocking(LPI2C_Type *base,
                                         uint8_t slaveAddress,
                                         const uint8_t *data,
                                         uint16_t size,
                                         uint32_t timeout)
{
    LPI2C_Status_t result = LPI2C_STATUS_OK;
    uint16_t index = 0U;

    result = LPI2C_MasterValidateCommon(base, slaveAddress);

    if (LPI2C_STATUS_OK == result)
    {
        if ((NULL == data) && (0U < size))
        {
            result = LPI2C_STATUS_INVALID_ARGUMENT;
        }
    }

    if (LPI2C_STATUS_OK == result)
    {
        LPI2C_MasterPrepareTransfer(base);

        result = LPI2C_MasterWaitTxReady(base, timeout);
    }

    if (LPI2C_STATUS_OK == result)
    {
        LPI2C_MasterSendStart(base, slaveAddress, false);

        for (index = 0U; index < size; index++)
        {
            result = LPI2C_MasterWaitTxReady(base, timeout);

            if (LPI2C_STATUS_OK != result)
            {
                break;
            }

            LPI2C_MasterSendData(base, data[index]);
        }
    }

    if (LPI2C_STATUS_OK == result)
    {
        result = LPI2C_MasterWaitTxReady(base, timeout);
    }

    if (LPI2C_STATUS_OK == result)
    {
        LPI2C_MasterSendStop(base);
        result = LPI2C_MasterWaitStop(base, timeout);
    }
    else if (true == LPI2C_IsValidBase(base))
    {
        LPI2C_MasterSendStop(base);
    }
    else
    {
        /* Invalid base: nothing to stop. */
    }

    if (true == LPI2C_IsValidBase(base))
    {
        if (LPI2C_STATUS_OK == result)
        {
            result = LPI2C_MasterCheckError(base);
        }

        LPI2C_MasterClearStatus(base);
    }

    return result;
}

/**
 * @brief Read data from an I2C slave using blocking transfer.
 *
 * @details
 * Bus sequence:
 * START -> ADDRESS + R -> RECEIVE command -> DATA bytes -> STOP
 *
 * @param base LPI2C peripheral base address.
 * @param slaveAddress 7-bit slave address.
 * @param data Pointer to receive buffer.
 * @param size Number of bytes to receive.
 * @param timeout Timeout counter.
 *
 * @return LPI2C status code.
 */
LPI2C_Status_t LPI2C_MasterReadBlocking(LPI2C_Type *base,
                                        uint8_t slaveAddress,
                                        uint8_t *data,
                                        uint16_t size,
                                        uint32_t timeout)
{
    LPI2C_Status_t result = LPI2C_STATUS_OK;
    uint16_t index = 0U;
    uint32_t rxWord = 0U;

    result = LPI2C_MasterValidateCommon(base, slaveAddress);

    if (LPI2C_STATUS_OK == result)
    {
        if ((NULL == data) ||
            (0U == size) ||
            (LPI2C_MAX_READ_SIZE < size))
        {
            result = LPI2C_STATUS_INVALID_ARGUMENT;
        }
    }

    if (LPI2C_STATUS_OK == result)
    {
        LPI2C_MasterPrepareTransfer(base);

        result = LPI2C_MasterWaitTxReady(base, timeout);
    }

    if (LPI2C_STATUS_OK == result)
    {
        LPI2C_MasterSendStart(base, slaveAddress, true);

        result = LPI2C_MasterWaitTxReady(base, timeout);
    }

    if (LPI2C_STATUS_OK == result)
    {
        LPI2C_MasterReceiveCommand(base, size);

        for (index = 0U; index < size; index++)
        {
            result = LPI2C_MasterWaitRxReady(base, timeout);

            if (LPI2C_STATUS_OK != result)
            {
                break;
            }

            rxWord = base->MRDR;

            if (0U != (rxWord & LPI2C_MRDR_RXEMPTY_MASK))
            {
                result = LPI2C_STATUS_ERROR;
                break;
            }

            data[index] = (uint8_t)(rxWord & LPI2C_MRDR_DATA_MASK);
        }
    }

    if (LPI2C_STATUS_OK == result)
    {
        result = LPI2C_MasterWaitTxReady(base, timeout);
    }

    if (LPI2C_STATUS_OK == result)
    {
        LPI2C_MasterSendStop(base);
        result = LPI2C_MasterWaitStop(base, timeout);
    }
    else if (true == LPI2C_IsValidBase(base))
    {
        LPI2C_MasterSendStop(base);
    }
    else
    {
        /* Invalid base: nothing to stop. */
    }

    if (true == LPI2C_IsValidBase(base))
    {
        LPI2C_MasterClearStatus(base);
    }

    return result;
}

/**
 * @brief Write then read data from an I2C slave using blocking transfer.
 *
 * @details
 * Typical use case:
 * - Write register address
 * - Repeated START
 * - Read register data
 *
 * Bus sequence:
 * START -> ADDRESS + W -> TX bytes -> repeated START -> ADDRESS + R
 * -> RX bytes -> STOP
 *
 * @param base LPI2C peripheral base address.
 * @param slaveAddress 7-bit slave address.
 * @param txData Pointer to transmit buffer.
 * @param txSize Number of bytes to transmit.
 * @param rxData Pointer to receive buffer.
 * @param rxSize Number of bytes to receive.
 * @param timeout Timeout counter.
 *
 * @return LPI2C status code.
 */
LPI2C_Status_t LPI2C_MasterWriteReadBlocking(LPI2C_Type *base,
                                             uint8_t slaveAddress,
                                             const uint8_t *txData,
                                             uint16_t txSize,
                                             uint8_t *rxData,
                                             uint16_t rxSize,
                                             uint32_t timeout)
{
    LPI2C_Status_t result = LPI2C_STATUS_OK;
    uint16_t index = 0U;
    uint32_t rxWord = 0U;

    result = LPI2C_MasterValidateCommon(base, slaveAddress);

    if (LPI2C_STATUS_OK == result)
    {
        if ((NULL == txData) ||
            (0U == txSize) ||
            (NULL == rxData) ||
            (0U == rxSize) ||
            (LPI2C_MAX_READ_SIZE < rxSize))
        {
            result = LPI2C_STATUS_INVALID_ARGUMENT;
        }
    }

    if (LPI2C_STATUS_OK == result)
    {
        LPI2C_MasterPrepareTransfer(base);

        result = LPI2C_MasterWaitTxReady(base, timeout);
    }

    if (LPI2C_STATUS_OK == result)
    {
        LPI2C_MasterSendStart(base, slaveAddress, false);

        for (index = 0U; index < txSize; index++)
        {
            result = LPI2C_MasterWaitTxReady(base, timeout);

            if (LPI2C_STATUS_OK != result)
            {
                break;
            }

            LPI2C_MasterSendData(base, txData[index]);
        }
    }

    if (LPI2C_STATUS_OK == result)
    {
        result = LPI2C_MasterWaitTxReady(base, timeout);
    }

    if (LPI2C_STATUS_OK == result)
    {
        LPI2C_MasterSendStart(base, slaveAddress, true);

        result = LPI2C_MasterWaitTxReady(base, timeout);
    }

    if (LPI2C_STATUS_OK == result)
    {
        LPI2C_MasterReceiveCommand(base, rxSize);

        for (index = 0U; index < rxSize; index++)
        {
            result = LPI2C_MasterWaitRxReady(base, timeout);

            if (LPI2C_STATUS_OK != result)
            {
                break;
            }

            rxWord = base->MRDR;

            if (0U != (rxWord & LPI2C_MRDR_RXEMPTY_MASK))
            {
                result = LPI2C_STATUS_ERROR;
                break;
            }

            rxData[index] = (uint8_t)(rxWord & LPI2C_MRDR_DATA_MASK);
        }
    }

    if (LPI2C_STATUS_OK == result)
    {
        result = LPI2C_MasterWaitTxReady(base, timeout);
    }

    if (LPI2C_STATUS_OK == result)
    {
        LPI2C_MasterSendStop(base);
        result = LPI2C_MasterWaitStop(base, timeout);
    }
    else if (true == LPI2C_IsValidBase(base))
    {
        LPI2C_MasterSendStop(base);
    }
    else
    {
        /* Invalid base: nothing to stop. */
    }

    if (true == LPI2C_IsValidBase(base))
    {
        LPI2C_MasterClearStatus(base);
    }

    return result;
}

/**
 * @brief Execute a blocking master transaction.
 *
 * @param base LPI2C peripheral base address.
 * @param transfer Pointer to transfer descriptor.
 * @param timeout Timeout counter.
 *
 * @return LPI2C status code.
 */
LPI2C_Status_t LPI2C_MasterTransferBlocking(LPI2C_Type *base,
                                            const LPI2C_MasterTransfer_t *transfer,
                                            uint32_t timeout)
{
    LPI2C_Status_t result = LPI2C_STATUS_OK;

    if (false == LPI2C_IsValidBase(base))
    {
        result = LPI2C_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        result = LPI2C_MasterValidateTransfer(transfer);
    }

    if (LPI2C_STATUS_OK == result)
    {
        if (LPI2C_TRANSFER_WRITE == transfer->type)
        {
            result = LPI2C_MasterWriteBlocking(base,
                                               transfer->slaveAddress,
                                               transfer->txData,
                                               transfer->txSize,
                                               timeout);
        }
        else if (LPI2C_TRANSFER_READ == transfer->type)
        {
            result = LPI2C_MasterReadBlocking(base,
                                              transfer->slaveAddress,
                                              transfer->rxData,
                                              transfer->rxSize,
                                              timeout);
        }
        else if (LPI2C_TRANSFER_WRITE_READ == transfer->type)
        {
            result = LPI2C_MasterWriteReadBlocking(base,
                                                   transfer->slaveAddress,
                                                   transfer->txData,
                                                   transfer->txSize,
                                                   transfer->rxData,
                                                   transfer->rxSize,
                                                   timeout);
        }
        else
        {
            result = LPI2C_STATUS_INVALID_ARGUMENT;
        }
    }

    return result;
}

/* ============================================================
 * Master API - Interrupt
 * ============================================================ */

/**
 * @brief Mark interrupt transfer as completed.
 *
 * @param base LPI2C peripheral base address.
 * @param handle Pointer to master transfer handle.
 */
static void LPI2C_MasterIT_Complete(LPI2C_Type *base,
                                    LPI2C_MasterHandle_t *handle)
{
    base->MIER = 0U;
    LPI2C_MasterClearStatus(base);

    handle->status = LPI2C_STATUS_OK;
    handle->state = LPI2C_MASTER_STATE_DONE;
}

/**
 * @brief Abort interrupt transfer because of an error.
 *
 * @param base LPI2C peripheral base address.
 * @param handle Pointer to master transfer handle.
 * @param status Error status.
 */
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

/**
 * @brief Start an interrupt-based master transfer.
 *
 * @details
 * This function starts the transfer state machine. Actual bus operations are
 * performed in LPI2C_MasterIRQHandler().
 *
 * @param base LPI2C peripheral base address.
 * @param handle Pointer to transfer handle.
 * @param transfer Pointer to transfer descriptor.
 *
 * @return LPI2C status code.
 */
LPI2C_Status_t LPI2C_MasterTransferIT(LPI2C_Type *base,
                                      LPI2C_MasterHandle_t *handle,
                                      const LPI2C_MasterTransfer_t *transfer)
{
    LPI2C_Status_t result = LPI2C_STATUS_OK;

    if ((false == LPI2C_IsValidBase(base)) || (NULL == handle))
    {
        result = LPI2C_STATUS_INVALID_ARGUMENT;
    }
    else if (true == LPI2C_MasterIsBusy(base))
    {
        result = LPI2C_STATUS_BUSY;
    }
    else
    {
        result = LPI2C_MasterValidateTransfer(transfer);
    }

    if (LPI2C_STATUS_OK == result)
    {
        handle->transfer = *transfer;
        handle->state = LPI2C_MASTER_STATE_START;
        handle->status = LPI2C_STATUS_BUSY;
        handle->txCount = 0U;
        handle->rxCount = 0U;
        handle->rxCommandSent = false;

        s_pLpi2c0MasterHandle = handle;

        LPI2C_MasterPrepareTransfer(base);

        base->MIER = LPI2C_MASTER_IT_ERROR_INTERRUPTS |
                     LPI2C_MASTER_IT_TRANSFER_INTERRUPTS;
    }

    return result;
}

/**
 * @brief Handle LPI2C master interrupt.
 *
 * @details
 * This ISR-level function advances the master transfer state machine.
 * It must be called from the real LPI2C0 interrupt handler in irq.c.
 *
 * @param base LPI2C peripheral base address.
 */
void LPI2C_MasterIRQHandler(LPI2C_Type *base)
{
    LPI2C_MasterHandle_t *handle = NULL;
    LPI2C_Status_t errorStatus = LPI2C_STATUS_OK;
    uint32_t status = 0U;
    uint32_t rxWord = 0U;

    if (true == LPI2C_IsValidBase(base))
    {
        handle = s_pLpi2c0MasterHandle;

        if (NULL == handle)
        {
            base->MIER = 0U;
            LPI2C_MasterClearStatus(base);
        }
        else
        {
            status = base->MSR;

            errorStatus = LPI2C_MasterCheckError(base);

            if (LPI2C_STATUS_OK != errorStatus)
            {
                LPI2C_MasterIT_Error(base, handle, errorStatus);
            }
            else
            {
                if ((0U != (status & LPI2C_MSR_RDF_MASK)) &&
                    (LPI2C_MASTER_STATE_RECEIVE == handle->state))
                {
                    rxWord = base->MRDR;

                    if (0U == (rxWord & LPI2C_MRDR_RXEMPTY_MASK))
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

                if (0U != (status & LPI2C_MSR_TDF_MASK))
                {
                    switch (handle->state)
                    {
                        case LPI2C_MASTER_STATE_START:
                        {
                            if (LPI2C_TRANSFER_READ == handle->transfer.type)
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
                                if (LPI2C_TRANSFER_WRITE == handle->transfer.type)
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
                            if (false == handle->rxCommandSent)
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

                if (0U != (base->MSR & LPI2C_MSR_SDF_MASK))
                {
                    if (LPI2C_MASTER_STATE_STOP == handle->state)
                    {
                        LPI2C_MasterIT_Complete(base, handle);
                    }
                    else
                    {
                        base->MSR = LPI2C_MSR_SDF_MASK;
                    }
                }
            }
        }
    }
}

/**
 * @brief Get current state of an interrupt-based master transfer.
 *
 * @param handle Pointer to transfer handle.
 *
 * @return Master state.
 */
LPI2C_MasterState_t LPI2C_MasterGetState(const LPI2C_MasterHandle_t *handle)
{
    LPI2C_MasterState_t state = LPI2C_MASTER_STATE_ERROR;

    if (NULL != handle)
    {
        state = handle->state;
    }

    return state;
}

/**
 * @brief Get current status of an interrupt-based master transfer.
 *
 * @param handle Pointer to transfer handle.
 *
 * @return LPI2C status code.
 */
LPI2C_Status_t LPI2C_MasterGetStatus(const LPI2C_MasterHandle_t *handle)
{
    LPI2C_Status_t status = LPI2C_STATUS_INVALID_ARGUMENT;

    if (NULL != handle)
    {
        status = handle->status;
    }

    return status;
}

/* ============================================================
 * Master Helper API
 * ============================================================ */

/**
 * @brief Reset master TX and RX FIFOs.
 *
 * @param base LPI2C peripheral base address.
 */
void LPI2C_MasterResetFIFO(LPI2C_Type *base)
{
    if (true == LPI2C_IsValidBase(base))
    {
        LPI2C_MasterResetFifoInternal(base);
    }
}

/**
 * @brief Clear master status flags.
 *
 * @param base LPI2C peripheral base address.
 */
void LPI2C_MasterClearFlags(LPI2C_Type *base)
{
    if (true == LPI2C_IsValidBase(base))
    {
        LPI2C_MasterClearStatus(base);
    }
}

/**
 * @brief Get raw master status flags.
 *
 * @param base LPI2C peripheral base address.
 *
 * @return Raw MSR value. Returns 0 if base is invalid.
 */
uint32_t LPI2C_MasterGetStatusFlags(LPI2C_Type *base)
{
    uint32_t flags = 0U;

    if (true == LPI2C_IsValidBase(base))
    {
        flags = base->MSR;
    }

    return flags;
}

/**
 * @brief Check whether LPI2C master is busy.
 *
 * @param base LPI2C peripheral base address.
 *
 * @retval true Master is busy.
 * @retval false Master is idle or base is invalid.
 */
bool LPI2C_MasterIsBusy(LPI2C_Type *base)
{
    bool isBusy = false;

    if (true == LPI2C_IsValidBase(base))
    {
        if (0U != (base->MSR & LPI2C_MSR_MBF_MASK))
        {
            isBusy = true;
        }
    }

    return isBusy;
}

/* ============================================================
 * Internal Helpers - Slave
 * ============================================================ */

/**
 * @brief Clear slave status flags.
 *
 * @param base LPI2C peripheral base address.
 */
static void LPI2C_SlaveClearStatus(LPI2C_Type *base)
{
    base->SSR = LPI2C_SLAVE_CLEAR_FLAGS;
}

/**
 * @brief Reset slave TX and RX FIFOs.
 *
 * @param base LPI2C peripheral base address.
 */
static void LPI2C_SlaveResetFifoInternal(LPI2C_Type *base)
{
    base->SCR |= LPI2C_SCR_RTF_MASK | LPI2C_SCR_RRF_MASK;
}

/**
 * @brief Forward a slave event to the registered callback.
 *
 * @param base LPI2C peripheral base address.
 * @param event Slave event.
 */
static void LPI2C_SlaveCallEvent(LPI2C_Type *base,
                                 LPI2C_SlaveEvent_t event)
{
    if (NULL != s_Lpi2c0SlaveConfig.callback)
    {
        s_Lpi2c0SlaveConfig.callback(base,
                                     event,
                                     s_Lpi2c0SlaveConfig.userData);
    }
}

/* ============================================================
 * Slave API
 * ============================================================ */

/**
 * @brief Load default slave configuration.
 *
 * @param config Pointer to slave configuration structure.
 */
void LPI2C_SlaveGetDefaultConfig(LPI2C_SlaveConfig_t *config)
{
    if (NULL != config)
    {
        config->slaveAddress = 0x12U;

        config->enableGeneralCall = false;
        config->enableClockStretching = true;
        config->enableFilter = true;

        config->callback = NULL;
        config->userData = NULL;
    }
}

/**
 * @brief Initialize LPI2C peripheral in slave mode.
 *
 * @details
 * This function:
 * - Enables peripheral clock
 * - Resets slave logic
 * - Configures slave address
 * - Configures clock stretching
 * - Configures slave timing/filter
 * - Enables slave interrupts
 * - Enables slave mode
 *
 * Pin muxing is not configured here.
 *
 * @param base LPI2C peripheral base address.
 * @param config Pointer to slave configuration.
 *
 * @return LPI2C status code.
 */
LPI2C_Status_t LPI2C_SlaveInit(LPI2C_Type *base,
                               const LPI2C_SlaveConfig_t *config)
{
    LPI2C_Status_t result = LPI2C_STATUS_OK;
    uint32_t scfgr1 = 0U;
    uint32_t scr = 0U;

    if ((false == LPI2C_IsValidBase(base)) || (NULL == config))
    {
        result = LPI2C_STATUS_INVALID_ARGUMENT;
    }
    else if (false == LPI2C_IsValid7BitAddress(config->slaveAddress))
    {
        result = LPI2C_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        LPI2C_EnableClock(base);

        s_Lpi2c0SlaveConfig = *config;

        base->SCR = LPI2C_SCR_RST_MASK;
        base->SCR = 0U;

        base->SIER = 0U;
        base->SDER = 0U;

        LPI2C_SlaveClearStatus(base);
        LPI2C_SlaveResetFifoInternal(base);

        base->SAMR = LPI2C_SAMR_ADDR0((uint32_t)config->slaveAddress);

        scfgr1 = LPI2C_SCFGR1_ADDRCFG(0U);

        if (true == config->enableGeneralCall)
        {
            scfgr1 |= LPI2C_SCFGR1_GCEN_MASK;
        }

        if (true == config->enableClockStretching)
        {
            scfgr1 |= LPI2C_SCFGR1_ADRSTALL_MASK |
                      LPI2C_SCFGR1_RXSTALL_MASK  |
                      LPI2C_SCFGR1_TXDSTALL_MASK;
        }

        base->SCFGR1 = scfgr1;

        base->SCFGR2 = LPI2C_SCFGR2_FILTSCL(1U) |
                       LPI2C_SCFGR2_FILTSDA(1U) |
                       LPI2C_SCFGR2_DATAVD(2U)  |
                       LPI2C_SCFGR2_CLKHOLD(2U);

        base->SIER = LPI2C_SLAVE_IT_INTERRUPTS;

        scr = 0U;

        if (true == config->enableFilter)
        {
            scr |= LPI2C_SCR_FILTEN_MASK;
        }

        base->SCR = scr | LPI2C_SCR_SEN_MASK;
    }

    return result;
}

/**
 * @brief Deinitialize LPI2C slave mode.
 *
 * @param base LPI2C peripheral base address.
 */
void LPI2C_SlaveDeinit(LPI2C_Type *base)
{
    if (true == LPI2C_IsValidBase(base))
    {
        base->SIER = 0U;
        base->SDER = 0U;
        base->SCR = 0U;

        s_Lpi2c0SlaveConfig.callback = NULL;
        s_Lpi2c0SlaveConfig.userData = NULL;
    }
}

/**
 * @brief Handle LPI2C slave interrupt.
 *
 * @details
 * This function only reports slave events to the registered callback.
 * It does not manage application buffers. Wire.c is responsible for
 * RX/TX buffer handling.
 *
 * @param base LPI2C peripheral base address.
 */
void LPI2C_SlaveIRQHandler(LPI2C_Type *base)
{
    uint32_t status = 0U;

    if (true == LPI2C_IsValidBase(base))
    {
        status = base->SSR;

        if ((0U != (status & LPI2C_SSR_FEF_MASK)) ||
            (0U != (status & LPI2C_SSR_BEF_MASK)))
        {
            LPI2C_SlaveCallEvent(base, LPI2C_SLAVE_EVENT_ERROR);

            base->SSR = LPI2C_SSR_FEF_MASK |
                        LPI2C_SSR_BEF_MASK;
        }

        if (0U != (status & LPI2C_SSR_AVF_MASK))
        {
            (void)base->SASR;
            LPI2C_SlaveCallEvent(base, LPI2C_SLAVE_EVENT_ADDRESS_MATCH);
        }

        if (0U != (status & LPI2C_SSR_RDF_MASK))
        {
            LPI2C_SlaveCallEvent(base, LPI2C_SLAVE_EVENT_RX_DATA);
        }

        if (0U != (status & LPI2C_SSR_TDF_MASK))
        {
            LPI2C_SlaveCallEvent(base, LPI2C_SLAVE_EVENT_TX_REQUEST);
        }

        if (0U != (status & LPI2C_SSR_RSF_MASK))
        {
            base->SSR = LPI2C_SSR_RSF_MASK;
            LPI2C_SlaveCallEvent(base, LPI2C_SLAVE_EVENT_REPEATED_START);
        }

        if (0U != (status & LPI2C_SSR_SDF_MASK))
        {
            base->SSR = LPI2C_SSR_SDF_MASK;
            LPI2C_SlaveCallEvent(base, LPI2C_SLAVE_EVENT_STOP);
        }

        if (0U != (status & LPI2C_SSR_AM0F_MASK))
        {
            base->SSR = LPI2C_SSR_AM0F_MASK;
        }

        if (0U != (status & LPI2C_SSR_AM1F_MASK))
        {
            base->SSR = LPI2C_SSR_AM1F_MASK;
        }

        if (0U != (status & LPI2C_SSR_GCF_MASK))
        {
            base->SSR = LPI2C_SSR_GCF_MASK;
        }

        if (0U != (status & LPI2C_SSR_SARF_MASK))
        {
            base->SSR = LPI2C_SSR_SARF_MASK;
        }
    }
}

/**
 * @brief Write one byte to slave TX FIFO.
 *
 * @param base LPI2C peripheral base address.
 * @param data Byte to transmit.
 *
 * @return LPI2C status code.
 */
LPI2C_Status_t LPI2C_SlaveWriteByte(LPI2C_Type *base,
                                    uint8_t data)
{
    LPI2C_Status_t result = LPI2C_STATUS_OK;

    if (false == LPI2C_IsValidBase(base))
    {
        result = LPI2C_STATUS_INVALID_ARGUMENT;
    }
    else if (0U == (base->SSR & LPI2C_SSR_TDF_MASK))
    {
        result = LPI2C_STATUS_BUSY;
    }
    else
    {
        base->STDR = LPI2C_STDR_DATA(data);
        result = LPI2C_STATUS_OK;
    }

    return result;
}

/**
 * @brief Read one byte from slave RX FIFO.
 *
 * @param base LPI2C peripheral base address.
 * @param data Pointer to receive byte.
 *
 * @return LPI2C status code.
 */
LPI2C_Status_t LPI2C_SlaveReadByte(LPI2C_Type *base,
                                   uint8_t *data)
{
    LPI2C_Status_t result = LPI2C_STATUS_OK;
    uint32_t rxWord = 0U;

    if ((false == LPI2C_IsValidBase(base)) || (NULL == data))
    {
        result = LPI2C_STATUS_INVALID_ARGUMENT;
    }
    else if (0U == (base->SSR & LPI2C_SSR_RDF_MASK))
    {
        result = LPI2C_STATUS_BUSY;
    }
    else
    {
        rxWord = base->SRDR;

        if (0U != (rxWord & LPI2C_SRDR_RXEMPTY_MASK))
        {
            result = LPI2C_STATUS_ERROR;
        }
        else
        {
            *data = (uint8_t)(rxWord & LPI2C_SRDR_DATA_MASK);
            result = LPI2C_STATUS_OK;
        }
    }

    return result;
}
