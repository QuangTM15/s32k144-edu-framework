/**
 * @file sd_card.c
 * @brief SD SPI-mode block driver and FatFs-backed storage API.
 * @details Shared LPSPI0 transactions: SD initialization at 400 kHz,
 * data at 4 MHz, Mode 0, MSB-first, 8-bit. No SPI_begin/ SPI_end here.
 * Caller must initialize the shared master bus before SD_Begin().
 * All operations are blocking, single-context, not ISR/RTOS safe.
 */
#include "sd_card.h"
#include "Arduino.h"
#include "arduino_pins.h"
#include "spi.h"
#include "ff.h"
#include <stddef.h>
#include <string.h>

#define SD_MAX_OPEN_FILES (4U)
#define SD_MAX_OPEN_DIRECTORIES (2U)
#define SD_LINE_ENDING "\r\n"
#define SD_LINE_ENDING_LENGTH (2U)
#define SD_SPI_INITIAL_FREQUENCY_HZ (400000UL)
#define SD_SPI_OPERATING_FREQUENCY_HZ (4000000UL)
#define SD_SPI_DUMMY_BYTE (0xFFU)
#define SD_STARTUP_DUMMY_BYTES (10U)
#define SD_CMD0_GO_IDLE_STATE (0U)
#define SD_CMD8_SEND_IF_COND (8U)
#define SD_CMD16_SET_BLOCKLEN (16U)
#define SD_CMD17_READ_SINGLE_BLOCK (17U)
#define SD_CMD24_WRITE_SINGLE_BLOCK (24U)
#define SD_CMD55_APP_CMD (55U)
#define SD_CMD58_READ_OCR (58U)
#define SD_ACMD41_SEND_OP_COND (41U)
#define SD_CMD0_ARGUMENT (0x00000000UL)
#define SD_CMD8_ARGUMENT (0x000001AAUL)
#define SD_ACMD41_HCS_ARGUMENT (0x40000000UL)
#define SD_CMD58_ARGUMENT (0x00000000UL)
#define SD_CMD16_BLOCK_LENGTH_ARGUMENT ((uint32_t)SD_BLOCK_SIZE)
#define SD_CMD0_CRC (0x95U)
#define SD_CMD8_CRC (0x87U)
#define SD_DEFAULT_CRC (0x01U)
#define SD_R1_READY_STATE (0x00U)
#define SD_R1_IDLE_STATE (0x01U)
#define SD_R1_ILLEGAL_COMMAND_MASK (0x04U)
#define SD_DATA_START_BLOCK_TOKEN (0xFEU)
#define SD_DATA_RESPONSE_MASK (0x1FU)
#define SD_DATA_RESPONSE_ACCEPTED (0x05U)
#define SD_COMMAND_RESPONSE_TIMEOUT_COUNT (16U)
#define SD_CMD0_RETRY_LIMIT (40U)
#define SD_INITIALIZATION_TIMEOUT_MS (2000UL)
#define SD_INITIALIZATION_MAX_POLLS (10000UL)
#define SD_DATA_TOKEN_TIMEOUT_MS (300UL)
#define SD_DATA_TOKEN_MAX_POLLS (100000UL)
#define SD_WRITE_BUSY_TIMEOUT_MS (600UL)
#define SD_WRITE_BUSY_MAX_POLLS (200000UL)

static uint8_t s_u8SdCsPin = GPIO6;
static bool s_bSdPinsConfigured = false;
static bool s_bSdInitialized = false;
static bool s_bSdBlockAddressing = false;
static bool s_bSdMounted = false;
static SPI_Status_t s_lastSpi = SPI_STATUS_NOT_INIT;
static FATFS s_sdFileSystem;
static FIL s_asdFilePool[SD_MAX_OPEN_FILES];
static bool s_abSdFileUsed[SD_MAX_OPEN_FILES] = {false};
static DIR s_asdDirectoryPool[SD_MAX_OPEN_DIRECTORIES];
static bool s_abSdDirectoryUsed[SD_MAX_OPEN_DIRECTORIES] = {false};
static const SPI_Settings_t s_initSettings =
    {SPI_MODE0, SPI_MSBFIRST, SPI_FRAME_SIZE_8, SD_SPI_INITIAL_FREQUENCY_HZ};
static const SPI_Settings_t s_runSettings =
    {SPI_MODE0, SPI_MSBFIRST, SPI_FRAME_SIZE_8, SD_SPI_OPERATING_FREQUENCY_HZ};

static void SD_ResetFileSystemState(void);
static bool SD_IsValidFileHandle(const SD_File_t *file);
static bool SD_IsValidDirectoryHandle(const SD_Dir_t *dir);
static bool SD_AllocateFileHandle(SD_File_t *file);
static void SD_ReleaseFileHandle(SD_File_t *file);
static bool SD_AllocateDirectoryHandle(SD_Dir_t *dir);
static void SD_ReleaseDirectoryHandle(SD_Dir_t *dir);
static BYTE SD_GetFatFsMode(SD_FileMode_t mode);
static void SD_CopyFileInfo(const FILINFO *pFatInfo, SD_FileInfo_t *pInfo);

