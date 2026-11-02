/**
 * @file pn532.h
 * @brief PN532 NFC controller driver for EduFramework (HSU / Serial2).
 * @note One PN532 instance, synchronous APIs, no dynamic allocation.
 *       Uses EduFramework Serial2 (LPUART2 RX interrupt), never raw UART registers.
 *       Not reentrant; millis() and delay() must be available.
 */
#ifndef PN532_H
#define PN532_H

#include <stdbool.h>
#include <stdint.h>

#define PN532_HSU_BAUD_RATE          (115200U)
#define PN532_UID_MAX_LENGTH         (10U)
#define PN532_MIFARE_KEY_LENGTH      (6U)
#define PN532_MIFARE_BLOCK_SIZE      (16U)
#define PN532_ULTRALIGHT_PAGE_SIZE   (4U)
#define PN532_MAX_FRAME_DATA         (300U)
#define PN532_DIAG_TRACE_CAPACITY    (48U)

#define PN532_CMD_GET_FIRMWARE       (0x02U)
#define PN532_CMD_SAM_CONFIGURATION  (0x14U)
#define PN532_CMD_RF_CONFIGURATION   (0x32U)
#define PN532_CMD_IN_DATA_EXCHANGE   (0x40U)
#define PN532_CMD_IN_LIST_PASSIVE    (0x4AU)
#define PN532_CMD_IN_RELEASE         (0x52U)

typedef enum
{
    PN532_STATUS_OK = 0,
    PN532_STATUS_INVALID_ARGUMENT,
    PN532_STATUS_NOT_INITIALIZED,
    PN532_STATUS_UART_ERROR,
    PN532_STATUS_TIMEOUT,
    PN532_STATUS_NACK,
    PN532_STATUS_ACK_ERROR,
    PN532_STATUS_FRAME_ERROR,
    PN532_STATUS_CHECKSUM_ERROR,
    PN532_STATUS_BUFFER_TOO_SMALL,
    PN532_STATUS_PROTOCOL_ERROR,
    PN532_STATUS_DEVICE_ERROR,
    PN532_STATUS_NO_CARD,
    PN532_STATUS_NO_TARGET,
    PN532_STATUS_MORE_DATA
} PN532_Status_t;

/** @brief Stage recorded for the most recent PN532 transaction. */
typedef enum
{
    PN532_STAGE_IDLE = 0,
    PN532_STAGE_WAKE,
    PN532_STAGE_SEND_COMMAND,
    PN532_STAGE_WAIT_ACK,
    PN532_STAGE_WAIT_RESPONSE,
    PN532_STAGE_READY
} PN532_Stage_t;

/** @brief Diagnostic snapshot; RX trace includes the bytes consumed by the frame parser. */
typedef struct
{
    PN532_Stage_t eStage;
    uint8_t u8Command;
    uint8_t u8Attempt;
    uint16_t u16RxByteCount;
    uint8_t u8RxTraceLength;
    uint8_t au8RxTrace[PN532_DIAG_TRACE_CAPACITY];
} PN532_Diagnostics_t;

typedef struct
{
    uint32_t u32AckTimeoutMs;
    uint32_t u32ResponseTimeoutMs;
    uint8_t u8PassiveActivationRetries;
    uint8_t u8CommandRetries; /* Retransmissions only when an ACK is not accepted. */
} PN532_Config_t;

typedef struct
{
    uint8_t u8IC;
    uint8_t u8Version;
    uint8_t u8Revision;
    uint8_t u8Support;
} PN532_Firmware_t;

typedef struct
{
    uint8_t u8TargetNumber;
    uint16_t u16ATQA;
    uint8_t u8SAK;
    uint8_t u8UIDLength;
    uint8_t au8UID[PN532_UID_MAX_LENGTH];
} PN532_Card_t;

/** @brief Initialize Serial2 and PN532 with tested 34-byte HSU wake preamble.
 *         Defaults: 100ms ACK, 1000ms response, one card activation attempt,
 *         no automatic command retransmission.
 */
PN532_Status_t PN532_Begin(void);
/** @brief Initialize Serial2 and PN532 with custom timeouts and card polling retries. */
PN532_Status_t PN532_Init(const PN532_Config_t *pxConfig);
/** @brief Read the PN532 firmware identification (4 bytes). */
PN532_Status_t PN532_GetFirmwareVersion(PN532_Firmware_t *pxFirmware);
/** @brief Execute a PN532 command. Response excludes TFI and response command code. */
PN532_Status_t PN532_Transceive(uint8_t u8Command, const uint8_t *pu8Parameters, uint16_t u16ParameterLength, uint8_t *pu8Response, uint16_t u16ResponseCapacity, uint16_t *pu16ResponseLength);
/** @brief Configure Normal SAM mode for NFC reading. */
PN532_Status_t PN532_SAMConfiguration(void);
/** @brief Turn the PN532 RF field on or off. */
PN532_Status_t PN532_SetRFField(bool bEnabled);
/** @brief Set ISO14443A passive target activation retries (0 = one attempt, 0xFF = infinite).
 * @warning Infinite retry mode may require waiting indefinitely for a card.
 *          On host response timeout, the driver aborts the pending command.
 */
