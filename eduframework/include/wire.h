#ifndef WIRE_H
#define WIRE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file Wire.h
 * @brief Arduino-style I2C API for EduFramework_v2.
 *
 * @details
 * This module provides a simple Arduino-style Wire API on top of the
 * low-level LPI2C driver.
 *
 * Current EduFramework_v2 scope:
 * - Use LPI2C0 as the default I2C peripheral.
 * - Board-specific SDA/SCL pin initialization is handled in Wire.c.
 * - Low-level LPI2C driver must not depend on board pin mapping.
 */

/* ============================================================
 * Status
 * ============================================================ */

/**
 * @brief Wire API status type.
 */
typedef uint8_t Wire_Status_t;

#define WIRE_STATUS_OK ((Wire_Status_t)0U)
#define WIRE_STATUS_BUSY ((Wire_Status_t)1U)
#define WIRE_STATUS_TIMEOUT ((Wire_Status_t)2U)
#define WIRE_STATUS_NACK ((Wire_Status_t)3U)
#define WIRE_STATUS_ERROR ((Wire_Status_t)4U)

/* ============================================================
 * Init
 * ============================================================ */

/**
 * @brief Initialize Wire in I2C master mode.
 *
 * @details
 * This function initializes the board I2C pins and configures LPI2C0
 * as an I2C master using the default configuration.
 */
void Wire_begin(void);

/**
 * @brief Initialize Wire in I2C slave mode.
 *
 * @details
 * This function initializes the board I2C pins and configures LPI2C0
 * as an I2C slave.
 *
 * @param address 7-bit slave address.
 */
void Wire_beginAddress(uint8_t address);

/**
 * @brief Set I2C master clock frequency.
 *
 * @details
 * This function is valid only when Wire is initialized in master mode.
 * It reconfigures the LPI2C master timing while keeping the Wire layer
 * in master role.
 *
 * @param frequency Target I2C bus frequency in Hz.
 */
void Wire_setClock(uint32_t frequency);

/* ============================================================
 * Master - Write
 * ============================================================ */

/**
 * @brief Begin a master transmit transaction.
 *
 * @details
 * This function stores the target slave address and clears the Wire
 * transmit buffer.
 *
 * @param address 7-bit slave address.
 */
void Wire_beginTransmission(uint8_t address);

/**
 * @brief Write one byte into the Wire transmit buffer.
 *
 * @param data Byte to store.
 *
 * @return 1 if the byte was stored successfully, otherwise 0.
 */
uint8_t Wire_write(uint8_t data);

/**
 * @brief Write multiple bytes into the Wire transmit buffer.
 *
 * @param data Pointer to source data buffer.
 * @param length Number of bytes to write.
 *
 * @return Number of bytes stored into the transmit buffer.
 */
uint8_t Wire_writeBuffer(const uint8_t *data, uint8_t length);

/**
 * @brief End master transmit transaction and send buffered data.
 *
 * @details
 * This function sends all bytes previously stored by Wire_write()
 * or Wire_writeBuffer().
 *
 * @return Wire status code.
 */
uint8_t Wire_endTransmission(void);

/* ============================================================
 * Master - Read
 * ============================================================ */

/**
 * @brief Request bytes from an I2C slave.
 *
 * @param address 7-bit slave address.
 * @param quantity Number of bytes requested.
 *
 * @return Number of bytes received and stored in the Wire RX buffer.
 */
uint8_t Wire_requestFrom(uint8_t address, uint8_t quantity);

/**
 * @brief Get number of unread bytes in the Wire receive buffer.
 *
 * @return Number of available bytes.
 */
int Wire_available(void);

/**
 * @brief Read one byte from the Wire receive buffer.
 *
 * @return Byte value in range 0..255, or -1 if no data is available.
 */
int Wire_read(void);

/* ============================================================
 * Slave API
 * ============================================================ */

/**
 * @brief Register slave receive callback.
 *
 * @details
 * The callback is called when a master write transaction ends and
 * received bytes are available in the Wire RX buffer.
 *
 * @param callback Function pointer with received byte count parameter.
 */
void Wire_onReceive(void (*callback)(int));

/**
 * @brief Register slave request callback.
 *
 * @details
 * The callback is called when a master requests data from this slave.
 * The application should fill the TX buffer by calling Wire_write().
 *
 * @param callback Function pointer.
 */
void Wire_onRequest(void (*callback)(void));

/* ============================================================
 * Optional - Async
 * ============================================================ */

/**
 * @brief Start an interrupt-based I2C master transfer.
 *
 * @param address 7-bit slave address.
 * @param txData Pointer to transmit data buffer.
 * @param txSize Number of bytes to transmit.
 * @param rxData Pointer to receive data buffer.
 * @param rxSize Number of bytes to receive.
 *
 * @return Wire status code.
 */
uint8_t Wire_transferAsync(uint8_t address,
                           const uint8_t *txData,
                           uint16_t txSize,
                           uint8_t *rxData,
                           uint16_t rxSize);

/**
 * @brief Check whether an async master transfer is busy.
 *
 * @retval 1 Transfer is running.
 * @retval 0 Transfer is idle, done, or failed.
 */
uint8_t Wire_isBusy(void);

/**
 * @brief Check whether an async master transfer is done.
 *
 * @retval 1 Transfer is done.
 * @retval 0 Transfer is not done.
 */
uint8_t Wire_isDone(void);

/**
 * @brief Get last Wire status.
 *
 * @return Wire status code.
 */
uint8_t Wire_getStatus(void);

#endif /* WIRE_H */