/** @brief Acquire SD settings; do not assert CS on failure. */
static bool SD_BeginSession(bool initialization)
{
    bool success = false;
    const SPI_Settings_t *settings = initialization ? &s_initSettings : &s_runSettings;
    if (true == s_bSdPinsConfigured)
    {
        s_lastSpi = SPI_beginTransaction(settings);
        if (SPI_STATUS_OK == s_lastSpi)
        {
            digitalWrite(s_u8SdCsPin, LOW);
            success = true;
        }
    }
    return success;
}

/** @brief Exchange one byte, detecting hardware failures independently of data. */
static bool SD_Exchange(uint8_t tx, uint8_t *rx)
{
    bool success = false;
    uint8_t value = SPI_transfer(tx);
    SPI_Status_t status = SPI_getLastStatus();
    if (SPI_STATUS_OK == status)
    {
        if (NULL != rx)
        {
            *rx = value;
        }
        success = true;
    }
    else if (SPI_STATUS_OK == s_lastSpi)
    {
        s_lastSpi = status;
    }
    return success;
}

/** @brief Transfer a data block with the SPI wrapper's 0xFF read dummy. */
static bool SD_ExchangeBuffer(const uint8_t *tx, uint8_t *rx, uint32_t length)
{
    bool success = false;
    SPI_transferBuffer(tx, rx, length);
    SPI_Status_t status = SPI_getLastStatus();
    if (SPI_STATUS_OK == status)
    {
        success = true;
    }
    else if (SPI_STATUS_OK == s_lastSpi)
    {
        s_lastSpi = status;
    }
    return success;
}

/** @brief Eight extra clocks with SD deselected; never select another slave. */
static bool SD_ClockIdle(bool initialization, uint8_t count)
{
    bool success = false;
    SPI_Status_t firstError = s_lastSpi;
    const SPI_Settings_t *settings = initialization ? &s_initSettings : &s_runSettings;
    SPI_Status_t beginStatus = SPI_beginTransaction(settings);
    if (SPI_STATUS_OK == beginStatus)
    {
        uint8_t i = 0U;
        bool transferred = true;
        SPI_Status_t transferStatus = SPI_STATUS_OK;
        for (i = 0U; (i < count) && (true == transferred); i++)
        {
            (void)SPI_transfer(SD_SPI_DUMMY_BYTE);
            transferStatus = SPI_getLastStatus();
            transferred = (SPI_STATUS_OK == transferStatus);
        }
        SPI_Status_t endStatus = SPI_endTransaction();
        success = transferred && (SPI_STATUS_OK == endStatus);
        if ((SPI_STATUS_OK == firstError) && (false == success))
        {
            s_lastSpi = (SPI_STATUS_OK != transferStatus) ? transferStatus : endStatus;
        }
    }
    else if (SPI_STATUS_OK == firstError)
    {
        s_lastSpi = beginStatus;
    }
    if (SPI_STATUS_OK != firstError)
    {
        s_lastSpi = firstError;
    }
    return success;
}

/** @brief Wait for hardware idle while CS is LOW; then release CS and clock idle. */
static bool SD_EndSession(bool initialization)
{
    bool success = false;
    SPI_Status_t transferStatus = s_lastSpi;
    SPI_Status_t endStatus = SPI_endTransaction();
    digitalWrite(s_u8SdCsPin, HIGH);
    if ((SPI_STATUS_OK == transferStatus) && (SPI_STATUS_OK != endStatus))
    {
        s_lastSpi = endStatus;
    }
    /* Keep CS HIGH during the release clocks. */
    bool idleOk = SD_ClockIdle(initialization, 1U);
    success = (SPI_STATUS_OK == transferStatus) &&
              (SPI_STATUS_OK == endStatus) && idleOk;
    return success;
}

/** @brief Send a six-byte command and receive its R1 response. CS stays LOW. */
static bool SD_SendCommand(uint8_t command, uint32_t argument,
                           uint8_t crc, uint8_t *response)
{
    bool success = (NULL != response);
    uint8_t value = SD_SPI_DUMMY_BYTE;
    uint8_t i = 0U;
    uint8_t frame[6] =
        {
            (uint8_t)(0x40U | command),
            (uint8_t)(argument >> 24U),
            (uint8_t)(argument >> 16U),
            (uint8_t)(argument >> 8U),
            (uint8_t)argument,
            crc};
    if (true == success)
    {
        *response = SD_SPI_DUMMY_BYTE;
        success = SD_Exchange(SD_SPI_DUMMY_BYTE, NULL);
    }
    if (true == success)
    {
        success = SD_ExchangeBuffer(frame, NULL, 6U);
    }
    for (i = 0U; (i < SD_COMMAND_RESPONSE_TIMEOUT_COUNT) && (true == success); i++)
    {
        success = SD_Exchange(SD_SPI_DUMMY_BYTE, &value);
        if ((true == success) && (0U == (value & 0x80U)))
        {
            *response = value;
            break;
        }
    }
    success = success && (0U == (*response & 0x80U));
    return success;
}

/** @brief Read R3/R7 response bytes while CS remains asserted. */
static bool SD_ReadResponseBytes(uint8_t *buffer, uint8_t length)
{
    bool success = (NULL != buffer) && (0U != length);
    if (true == success)
    {
        success = SD_ExchangeBuffer(NULL, buffer, (uint32_t)length);
    }
    return success;
}

