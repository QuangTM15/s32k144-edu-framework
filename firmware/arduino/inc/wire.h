#ifndef WIRE_H
#define WIRE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file Wire.h
 * @brief Arduino-style I2C API for EduFramework_v2.
 *
 * @details
 * LPI2C0 is shared by multiple 7-bit I2C devices on the same bus.
 * All operations use the target address supplied for that transaction.
 * Master blocking and interrupt-based transfers are supported.
 * The module is not thread-safe; do not access Wire concurrently from
 * multiple contexts or use blocking methods inside an I2C callback/ISR.
 */

typedef uint8_t Wire_Status_t;

#define WIRE_STATUS_OK       ((Wire_Status_t)0U)
#define WIRE_STATUS_BUSY     ((Wire_Status_t)1U)
#define WIRE_STATUS_TIMEOUT  ((Wire_Status_t)2U)
#define WIRE_STATUS_NACK     ((Wire_Status_t)3U)
#define WIRE_STATUS_ERROR    ((Wire_Status_t)4U)

/** Initialize master mode once. Repeated calls in master mode do nothing. */
void Wire_begin(void);

/** Initialize slave mode at the specified 7-bit address. */
void Wire_beginAddress(uint8_t address);

/**
 * @brief Set master clock speed (currently 100000 or 400000 Hz).
 * @note Reconfigures the peripheral. Do not call during a transfer.
 */
void Wire_setClock(uint32_t frequency);

/** Begin buffered master write to a 7-bit slave address. */
void Wire_beginTransmission(uint8_t address);

/** Append one byte; return 1 on success or 0 on failure. */
uint8_t Wire_write(uint8_t data);

/** Append bytes; return the number accepted (max TX buffer: 32 bytes). */
uint8_t Wire_writeBuffer(const uint8_t *data, uint8_t length);

/**
 * @brief Send buffered data and STOP. Zero-length write is supported for ACK probing.
 * @return WIRE_STATUS_*.
 */
uint8_t Wire_endTransmission(void);

/**
 * @brief Request bytes and STOP, storing up to 32 bytes internally.
 * @return Number of bytes received, or zero on error.
 */
uint8_t Wire_requestFrom(uint8_t address, uint8_t quantity);

/** Number of unread bytes in internal RX buffer. */
int Wire_available(void);

/** Next received byte, or -1 when no data remains. */
int Wire_read(void);

/**
 * @brief One blocking transaction with optional write and optional read.
 *
 * @details
 * Write+read uses a repeated START without an intermediate STOP.
 * For write-only or read-only, the unused buffer must be NULL and size 0.
 * Buffers are supplied by the caller (not limited by Wire's 32-byte buffers);
 * the low-level driver's transfer-size limits still apply.
 * @param address 7-bit slave address.
 * @param txData Data to send; may be NULL when txSize is zero.
 * @param txSize Number of bytes to send.
 * @param rxData Receive buffer; may be NULL when rxSize is zero.
 * @param rxSize Number of bytes to receive.
 * @return WIRE_STATUS_*.
 */
uint8_t Wire_transferBlocking(uint8_t address,
                              const uint8_t *txData,
                              uint16_t txSize,
                              uint8_t *rxData,
                              uint16_t rxSize);

/** Slave receive callback registration. */
void Wire_onReceive(void (*callback)(int));

/** Slave transmit request callback registration. */
void Wire_onRequest(void (*callback)(void));

/**
 * @brief Begin an interrupt-based master transfer (no buffer copy).
 * @note Caller must keep txData/rxData valid until the operation completes.
 *       Do not start another transfer while this one is active.
 */
uint8_t Wire_transferAsync(uint8_t address,
                           const uint8_t *txData,
                           uint16_t txSize,
                           uint8_t *rxData,
                           uint16_t rxSize);

/** 1 while an asynchronous master transfer is in progress. */
uint8_t Wire_isBusy(void);

/** 1 after the most recently started asynchronous transfer succeeds. */
uint8_t Wire_isDone(void);

/** Last Wire result (poll this for final asynchronous transfer status). */
uint8_t Wire_getStatus(void);

#endif /* WIRE_H */
