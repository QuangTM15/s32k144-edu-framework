/**
 * @file spi.h
 * @brief Arduino-style SPI API for EduFramework on NXP S32K144.
 *
 * @details
 * This header provides a simple Arduino-like SPI interface built on top
 * of the low-level LPSPI driver.
 *
 * The API is intentionally simple for educational use:
 * - Single SPI bus
 * - Blocking transfer
 * - 8-bit and 16-bit transfer helpers
 * - Master and slave role selection
 *
 * The current implementation uses LPSPI0 with the MaaZEDU-tested pin map:
 * - PTB0 = PCS0 / CS
 * - PTB1 = SOUT / MOSI
 * - PTB2 = SCK
 * - PTB3 = SIN  / MISO
 */

#ifndef SPI_H
#define SPI_H

#include <stdint.h>
#include <stdbool.h>

/* ========================================================================= */
/* Public Types                                                               */
/* ========================================================================= */

/**
 * @brief SPI role type.
 */
typedef uint8_t SPI_Role_t;

/**
 * @brief SPI master role.
 */
#define SPI_ROLE_MASTER        ((SPI_Role_t)0U)

/**
 * @brief SPI slave role.
 */
#define SPI_ROLE_SLAVE         ((SPI_Role_t)1U)

/**
 * @brief SPI clock mode type.
 */
typedef uint8_t SPI_Mode_t;

/**
 * @brief SPI mode 0: CPOL = 0, CPHA = 0.
 */
#define SPI_MODE0              ((SPI_Mode_t)0U)

/**
 * @brief SPI mode 1: CPOL = 0, CPHA = 1.
 */
#define SPI_MODE1              ((SPI_Mode_t)1U)

/**
 * @brief SPI mode 2: CPOL = 1, CPHA = 0.
 */
#define SPI_MODE2              ((SPI_Mode_t)2U)

/**
 * @brief SPI mode 3: CPOL = 1, CPHA = 1.
 */
#define SPI_MODE3              ((SPI_Mode_t)3U)

/**
 * @brief SPI bit order type.
 */
typedef uint8_t SPI_BitOrder_t;

/**
 * @brief Transmit most significant bit first.
 */
#define SPI_MSBFIRST           ((SPI_BitOrder_t)0U)

/**
 * @brief Transmit least significant bit first.
 */
#define SPI_LSBFIRST           ((SPI_BitOrder_t)1U)

/* ========================================================================= */
/* Public API                                                                 */
/* ========================================================================= */

/**
 * @brief Initialize the SPI bus with default settings.
 *
 * @details
 * In master mode, the default configuration is:
 * - Frequency: 1 MHz
 * - Mode: SPI_MODE0
 * - Bit order: SPI_MSBFIRST
 *
 * In slave mode, the frequency parameter is ignored because the clock is
 * provided by the external master.
 *
 * @param role SPI role.
 */
void SPI_begin(SPI_Role_t role);

/**
 * @brief Initialize the SPI bus with extended configuration.
 *
 * @details
 * This function configures the SPI pins, stores the selected SPI settings,
 * and initializes the low-level LPSPI driver.
 *
 * If the SPI bus has already been initialized, it is disabled first before
 * applying the new configuration.
 *
 * @param role SPI role.
 * @param frequency Target SPI frequency in Hz. Only used in master mode.
 * @param mode SPI clock mode.
 * @param bitOrder SPI bit order.
 */
void SPI_beginEx(SPI_Role_t role,
                 uint32_t frequency,
                 SPI_Mode_t mode,
                 SPI_BitOrder_t bitOrder);

/**
 * @brief Disable the SPI bus.
 *
 * @details
 * This function disables the underlying LPSPI module and marks the Arduino
 * SPI layer as uninitialized.
 */
void SPI_end(void);

/**
 * @brief Set SPI bus frequency.
 *
 * @details
 * This function updates the baudrate of the underlying LPSPI driver.
 * It only has effect when SPI is initialized in master mode.
 *
 * @param frequency Target SPI frequency in Hz.
 */
void SPI_setFrequency(uint32_t frequency);

/**
 * @brief Set SPI clock mode.
 *
 * @param mode SPI clock mode.
 */
void SPI_setDataMode(SPI_Mode_t mode);

/**
 * @brief Set SPI bit order.
 *
 * @param bitOrder SPI bit order.
 */
void SPI_setBitOrder(SPI_BitOrder_t bitOrder);

/**
 * @brief Transfer one 8-bit SPI frame.
 *
 * @details
 * SPI is full-duplex. One byte is transmitted and one byte is received
 * at the same time.
 *
 * @param data Byte to transmit.
 *
 * @return Received byte.
 *
 * @retval 0U SPI is not initialized or transfer failed.
 */
uint8_t SPI_transfer(uint8_t data);

/**
 * @brief Transfer one 16-bit SPI frame.
 *
 * @param data 16-bit value to transmit.
 *
 * @return Received 16-bit value.
 *
 * @retval 0U SPI is not initialized or transfer failed.
 */
uint16_t SPI_transfer16(uint16_t data);

/**
 * @brief Transfer a byte buffer.
 *
 * @details
 * This function supports:
 * - Full-duplex transfer: txBuffer != NULL and rxBuffer != NULL
 * - Write-only transfer: txBuffer != NULL and rxBuffer == NULL
 * - Read-only transfer: txBuffer == NULL and rxBuffer != NULL
 *
 * In read-only transfer, dummy bytes are transmitted by the low-level driver
 * to generate SPI clock in master mode.
 *
 * @param txBuffer Pointer to transmit buffer, or NULL for read-only transfer.
 * @param rxBuffer Pointer to receive buffer, or NULL for write-only transfer.
 * @param length Number of bytes to transfer.
 */
void SPI_transferBuffer(const uint8_t *txBuffer,
                        uint8_t *rxBuffer,
                        uint32_t length);

/**
 * @brief Check whether received SPI data is available.
 *
 * @return Availability state.
 *
 * @retval true At least one received frame is available.
 * @retval false No received frame is available or SPI is not initialized.
 */
bool SPI_available(void);

/**
 * @brief Read one 8-bit SPI frame.
 *
 * @details
 * In master mode, the low-level driver transmits a dummy byte to generate
 * the clock before reading the received byte.
 *
 * @return Received byte.
 *
 * @retval 0U SPI is not initialized or read failed.
 */
uint8_t SPI_read(void);

/**
 * @brief Read one 16-bit SPI frame.
 *
 * @details
 * In master mode, the low-level driver transmits a dummy frame to generate
 * the clock before reading the received frame.
 *
 * @return Received 16-bit value.
 *
 * @retval 0U SPI is not initialized or read failed.
 */
uint16_t SPI_read16(void);

/**
 * @brief Write one 8-bit SPI frame.
 *
 * @param data Byte to transmit.
 */
void SPI_write(uint8_t data);

/**
 * @brief Write one 16-bit SPI frame.
 *
 * @param data 16-bit value to transmit.
 */
void SPI_write16(uint16_t data);

/**
 * @brief Get current SPI role.
 *
 * @return Current SPI role.
 */
SPI_Role_t SPI_getRole(void);

#endif /* SPI_H */
