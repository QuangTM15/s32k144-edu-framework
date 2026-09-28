/**
 * @file sd_card.h
 * @brief SD card device library for EduFramework.
 *
 * @details
 * This module provides low-level SD card access through the
 * EduFramework SPI interface.
 *
 * Hardware connection:
 *
 * @code
 * SD Card SCK  -> EduFramework SPI_SCK
 * SD Card MOSI -> EduFramework SPI_SOUT
 * SD Card MISO -> EduFramework SPI_SIN
 * SD Card CS   -> User-selected digital GPIO used as software chip-select
 * @endcode
 *
 * The current implementation provides:
 *
 * - User-selected software chip-select.
 * - SD card initialization in SPI mode.
 * - SD version detection.
 * - Byte-addressed and block-addressed card support.
 * - Single-block read.
 * - Single-block write.
 *
 * Filesystem operations are not implemented by this module.
 */

#ifndef SD_CARD_H
#define SD_CARD_H

#include <stdbool.h>
#include <stdint.h>

/* ========================================================================= */
/* Public Constants                                                          */
/* ========================================================================= */

/**
 * @brief Size of one SD card data block in bytes.
 */
#define SD_BLOCK_SIZE (512U)

/* ========================================================================= */
/* Public API                                                                */
/* ========================================================================= */

/**
 * @brief Initialize the SD card.
 *
 * @details
 * This function:
 *
 * 1. Validates the selected chip-select pin.
 * 2. Configures the chip-select pin as a digital output.
 * 3. Initializes SPI in Mode 0 with MSB-first transmission.
 * 4. Provides the required startup clocks with chip-select high.
 * 5. Enters SD SPI mode using CMD0.
 * 6. Checks the interface condition using CMD8.
 * 7. Completes card initialization using CMD55 and ACMD41.
 * 8. Reads the OCR using CMD58.
 * 9. Determines whether block addressing is supported.
 * 10. Switches SPI to the normal operating frequency.
 *
 * @param[in] csPin
 * Digital-capable logical pin connected to the SD card CS signal.
 *
 * @return Initialization state.
 *
 * @retval true
 * SD card initialization succeeded.
 *
 * @retval false
 * Pin validation or SD card initialization failed.
 */
bool SD_Begin(uint8_t csPin);

/**
 * @brief Check whether the SD card is initialized.
 *
 * @return Initialization state.
 *
 * @retval true
 * The SD card is initialized.
 *
 * @retval false
 * The SD card is not initialized.
 */
bool SD_IsInitialized(void);

/**
 * @brief Read one 512-byte block from the SD card.
 *
 * @param[in] block
 * Logical block number.
 *
 * @param[out] pBuffer
 * Destination buffer with space for at least SD_BLOCK_SIZE bytes.
 *
 * @return Read state.
 *
 * @retval true
 * The block was read successfully.
 *
 * @retval false
 * The card is not initialized, the buffer is invalid,
 * or communication failed.
 */
bool SD_ReadBlock(uint32_t block,
                  uint8_t *pBuffer);

/**
 * @brief Write one 512-byte block to the SD card.
 *
 * @param[in] block
 * Logical block number.
 *
 * @param[in] pBuffer
 * Source buffer containing SD_BLOCK_SIZE bytes.
 *
 * @return Write state.
 *
 * @retval true
 * The block was written successfully.
 *
 * @retval false
 * The card is not initialized, the buffer is invalid,
 * or communication failed.
 *
 * @warning
 * Writing an arbitrary block can corrupt an existing filesystem.
 */
bool SD_WriteBlock(uint32_t block,
                   const uint8_t *pBuffer);

#endif /* SD_CARD_H */
