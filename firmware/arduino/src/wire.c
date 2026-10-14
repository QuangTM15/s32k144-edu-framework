#include "Wire.h"
#include "lpi2c.h"
#include "irq.h"
#include "board_pins.h"
#include <stddef.h>

/**
 * @file Wire.c
 * @brief Arduino-style Wire implementation on S32K144 LPI2C0.
 *
 * @details
 * One physical bus can address several slaves. The slave address is
 * selected per transfer; Wire_begin() never binds the bus to a device.
 */

#define WIRE_TX_BUFFER_SIZE       (32U)
#define WIRE_RX_BUFFER_SIZE       (32U)
#define WIRE_DEFAULT_TIMEOUT      (1000000U)
#define WIRE_I2C_PORT_MUX         (3U)
#define WIRE_SLAVE_DUMMY_BYTE     (0xFFU)
#define WIRE_ADDRESS_MAX          (0x7FU)

typedef uint8_t Wire_Role_t;
#define WIRE_ROLE_NONE            ((Wire_Role_t)0U)
#define WIRE_ROLE_MASTER          ((Wire_Role_t)1U)
#define WIRE_ROLE_SLAVE           ((Wire_Role_t)2U)

static uint8_t s_u8WireTxBuffer[WIRE_TX_BUFFER_SIZE];
static uint8_t s_u8WireRxBuffer[WIRE_RX_BUFFER_SIZE];
static uint8_t s_u8WireTxLength = 0U;
static uint8_t s_u8WireTxIndex = 0U;
static uint8_t s_u8WireRxLength = 0U;
static uint8_t s_u8WireRxIndex = 0U;
static uint8_t s_u8WireAddress = 0U;
static uint8_t s_u8WireInitialized = 0U;
static uint8_t s_u8WireTxOpen = 0U;
static uint8_t s_u8WireTxOverflow = 0U;
static uint8_t s_u8WireSlaveTxPrepared = 0U;
static uint8_t s_u8WireLastAsync = 0U;
static uint32_t s_u32WireClock = LPI2C_SPEED_STANDARD;
static Wire_Role_t s_WireRole = WIRE_ROLE_NONE;
static Wire_Status_t s_WireStatus = WIRE_STATUS_OK;
static LPI2C_MasterHandle_t s_WireMasterHandle;
static void (*s_pWireOnReceive)(int) = NULL;
static void (*s_pWireOnRequest)(void) = NULL;

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

static void Wire_ResetTxBuffer(void)
{
    s_u8WireTxLength = 0U;
    s_u8WireTxIndex = 0U;
    s_u8WireTxOverflow = 0U;
}

static void Wire_ResetRxBuffer(void)
{
    s_u8WireRxLength = 0U;
    s_u8WireRxIndex = 0U;
}

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

static uint8_t Wire_IsAsyncActive(void)
{
    LPI2C_MasterState_t state;
    uint8_t active = 0U;

    if (0U != s_u8WireLastAsync)
    {
        state = LPI2C_MasterGetState(&s_WireMasterHandle);
        if ((LPI2C_MASTER_STATE_START == state) ||
            (LPI2C_MASTER_STATE_SEND == state) ||
            (LPI2C_MASTER_STATE_RECEIVE == state) ||
            (LPI2C_MASTER_STATE_STOP == state))
        {
            active = 1U;
        }
    }

    return active;
}

static uint8_t Wire_IsMasterUnavailable(void)
{
    uint8_t unavailable = 0U;

    if ((0U != Wire_IsAsyncActive()) ||
        (true == LPI2C_MasterIsBusy(IP_LPI2C0)))
    {
        unavailable = 1U;
    }

    return unavailable;
}

/* ============================================================
 * Slave callback helpers (original public slave API is retained)
 * ============================================================ */

static void Wire_HandleSlaveAddressMatch(void)
{
    Wire_ResetRxBuffer();
    s_u8WireTxIndex = 0U;
    s_u8WireSlaveTxPrepared = 0U;
}

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
            /* Other events do not require Wire-level handling. */
        }
    }
}

/* ============================================================
 * Initialization
 * ============================================================ */

