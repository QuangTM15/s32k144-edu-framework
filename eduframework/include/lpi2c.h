#ifndef LPI2C_H
#define LPI2C_H

#include <stdint.h>
#include <stdbool.h>
#include "S32K144.h"

/* ============================================================
 * EduFramework - LPI2C Driver
 *
 * Design:
 * - Bare-metal register-level driver
 * - Transaction-based
 * - Master + Slave support
 * - Blocking + Interrupt
 * - Wire API will wrap this driver later
 * ============================================================ */

/* ============================================================
 * Status
 * ============================================================ */

typedef enum
{
    LPI2C_STATUS_OK = 0,
    LPI2C_STATUS_BUSY,
    LPI2C_STATUS_TIMEOUT,
    LPI2C_STATUS_NACK,
    LPI2C_STATUS_ARBITRATION_LOST,
    LPI2C_STATUS_FIFO_ERROR,
    LPI2C_STATUS_PIN_LOW_TIMEOUT,
    LPI2C_STATUS_INVALID_ARGUMENT,
    LPI2C_STATUS_ERROR
} LPI2C_Status_t;

/* ============================================================
 * Speed
 * ============================================================ */

typedef enum
{
    LPI2C_SPEED_STANDARD = 100000U,
    LPI2C_SPEED_FAST     = 400000U,
    LPI2C_SPEED_FASTPLUS = 1000000U
} LPI2C_Speed_t;

/* ============================================================
 * Master Configuration
 * ============================================================ */

typedef struct
{
    uint32_t srcClockHz;
    uint32_t baudRate;

    bool enableDebug;
    bool enableDoze;

    uint32_t timeout;
} LPI2C_MasterConfig_t;

/* ============================================================
 * Master Transfer
 * ============================================================ */

typedef enum
{
    LPI2C_TRANSFER_WRITE = 0,
    LPI2C_TRANSFER_READ,
    LPI2C_TRANSFER_WRITE_READ
} LPI2C_TransferType_t;

typedef struct
{
    uint8_t slaveAddress;

    const uint8_t *txData;
    uint16_t txSize;

    uint8_t *rxData;
    uint16_t rxSize;

    LPI2C_TransferType_t type;

    bool sendStop;
} LPI2C_MasterTransfer_t;

/* ============================================================
 * Master Interrupt Handle
 * ============================================================ */

typedef enum
{
    LPI2C_MASTER_STATE_IDLE = 0,
    LPI2C_MASTER_STATE_START,
    LPI2C_MASTER_STATE_SEND,
    LPI2C_MASTER_STATE_RECEIVE,
    LPI2C_MASTER_STATE_STOP,
    LPI2C_MASTER_STATE_DONE,
    LPI2C_MASTER_STATE_ERROR
} LPI2C_MasterState_t;

typedef struct
{
    LPI2C_MasterTransfer_t transfer;

    LPI2C_MasterState_t state;
    LPI2C_Status_t status;

    uint16_t txCount;
    uint16_t rxCount;

    bool rxCommandSent;
} LPI2C_MasterHandle_t;

/* ============================================================
 * Slave Configuration
 * ============================================================ */

typedef enum
{
    LPI2C_SLAVE_EVENT_ADDRESS_MATCH = 0,
    LPI2C_SLAVE_EVENT_RX_DATA,
    LPI2C_SLAVE_EVENT_TX_REQUEST,
    LPI2C_SLAVE_EVENT_STOP,
    LPI2C_SLAVE_EVENT_REPEATED_START,
    LPI2C_SLAVE_EVENT_ERROR
} LPI2C_SlaveEvent_t;

typedef void (*LPI2C_SlaveCallback_t)(LPI2C_Type *base,
                                      LPI2C_SlaveEvent_t event,
                                      void *userData);

typedef struct
{
    uint8_t slaveAddress;

    bool enableGeneralCall;
    bool enableClockStretching;
    bool enableFilter;

    LPI2C_SlaveCallback_t callback;
    void *userData;
} LPI2C_SlaveConfig_t;

/* ============================================================
 * Master API - Blocking
 * ============================================================ */

void LPI2C_MasterGetDefaultConfig(LPI2C_MasterConfig_t *config);

LPI2C_Status_t LPI2C_MasterInit(LPI2C_Type *base,
                                const LPI2C_MasterConfig_t *config);

void LPI2C_MasterDeinit(LPI2C_Type *base);

LPI2C_Status_t LPI2C_MasterWriteBlocking(LPI2C_Type *base,
                                         uint8_t slaveAddress,
                                         const uint8_t *data,
                                         uint16_t size,
                                         uint32_t timeout);

LPI2C_Status_t LPI2C_MasterReadBlocking(LPI2C_Type *base,
                                        uint8_t slaveAddress,
                                        uint8_t *data,
                                        uint16_t size,
                                        uint32_t timeout);

LPI2C_Status_t LPI2C_MasterWriteReadBlocking(LPI2C_Type *base,
                                             uint8_t slaveAddress,
                                             const uint8_t *txData,
                                             uint16_t txSize,
                                             uint8_t *rxData,
                                             uint16_t rxSize,
                                             uint32_t timeout);

LPI2C_Status_t LPI2C_MasterTransferBlocking(LPI2C_Type *base,
                                            const LPI2C_MasterTransfer_t *transfer,
                                            uint32_t timeout);

/* ============================================================
 * Master API - Interrupt
 * ============================================================ */

LPI2C_Status_t LPI2C_MasterTransferIT(LPI2C_Type *base,
                                      LPI2C_MasterHandle_t *handle,
                                      const LPI2C_MasterTransfer_t *transfer);

void LPI2C_MasterIRQHandler(LPI2C_Type *base);

LPI2C_MasterState_t LPI2C_MasterGetState(const LPI2C_MasterHandle_t *handle);

LPI2C_Status_t LPI2C_MasterGetStatus(const LPI2C_MasterHandle_t *handle);

/* ============================================================
 * Slave API
 * ============================================================ */

void LPI2C_SlaveGetDefaultConfig(LPI2C_SlaveConfig_t *config);

LPI2C_Status_t LPI2C_SlaveInit(LPI2C_Type *base,
                               const LPI2C_SlaveConfig_t *config);

void LPI2C_SlaveDeinit(LPI2C_Type *base);

void LPI2C_SlaveIRQHandler(LPI2C_Type *base);

LPI2C_Status_t LPI2C_SlaveWriteByte(LPI2C_Type *base,
                                    uint8_t data);

LPI2C_Status_t LPI2C_SlaveReadByte(LPI2C_Type *base,
                                   uint8_t *data);

/* ============================================================
 * Low-level Helper API
 * ============================================================ */

void LPI2C_MasterResetFIFO(LPI2C_Type *base);

void LPI2C_MasterClearFlags(LPI2C_Type *base);

uint32_t LPI2C_MasterGetStatusFlags(LPI2C_Type *base);

bool LPI2C_MasterIsBusy(LPI2C_Type *base);

#endif /* LPI2C_H */
