#ifndef LPSPI_H
#define LPSPI_H

/**
 * @file lpspi.h
 * @brief Low Power SPI driver public interface.
 *
 * @details
 * This file declares the register-level LPSPI driver APIs used by
 * EduFramework.
 *
 * The current driver focuses on LPSPI0 because this is the SPI instance
 * exposed by the current MaaZEDU board support configuration.
 *
 * Supported features:
 * - Master and slave role configuration.
 * - SPI clock mode 0, 1, 2, and 3.
 * - MSB-first and LSB-first transfer.
 * - 8-bit and 16-bit frame transfer.
 * - Blocking polling-based transfer with timeout protection.
 * - Runtime clock mode, bit order, frame size, and baudrate update.
 *
 * This driver belongs to the low-level driver layer. Application code
 * should normally use the Arduino-style SPI API instead of calling this
 * driver directly.
 */

#include "S32K144.h"
#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * LPSPI hardware instance
 * ============================================================ */

/**
 * @brief Default LPSPI hardware instance used by this driver.
 *
 * @details
 * The current EduFramework SPI stack is built around LPSPI0.
 */
#define LPSPI_INSTANCE    IP_LPSPI0

/* ============================================================
 * Driver status
 * ============================================================ */

/**
 * @brief LPSPI driver status type.
 */
typedef uint8_t lpspi_status_t;

#define LPSPI_STATUS_OK             ((lpspi_status_t)0U)
#define LPSPI_STATUS_ERROR          ((lpspi_status_t)1U)
#define LPSPI_STATUS_INVALID_ARG    ((lpspi_status_t)2U)
#define LPSPI_STATUS_NOT_INIT       ((lpspi_status_t)3U)
#define LPSPI_STATUS_TIMEOUT        ((lpspi_status_t)4U)

/* ============================================================
 * LPSPI role
 * ============================================================ */

/**
 * @brief LPSPI operating role type.
 */
typedef uint8_t lpspi_mode_t;

#define LPSPI_MODE_MASTER    ((lpspi_mode_t)0U)
#define LPSPI_MODE_SLAVE     ((lpspi_mode_t)1U)

/* ============================================================
 * SPI clock mode
 * ============================================================ */

/**
 * @brief SPI clock mode type.
 *
 * @details
 * SPI modes define CPOL and CPHA:
 * - Mode 0: CPOL = 0, CPHA = 0
 * - Mode 1: CPOL = 0, CPHA = 1
 * - Mode 2: CPOL = 1, CPHA = 0
 * - Mode 3: CPOL = 1, CPHA = 1
 */
typedef uint8_t lpspi_clock_mode_t;

#define LPSPI_MODE0    ((lpspi_clock_mode_t)0U)
#define LPSPI_MODE1    ((lpspi_clock_mode_t)1U)
#define LPSPI_MODE2    ((lpspi_clock_mode_t)2U)
#define LPSPI_MODE3    ((lpspi_clock_mode_t)3U)

/* ============================================================
 * Bit order
 * ============================================================ */

/**
 * @brief LPSPI bit order type.
 */
typedef uint8_t lpspi_bit_order_t;

#define LPSPI_MSB_FIRST    ((lpspi_bit_order_t)0U)
#define LPSPI_LSB_FIRST    ((lpspi_bit_order_t)1U)

/* ============================================================
 * Frame size
 * ============================================================ */

/**
 * @brief LPSPI frame size type.
 *
 * @details
 * The values represent the actual number of bits in one SPI frame.
 */
typedef uint8_t lpspi_frameSize_t;

#define LPSPI_FRAME_SIZE_8     ((lpspi_frameSize_t)8U)
#define LPSPI_FRAME_SIZE_16    ((lpspi_frameSize_t)16U)

/* ============================================================
 * Configuration structure
 * ============================================================ */

/**
 * @brief LPSPI initialization configuration.
 */
typedef struct
{
    lpspi_mode_t mode;                 /**< Master or slave role. */
    lpspi_clock_mode_t clockMode;      /**< SPI clock mode. */
    lpspi_bit_order_t bitOrder;        /**< MSB-first or LSB-first transfer. */
    lpspi_frameSize_t frameSize;       /**< SPI frame size in bits. */
    uint32_t baudrate;                 /**< Target baudrate in Hz, used in master mode. */
} lpspi_config_t;

/* ============================================================
 * Init / control API
 * ============================================================ */

