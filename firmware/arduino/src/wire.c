#include "Wire.h"
#include "lpi2c.h"
#include "irq.h"
#include "board_pins.h"
#include <stddef.h>

/**
 * @file Wire.c
 * @brief Arduino-style I2C Wire API implementation for EduFramework_v2.
 *
 * @details
 * This module wraps the low-level LPI2C driver and provides a simple
 * Arduino-style API for I2C master and slave communication.
 *
 * Board-specific I2C pin configuration is handled here, not inside
 * the low-level LPI2C driver.
 */

/* ============================================================
 * Internal Definitions
 * ============================================================ */

#define WIRE_TX_BUFFER_SIZE         32U
#define WIRE_RX_BUFFER_SIZE         32U
#define WIRE_DEFAULT_TIMEOUT        1000000U

#define WIRE_I2C_PORT_MUX           3U
#define WIRE_SLAVE_DUMMY_BYTE       0xFFU

typedef uint8_t Wire_Role_t;

#define WIRE_ROLE_NONE              ((Wire_Role_t)0U)
#define WIRE_ROLE_MASTER            ((Wire_Role_t)1U)
#define WIRE_ROLE_SLAVE             ((Wire_Role_t)2U)

/* ============================================================
 * Internal State
 * ============================================================ */

static uint8_t s_u8WireTxBuffer[WIRE_TX_BUFFER_SIZE];
static uint8_t s_u8WireRxBuffer[WIRE_RX_BUFFER_SIZE];

static uint8_t s_u8WireTxLength = 0U;
static uint8_t s_u8WireRxLength = 0U;
static uint8_t s_u8WireRxIndex  = 0U;
static uint8_t s_u8WireTxIndex  = 0U;

static uint8_t s_u8WireAddress = 0U;
static uint8_t s_u8WireInitialized = 0U;
static uint8_t s_u8WireSlaveTxPrepared = 0U;

static Wire_Role_t s_WireRole = WIRE_ROLE_NONE;
static Wire_Status_t s_WireStatus = WIRE_STATUS_OK;

static LPI2C_MasterHandle_t s_WireMasterHandle;

static void (*s_pWireOnReceive)(int) = NULL;
static void (*s_pWireOnRequest)(void) = NULL;

/* ============================================================
 * Internal Helper Functions
 * ============================================================ */

/**
 * @brief Initialize MaaZEDU board I2C0 pins.
 *
 * @details
 * Current board scope:
 * - LPI2C0 SCL: PTA3
 * - LPI2C0 SDA: PTA2
 *
 * This function belongs to Wire layer because the low-level LPI2C
 * driver must not know board-specific pin mapping.
 */
static void Wire_InitPins(void)
{
    IP_PCC->PCCn[PCC_PORTA_INDEX] |= PCC_PCCn_CGC_MASK;

    BOARD_I2C0_SCL_PORT->PCR[BOARD_I2C0_SCL_PIN] =
        PORT_PCR_MUX(WIRE_I2C_PORT_MUX) |
        PORT_PCR_PE_MASK |
        PORT_PCR_PS_MASK;

    BOARD_I2C0_SDA_PORT->PCR[BOARD_I2C0_SDA_PIN] =
        PORT_PCR_MUX(WIRE_I2C_PORT_MUX) |
        PORT_PCR_PE_MASK |
        PORT_PCR_PS_MASK;
}

/**
 * @brief Reset Wire transmit buffer state.
 */
static void Wire_ResetTxBuffer(void)
{
    s_u8WireTxLength = 0U;
    s_u8WireTxIndex = 0U;
}

/**
 * @brief Reset Wire receive buffer state.
 */
static void Wire_ResetRxBuffer(void)
{
    s_u8WireRxLength = 0U;
    s_u8WireRxIndex = 0U;
}

/**
 * @brief Convert LPI2C driver status to Wire status.
 *
 * @param status LPI2C driver status.
 *
 * @return Wire status.
 */
static Wire_Status_t Wire_MapStatus(LPI2C_Status_t status)
{
    Wire_Status_t result = WIRE_STATUS_ERROR;

    if (LPI2C_STATUS_OK == status)
    {
        result = WIRE_STATUS_OK;
    }
    else if (LPI2C_STATUS_BUSY == status)
    {
        result = WIRE_STATUS_BUSY;
    }
    else if (LPI2C_STATUS_TIMEOUT == status)
    {
        result = WIRE_STATUS_TIMEOUT;
    }
    else if (LPI2C_STATUS_NACK == status)
    {
        result = WIRE_STATUS_NACK;
    }
    else
    {
        result = WIRE_STATUS_ERROR;
    }

    return result;
}