static bool SD_WaitReady(void)
{
    bool ready = false;
    bool valid = true;
    uint32_t count = 0UL;
    uint32_t started = millis();
    uint8_t value = 0U;
    while ((false == ready) && (true == valid) &&
           (count < SD_WRITE_BUSY_MAX_POLLS) &&
           ((uint32_t)(millis() - started) < SD_WRITE_BUSY_TIMEOUT_MS))
    {
        valid = SD_Exchange(SD_SPI_DUMMY_BYTE, &value);
        ready = valid && (SD_SPI_DUMMY_BYTE == value);
        count++;
    }
    return ready;
}

static bool SD_WaitDataToken(uint8_t token)
{
    bool found = false;
    bool valid = true;
    uint32_t count = 0UL;
    uint32_t started = millis();
    uint8_t value = 0U;
    while ((false == found) && (true == valid) &&
           (count < SD_DATA_TOKEN_MAX_POLLS) &&
           ((uint32_t)(millis() - started) < SD_DATA_TOKEN_TIMEOUT_MS))
    {
        valid = SD_Exchange(SD_SPI_DUMMY_BYTE, &value);
        found = valid && (token == value);
        count++;
    }
    return found;
}

/** @brief One complete command transaction, no extra response bytes. */
static bool SD_Command(uint8_t command, uint32_t argument,
                       uint8_t crc, uint8_t *response)
{
    bool success = SD_BeginSession(true);
    if (true == success)
    {
        success = SD_SendCommand(command, argument, crc, response);
        success = SD_EndSession(true) && success;
    }
    return success;
}

/** @brief Initialize SD v1/v2 in SPI mode; does not mount FAT. */
static bool SD_InitializeCard(void)
{
    bool success = false;
    bool v2 = false;
    bool initialized = false;
    uint8_t r1 = SD_SPI_DUMMY_BYTE;
    uint8_t r7[4] = {0U};
    uint8_t ocr[4] = {0U};
    uint32_t i = 0UL;
    uint32_t started = 0UL;

    /* CMD0 can require retries after power-up. */
    for (i = 0UL; (i < SD_CMD0_RETRY_LIMIT) && (false == success); i++)
    {
        success = SD_Command(SD_CMD0_GO_IDLE_STATE, SD_CMD0_ARGUMENT,
                             SD_CMD0_CRC, &r1) &&
                  (SD_R1_IDLE_STATE == r1);
        if (SPI_STATUS_OK != s_lastSpi)
        {
            break;
        }
    }
    if (true == success)
    {
        success = SD_BeginSession(true);
        if (true == success)
        {
            success = SD_SendCommand(SD_CMD8_SEND_IF_COND, SD_CMD8_ARGUMENT,
                                     SD_CMD8_CRC, &r1);
            if ((true == success) && (SD_R1_IDLE_STATE == r1))
            {
                success = SD_ReadResponseBytes(r7, 4U);
                v2 = success && (0x01U == (r7[2] & 0x0FU)) && (0xAAU == r7[3]);
                success = v2;
            }
            else if ((true == success) && (0U != (r1 & SD_R1_ILLEGAL_COMMAND_MASK)))
            {
                v2 = false; /* SDSC v1. */
            }
            else
            {
                success = false;
            }
            success = SD_EndSession(true) && success;
        }
    }
    if (true == success)
    {
        started = millis();
        for (i = 0UL; (i < SD_INITIALIZATION_MAX_POLLS) && (false == initialized) &&
                      ((uint32_t)(millis() - started) < SD_INITIALIZATION_TIMEOUT_MS);
             i++)
        {
            /* CMD55 + ACMD41 in one CS assertion. */
            success = SD_BeginSession(true);
            if (true == success)
            {
                success = SD_SendCommand(SD_CMD55_APP_CMD, 0UL,
                                         SD_DEFAULT_CRC, &r1);
                success = success && ((SD_R1_IDLE_STATE == r1) ||
                                      (SD_R1_READY_STATE == r1));
                if (true == success)
                {
                    success = SD_SendCommand(SD_ACMD41_SEND_OP_COND,
                                             v2 ? SD_ACMD41_HCS_ARGUMENT : 0UL,
                                             SD_DEFAULT_CRC, &r1);
                }
                success = SD_EndSession(true) && success;
                initialized = success && (SD_R1_READY_STATE == r1);
            }
            if ((false == success) || (SPI_STATUS_OK != s_lastSpi))
            {
                break;
            }
        }
        success = initialized;
    }
    if (true == success)
    {
        success = SD_BeginSession(true);
        if (true == success)
        {
            success = SD_SendCommand(SD_CMD58_READ_OCR, SD_CMD58_ARGUMENT,
                                     SD_DEFAULT_CRC, &r1);
            success = success && (SD_R1_READY_STATE == r1);
            if (true == success)
            {
                success = SD_ReadResponseBytes(ocr, 4U);
                /* OCR power-up complete bit 31; CCS bit 30. */
                success = success && (0U != (ocr[0] & 0x80U));
                s_bSdBlockAddressing = v2 && (0U != (ocr[0] & 0x40U));
            }
            success = SD_EndSession(true) && success;
        }
    }
    if ((true == success) && (false == s_bSdBlockAddressing))
    {
        success = SD_Command(SD_CMD16_SET_BLOCKLEN,
                             SD_CMD16_BLOCK_LENGTH_ARGUMENT,
                             SD_DEFAULT_CRC, &r1) &&
                  (SD_R1_READY_STATE == r1);
    }
    return success;
}