/**
 * @brief Initialize LPSPI0.
 *
 * @details
 * This function configures the LPSPI0 peripheral clock, operating role,
 * pin behavior, frame format, FIFO watermarks, and baudrate.
 *
 * In master mode, baudrate is converted to the closest supported
 * PRESCALE and SCKDIV configuration that does not exceed the requested
 * target baudrate when possible.
 *
 * @param[in] config
 * Pointer to LPSPI configuration.
 *
 * @return lpspi_status_t
 *
 * @retval LPSPI_STATUS_OK
 * LPSPI initialized successfully.
 *
 * @retval LPSPI_STATUS_INVALID_ARG
 * Configuration pointer or field is invalid.
 *
 * @retval LPSPI_STATUS_ERROR
 * Baudrate calculation or hardware setup failed.
 */
lpspi_status_t LPSPI_Init(const lpspi_config_t *config);

/**
 * @brief Enable the LPSPI module.
 *
 * @details
 * This function sets the module enable bit after the peripheral has been
 * initialized.
 *
 * @return None.
 */
void LPSPI_Enable(void);

/**
 * @brief Disable the LPSPI module.
 *
 * @details
 * This function clears the module enable bit. Configuration registers are
 * not reset.
 *
 * @return None.
 */
void LPSPI_Disable(void);

/* ============================================================
 * Runtime configuration API
 * ============================================================ */

/**
 * @brief Set SPI clock mode.
 *
 * @param[in] mode
 * SPI clock mode.
 *
 * @return lpspi_status_t
 *
 * @retval LPSPI_STATUS_OK
 * Clock mode updated successfully.
 *
 * @retval LPSPI_STATUS_INVALID_ARG
 * Unsupported clock mode.
 *
 * @retval LPSPI_STATUS_NOT_INIT
 * LPSPI has not been initialized.
 *
 * @retval LPSPI_STATUS_TIMEOUT
 * Module did not become idle before reconfiguration.
 */
lpspi_status_t LPSPI_SetMode(lpspi_clock_mode_t mode);

/**
 * @brief Set SPI bit order.
 *
 * @param[in] bitOrder
 * Bit order configuration.
 *
 * @return lpspi_status_t
 *
 * @retval LPSPI_STATUS_OK
 * Bit order updated successfully.
 *
 * @retval LPSPI_STATUS_INVALID_ARG
 * Unsupported bit order.
 *
 * @retval LPSPI_STATUS_NOT_INIT
 * LPSPI has not been initialized.
 *
 * @retval LPSPI_STATUS_TIMEOUT
 * Module did not become idle before reconfiguration.
 */
lpspi_status_t LPSPI_SetBitOrder(lpspi_bit_order_t bitOrder);

/**
 * @brief Set SPI baudrate.
 *
 * @details
 * This function recalculates the LPSPI PRESCALE and SCKDIV fields based
 * on the requested target baudrate.
 *
 * @param[in] baudrate
 * Target SPI baudrate in Hz.
 *
 * @return lpspi_status_t
 *
 * @retval LPSPI_STATUS_OK
 * Baudrate updated successfully.
 *
 * @retval LPSPI_STATUS_INVALID_ARG
 * Baudrate is invalid.
 *
 * @retval LPSPI_STATUS_NOT_INIT
 * LPSPI has not been initialized.
 *
 * @retval LPSPI_STATUS_TIMEOUT
 * Module did not become idle before reconfiguration.
 *
 * @retval LPSPI_STATUS_ERROR
 * Requested baudrate cannot be generated by the current clock setup.
 */
lpspi_status_t LPSPI_SetBaudRate(uint32_t baudrate);

/**
 * @brief Set SPI frame size.
 *
 * @details
 * This function updates the LPSPI TCR FRAMESZ field. It is kept as a
 * void function to preserve the existing public API.
 *
 * @param[in] frameSize
 * Target frame size.
 *
 * @return None.
 */
void LPSPI_SetFrameSize(lpspi_frameSize_t frameSize);

/* ============================================================
 * Transfer API
 * ============================================================ */

/**
 * @brief Transfer one 8-bit SPI frame.
 *
 * @details
 * In SPI master mode, writing one frame also receives one frame. This
 * function writes txData and stores the received byte in rxData.
 *
 * @param[in] txData
 * Byte to transmit.
 *
 * @param[out] rxData
 * Pointer used to receive one byte.
 *
 * @return lpspi_status_t
 *
 * @retval LPSPI_STATUS_OK
 * Transfer completed successfully.
 *
 * @retval LPSPI_STATUS_INVALID_ARG
 * rxData pointer is invalid.
 *
 * @retval LPSPI_STATUS_NOT_INIT
 * LPSPI has not been initialized or enabled.
 *
 * @retval LPSPI_STATUS_TIMEOUT
 * TX or RX polling timed out.
 */
