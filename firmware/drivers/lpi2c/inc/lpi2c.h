#ifndef LPI2C_H
#define LPI2C_H

#include <stdint.h>
#include <stdbool.h>
#include "S32K144.h"

/**
 * @file lpi2c.h
 * @brief Low-level LPI2C driver for EduFramework_v2.
 *
 * @details
 * This driver provides bare-metal register-level access to the S32K144 LPI2C
 * peripheral. It supports master blocking transfer, master interrupt transfer,
 * and slave callback mode.
 *
 * Board-specific pin configuration is intentionally not handled in this driver.
 * Pin muxing such as PTA2 SDA and PTA3 SCL must be configured by the board layer
 * or Arduino Wire layer before calling LPI2C initialization APIs.
 */

/* ============================================================
 * Status
 * ============================================================ */

typedef uint8_t LPI2C_Status_t;

#define LPI2C_STATUS_OK                    ((LPI2C_Status_t)0U)
#define LPI2C_STATUS_BUSY                  ((LPI2C_Status_t)1U)
#define LPI2C_STATUS_TIMEOUT               ((LPI2C_Status_t)2U)
#define LPI2C_STATUS_NACK                  ((LPI2C_Status_t)3U)
#define LPI2C_STATUS_ARBITRATION_LOST      ((LPI2C_Status_t)4U)
#define LPI2C_STATUS_FIFO_ERROR            ((LPI2C_Status_t)5U)
#define LPI2C_STATUS_PIN_LOW_TIMEOUT       ((LPI2C_Status_t)6U)
#define LPI2C_STATUS_INVALID_ARGUMENT      ((LPI2C_Status_t)7U)
#define LPI2C_STATUS_ERROR                 ((LPI2C_Status_t)8U)

/* ============================================================
 * Speed
 * ============================================================ */

typedef uint32_t LPI2C_Speed_t;

#define LPI2C_SPEED_STANDARD               ((LPI2C_Speed_t)100000U)
#define LPI2C_SPEED_FAST                   ((LPI2C_Speed_t)400000U)
#define LPI2C_SPEED_FASTPLUS               ((LPI2C_Speed_t)1000000U)

/* ============================================================
 * Master Transfer Type
 * ============================================================ */

typedef uint8_t LPI2C_TransferType_t;

#define LPI2C_TRANSFER_WRITE               ((LPI2C_TransferType_t)0U)
#define LPI2C_TRANSFER_READ                ((LPI2C_TransferType_t)1U)
#define LPI2C_TRANSFER_WRITE_READ          ((LPI2C_TransferType_t)2U)

/* ============================================================
 * Master State
 * ============================================================ */

typedef uint8_t LPI2C_MasterState_t;

#define LPI2C_MASTER_STATE_IDLE            ((LPI2C_MasterState_t)0U)
#define LPI2C_MASTER_STATE_START           ((LPI2C_MasterState_t)1U)
#define LPI2C_MASTER_STATE_SEND            ((LPI2C_MasterState_t)2U)
#define LPI2C_MASTER_STATE_RECEIVE         ((LPI2C_MasterState_t)3U)
#define LPI2C_MASTER_STATE_STOP            ((LPI2C_MasterState_t)4U)
#define LPI2C_MASTER_STATE_DONE            ((LPI2C_MasterState_t)5U)
#define LPI2C_MASTER_STATE_ERROR           ((LPI2C_MasterState_t)6U)

/* ============================================================
 * Slave Event
 * ============================================================ */

typedef uint8_t LPI2C_SlaveEvent_t;

#define LPI2C_SLAVE_EVENT_ADDRESS_MATCH    ((LPI2C_SlaveEvent_t)0U)
#define LPI2C_SLAVE_EVENT_RX_DATA          ((LPI2C_SlaveEvent_t)1U)
#define LPI2C_SLAVE_EVENT_TX_REQUEST       ((LPI2C_SlaveEvent_t)2U)
#define LPI2C_SLAVE_EVENT_STOP             ((LPI2C_SlaveEvent_t)3U)
#define LPI2C_SLAVE_EVENT_REPEATED_START   ((LPI2C_SlaveEvent_t)4U)
#define LPI2C_SLAVE_EVENT_ERROR            ((LPI2C_SlaveEvent_t)5U)

/* ============================================================
 * Master Configuration
 * ============================================================ */

/**
 * @brief LPI2C master configuration structure.
 *
 * @details
 * The source clock must match the functional clock selected for the LPI2C
 * peripheral. The driver uses this value to calculate timing registers.
 */
typedef struct
{
    uint32_t srcClockHz;     /**< LPI2C functional clock in Hz. */
    uint32_t baudRate;       /**< Target I2C bus speed in Hz. */

    bool enableDebug;        /**< Keep module running in debug mode when true. */
    bool enableDoze;         /**< Keep module running in doze mode when true. */

    uint32_t timeout;        /**< Default timeout count for blocking operations. */
} LPI2C_MasterConfig_t;