static uint32_t SD_GetCommandAddress(uint32_t block)
{
    uint32_t address = block;
    if (false == s_bSdBlockAddressing)
    {
        address = block * (uint32_t)SD_BLOCK_SIZE;
    }
    return address;
}

/* ========================================================================= */
/* Filesystem Helpers                                                        */
/* ========================================================================= */

static void SD_ResetFileSystemState(void)
{
    uint8_t u8Index = 0U;

    s_bSdMounted = false;

    for (u8Index = 0U;
         u8Index < SD_MAX_OPEN_FILES;
         u8Index++)
    {
        s_abSdFileUsed[u8Index] = false;
    }

    for (u8Index = 0U;
         u8Index < SD_MAX_OPEN_DIRECTORIES;
         u8Index++)
    {
        s_abSdDirectoryUsed[u8Index] = false;
    }

    return;
}

static bool SD_IsValidFileHandle(
    const SD_File_t *file)
{
    bool bValid = false;

    if (((const SD_File_t *)0 != file) &&
        (file->handle < SD_MAX_OPEN_FILES))
    {
        if (true ==
            s_abSdFileUsed[file->handle])
        {
            bValid = true;
        }
    }

    return bValid;
}

static bool SD_IsValidDirectoryHandle(
    const SD_Dir_t *dir)
{
    bool bValid = false;

    if (((const SD_Dir_t *)0 != dir) &&
        (dir->handle < SD_MAX_OPEN_DIRECTORIES))
    {
        if (true ==
            s_abSdDirectoryUsed[dir->handle])
        {
            bValid = true;
        }
    }

    return bValid;
}

static bool SD_AllocateFileHandle(
    SD_File_t *file)
{
    bool bAllocated = false;
    uint8_t u8Index = 0U;

    if ((SD_File_t *)0 != file)
    {
        file->handle = SD_INVALID_HANDLE;

        for (u8Index = 0U;
             (u8Index < SD_MAX_OPEN_FILES) &&
             (false == bAllocated);
             u8Index++)
        {
            if (false ==
                s_abSdFileUsed[u8Index])
            {
                s_abSdFileUsed[u8Index] = true;

                file->handle = u8Index;

                bAllocated = true;
            }
        }
    }

    return bAllocated;
}

static void SD_ReleaseFileHandle(
    SD_File_t *file)
{
    if ((SD_File_t *)0 != file)
    {
        if (file->handle <
            SD_MAX_OPEN_FILES)
        {
            s_abSdFileUsed[file->handle] =
                false;
        }

        file->handle = SD_INVALID_HANDLE;
    }

    return;
}

static bool SD_AllocateDirectoryHandle(
    SD_Dir_t *dir)
{
    bool bAllocated = false;
    uint8_t u8Index = 0U;

    if ((SD_Dir_t *)0 != dir)
    {
        dir->handle = SD_INVALID_HANDLE;

        for (u8Index = 0U;
             (u8Index < SD_MAX_OPEN_DIRECTORIES) &&
             (false == bAllocated);
             u8Index++)
        {
            if (false ==
                s_abSdDirectoryUsed[u8Index])
            {
                s_abSdDirectoryUsed[u8Index] =
                    true;

                dir->handle = u8Index;

                bAllocated = true;
            }
        }
    }

    return bAllocated;
}

static void SD_ReleaseDirectoryHandle(
    SD_Dir_t *dir)
{
    if ((SD_Dir_t *)0 != dir)
    {
        if (dir->handle <
            SD_MAX_OPEN_DIRECTORIES)
        {
            s_abSdDirectoryUsed[dir->handle] =
                false;
        }

        dir->handle = SD_INVALID_HANDLE;
    }

    return;
}

static BYTE SD_GetFatFsMode(
    SD_FileMode_t mode)
{
    BYTE fatMode = 0U;

    if (SD_FILE_READ == mode)
    {
        fatMode = FA_READ;
    }
    else if (SD_FILE_WRITE == mode)
    {
        fatMode =
            FA_WRITE |
            FA_CREATE_ALWAYS;
    }
    else if (SD_FILE_APPEND == mode)
    {
        fatMode =
            FA_WRITE |
            FA_OPEN_APPEND;
    }
    else if (SD_FILE_READ_WRITE == mode)
    {
        fatMode =
            FA_READ |
            FA_WRITE;
    }
    else
    {
        fatMode = 0U;
    }

    return fatMode;
}