lpspi_status_t LPSPI_Transfer8(uint8_t txData, uint8_t *rxData);

/**
 * @brief Transfer one 16-bit SPI frame.
 *
 * @param[in] txData
 * 16-bit data to transmit.
 *
 * @param[out] rxData
 * Pointer used to receive 16-bit data.
 *
 * @return lpspi_status_t
 *
 * @retval LPSPI_STATUS_OK
 * Transfer completed successfully.
 *
 * @retval LPSPI_STATUS_INVALID_ARG
 * rxData pointer is invalid.
 *
 * @retval LPSPI_STATUS_NOT_INIT
 * LPSPI has not been initialized or enabled.
 *
 * @retval LPSPI_STATUS_TIMEOUT
 * TX or RX polling timed out.
 */
lpspi_status_t LPSPI_Transfer16(uint16_t txData, uint16_t *rxData);

/**
 * @brief Transfer a buffer using 8-bit SPI frames.
 *
 * @details
 * If txBuf is null, dummy bytes are transmitted. If rxBuf is null,
 * received bytes are discarded.
 *
 * At least one of txBuf or rxBuf must be non-null.
 *
 * @param[in] txBuf
 * Transmit buffer pointer. Can be null for read-only transfer.
 *
 * @param[out] rxBuf
 * Receive buffer pointer. Can be null for write-only transfer.
 *
 * @param[in] length
 * Number of bytes to transfer.
 *
 * @return lpspi_status_t
 *
 * @retval LPSPI_STATUS_OK
 * Buffer transfer completed successfully.
 *
 * @retval LPSPI_STATUS_INVALID_ARG
 * Both buffers are null.
 *
 * @retval LPSPI_STATUS_NOT_INIT
 * LPSPI has not been initialized or enabled.
 *
 * @retval LPSPI_STATUS_TIMEOUT
 * TX or RX polling timed out.
 */
lpspi_status_t LPSPI_TransferBuffer(const uint8_t *txBuf,
                                    uint8_t *rxBuf,
                                    uint32_t length);

/* ============================================================
 * Basic read / write API
 * ============================================================ */

/**
 * @brief Write one 8-bit SPI frame.
 *
 * @details
 * In master mode, this function discards the received byte generated by
 * the SPI clock. This prevents stale data from accumulating in RX FIFO.
 *
 * @param[in] data
 * Byte to write.
 *
 * @return lpspi_status_t
 */
lpspi_status_t LPSPI_Write8(uint8_t data);

/**
 * @brief Write one 16-bit SPI frame.
 *
 * @details
 * In master mode, this function discards the received frame generated by
 * the SPI clock.
 *
 * @param[in] data
 * 16-bit data to write.
 *
 * @return lpspi_status_t
 */
lpspi_status_t LPSPI_Write16(uint16_t data);

/**
 * @brief Read one 8-bit SPI frame.
 *
 * @details
 * In master mode, SPI cannot read without generating clock. Therefore,
 * this function transmits a dummy byte and returns the received byte.
 *
 * @param[out] data
 * Pointer used to receive one byte.
 *
 * @return lpspi_status_t
 */
lpspi_status_t LPSPI_Read8(uint8_t *data);

/**
 * @brief Read one 16-bit SPI frame.
 *
 * @details
 * In master mode, this function transmits a dummy 16-bit frame to
 * generate clock for reading.
 *
 * @param[out] data
 * Pointer used to receive 16-bit data.
 *
 * @return lpspi_status_t
 */
lpspi_status_t LPSPI_Read16(uint16_t *data);

/* ============================================================
 * Status API
 * ============================================================ */

/**
 * @brief Check whether TX FIFO can accept data.
 *
 * @return bool
 *
 * @retval true TX data can be written.
 * @retval false TX FIFO is not ready.
 */
bool LPSPI_IsTxReady(void);

/**
 * @brief Check whether RX FIFO contains unread data.
 *
 * @return bool
 *
 * @retval true RX data is available.
 * @retval false RX FIFO is empty.
 */
bool LPSPI_IsRxReady(void);

/**
 * @brief Check whether the last transfer is complete.
 *
 * @return bool
 *
 * @retval true Transfer complete flag is set.
 * @retval false Transfer complete flag is not set.
 */
bool LPSPI_IsTransferComplete(void);

#endif /* LPSPI_H */
