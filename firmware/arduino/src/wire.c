#include "Wire.h"
#include "lpi2c.h"
#include "irq.h"

#define WIRE_TX_BUFFER_SIZE     32U
#define WIRE_RX_BUFFER_SIZE     32U
#define WIRE_DEFAULT_TIMEOUT    1000000U

static uint8_t s_wireTxBuffer[WIRE_TX_BUFFER_SIZE];
static uint8_t s_wireRxBuffer[WIRE_RX_BUFFER_SIZE];

static uint8_t s_wireTxLength = 0U;
static uint8_t s_wireRxLength = 0U;
static uint8_t s_wireRxIndex  = 0U;

static uint8_t s_wireAddress = 0U;
static uint8_t s_wireInitialized = 0U;

static Wire_Status_t s_wireStatus = WIRE_STATUS_OK;

static LPI2C_MasterHandle_t s_wireMasterHandle;

static void (*s_wireOnReceive)(int) = 0;
static void (*s_wireOnRequest)(void) = 0;

static Wire_Status_t Wire_MapStatus(LPI2C_Status_t status)
{
    switch (status)
    {
        case LPI2C_STATUS_OK:
            return WIRE_STATUS_OK;

        case LPI2C_STATUS_BUSY:
            return WIRE_STATUS_BUSY;

        case LPI2C_STATUS_TIMEOUT:
            return WIRE_STATUS_TIMEOUT;

        case LPI2C_STATUS_NACK:
            return WIRE_STATUS_NACK;

        default:
            return WIRE_STATUS_ERROR;
    }
}

static void Wire_SlaveCallback(LPI2C_Type *base,
                               LPI2C_SlaveEvent_t event,
                               void *userData)
{
    uint8_t data;

    (void)userData;

    if (base != IP_LPI2C0)
    {
        return;
    }

    if (event == LPI2C_SLAVE_EVENT_RX_DATA)
    {
        while (LPI2C_SlaveReadByte(base, &data) == LPI2C_STATUS_OK)
        {
            if (s_wireRxLength < WIRE_RX_BUFFER_SIZE)
            {
                s_wireRxBuffer[s_wireRxLength] = data;
                s_wireRxLength++;
            }
        }
    }
    else if (event == LPI2C_SLAVE_EVENT_TX_REQUEST)
    {
        if (s_wireOnRequest != 0)
        {
            s_wireTxLength = 0U;
            s_wireOnRequest();
        }

        if (s_wireTxLength > 0U)
        {
            (void)LPI2C_SlaveWriteByte(base, s_wireTxBuffer[0]);
        }
        else
        {
            (void)LPI2C_SlaveWriteByte(base, 0xFFU);
        }
    }
    else if (event == LPI2C_SLAVE_EVENT_STOP)
    {
        if ((s_wireOnReceive != 0) && (s_wireRxLength > 0U))
        {
            s_wireRxIndex = 0U;
            s_wireOnReceive((int)s_wireRxLength);
        }
    }
    else
    {
        /* Other slave events are ignored in Wire layer V1 */
    }
}

/* ============================================================
 * Init
 * ============================================================ */

void Wire_begin(void)
{
    LPI2C_MasterConfig_t config;

    LPI2C_MasterGetDefaultConfig(&config);

    s_wireStatus = Wire_MapStatus(LPI2C_MasterInit(IP_LPI2C0, &config));

    IRQ_LPI2C0_Master_Init();

    s_wireTxLength = 0U;
    s_wireRxLength = 0U;
    s_wireRxIndex = 0U;
    s_wireInitialized = 1U;
}

void Wire_beginAddress(uint8_t address)
{
    LPI2C_SlaveConfig_t config;

    LPI2C_SlaveGetDefaultConfig(&config);

    config.slaveAddress = address;
    config.callback = Wire_SlaveCallback;
    config.userData = 0;

    s_wireStatus = Wire_MapStatus(LPI2C_SlaveInit(IP_LPI2C0, &config));

    IRQ_LPI2C0_Slave_Init();

    s_wireTxLength = 0U;
    s_wireRxLength = 0U;
    s_wireRxIndex = 0U;
    s_wireInitialized = 1U;
}

void Wire_setClock(uint32_t frequency)
{
    LPI2C_MasterConfig_t config;

    LPI2C_MasterGetDefaultConfig(&config);
    config.baudRate = frequency;

    s_wireStatus = Wire_MapStatus(LPI2C_MasterInit(IP_LPI2C0, &config));
}

/* ============================================================
 * Master - Write
 * ============================================================ */

void Wire_beginTransmission(uint8_t address)
{
    s_wireAddress = address;
    s_wireTxLength = 0U;
}