static void SD_CopyFileInfo(
    const FILINFO *pFatInfo,
    SD_FileInfo_t *pInfo)
{
    uint32_t u32Index = 0UL;

    if (((const FILINFO *)0 != pFatInfo) &&
        ((SD_FileInfo_t *)0 != pInfo))
    {
        while ((u32Index <
                SD_MAX_NAME_LENGTH) &&
               ('\0' !=
                pFatInfo->fname[u32Index]))
        {
            pInfo->name[u32Index] =
                pFatInfo->fname[u32Index];

            u32Index++;
        }

        pInfo->name[u32Index] = '\0';

        pInfo->size =
            (uint32_t)pFatInfo->fsize;

        pInfo->isDirectory =
            (0U !=
             (pFatInfo->fattrib &
              AM_DIR));

        pInfo->isReadOnly =
            (0U !=
             (pFatInfo->fattrib &
              AM_RDO));

        pInfo->isHidden =
            (0U !=
             (pFatInfo->fattrib &
              AM_HID));
    }

    return;
}

/* ========================================================================= */
/* Public API                                                                */
/* ========================================================================= */
bool SD_Begin(uint8_t csPin)
{
    bool initialized = false;
    if ((ARDUINO_VALID_TRUE == Arduino_HasDigitalCapability(csPin)) &&
        (false == SPI_isTransactionActive()))
    {
        /* Reinitialization closes our resources, but NEVER stops the SPI bus. */
        SD_End();
        SD_ResetFileSystemState();
        s_u8SdCsPin = csPin;
        s_bSdPinsConfigured = true;
        s_bSdBlockAddressing = false;
        s_lastSpi = SPI_STATUS_OK;
        pinMode(s_u8SdCsPin, OUTPUT);
        digitalWrite(s_u8SdCsPin, HIGH);
        /* At least 74 clocks while ALL device chip-selects remain HIGH. */
        initialized = SD_ClockIdle(true, SD_STARTUP_DUMMY_BYTES);
        if (true == initialized)
        {
            initialized = SD_InitializeCard();
        }
        if (true == initialized)
        {
            s_bSdInitialized = true;
            initialized = SD_Mount();
            if (false == initialized)
            {
                s_bSdInitialized = false;
            }
        }
    }
    else if (true == SPI_isTransactionActive())
    {
        s_lastSpi = SPI_STATUS_BUSY;
    }
    else
    {
        s_lastSpi = SPI_STATUS_INVALID_ARG;
    }
    return initialized;
}

bool SD_IsInitialized(void)
{
    return s_bSdInitialized;
}

SPI_Status_t SD_GetLastSpiStatus(void)
{
    return s_lastSpi;
}

void SD_End(void)
{
    uint8_t index = 0U;
    for (index = 0U; index < SD_MAX_OPEN_FILES; index++)
    {
        if (true == s_abSdFileUsed[index])
        {
            (void)f_close(&s_asdFilePool[index]);
            s_abSdFileUsed[index] = false;
        }
    }
    for (index = 0U; index < SD_MAX_OPEN_DIRECTORIES; index++)
    {
        if (true == s_abSdDirectoryUsed[index])
        {
            (void)f_closedir(&s_asdDirectoryPool[index]);
            s_abSdDirectoryUsed[index] = false;
        }
    }
    (void)SD_Unmount();
    s_bSdInitialized = false;
    s_bSdBlockAddressing = false;
    if (true == s_bSdPinsConfigured)
    {
        digitalWrite(s_u8SdCsPin, HIGH);
    }
    /* The shared SPI bus belongs to the application, not the SD library. */
}

bool SD_IsMounted(void)
{
    return s_bSdMounted;
}

bool SD_Mount(void)
{
    bool mounted = false;
    if (true == s_bSdInitialized)
    {
        FRESULT result = f_mount(&s_sdFileSystem, "", 1U);
        mounted = (FR_OK == result);
        s_bSdMounted = mounted;
    }
    return mounted;
}

bool SD_Unmount(void)
{
    bool unmounted = true;
    if (true == s_bSdMounted)
    {
        unmounted = (FR_OK == f_mount(NULL, "", 0U));
        if (true == unmounted)
        {
            s_bSdMounted = false;
        }
    }
    return unmounted;
}

bool SD_ReadBlock(uint32_t block, uint8_t *buffer)
{
    bool success = (NULL != buffer) && s_bSdInitialized;
    uint8_t response = SD_SPI_DUMMY_BYTE;
    uint8_t crc[2] = {0U};
    if ((true == success) && (false == s_bSdBlockAddressing) &&
        (block > (UINT32_MAX / SD_BLOCK_SIZE)))
    {
        success = false;
    }
    if (true == success)
    {
        success = SD_BeginSession(false);
        if (true == success)
        {
            success = SD_SendCommand(SD_CMD17_READ_SINGLE_BLOCK,
                                     SD_GetCommandAddress(block),
                                     SD_DEFAULT_CRC, &response);
            success = success && (SD_R1_READY_STATE == response);
            if (true == success)
            {
                success = SD_WaitDataToken(SD_DATA_START_BLOCK_TOKEN);
            }
            if (true == success)
            {
                success = SD_ExchangeBuffer(NULL, buffer, SD_BLOCK_SIZE);
            }
            if (true == success)
            {
                success = SD_ExchangeBuffer(NULL, crc, 2U);
            }
            success = SD_EndSession(false) && success;
        }
    }
    return success;
}

