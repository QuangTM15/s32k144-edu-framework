/**
 * @file rc522.h
 * @brief MFRC522 RFID device library for EduFramework.
 *
 * @details
 * This module provides an Arduino-style interface for an
 * MFRC522-compatible RFID reader connected to EduFramework LPSPI0.
 *
 * Hardware connection:
 *
 * @code
 * MFRC522 SCK  -> EduFramework SPI_SCK
 * MFRC522 MOSI -> EduFramework SPI_SOUT
 * MFRC522 MISO -> EduFramework SPI_SIN
 * MFRC522 SDA  -> User-selected digital GPIO used as software chip-select
 * MFRC522 RST  -> User-selected digital GPIO used as hardware reset
 * @endcode
 *
 * The MFRC522 pin labelled SDA acts as the active-low SPI chip-select
 * signal when the module operates in SPI mode.
 *
 * Supported features:
 *
 * - User-selected software chip-select and reset pins.
 * - Hardware and software reset.
 * - MFRC522 register access.
 * - Hardware CRC calculation.
 * - FIFO-based communication.
 * - ISO/IEC 14443A REQA.
 * - Cascade Level 1 anti-collision and selection.
 * - Four-byte UID reading.
 * - SAK reading.
 * - PICC halt command.
 *
 * Current limitations:
 *
 * - Only single-size, four-byte UIDs are supported.
 * - Seven-byte and ten-byte cascade UIDs are not supported.
 * - MIFARE authentication and block read/write are not supported.
 */

#ifndef RC522_H
#define RC522_H

#include <stdbool.h>
#include <stdint.h>

/* ========================================================================= */
/* Public Constants                                                          */
/* ========================================================================= */

/**
 * @brief Maximum UID storage reserved by the public UID structure.
 */
#define RC522_UID_MAX_SIZE (10U)

/**
 * @brief UID size supported by the current implementation.
 */
#define RC522_UID_SINGLE_SIZE (4U)

/**
 * @brief Invalid or unavailable VersionReg value.
 */
#define RC522_VERSION_INVALID (0x00U)

/**
 * @brief Version value returned by the tested compatible module.
 *
 * @details
 * The tested module returned 0x82 on both ESP32 and S32K144 and
 * successfully detected and selected MIFARE Classic cards.
 */
#define RC522_VERSION_COMPATIBLE_82 (0x82U)

/**
 * @brief Common compatible-clone VersionReg value.
 */
#define RC522_VERSION_CLONE_88 (0x88U)

/**
 * @brief MFRC522 silicon version 1.0.
 */
#define RC522_VERSION_1_0 (0x91U)

/**
 * @brief MFRC522 silicon version 2.0.
 */
#define RC522_VERSION_2_0 (0x92U)

/* ========================================================================= */
/* Public Types                                                              */
/* ========================================================================= */

/**
 * @brief RFID card UID information.
 */
typedef struct
{
    /**
     * @brief UID byte storage.
     */
    uint8_t bytes[RC522_UID_MAX_SIZE];

    /**
     * @brief Number of valid UID bytes.
     */
    uint8_t size;

    /**
     * @brief Select Acknowledge value returned by the card.
     */
    uint8_t sak;
} rc522_uid_t;

/* ========================================================================= */
/* PCD API                                                                   */
/* ========================================================================= */

/**
 * @brief Initialize the MFRC522 reader.
 *
 * @details
 * This function:
 *
 * 1. Validates the selected chip-select and reset pins.
 * 2. Configures both pins as digital outputs.
 * 3. Initializes LPSPI0 as a 1 MHz Mode 0 master.
 * 4. Performs hardware and software reset.
 * 5. Applies the default MFRC522 configuration.
 * 6. Enables the RF antenna driver.
 * 7. Validates SPI communication using VersionReg.
 *
 * @param[in] csPin
 * Digital-capable logical pin connected to MFRC522 SDA/SS.
 *
 * @param[in] resetPin
 * Digital-capable logical pin connected to MFRC522 RST.
 *
 * @return Initialization state.
 *
 * @retval true
 * Initialization and communication validation succeeded.
 *
 * @retval false
 * Pin validation, reset, or communication validation failed.
 */
bool RC522_PCD_Init(uint8_t csPin,
                    uint8_t resetPin);

/**
 * @brief Reset the MFRC522.
 *
 * @details
 * This function performs hardware reset using the selected reset GPIO,
 * followed by an MFRC522 software-reset command.
 *
 * The complete default configuration is not reapplied by this function.
 * Call RC522_PCD_Init() to fully reinitialize the reader after a reset.
 *
 * @return Reset state.
 *
 * @retval true
 * Reset completed before timeout.
 *
 * @retval false
 * Control pins are not configured or reset timed out.
 */
bool RC522_PCD_Reset(void);

/**
 * @brief Enable the MFRC522 antenna driver.
 *
 * @details
 * This function enables Tx1 and Tx2 while preserving unrelated bits in
 * TxControlReg.
 *
 * @return None.
 */
void RC522_PCD_AntennaOn(void);

/**
 * @brief Disable the MFRC522 antenna driver.
 *
 * @details
 * This function disables Tx1 and Tx2 while preserving unrelated bits in
 * TxControlReg.
 *
 * @return None.
 */
void RC522_PCD_AntennaOff(void);

/**
 * @brief Read the MFRC522 VersionReg value.
 *
 * @return Raw VersionReg value.
 *
 * @retval RC522_VERSION_INVALID
 * The library is not initialized.
 */
uint8_t RC522_PCD_GetVersion(void);

/**
 * @brief Check whether the MFRC522 initialized successfully.
 *
 * @return Initialization state.
 *
 * @retval true
 * The reader is initialized.
 *
 * @retval false
 * The reader is not initialized.
 */
bool RC522_PCD_IsInitialized(void);

/* ========================================================================= */
/* PICC API                                                                  */
/* ========================================================================= */

/**
 * @brief Check whether a new ISO/IEC 14443A card is present.
 *
 * @details
 * This function sends the seven-bit REQA command and validates the
 * two-byte ATQA response.
 *
 * @return Card presence state.
 *
 * @retval true
 * A card returned a valid ATQA response.
 *
 * @retval false
 * No card responded or communication failed.
 */
bool RC522_PICC_IsNewCardPresent(void);

/**
 * @brief Read and select a four-byte UID card.
 *
 * @details
 * This function:
 *
 * 1. Performs Cascade Level 1 anti-collision.
 * 2. Validates the UID block-check character.
 * 3. Selects the card.
 * 4. Validates the SAK CRC.
 * 5. Stores the four-byte UID and SAK.
 *
 * @param[out] pUid
 * Destination UID structure.
 *
 * @return UID read state.
 *
 * @retval true
 * UID and SAK were read successfully.
 *
 * @retval false
 * The argument is invalid, communication failed, or the card uses an
 * unsupported UID cascade level.
 */
bool RC522_PICC_ReadCardSerial(rc522_uid_t *pUid);

/**
 * @brief Put the selected card into the HALT state.
 *
 * @details
 * A correctly halted PICC does not return a normal response. A communication
 * timeout after sending HLTA is therefore expected.
 *
 * @return None.
 */
void RC522_PICC_HaltA(void);

#endif /* RC522_H */
