#ifndef RC522_H
#define RC522_H

/**
 * @file rc522.h
 * @brief MFRC522 RFID library for the EduFramework shared LPSPI0 bus.
 * @details The application must call SPI_begin(SPI_ROLE_MASTER) before
 * RC522_PCD_Init(). CS is controlled by software GPIO. Each register access
 * owns one SPI transaction; the library never reinitializes the shared bus.
 * Supported: REQA, CL1 anticollision/selection, four-byte UID, SAK, HLTA.
 * Not supported: seven/ten-byte UIDs, authentication, block read/write.
 */

#include <stdbool.h>
#include <stdint.h>
#include "spi.h"

#define RC522_UID_MAX_SIZE (10U)
#define RC522_UID_SINGLE_SIZE (4U)
#define RC522_VERSION_INVALID (0x00U)
#define RC522_VERSION_COMPATIBLE_82 (0x82U)
#define RC522_VERSION_CLONE_88 (0x88U)
#define RC522_VERSION_1_0 (0x91U)
#define RC522_VERSION_2_0 (0x92U)
#define RC522_SPI_FREQUENCY_HZ (1000000UL)

typedef struct
{
    uint8_t bytes[RC522_UID_MAX_SIZE];
    uint8_t size;
    uint8_t sak;
} rc522_uid_t;

/** @brief Initialize GPIO, reset reader, configure RF and verify VersionReg. */
bool RC522_PCD_Init(uint8_t csPin, uint8_t resetPin);
/** @brief Hardware and software reset; full settings are not reapplied. */
bool RC522_PCD_Reset(void);
void RC522_PCD_AntennaOn(void);
void RC522_PCD_AntennaOff(void);
/** @brief Return VersionReg, or 0 if uninitialized/read failed. */
uint8_t RC522_PCD_GetVersion(void);
bool RC522_PCD_IsInitialized(void);
/** @brief Last SPI result recorded by this device, not the shared bus. */
SPI_Status_t RC522_PCD_GetLastSpiStatus(void);
/** @brief Send REQA; false also means no new card (normal timeout). */
bool RC522_PICC_IsNewCardPresent(void);
/** @brief Read CL1 four-byte UID; clears output before attempting read. */
bool RC522_PICC_ReadCardSerial(rc522_uid_t *uid);
/** @brief Halt the selected PICC; no response is expected. */
void RC522_PICC_HaltA(void);

#endif /* RC522_H */