void Wire_begin(void)
{
    LPI2C_MasterConfig_t config;

    if ((0U != s_u8WireInitialized) && (WIRE_ROLE_MASTER == s_WireRole))
    {
        /* Idempotent: never reset an initialized master or its buffers. */
    }
    else if (0U != Wire_IsAsyncActive())
    {
        s_WireStatus = WIRE_STATUS_BUSY;
    }
    else
    {
        if ((0U != s_u8WireInitialized) && (WIRE_ROLE_SLAVE == s_WireRole))
        {
            LPI2C_SlaveDeinit(IP_LPI2C0);
            s_u8WireInitialized = 0U;
            s_WireRole = WIRE_ROLE_NONE;
        }

        Wire_InitPins();
        LPI2C_MasterGetDefaultConfig(&config);
        s_WireStatus = Wire_MapStatus(LPI2C_MasterInit(IP_LPI2C0, &config));

        if (WIRE_STATUS_OK == s_WireStatus)
        {
            Wire_ResetTxBuffer();
            Wire_ResetRxBuffer();
            s_u8WireAddress = 0U;
            s_u8WireTxOpen = 0U;
            s_u8WireLastAsync = 0U;
            s_u32WireClock = config.baudRate;
            s_u8WireInitialized = 1U;
            s_WireRole = WIRE_ROLE_MASTER;
        }
        else
        {
            s_u8WireInitialized = 0U;
            s_WireRole = WIRE_ROLE_NONE;
        }
    }
}

void Wire_beginAddress(uint8_t address)
{
    LPI2C_SlaveConfig_t config;

    if (WIRE_ADDRESS_MAX < address)
    {
        s_WireStatus = WIRE_STATUS_ERROR;
    }
    else if ((0U != s_u8WireInitialized) &&
             (WIRE_ROLE_SLAVE == s_WireRole) &&
             (s_u8WireAddress == address))
    {
        /* Already configured at this address. */
    }
    else if ((WIRE_ROLE_MASTER == s_WireRole) &&
             ((0U != Wire_IsMasterUnavailable()) || (0U != s_u8WireTxOpen)))
    {
        s_WireStatus = WIRE_STATUS_BUSY;
    }
    else
    {
        if ((0U != s_u8WireInitialized) && (WIRE_ROLE_MASTER == s_WireRole))
        {
            LPI2C_MasterDeinit(IP_LPI2C0);
        }
        else if ((0U != s_u8WireInitialized) && (WIRE_ROLE_SLAVE == s_WireRole))
        {
            LPI2C_SlaveDeinit(IP_LPI2C0);
        }
        else
        {
            /* First initialization. */
        }

        s_u8WireInitialized = 0U;
        s_WireRole = WIRE_ROLE_NONE;
        Wire_InitPins();
        LPI2C_SlaveGetDefaultConfig(&config);
        config.slaveAddress = address;
        config.callback = Wire_SlaveCallback;
        config.userData = NULL;
        s_WireStatus = Wire_MapStatus(LPI2C_SlaveInit(IP_LPI2C0, &config));

        if (WIRE_STATUS_OK == s_WireStatus)
        {
            IRQ_LPI2C0_Slave_Init();
            Wire_ResetTxBuffer();
            Wire_ResetRxBuffer();
            s_u8WireAddress = address;
            s_u8WireSlaveTxPrepared = 0U;
            s_u8WireTxOpen = 0U;
            s_u8WireLastAsync = 0U;
            s_u8WireInitialized = 1U;
            s_WireRole = WIRE_ROLE_SLAVE;
        }
    }
}