PN532_Status_t PN532_SetPassiveActivationRetries(uint8_t u8Retries);
/** @brief Poll for one ISO14443A card and obtain UID, ATQA and SAK. */
PN532_Status_t PN532_ReadPassiveTarget(PN532_Card_t *pxCard);
/** @brief Convenience wrapper to read the current card UID. */
PN532_Status_t PN532_ReadUID(uint8_t *pu8UID, uint8_t u8Capacity, uint8_t *pu8UIDLength);
/** @brief Return true if a card is discovered by polling. Errors are available using GetLastError. */
bool PN532_IsCardPresent(void);
/** @brief Send bytes to an activated target using InDataExchange. */
PN532_Status_t PN532_DataExchange(const uint8_t *pu8TxData, uint16_t u16TxLength, uint8_t *pu8RxData, uint16_t u16RxCapacity, uint16_t *pu16RxLength);
/** @brief Authenticate MIFARE Classic block using the last detected card UID. */
PN532_Status_t PN532_MifareAuthenticate(uint8_t u8Block, bool bKeyB, const uint8_t au8Key[PN532_MIFARE_KEY_LENGTH]);
/** @brief Read one authenticated MIFARE Classic 16-byte block. */
PN532_Status_t PN532_MifareReadBlock(uint8_t u8Block, uint8_t au8Data[PN532_MIFARE_BLOCK_SIZE]);
/** @brief Write one MIFARE Classic 16-byte block (irreversible data change).
 * @warning Never write manufacturer blocks or sector trailers without validation.
 */
PN532_Status_t PN532_MifareWriteBlock(uint8_t u8Block, const uint8_t au8Data[PN532_MIFARE_BLOCK_SIZE]);
/** @brief Read four consecutive Ultralight/NTAG pages (16 bytes). */
PN532_Status_t PN532_UltralightReadPages(uint8_t u8FirstPage, uint8_t au8Data[PN532_MIFARE_BLOCK_SIZE]);
/** @brief Write one Ultralight/NTAG 4-byte page (irreversible data change).
 * @warning Never write lock, OTP, configuration, or authentication pages blindly.
 */
PN532_Status_t PN532_UltralightWritePage(uint8_t u8Page, const uint8_t au8Data[PN532_ULTRALIGHT_PAGE_SIZE]);
/** @brief Release the currently activated target, if any. */
PN532_Status_t PN532_ReleaseTarget(void);
/** @brief Retrieve the result of the most recent PN532 public operation. */
PN532_Status_t PN532_GetLastError(void);
/** @brief Retrieve the most recent PN532 InDataExchange status byte. */
uint8_t PN532_GetLastChipStatus(void);
/** @brief Convert an EduFramework PN532 status to an English label. */
const char *PN532_StatusString(PN532_Status_t eStatus);
/** @brief Read the latest command, timeout stage and captured raw RX bytes. */
void PN532_GetDiagnostics(PN532_Diagnostics_t *pxDiagnostics);
/** @brief Convert an internal transaction stage to a printable label. */
const char *PN532_StageString(PN532_Stage_t eStage);


/* ======================= EASY API ===========================
 * Begin() and IsCardPresent() perform I/O.
 * Getters read the most recently detected card, without rescanning.
 * String pointers refer to static storage and must not be modified.
 * This driver is single-instance and not thread-safe.
 */
#define PN532_UID_TEXT_LENGTH (PN532_UID_MAX_LENGTH * 3U)

const char *PN532_GetUID(void);           /* "EE 68 17 05"; empty if invalid */
uint8_t PN532_GetUIDLength(void);         /* 0 if no valid target */
uint16_t PN532_GetATQA(void);             /* 0 if no valid target */
uint8_t PN532_GetSAK(void);               /* 0 if no valid target */
const char *PN532_GetCardType(void);      /* heuristic only, not proof of type */

/* Key must be exactly 12 hexadecimal characters, e.g. "FFFFFFFFFFFF".
 * Returns false on error; GetLastError() gives the reason.
 */
bool PN532_Authenticate(uint8_t u8Block, const char *pcKeyHex);

/* Cached block access: reads 16 bytes, exposes them as hex text.
 * ReadBlock() is for MIFARE Classic; requires prior authentication.
 */
bool PN532_ReadBlock(uint8_t u8Block);
const char *PN532_GetBlockHex(void);      /* empty if cache invalid */

/* Explicitly release the current target. No PN532 power-down implied. */
bool PN532_End(void);

#endif /* PN532_H */