uint8_t Wire_write(uint8_t data)
{
    if (s_wireTxLength >= WIRE_TX_BUFFER_SIZE)
    {
        return 0U;
    }

    s_wireTxBuffer[s_wireTxLength] = data;
    s_wireTxLength++;

    return 1U;
}

uint8_t Wire_writeBuffer(const uint8_t *data, uint8_t length)
{
    uint8_t i;
    uint8_t count = 0U;

    if (data == 0)
    {
        return 0U;
    }

    for (i = 0U; i < length; i++)
    {
        if (Wire_write(data[i]) == 0U)
        {
            break;
        }

        count++;
    }

    return count;
}

uint8_t Wire_endTransmission(void)
{
    LPI2C_Status_t status;

    if (s_wireInitialized == 0U)
    {
        s_wireStatus = WIRE_STATUS_ERROR;
        return (uint8_t)s_wireStatus;
    }

    status = LPI2C_MasterWriteBlocking(IP_LPI2C0,
                                       s_wireAddress,
                                       s_wireTxBuffer,
                                       s_wireTxLength,
                                       WIRE_DEFAULT_TIMEOUT);

    s_wireStatus = Wire_MapStatus(status);

    s_wireTxLength = 0U;

    return (uint8_t)s_wireStatus;
}

/* ============================================================
 * Master - Read
 * ============================================================ */

uint8_t Wire_requestFrom(uint8_t address, uint8_t quantity)
{
    LPI2C_Status_t status;

    if (s_wireInitialized == 0U)
    {
        s_wireStatus = WIRE_STATUS_ERROR;
        return 0U;
    }

    if (quantity > WIRE_RX_BUFFER_SIZE)
    {
        quantity = WIRE_RX_BUFFER_SIZE;
    }

    s_wireRxLength = 0U;
    s_wireRxIndex = 0U;

    status = LPI2C_MasterReadBlocking(IP_LPI2C0,
                                      address,
                                      s_wireRxBuffer,
                                      quantity,
                                      WIRE_DEFAULT_TIMEOUT);

    s_wireStatus = Wire_MapStatus(status);

    if (s_wireStatus != WIRE_STATUS_OK)
    {
        return 0U;
    }

    s_wireRxLength = quantity;

    return quantity;
}

int Wire_available(void)
{
    return (int)(s_wireRxLength - s_wireRxIndex);
}

int Wire_read(void)
{
    uint8_t data;

    if (s_wireRxIndex >= s_wireRxLength)
    {
        return -1;
    }

    data = s_wireRxBuffer[s_wireRxIndex];
    s_wireRxIndex++;

    return (int)data;
}

/* ============================================================
 * Slave API
 * ============================================================ */

void Wire_onReceive(void (*callback)(int))
{
    s_wireOnReceive = callback;
}

void Wire_onRequest(void (*callback)(void))
{
    s_wireOnRequest = callback;
}

/* ============================================================
 * Optional - Async
 * ============================================================ */

uint8_t Wire_transferAsync(uint8_t address,
                           const uint8_t *txData,
                           uint16_t txSize,
                           uint8_t *rxData,
                           uint16_t rxSize)
{
    LPI2C_MasterTransfer_t transfer;
    LPI2C_Status_t status;

    transfer.slaveAddress = address;
    transfer.txData = txData;
    transfer.txSize = txSize;
    transfer.rxData = rxData;
    transfer.rxSize = rxSize;
    transfer.sendStop = true;

    if ((txSize > 0U) && (rxSize > 0U))
    {
        transfer.type = LPI2C_TRANSFER_WRITE_READ;
    }
    else if (txSize > 0U)
    {
        transfer.type = LPI2C_TRANSFER_WRITE;
    }
    else
    {
        transfer.type = LPI2C_TRANSFER_READ;
    }

    status = LPI2C_MasterTransferIT(IP_LPI2C0,
                                    &s_wireMasterHandle,
                                    &transfer);

    s_wireStatus = Wire_MapStatus(status);

    return (uint8_t)s_wireStatus;
}

uint8_t Wire_isBusy(void)
{
    return (LPI2C_MasterGetState(&s_wireMasterHandle) != LPI2C_MASTER_STATE_DONE) &&
           (LPI2C_MasterGetState(&s_wireMasterHandle) != LPI2C_MASTER_STATE_ERROR);
}

uint8_t Wire_isDone(void)
{
    return (LPI2C_MasterGetState(&s_wireMasterHandle) == LPI2C_MASTER_STATE_DONE);
}

uint8_t Wire_getStatus(void)
{
    LPI2C_Status_t status;

    status = LPI2C_MasterGetStatus(&s_wireMasterHandle);

    if (status != LPI2C_STATUS_INVALID_ARGUMENT)
    {
        s_wireStatus = Wire_MapStatus(status);
    }

    return (uint8_t)s_wireStatus;
}