void Wire_setClock(uint32_t frequency)
{
    LPI2C_MasterConfig_t config;

    if ((0U == s_u8WireInitialized) || (WIRE_ROLE_MASTER != s_WireRole))
    {
        s_WireStatus = WIRE_STATUS_ERROR;
    }
    else if ((LPI2C_SPEED_STANDARD != frequency) &&
             (LPI2C_SPEED_FAST != frequency))
    {
        /* Reject unsupported rates without disturbing the running bus. */
        s_WireStatus = WIRE_STATUS_ERROR;
    }
    else if ((0U != Wire_IsMasterUnavailable()) || (0U != s_u8WireTxOpen))
    {
        s_WireStatus = WIRE_STATUS_BUSY;
    }
    else if (s_u32WireClock == frequency)
    {
        s_WireStatus = WIRE_STATUS_OK;
        s_u8WireLastAsync = 0U;
    }
    else
    {
        LPI2C_MasterGetDefaultConfig(&config);
        config.baudRate = frequency;
        s_WireStatus = Wire_MapStatus(LPI2C_MasterInit(IP_LPI2C0, &config));
        s_u8WireLastAsync = 0U;

        if (WIRE_STATUS_OK == s_WireStatus)
        {
            s_u32WireClock = frequency;
        }
        else
        {
            /* LPI2C_MasterInit may leave master mode disabled on failure. */
            s_u8WireInitialized = 0U;
            s_WireRole = WIRE_ROLE_NONE;
        }
    }
}

/* ============================================================
 * Buffered master write
 * ============================================================ */

void Wire_beginTransmission(uint8_t address)
{
    if ((0U == s_u8WireInitialized) || (WIRE_ROLE_MASTER != s_WireRole) ||
        (WIRE_ADDRESS_MAX < address))
    {
        s_WireStatus = WIRE_STATUS_ERROR;
        s_u8WireTxOpen = 0U;
    }
    else if (0U != Wire_IsMasterUnavailable())
    {
        s_WireStatus = WIRE_STATUS_BUSY;
    }
    else
    {
        s_u8WireAddress = address;
        Wire_ResetTxBuffer();
        s_u8WireTxOpen = 1U;
        s_u8WireLastAsync = 0U;
        s_WireStatus = WIRE_STATUS_OK;
    }
}

uint8_t Wire_write(uint8_t data)
{
    uint8_t written = 0U;

    if ((0U != s_u8WireInitialized) &&
        (((WIRE_ROLE_MASTER == s_WireRole) && (0U != s_u8WireTxOpen)) ||
         (WIRE_ROLE_SLAVE == s_WireRole)))
    {
        if (s_u8WireTxLength < WIRE_TX_BUFFER_SIZE)
        {
            s_u8WireTxBuffer[s_u8WireTxLength] = data;
            s_u8WireTxLength++;
            written = 1U;
        }
        else
        {
            s_u8WireTxOverflow = 1U;
            s_WireStatus = WIRE_STATUS_ERROR;
        }
    }

    return written;
}

uint8_t Wire_writeBuffer(const uint8_t *data, uint8_t length)
{
    uint8_t index = 0U;
    uint8_t count = 0U;

    if (NULL != data)
    {
        for (index = 0U; index < length; index++)
        {
            if (0U == Wire_write(data[index]))
            {
                break;
            }
            count++;
        }
    }

    return count;
}

uint8_t Wire_endTransmission(void)
{
    LPI2C_Status_t status = LPI2C_STATUS_ERROR;

    if ((0U == s_u8WireInitialized) || (WIRE_ROLE_MASTER != s_WireRole) ||
        (0U == s_u8WireTxOpen))
    {
        s_WireStatus = WIRE_STATUS_ERROR;
    }
    else if (0U != Wire_IsMasterUnavailable())
    {
        s_WireStatus = WIRE_STATUS_BUSY;
    }
    else
    {
        s_u8WireLastAsync = 0U;
        if (0U != s_u8WireTxOverflow)
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
        }

        Wire_ResetTxBuffer();
        s_u8WireTxOpen = 0U;
    }

    return (uint8_t)s_WireStatus;
}

/* ============================================================
 * Master read / combined blocking transfer
 * ============================================================ */