bool SD_WriteBlock(uint32_t block, const uint8_t *buffer)
{
    bool success = (NULL != buffer) && s_bSdInitialized;
    uint8_t response = SD_SPI_DUMMY_BYTE;
    uint8_t dataResponse = SD_SPI_DUMMY_BYTE;
    if ((true == success) && (false == s_bSdBlockAddressing) &&
        (block > (UINT32_MAX / SD_BLOCK_SIZE)))
    {
        success = false;
    }
    if (true == success)
    {
        success = SD_BeginSession(false);
        if (true == success)
        {
            success = SD_WaitReady();
            if (true == success)
            {
                success = SD_SendCommand(SD_CMD24_WRITE_SINGLE_BLOCK,
                                         SD_GetCommandAddress(block),
                                         SD_DEFAULT_CRC, &response);
                success = success && (SD_R1_READY_STATE == response);
            }
            if (true == success)
            {
                success = SD_Exchange(SD_SPI_DUMMY_BYTE, NULL) &&
                          SD_Exchange(SD_DATA_START_BLOCK_TOKEN, NULL);
            }
            if (true == success)
            {
                success = SD_ExchangeBuffer(buffer, NULL, SD_BLOCK_SIZE);
            }
            if (true == success)
            {
                success = SD_Exchange(SD_SPI_DUMMY_BYTE, NULL) &&
                          SD_Exchange(SD_SPI_DUMMY_BYTE, NULL);
            }
            if (true == success)
            {
                success = SD_Exchange(SD_SPI_DUMMY_BYTE, &dataResponse);
                success = success &&
                          (SD_DATA_RESPONSE_ACCEPTED ==
                           (dataResponse & SD_DATA_RESPONSE_MASK));
            }
            if (true == success)
            {
                success = SD_WaitReady();
            }
            success = SD_EndSession(false) && success;
        }
    }
    return success;
}

bool SD_Open(SD_File_t *file,
             const char *path,
             SD_FileMode_t mode)
{
    bool bOpened = false;
    BYTE fatMode = 0U;
    FRESULT result = FR_INVALID_PARAMETER;

    if (((SD_File_t *)0 != file) &&
        ((const char *)0 != path) &&
        (true == s_bSdMounted))
    {
        file->handle = SD_INVALID_HANDLE;

        fatMode =
            SD_GetFatFsMode(mode);

        if (0U != fatMode)
        {
            if (true ==
                SD_AllocateFileHandle(file))
            {
                result =
                    f_open(
                        &s_asdFilePool[file->handle],
                        path,
                        fatMode);

                if (FR_OK == result)
                {
                    bOpened = true;
                }
                else
                {
                    SD_ReleaseFileHandle(file);
                }
            }
        }
    }

    return bOpened;
}

bool SD_Close(SD_File_t *file)
{
    bool bClosed = false;
    FRESULT result = FR_INVALID_OBJECT;

    if (true ==
        SD_IsValidFileHandle(file))
    {
        result =
            f_close(
                &s_asdFilePool[file->handle]);

        if (FR_OK == result)
        {
            SD_ReleaseFileHandle(file);

            bClosed = true;
        }
    }

    return bClosed;
}

bool SD_IsOpen(const SD_File_t *file)
{
    return SD_IsValidFileHandle(file);
}

bool SD_Read(SD_File_t *file,
             void *buffer,
             uint32_t length,
             uint32_t *bytesRead)
{
    bool bRead = false;
    UINT fatBytesRead = 0U;
    FRESULT result = FR_INVALID_OBJECT;

    if ((true ==
         SD_IsValidFileHandle(file)) &&
        ((void *)0 != buffer) &&
        ((uint32_t *)0 != bytesRead))
    {
        *bytesRead = 0UL;

        result =
            f_read(
                &s_asdFilePool[file->handle],
                buffer,
                (UINT)length,
                &fatBytesRead);

        *bytesRead =
            (uint32_t)fatBytesRead;

        if (FR_OK == result)
        {
            bRead = true;
        }
    }

    return bRead;
}

bool SD_Write(SD_File_t *file,
              const void *buffer,
              uint32_t length,
              uint32_t *bytesWritten)
{
    bool bWritten = false;
    UINT fatBytesWritten = 0U;
    FRESULT result = FR_INVALID_OBJECT;

    if ((true ==
         SD_IsValidFileHandle(file)) &&
        ((const void *)0 != buffer) &&
        ((uint32_t *)0 != bytesWritten))
    {
        *bytesWritten = 0UL;

        result =
            f_write(
                &s_asdFilePool[file->handle],
                buffer,
                (UINT)length,
                &fatBytesWritten);

        *bytesWritten =
            (uint32_t)fatBytesWritten;

        if ((FR_OK == result) &&
            (length ==
             (uint32_t)fatBytesWritten))
        {
            bWritten = true;
        }
    }

    return bWritten;
}

bool SD_ReadByte(SD_File_t *file,
                 uint8_t *data)
{
    bool bRead = false;
    uint32_t u32BytesRead = 0UL;

    if ((uint8_t *)0 != data)
    {
        bRead =
            SD_Read(
                file,
                data,
                1UL,
                &u32BytesRead);

        if (1UL != u32BytesRead)
        {
            bRead = false;
        }
    }

    return bRead;
}