/* ============================================================
 * Master Transfer
 * ============================================================ */

/**
 * @brief LPI2C master transaction descriptor.
 *
 * @details
 * This structure describes one complete I2C master transaction. It is used by
 * both blocking and interrupt-based transfer APIs.
 */
typedef struct
{
    uint8_t slaveAddress;            /**< 7-bit slave address. */

    const uint8_t *txData;           /**< Pointer to transmit buffer. */
    uint16_t txSize;                 /**< Number of bytes to transmit. */

    uint8_t *rxData;                 /**< Pointer to receive buffer. */
    uint16_t rxSize;                 /**< Number of bytes to receive. */

    LPI2C_TransferType_t type;       /**< Transfer type: write, read, or write-read. */

    bool sendStop;                   /**< Send STOP condition at the end when true. */
} LPI2C_MasterTransfer_t;

/* ============================================================
 * Master Interrupt Handle
 * ============================================================ */

/**
 * @brief Runtime handle for interrupt-based master transfer.
 *
 * @details
 * The handle stores transfer progress and state. The application must keep this
 * object valid until the interrupt transfer is done or fails.
 */
typedef struct
{
    LPI2C_MasterTransfer_t transfer;  /**< Current transfer descriptor. */

    LPI2C_MasterState_t state;        /**< Current master interrupt state. */
    LPI2C_Status_t status;            /**< Current transfer status. */

    uint16_t txCount;                 /**< Number of transmitted bytes. */
    uint16_t rxCount;                 /**< Number of received bytes. */

    bool rxCommandSent;               /**< Internal flag for receive command stage. */
} LPI2C_MasterHandle_t;

/* ============================================================
 * Slave Configuration
 * ============================================================ */

/**
 * @brief LPI2C slave callback type.
 *
 * @param base LPI2C peripheral base address.
 * @param event Slave event reported by the driver.
 * @param userData User-defined pointer from slave configuration.
 */
typedef void (*LPI2C_SlaveCallback_t)(LPI2C_Type *base,
                                      LPI2C_SlaveEvent_t event,
                                      void *userData);

/**
 * @brief LPI2C slave configuration structure.
 */
typedef struct
{
    uint8_t slaveAddress;             /**< 7-bit slave address. */

    bool enableGeneralCall;           /**< Enable general call address response when true. */
    bool enableClockStretching;       /**< Enable clock stretching when true. */
    bool enableFilter;                /**< Enable digital input filter when true. */

    LPI2C_SlaveCallback_t callback;   /**< Callback invoked from slave IRQ handler. */
    void *userData;                   /**< User data passed to callback. */
} LPI2C_SlaveConfig_t;

/* ============================================================
 * Master API - Blocking
 * ============================================================ */

/**
 * @brief Fill master configuration with default values.
 *
 * @param config Pointer to configuration structure.
 */
void LPI2C_MasterGetDefaultConfig(LPI2C_MasterConfig_t *config);

/**
 * @brief Initialize LPI2C peripheral in master mode.
 *
 * @param base LPI2C peripheral base address.
 * @param config Pointer to master configuration.
 *
 * @return LPI2C status code.
 */
LPI2C_Status_t LPI2C_MasterInit(LPI2C_Type *base,
                                const LPI2C_MasterConfig_t *config);

/**
 * @brief Deinitialize LPI2C master peripheral.
 *
 * @param base LPI2C peripheral base address.
 */
void LPI2C_MasterDeinit(LPI2C_Type *base);

/**
 * @brief Write bytes to an I2C slave using blocking transfer.
 *
 * @param base LPI2C peripheral base address.
 * @param slaveAddress 7-bit slave address.
 * @param data Pointer to transmit buffer.
 * @param size Number of bytes to transmit.
 * @param timeout Timeout count.
 *
 * @return LPI2C status code.
 */
LPI2C_Status_t LPI2C_MasterWriteBlocking(LPI2C_Type *base,
                                         uint8_t slaveAddress,
                                         const uint8_t *data,
                                         uint16_t size,
                                         uint32_t timeout);

/**
 * @brief Read bytes from an I2C slave using blocking transfer.
 *
 * @param base LPI2C peripheral base address.
 * @param slaveAddress 7-bit slave address.
 * @param data Pointer to receive buffer.
 * @param size Number of bytes to receive.
 * @param timeout Timeout count.
 *
 * @return LPI2C status code.
 */
LPI2C_Status_t LPI2C_MasterReadBlocking(LPI2C_Type *base,
                                        uint8_t slaveAddress,
                                        uint8_t *data,
                                        uint16_t size,
                                        uint32_t timeout);

/**
 * @brief Write then read bytes from an I2C slave using blocking transfer.
 *
 * @param base LPI2C peripheral base address.
 * @param slaveAddress 7-bit slave address.
 * @param txData Pointer to transmit buffer.
 * @param txSize Number of bytes to transmit.
 * @param rxData Pointer to receive buffer.
 * @param rxSize Number of bytes to receive.
 * @param timeout Timeout count.
 *
 * @return LPI2C status code.
 */