uint8_t Wire_requestFrom(uint8_t address, uint8_t quantity)
{
    LPI2C_Status_t status = LPI2C_STATUS_ERROR;
    uint8_t received = 0U;

    if ((0U == s_u8WireInitialized) || (WIRE_ROLE_MASTER != s_WireRole) ||
        (WIRE_ADDRESS_MAX < address) || (0U == quantity))
    {
        s_WireStatus = WIRE_STATUS_ERROR;
    }
    else if ((0U != Wire_IsMasterUnavailable()) || (0U != s_u8WireTxOpen))
    {
        s_WireStatus = WIRE_STATUS_BUSY;
    }
    else
    {
        if (WIRE_RX_BUFFER_SIZE < quantity)
        {
            quantity = WIRE_RX_BUFFER_SIZE;
        }

        Wire_ResetRxBuffer();
        s_u8WireLastAsync = 0U;
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

int Wire_available(void)
{
    int available = 0;

    if (s_u8WireRxIndex <= s_u8WireRxLength)
    {
        available = (int)(s_u8WireRxLength - s_u8WireRxIndex);
    }

    return available;
}

int Wire_read(void)
{
    int data = -1;

    if (s_u8WireRxIndex < s_u8WireRxLength)
    {
        data = (int)s_u8WireRxBuffer[s_u8WireRxIndex];
        s_u8WireRxIndex++;
    }

    return data;
}

uint8_t Wire_transferBlocking(uint8_t address,
                              const uint8_t *txData,
                              uint16_t txSize,
                              uint8_t *rxData,
                              uint16_t rxSize)
{
    LPI2C_MasterTransfer_t transfer;
    LPI2C_Status_t status = LPI2C_STATUS_ERROR;

    if ((0U == s_u8WireInitialized) || (WIRE_ROLE_MASTER != s_WireRole) ||
        (WIRE_ADDRESS_MAX < address) || ((0U == txSize) && (0U == rxSize)) ||
        ((0U < txSize) && (NULL == txData)) ||
        ((0U < rxSize) && (NULL == rxData)))
    {
        s_WireStatus = WIRE_STATUS_ERROR;
    }
    else if ((0U != Wire_IsMasterUnavailable()) || (0U != s_u8WireTxOpen))
    {
        s_WireStatus = WIRE_STATUS_BUSY;
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

        s_u8WireLastAsync = 0U;
        status = LPI2C_MasterTransferBlocking(IP_LPI2C0,
                                               &transfer,
                                               WIRE_DEFAULT_TIMEOUT);
        s_WireStatus = Wire_MapStatus(status);
    }

    return (uint8_t)s_WireStatus;
}

/* ============================================================
 * Slave callback registrations
 * ============================================================ */

void Wire_onReceive(void (*callback)(int))
{
    s_pWireOnReceive = callback;
}

void Wire_onRequest(void (*callback)(void))
{
    s_pWireOnRequest = callback;
}

/* ============================================================
 * Interrupt-based master transfer
 * ============================================================ */

uint8_t Wire_transferAsync(uint8_t address,
                           const uint8_t *txData,
                           uint16_t txSize,
                           uint8_t *rxData,
                           uint16_t rxSize)
{
    LPI2C_MasterTransfer_t transfer;
    LPI2C_Status_t status = LPI2C_STATUS_ERROR;

    if ((0U == s_u8WireInitialized) || (WIRE_ROLE_MASTER != s_WireRole) ||
        (WIRE_ADDRESS_MAX < address) || ((0U == txSize) && (0U == rxSize)) ||
        ((0U < txSize) && (NULL == txData)) ||
        ((0U < rxSize) && (NULL == rxData)))
    {
        s_WireStatus = WIRE_STATUS_ERROR;
    }
    else if ((0U != Wire_IsMasterUnavailable()) || (0U != s_u8WireTxOpen))
    {
        s_WireStatus = WIRE_STATUS_BUSY;
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
        s_u8WireLastAsync = (WIRE_STATUS_OK == s_WireStatus) ? 1U : 0U;
    }

    return (uint8_t)s_WireStatus;
}

uint8_t Wire_isBusy(void)
{
    return Wire_IsAsyncActive();
}

uint8_t Wire_isDone(void)
{
    uint8_t done = 0U;

    if ((0U != s_u8WireLastAsync) &&
        (LPI2C_MASTER_STATE_DONE == LPI2C_MasterGetState(&s_WireMasterHandle)))
    {
        done = 1U;
    }

    return done;
}

uint8_t Wire_getStatus(void)
{
    if (0U != s_u8WireLastAsync)
    {
        s_WireStatus = Wire_MapStatus(LPI2C_MasterGetStatus(&s_WireMasterHandle));
    }

    return (uint8_t)s_WireStatus;
}
