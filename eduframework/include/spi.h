#ifndef SPI_H
#define SPI_H

/**
 * @file spi.h
 * @brief Arduino-style SPI0 wrapper with independent transactions.
 * @details One hardware LPSPI0 bus can serve any number of logical SPI slaves.
 * Each external device owns a GPIO chip-select and its own SPI_Settings_t.
 * SPI_beginTransaction() configures the bus but DOES NOT assert chip-select.
 * The caller must assert one CS after beginTransaction() succeeds and release
 * it after SPI_endTransaction() returns. All other slave CS pins must stay HIGH.
 * The wrapper is blocking, single-context, and not interrupt/thread-safe.
 */

#include <stdbool.h>
#include <stdint.h>

typedef uint8_t SPI_Role_t;
#define SPI_ROLE_MASTER ((SPI_Role_t)0U)
#define SPI_ROLE_SLAVE  ((SPI_Role_t)1U)

typedef uint8_t SPI_Mode_t;
#define SPI_MODE0 ((SPI_Mode_t)0U)
#define SPI_MODE1 ((SPI_Mode_t)1U)
#define SPI_MODE2 ((SPI_Mode_t)2U)
#define SPI_MODE3 ((SPI_Mode_t)3U)

typedef uint8_t SPI_BitOrder_t;
#define SPI_MSBFIRST ((SPI_BitOrder_t)0U)
#define SPI_LSBFIRST ((SPI_BitOrder_t)1U)

typedef uint8_t SPI_FrameSize_t;
#define SPI_FRAME_SIZE_8  ((SPI_FrameSize_t)8U)
#define SPI_FRAME_SIZE_16 ((SPI_FrameSize_t)16U)

typedef uint8_t SPI_Status_t;
#define SPI_STATUS_OK             ((SPI_Status_t)0U)
#define SPI_STATUS_ERROR          ((SPI_Status_t)1U)
#define SPI_STATUS_INVALID_ARG    ((SPI_Status_t)2U)
#define SPI_STATUS_NOT_INIT       ((SPI_Status_t)3U)
#define SPI_STATUS_TIMEOUT        ((SPI_Status_t)4U)
#define SPI_STATUS_FIFO_ERROR     ((SPI_Status_t)5U)
#define SPI_STATUS_BUSY           ((SPI_Status_t)6U)
#define SPI_STATUS_NO_TRANSACTION ((SPI_Status_t)7U)
#define SPI_STATUS_UNSUPPORTED    ((SPI_Status_t)8U)

/** @brief Per-device settings, passed directly at each transaction boundary. */
typedef struct
{
    SPI_Mode_t mode;
    SPI_BitOrder_t bitOrder;
    SPI_FrameSize_t frameSize;
    uint32_t frequency;
} SPI_Settings_t;

/** @brief Initialize LPSPI0 with 1 MHz, mode 0, MSB-first (or slave default). */
void SPI_begin(SPI_Role_t role);

/**
 * @brief Initialize/reconfigure SPI0 without reinitializing an active master bus.
 * @details Subsequent calls with the same master role use LPSPI_ApplySettings().
 * Role changes require LPSPI reinitialization. Calls during a transaction fail.
 * A master frequency of zero selects the legacy 1 MHz default.
 */
void SPI_beginEx(SPI_Role_t role, uint32_t frequency, SPI_Mode_t mode, SPI_BitOrder_t bitOrder);

/** @brief Disable SPI0. Fails with SPI_STATUS_BUSY during a transaction. */
void SPI_end(void);

/**
 * @brief Configure a single master's device settings and enter a transaction.
 * @details Requires SPI_begin() and an inactive external GPIO CS. No CS is
 * asserted by this function. Nested/reentrant transactions are rejected.
 * @return SPI_STATUS_OK or an explicit configuration, state or driver error.
 */
SPI_Status_t SPI_beginTransaction(const SPI_Settings_t *settings);

/**
 * @brief Finish the active transaction after the last bit has left the bus.
 * @details The caller must keep CS LOW until this function returns and then
 * drive CS HIGH, including on an error. No GPIO pin is changed internally.
 * @return SPI_STATUS_NO_TRANSACTION if no transaction is active.
 */
SPI_Status_t SPI_endTransaction(void);

/** @brief Report whether a transaction is active in this single-context API. */
bool SPI_isTransactionActive(void);

/** @brief Last wrapper operation/transfer result (data 0 is NOT an error code). */
SPI_Status_t SPI_getLastStatus(void);

/** @brief Actual frequency in Hz, reflecting the LPSPI clock divider. */
uint32_t SPI_getActualFrequency(void);

/** @brief Legacy runtime setters; rejected during an active transaction. */
void SPI_setFrequency(uint32_t frequency);
void SPI_setDataMode(SPI_Mode_t mode);
void SPI_setBitOrder(SPI_BitOrder_t bitOrder);

/** @brief Legacy blocking transfers; zero result may be valid SPI data. */
uint8_t SPI_transfer(uint8_t data);
uint16_t SPI_transfer16(uint16_t data);
void SPI_transferBuffer(const uint8_t *txBuffer, uint8_t *rxBuffer, uint32_t length);
bool SPI_available(void);
uint8_t SPI_read(void);
uint16_t SPI_read16(void);
void SPI_write(uint8_t data);
void SPI_write16(uint16_t data);
SPI_Role_t SPI_getRole(void);

#endif /* SPI_H */