LPI2C_Status_t LPI2C_MasterWriteReadBlocking(LPI2C_Type *base,
                                             uint8_t slaveAddress,
                                             const uint8_t *txData,
                                             uint16_t txSize,
                                             uint8_t *rxData,
                                             uint16_t rxSize,
                                             uint32_t timeout);

/**
 * @brief Execute a blocking master transaction.
 *
 * @param base LPI2C peripheral base address.
 * @param transfer Pointer to transfer descriptor.
 * @param timeout Timeout count.
 *
 * @return LPI2C status code.
 */
LPI2C_Status_t LPI2C_MasterTransferBlocking(LPI2C_Type *base,
                                            const LPI2C_MasterTransfer_t *transfer,
                                            uint32_t timeout);

/* ============================================================
 * Master API - Interrupt
 * ============================================================ */

/**
 * @brief Start an interrupt-based master transaction.
 *
 * @param base LPI2C peripheral base address.
 * @param handle Pointer to master interrupt handle.
 * @param transfer Pointer to transfer descriptor.
 *
 * @return LPI2C status code.
 */
LPI2C_Status_t LPI2C_MasterTransferIT(LPI2C_Type *base,
                                      LPI2C_MasterHandle_t *handle,
                                      const LPI2C_MasterTransfer_t *transfer);

/**
 * @brief Handle LPI2C master interrupt.
 *
 * @param base LPI2C peripheral base address.
 */
void LPI2C_MasterIRQHandler(LPI2C_Type *base);

/**
 * @brief Get current interrupt-based master transfer state.
 *
 * @param handle Pointer to master interrupt handle.
 *
 * @return Current master state.
 */
LPI2C_MasterState_t LPI2C_MasterGetState(const LPI2C_MasterHandle_t *handle);

/**
 * @brief Get current interrupt-based master transfer status.
 *
 * @param handle Pointer to master interrupt handle.
 *
 * @return Current LPI2C status code.
 */
LPI2C_Status_t LPI2C_MasterGetStatus(const LPI2C_MasterHandle_t *handle);

/* ============================================================
 * Slave API
 * ============================================================ */

/**
 * @brief Fill slave configuration with default values.
 *
 * @param config Pointer to slave configuration structure.
 */
void LPI2C_SlaveGetDefaultConfig(LPI2C_SlaveConfig_t *config);

/**
 * @brief Initialize LPI2C peripheral in slave mode.
 *
 * @param base LPI2C peripheral base address.
 * @param config Pointer to slave configuration.
 *
 * @return LPI2C status code.
 */
LPI2C_Status_t LPI2C_SlaveInit(LPI2C_Type *base,
                               const LPI2C_SlaveConfig_t *config);

/**
 * @brief Deinitialize LPI2C slave peripheral.
 *
 * @param base LPI2C peripheral base address.
 */
void LPI2C_SlaveDeinit(LPI2C_Type *base);

/**
 * @brief Handle LPI2C slave interrupt.
 *
 * @param base LPI2C peripheral base address.
 */
void LPI2C_SlaveIRQHandler(LPI2C_Type *base);

/**
 * @brief Write one byte to the slave transmit FIFO.
 *
 * @param base LPI2C peripheral base address.
 * @param data Byte to transmit.
 *
 * @return LPI2C status code.
 */
LPI2C_Status_t LPI2C_SlaveWriteByte(LPI2C_Type *base,
                                    uint8_t data);

/**
 * @brief Read one byte from the slave receive FIFO.
 *
 * @param base LPI2C peripheral base address.
 * @param data Pointer to store received byte.
 *
 * @return LPI2C status code.
 */
LPI2C_Status_t LPI2C_SlaveReadByte(LPI2C_Type *base,
                                   uint8_t *data);

/* ============================================================
 * Low-level Helper API
 * ============================================================ */

/**
 * @brief Reset LPI2C master TX and RX FIFOs.
 *
 * @param base LPI2C peripheral base address.
 */
void LPI2C_MasterResetFIFO(LPI2C_Type *base);

/**
 * @brief Clear LPI2C master status flags.
 *
 * @param base LPI2C peripheral base address.
 */
void LPI2C_MasterClearFlags(LPI2C_Type *base);

/**
 * @brief Get raw LPI2C master status flags.
 *
 * @param base LPI2C peripheral base address.
 *
 * @return Raw MSR register flags.
 */
uint32_t LPI2C_MasterGetStatusFlags(LPI2C_Type *base);

/**
 * @brief Check whether the LPI2C master bus is busy.
 *
 * @param base LPI2C peripheral base address.
 *
 * @retval true Master or bus is busy.
 * @retval false Master and bus are idle.
 */
bool LPI2C_MasterIsBusy(LPI2C_Type *base);

#endif /* LPI2C_H */