bool SD_WriteByte(SD_File_t *file,
                  uint8_t data)
{
    bool bWritten = false;
    uint32_t u32BytesWritten = 0UL;

    bWritten =
        SD_Write(
            file,
            &data,
            1UL,
            &u32BytesWritten);

    if (1UL != u32BytesWritten)
    {
        bWritten = false;
    }

    return bWritten;
}

bool SD_WriteString(SD_File_t *file,
                    const char *text)
{
    bool bWritten = false;
    uint32_t u32Length = 0UL;
    uint32_t u32BytesWritten = 0UL;

    if ((const char *)0 != text)
    {
        u32Length =
            (uint32_t)strlen(text);

        if (0UL == u32Length)
        {
            bWritten = true;
        }
        else
        {
            bWritten =
                SD_Write(
                    file,
                    text,
                    u32Length,
                    &u32BytesWritten);
        }
    }

    return bWritten;
}

bool SD_WriteLine(SD_File_t *file,
                  const char *text)
{
    bool bWritten = false;
    uint32_t u32BytesWritten = 0UL;

    if ((const char *)0 != text)
    {
        if (true ==
            SD_WriteString(
                file,
                text))
        {
            bWritten =
                SD_Write(
                    file,
                    SD_LINE_ENDING,
                    SD_LINE_ENDING_LENGTH,
                    &u32BytesWritten);
        }
    }

    return bWritten;
}

bool SD_Seek(SD_File_t *file,
             uint32_t position)
{
    bool bSeeked = false;
    FRESULT result = FR_INVALID_OBJECT;

    if (true ==
        SD_IsValidFileHandle(file))
    {
        result =
            f_lseek(
                &s_asdFilePool[file->handle],
                (FSIZE_t)position);

        if (FR_OK == result)
        {
            bSeeked = true;
        }
    }

    return bSeeked;
}

bool SD_Rewind(SD_File_t *file)
{
    return SD_Seek(
        file,
        0UL);
}

uint32_t SD_Position(const SD_File_t *file)
{
    uint32_t u32Position = 0UL;

    if (true ==
        SD_IsValidFileHandle(file))
    {
        u32Position =
            (uint32_t)f_tell(
                &s_asdFilePool[file->handle]);
    }

    return u32Position;
}

uint32_t SD_Size(const SD_File_t *file)
{
    uint32_t u32Size = 0UL;

    if (true ==
        SD_IsValidFileHandle(file))
    {
        u32Size =
            (uint32_t)f_size(
                &s_asdFilePool[file->handle]);
    }

    return u32Size;
}

uint32_t SD_Available(const SD_File_t *file)
{
    uint32_t u32Available = 0UL;
    uint32_t u32Size = 0UL;
    uint32_t u32Position = 0UL;

    if (true ==
        SD_IsValidFileHandle(file))
    {
        u32Size =
            SD_Size(file);

        u32Position =
            SD_Position(file);

        if (u32Position < u32Size)
        {
            u32Available =
                u32Size -
                u32Position;
        }
    }

    return u32Available;
}

bool SD_Flush(SD_File_t *file)
{
    bool bFlushed = false;
    FRESULT result = FR_INVALID_OBJECT;

    if (true ==
        SD_IsValidFileHandle(file))
    {
        result =
            f_sync(
                &s_asdFilePool[file->handle]);

        if (FR_OK == result)
        {
            bFlushed = true;
        }
    }

    return bFlushed;
}

bool SD_Truncate(SD_File_t *file)
{
    bool bTruncated = false;
    FRESULT result = FR_INVALID_OBJECT;

    if (true ==
        SD_IsValidFileHandle(file))
    {
        result =
            f_truncate(
                &s_asdFilePool[file->handle]);

        if (FR_OK == result)
        {
            bTruncated = true;
        }
    }

    return bTruncated;
}

bool SD_Exists(const char *path)
{
    bool bExists = false;
    FILINFO fileInfo;
    FRESULT result = FR_INVALID_NAME;

    if (((const char *)0 != path) &&
        (true == s_bSdMounted))
    {
        result =
            f_stat(
                path,
                &fileInfo);

        if (FR_OK == result)
        {
            bExists = true;
        }
    }
    return bExists;
}

bool SD_GetInfo(const char *path,
                SD_FileInfo_t *info)
{
    bool bRead = false;
    FILINFO fatInfo;
    FRESULT result = FR_INVALID_NAME;
    if (((const char *)0 != path) &&
        ((SD_FileInfo_t *)0 != info) &&
        (true == s_bSdMounted))
    {
        result =
            f_stat(
                path,
                &fatInfo);
        if (FR_OK == result)
        {
            SD_CopyFileInfo(
                &fatInfo,
                info);
            bRead = true;
        }
    }
    return bRead;
}

bool SD_Remove(const char *path)
{
    bool bRemoved = false;
    FILINFO fileInfo;
    FRESULT result = FR_INVALID_NAME;
    if (((const char *)0 != path) &&
        (true == s_bSdMounted))
    {
        result =
            f_stat(
                path,
                &fileInfo);
        if ((FR_OK == result) &&
            (0U ==
             (fileInfo.fattrib &
              AM_DIR)))
        {
            result =
                f_unlink(path);
            if (FR_OK == result)
            {
                bRemoved = true;
            }
        }
    }
    return bRemoved;
}