/**
 * @brief Handle slave address match event.
 *
 * @details
 * A new slave transaction starts when address match occurs.
 * RX and TX indexes are reset here to prevent stale data from the
 * previous transaction.
 */
static void Wire_HandleSlaveAddressMatch(void)
{
    Wire_ResetRxBuffer();

    s_u8WireTxIndex = 0U;
    s_u8WireSlaveTxPrepared = 0U;
}

/**
 * @brief Handle slave RX data event.
 *
 * @param base LPI2C peripheral base address.
 */
static void Wire_HandleSlaveRxData(LPI2C_Type *base)
{
    uint8_t data = 0U;

    while (LPI2C_STATUS_OK == LPI2C_SlaveReadByte(base, &data))
    {
        if (s_u8WireRxLength < WIRE_RX_BUFFER_SIZE)
        {
            s_u8WireRxBuffer[s_u8WireRxLength] = data;
            s_u8WireRxLength++;
        }
    }
}

/**
 * @brief Handle slave TX request event.
 *
 * @details
 * The onRequest callback is called once at the beginning of a master
 * read transaction. Then each TX request sends the next byte in the
 * TX buffer. If there is no data left, the slave returns 0xFF.
 *
 * @param base LPI2C peripheral base address.
 */
static void Wire_HandleSlaveTxRequest(LPI2C_Type *base)
{
    uint8_t data = WIRE_SLAVE_DUMMY_BYTE;

    if (0U == s_u8WireSlaveTxPrepared)
    {
        Wire_ResetTxBuffer();

        if (NULL != s_pWireOnRequest)
        {
            s_pWireOnRequest();
        }

        s_u8WireSlaveTxPrepared = 1U;
    }

    if (s_u8WireTxIndex < s_u8WireTxLength)
    {
        data = s_u8WireTxBuffer[s_u8WireTxIndex];
        s_u8WireTxIndex++;
    }

    (void)LPI2C_SlaveWriteByte(base, data);
}

/**
 * @brief Handle slave STOP event.
 *
 * @details
 * When a master write transaction ends, the user receive callback is called
 * with the number of received bytes. The RX buffer is kept available for
 * Wire_available() and Wire_read() inside the callback. It will be reset
 * at the next address match.
 */
static void Wire_HandleSlaveStop(void)
{
    if ((NULL != s_pWireOnReceive) && (0U < s_u8WireRxLength))
    {
        s_u8WireRxIndex = 0U;
        s_pWireOnReceive((int)s_u8WireRxLength);
    }

    s_u8WireTxIndex = 0U;
    s_u8WireSlaveTxPrepared = 0U;
}

/**
 * @brief LPI2C slave callback used by Wire slave mode.
 *
 * @param base LPI2C peripheral base address.
 * @param event Slave event from low-level driver.
 * @param userData User data pointer.
 */
static void Wire_SlaveCallback(LPI2C_Type *base,
                               LPI2C_SlaveEvent_t event,
                               void *userData)
{
    (void)userData;

    if (IP_LPI2C0 == base)
    {
        if (LPI2C_SLAVE_EVENT_ADDRESS_MATCH == event)
        {
            Wire_HandleSlaveAddressMatch();
        }
        else if (LPI2C_SLAVE_EVENT_RX_DATA == event)
        {
            Wire_HandleSlaveRxData(base);
        }
        else if (LPI2C_SLAVE_EVENT_TX_REQUEST == event)
        {
            Wire_HandleSlaveTxRequest(base);
        }
        else if (LPI2C_SLAVE_EVENT_STOP == event)
        {
            Wire_HandleSlaveStop();
        }
        else if (LPI2C_SLAVE_EVENT_REPEATED_START == event)
        {
            s_u8WireTxIndex = 0U;
            s_u8WireSlaveTxPrepared = 0U;
        }
        else
        {
            /* Other slave events are intentionally ignored in Wire layer. */
        }
    }
}

/* ============================================================
 * Init
 * ============================================================ */

/**
 * @brief Initialize Wire in I2C master mode.
 */
void Wire_begin(void)
{
    LPI2C_MasterConfig_t config;

    Wire_InitPins();

    LPI2C_MasterGetDefaultConfig(&config);

    s_WireStatus = Wire_MapStatus(LPI2C_MasterInit(IP_LPI2C0, &config));

    Wire_ResetTxBuffer();
    Wire_ResetRxBuffer();

    s_u8WireAddress = 0U;
    s_u8WireInitialized = 1U;
    s_WireRole = WIRE_ROLE_MASTER;
}

/**
 * @brief Initialize Wire in I2C slave mode.
 *
 * @param address 7-bit slave address.
 */