bool SD_Rename(const char *oldPath,
               const char *newPath)
{
    bool bRenamed = false;
    FRESULT result = FR_INVALID_NAME;
    if (((const char *)0 != oldPath) &&
        ((const char *)0 != newPath) &&
        (true == s_bSdMounted))
    {
        result =
            f_rename(
                oldPath,
                newPath);
        if (FR_OK == result)
        {
            bRenamed = true;
        }
    }
    return bRenamed;
}

bool SD_Mkdir(const char *path)
{
    bool bCreated = false;
    FRESULT result = FR_INVALID_NAME;
    if (((const char *)0 != path) &&
        (true == s_bSdMounted))
    {
        result =
        f_mkdir(path);
        if (FR_OK == result)
        {
            bCreated = true;
        }
    }
    return bCreated;
}

bool SD_Rmdir(const char *path)
{
    bool bRemoved = false;
    FILINFO fileInfo;
    FRESULT result = FR_INVALID_NAME;
    if (((const char *)0 != path) &&
        (true == s_bSdMounted))
    {
        result =
            f_stat(
                path,
                &fileInfo);
        if ((FR_OK == result) &&
            (0U !=
             (fileInfo.fattrib &
              AM_DIR)))
        {
            result =
                f_unlink(path);
            if (FR_OK == result)
            {
                bRemoved = true;
            }
        }
    }
    return bRemoved;
}

bool SD_OpenDir(SD_Dir_t *dir,
                const char *path)
{
    bool bOpened = false;
    FRESULT result = FR_INVALID_NAME;
    if (((SD_Dir_t *)0 != dir) &&
        ((const char *)0 != path) &&
        (true == s_bSdMounted))
    {
        dir->handle = SD_INVALID_HANDLE;
        if (true ==
            SD_AllocateDirectoryHandle(dir))
        {
            result =
                f_opendir(
                    &s_asdDirectoryPool[dir->handle],
                    path);
            if (FR_OK == result)
            {
                bOpened = true;
            }
            else
            {
                SD_ReleaseDirectoryHandle(dir);
            }
        }
    }
    return bOpened;
}

bool SD_ReadDir(SD_Dir_t *dir,
                SD_FileInfo_t *info)
{
    bool bRead = false;
    FILINFO fatInfo;
    FRESULT result = FR_INVALID_OBJECT;
    if ((true ==
         SD_IsValidDirectoryHandle(dir)) &&
        ((SD_FileInfo_t *)0 != info))
    {
        result =
            f_readdir(
                &s_asdDirectoryPool[dir->handle],
                &fatInfo);
        if ((FR_OK == result) &&
            ('\0' != fatInfo.fname[0U]))
        {
            SD_CopyFileInfo(
                &fatInfo,
                info);
            bRead = true;
        }
    }
    return bRead;
}

bool SD_RewindDir(SD_Dir_t *dir)
{
    bool bRewound = false;
    FRESULT result = FR_INVALID_OBJECT;
    if (true ==
        SD_IsValidDirectoryHandle(dir))
    {
        result =
            f_readdir(
                &s_asdDirectoryPool[dir->handle],
                (FILINFO *)0);
        if (FR_OK == result)
        {
            bRewound = true;
        }
    }
    return bRewound;
}

bool SD_CloseDir(SD_Dir_t *dir)
{
    bool bClosed = false;
    FRESULT result = FR_INVALID_OBJECT;
    if (true ==
        SD_IsValidDirectoryHandle(dir))
    {
        result =
            f_closedir(
                &s_asdDirectoryPool[dir->handle]);
        if (FR_OK == result)
        {
            SD_ReleaseDirectoryHandle(dir);
            bClosed = true;
        }
    }
    return bClosed;
}

bool SD_GetCapacity(uint64_t *totalBytes,
                    uint64_t *freeBytes)
{
    bool bRead = false;
    DWORD freeClusters = 0UL;
    DWORD totalClusters = 0UL;
    FATFS *pFileSystem = (FATFS *)0;
    FRESULT result = FR_NOT_READY;
    uint64_t u64ClusterSize = 0ULL;
    if (((uint64_t *)0 != totalBytes) &&
        ((uint64_t *)0 != freeBytes) &&
        (true == s_bSdMounted))
    {
        *totalBytes = 0ULL;
        *freeBytes = 0ULL;

        result =
            f_getfree(
                "",
                &freeClusters,
                &pFileSystem);

        if ((FR_OK == result) &&
            ((FATFS *)0 != pFileSystem))
        {
            totalClusters =
                (DWORD)(pFileSystem->n_fatent -
                        2U);

            u64ClusterSize =
                (uint64_t)
                    pFileSystem->csize *
                (uint64_t)SD_BLOCK_SIZE;

            *totalBytes =
                (uint64_t)totalClusters *
                u64ClusterSize;

            *freeBytes =
                (uint64_t)freeClusters *
                u64ClusterSize;
            bRead = true;
        }
    }
    return bRead;
}