void Wire_beginAddress(uint8_t address)
{
    LPI2C_SlaveConfig_t config;

    Wire_InitPins();

    LPI2C_SlaveGetDefaultConfig(&config);

    config.slaveAddress = address;
    config.callback = Wire_SlaveCallback;
    config.userData = NULL;

    s_WireStatus = Wire_MapStatus(LPI2C_SlaveInit(IP_LPI2C0, &config));

    IRQ_LPI2C0_Slave_Init();

    Wire_ResetTxBuffer();
    Wire_ResetRxBuffer();

    s_u8WireAddress = address;
    s_u8WireInitialized = 1U;
    s_u8WireSlaveTxPrepared = 0U;
    s_WireRole = WIRE_ROLE_SLAVE;
}

/**
 * @brief Set I2C clock frequency in master mode.
 *
 * @param frequency Target I2C frequency in Hz.
 */
void Wire_setClock(uint32_t frequency)
{
    LPI2C_MasterConfig_t config;

    if ((0U != s_u8WireInitialized) && (WIRE_ROLE_MASTER == s_WireRole))
    {
        LPI2C_MasterGetDefaultConfig(&config);
        config.baudRate = frequency;

        s_WireStatus = Wire_MapStatus(LPI2C_MasterInit(IP_LPI2C0, &config));

        if (WIRE_STATUS_OK == s_WireStatus)
        {
            s_WireRole = WIRE_ROLE_MASTER;
        }
    }
    else
    {
        s_WireStatus = WIRE_STATUS_ERROR;
    }
}

/* ============================================================
 * Master - Write
 * ============================================================ */

/**
 * @brief Begin a master transmit transaction.
 *
 * @param address 7-bit slave address.
 */
void Wire_beginTransmission(uint8_t address)
{
    s_u8WireAddress = address;
    Wire_ResetTxBuffer();
}

/**
 * @brief Write one byte to Wire TX buffer.
 *
 * @param data Byte to write.
 *
 * @return 1 if stored, 0 if buffer is full.
 */
uint8_t Wire_write(uint8_t data)
{
    uint8_t result = 0U;

    if (s_u8WireTxLength < WIRE_TX_BUFFER_SIZE)
    {
        s_u8WireTxBuffer[s_u8WireTxLength] = data;
        s_u8WireTxLength++;
        result = 1U;
    }

    return result;
}

/**
 * @brief Write multiple bytes to Wire TX buffer.
 *
 * @param data Pointer to source buffer.
 * @param length Number of bytes to write.
 *
 * @return Number of bytes actually stored.
 */
uint8_t Wire_writeBuffer(const uint8_t *data, uint8_t length)
{
    uint8_t index = 0U;
    uint8_t count = 0U;
    bool done = false;

    if (NULL != data)
    {
        while (false == done)
        {
            if (index >= length)
            {
                done = true;
            }
            else if (0U == Wire_write(data[index]))
            {
                done = true;
            }
            else
            {
                index++;
                count++;
            }
        }
    }

    return count;
}

/**
 * @brief End master transmit transaction.
 *
 * @return Wire status code.
 */
uint8_t Wire_endTransmission(void)
{
    LPI2C_Status_t status = LPI2C_STATUS_ERROR;

    if ((0U == s_u8WireInitialized) || (WIRE_ROLE_MASTER != s_WireRole))
    {
        s_WireStatus = WIRE_STATUS_ERROR;
    }
    else
    {
        status = LPI2C_MasterWriteBlocking(IP_LPI2C0,
                                           s_u8WireAddress,
                                           s_u8WireTxBuffer,
                                           s_u8WireTxLength,
                                           WIRE_DEFAULT_TIMEOUT);

        s_WireStatus = Wire_MapStatus(status);

        Wire_ResetTxBuffer();
    }

    return (uint8_t)s_WireStatus;
}

/* ============================================================
 * Master - Read
 * ============================================================ */

/**
 * @brief Request bytes from an I2C slave.
 *
 * @param address 7-bit slave address.
 * @param quantity Number of bytes requested.
 *
 * @return Number of received bytes.
 */
uint8_t Wire_requestFrom(uint8_t address, uint8_t quantity)
{
    LPI2C_Status_t status = LPI2C_STATUS_ERROR;
    uint8_t received = 0U;

    if ((0U == s_u8WireInitialized) || (WIRE_ROLE_MASTER != s_WireRole))
    {
        s_WireStatus = WIRE_STATUS_ERROR;
    }
    else
    {
        if (quantity > WIRE_RX_BUFFER_SIZE)
        {
            quantity = WIRE_RX_BUFFER_SIZE;
        }

        Wire_ResetRxBuffer();

        status = LPI2C_MasterReadBlocking(IP_LPI2C0,
                                          address,
                                          s_u8WireRxBuffer,
                                          quantity,
                                          WIRE_DEFAULT_TIMEOUT);

        s_WireStatus = Wire_MapStatus(status);

        if (WIRE_STATUS_OK == s_WireStatus)
        {
            s_u8WireRxLength = quantity;
            received = quantity;
        }
    }

    return received;
}

/**
 * @brief Get number of unread bytes in RX buffer.
 *
 * @return Number of available bytes.
 */
int Wire_available(void)
{
    int available = 0;

    if (s_u8WireRxLength >= s_u8WireRxIndex)
    {
        available = (int)(s_u8WireRxLength - s_u8WireRxIndex);
    }

    return available;
}

/**
 * @brief Read one byte from RX buffer.
 *
 * @return Byte value, or -1 if no data is available.
 */
int Wire_read(void)
{
    int result = -1;

    if (s_u8WireRxIndex < s_u8WireRxLength)
    {
        result = (int)s_u8WireRxBuffer[s_u8WireRxIndex];
        s_u8WireRxIndex++;
    }

    return result;
}

/* ============================================================
 * Slave API
 * ============================================================ */

/**
 * @brief Register slave receive callback.
 *
 * @param callback Callback function pointer.
 */
void Wire_onReceive(void (*callback)(int))
{
    s_pWireOnReceive = callback;
}

/**
 * @brief Register slave request callback.
 *
 * @param callback Callback function pointer.
 */
void Wire_onRequest(void (*callback)(void))
{
    s_pWireOnRequest = callback;
}

/* ============================================================
 * Optional - Async
 * ============================================================ */

/**
 * @brief Start an interrupt-based master transfer.
 *
 * @param address 7-bit slave address.
 * @param txData Pointer to TX data.
 * @param txSize TX size.
 * @param rxData Pointer to RX data.
 * @param rxSize RX size.
 *
 * @return Wire status code.
 */
uint8_t Wire_transferAsync(uint8_t address,
                           const uint8_t *txData,
                           uint16_t txSize,
                           uint8_t *rxData,
                           uint16_t rxSize)
{
    LPI2C_MasterTransfer_t transfer;
    LPI2C_Status_t status = LPI2C_STATUS_ERROR;

    if ((0U == s_u8WireInitialized) || (WIRE_ROLE_MASTER != s_WireRole))
    {
        s_WireStatus = WIRE_STATUS_ERROR;
    }
    else
    {
        transfer.slaveAddress = address;
        transfer.txData = txData;
        transfer.txSize = txSize;
        transfer.rxData = rxData;
        transfer.rxSize = rxSize;
        transfer.sendStop = true;

        if ((0U < txSize) && (0U < rxSize))
        {
            transfer.type = LPI2C_TRANSFER_WRITE_READ;
        }
        else if (0U < txSize)
        {
            transfer.type = LPI2C_TRANSFER_WRITE;
        }
        else
        {
            transfer.type = LPI2C_TRANSFER_READ;
        }

        IRQ_LPI2C0_Master_Init();

        status = LPI2C_MasterTransferIT(IP_LPI2C0,
                                        &s_WireMasterHandle,
                                        &transfer);

        s_WireStatus = Wire_MapStatus(status);
    }

    return (uint8_t)s_WireStatus;
}

/**
 * @brief Check whether async transfer is busy.
 *
 * @return 1 if busy, otherwise 0.
 */
uint8_t Wire_isBusy(void)
{
    LPI2C_MasterState_t state;
    uint8_t busy = 0U;

    state = LPI2C_MasterGetState(&s_WireMasterHandle);

    if ((LPI2C_MASTER_STATE_START == state) ||
        (LPI2C_MASTER_STATE_SEND == state) ||
        (LPI2C_MASTER_STATE_RECEIVE == state) ||
        (LPI2C_MASTER_STATE_STOP == state))
    {
        busy = 1U;
    }

    return busy;
}

/**
 * @brief Check whether async transfer is done.
 *
 * @return 1 if done, otherwise 0.
 */
uint8_t Wire_isDone(void)
{
    uint8_t done = 0U;

    if (LPI2C_MASTER_STATE_DONE == LPI2C_MasterGetState(&s_WireMasterHandle))
    {
        done = 1U;
    }

    return done;
}

/**
 * @brief Get last Wire status.
 *
 * @return Wire status code.
 */
uint8_t Wire_getStatus(void)
{
    LPI2C_Status_t status;

    status = LPI2C_MasterGetStatus(&s_WireMasterHandle);

    if (LPI2C_STATUS_INVALID_ARGUMENT != status)
    {
        s_WireStatus = Wire_MapStatus(status);
    }

    return (uint8_t)s_WireStatus;
}