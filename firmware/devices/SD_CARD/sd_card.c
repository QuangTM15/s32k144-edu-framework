/**
 * @file sd_card.c
 * @brief SD card device library implementation.
 *
 * @details
 * This implementation provides:
 *
 * - GPIO-based software chip-select.
 * - SPI-mode SD card initialization.
 * - SD version and addressing detection.
 * - Single-block read using CMD17.
 * - Single-block write using CMD24.
 *
 * Filesystem management is intentionally not implemented here.
 */

#include "sd_card.h"

#include "Arduino.h"
#include "arduino_pins.h"
#include "spi.h"

#include "ff.h"

#include <string.h>

/* ========================================================================= */
/* Private Constants                                                         */
/* ========================================================================= */

/* ------------------------------------------------------------------------- */
/* Filesystem configuration                                                  */
/* ------------------------------------------------------------------------- */

#define SD_MAX_OPEN_FILES (4U)

#define SD_MAX_OPEN_DIRECTORIES (2U)

#define SD_LINE_ENDING "\r\n"

#define SD_LINE_ENDING_LENGTH (2U)

/* ------------------------------------------------------------------------- */
/* SPI configuration                                                         */
/* ------------------------------------------------------------------------- */

#define SD_SPI_INITIAL_FREQUENCY_HZ (400000UL)
#define SD_SPI_OPERATING_FREQUENCY_HZ (4000000UL)

#define SD_SPI_DUMMY_BYTE (0xFFU)

/*
 * Ten dummy bytes provide 80 clock cycles, exceeding the minimum
 * startup clock requirement before CMD0.
 */
#define SD_STARTUP_DUMMY_BYTES (10U)

/* ------------------------------------------------------------------------- */
/* SD commands                                                               */
/* ------------------------------------------------------------------------- */

#define SD_CMD0_GO_IDLE_STATE (0U)
#define SD_CMD8_SEND_IF_COND (8U)
#define SD_CMD16_SET_BLOCKLEN (16U)
#define SD_CMD17_READ_SINGLE_BLOCK (17U)
#define SD_CMD24_WRITE_SINGLE_BLOCK (24U)
#define SD_CMD55_APP_CMD (55U)
#define SD_CMD58_READ_OCR (58U)

#define SD_ACMD41_SEND_OP_COND (41U)

/* ------------------------------------------------------------------------- */
/* Command arguments                                                         */
/* ------------------------------------------------------------------------- */

#define SD_CMD0_ARGUMENT (0x00000000UL)

#define SD_CMD8_ARGUMENT (0x000001AAUL)

#define SD_ACMD41_HCS_ARGUMENT (0x40000000UL)

#define SD_CMD58_ARGUMENT (0x00000000UL)

#define SD_CMD16_BLOCK_LENGTH_ARGUMENT \
    ((uint32_t)SD_BLOCK_SIZE)

/* ------------------------------------------------------------------------- */
/* Command CRC                                                               */
/* ------------------------------------------------------------------------- */

#define SD_CMD0_CRC (0x95U)
#define SD_CMD8_CRC (0x87U)

#define SD_DEFAULT_CRC (0x01U)

/* ------------------------------------------------------------------------- */
/* R1 response values                                                        */
/* ------------------------------------------------------------------------- */

#define SD_R1_READY_STATE (0x00U)
#define SD_R1_IDLE_STATE (0x01U)

#define SD_R1_ILLEGAL_COMMAND_MASK (0x04U)

/* ------------------------------------------------------------------------- */
/* CMD8 response                                                             */
/* ------------------------------------------------------------------------- */

#define SD_CMD8_RESPONSE_SIZE (4U)

#define SD_CMD8_VOLTAGE_ACCEPTED (0x01U)
#define SD_CMD8_CHECK_PATTERN (0xAAU)

/* ------------------------------------------------------------------------- */
/* OCR response                                                              */
/* ------------------------------------------------------------------------- */

#define SD_OCR_RESPONSE_SIZE (4U)

#define SD_OCR_CCS_MASK (0x40U)

/* ------------------------------------------------------------------------- */
/* Data tokens                                                               */
/* ------------------------------------------------------------------------- */

#define SD_DATA_START_BLOCK_TOKEN (0xFEU)

#define SD_DATA_RESPONSE_MASK (0x1FU)
#define SD_DATA_RESPONSE_ACCEPTED (0x05U)

/* ------------------------------------------------------------------------- */
/* CRC transfer                                                              */
/* ------------------------------------------------------------------------- */

#define SD_DATA_CRC_SIZE (2U)

/* ------------------------------------------------------------------------- */
/* Timeouts                                                                  */
/* ------------------------------------------------------------------------- */

#define SD_COMMAND_RESPONSE_TIMEOUT_COUNT (16UL)

#define SD_INITIALIZATION_TIMEOUT_COUNT (10000UL)

#define SD_DATA_TOKEN_TIMEOUT_COUNT (100000UL)

#define SD_WRITE_BUSY_TIMEOUT_COUNT (200000UL)

/* ========================================================================= */
/* Private State                                                             */
/* ========================================================================= */

/**
 * @brief User-selected software chip-select pin.
 */
static uint8_t s_u8SdCsPin = GPIO0;

/**
 * @brief Complete SD card initialization state.
 */
static bool s_bSdInitialized = false;

/**
 * @brief Addressing mode used by the initialized card.
 *
 * @details
 * true means CMD17/CMD24 arguments use 512-byte logical block numbers.
 * false means command arguments use byte addresses.
 */
static bool s_bSdBlockAddressing = false;

/**
 * @brief Filesystem mount state.
 */
static bool s_bSdMounted = false;

/**
 * @brief FatFs filesystem object.
 */
static FATFS s_sdFileSystem;

/**
 * @brief FatFs file object pool.
 */
static FIL s_asdFilePool[SD_MAX_OPEN_FILES];

/**
 * @brief File pool allocation state.
 */
static bool s_abSdFileUsed[SD_MAX_OPEN_FILES] = {false};

/**
 * @brief FatFs directory object pool.
 */
static DIR s_asdDirectoryPool[SD_MAX_OPEN_DIRECTORIES];

/**
 * @brief Directory pool allocation state.
 */
static bool s_abSdDirectoryUsed[SD_MAX_OPEN_DIRECTORIES] = {false};

/* Filesystem helpers */

static void SD_ResetFileSystemState(void);

static bool SD_IsValidFileHandle(const SD_File_t *file);

static bool SD_IsValidDirectoryHandle(const SD_Dir_t *dir);

static bool SD_AllocateFileHandle(SD_File_t *file);

static void SD_ReleaseFileHandle(SD_File_t *file);

static bool SD_AllocateDirectoryHandle(SD_Dir_t *dir);

static void SD_ReleaseDirectoryHandle(SD_Dir_t *dir);

static BYTE SD_GetFatFsMode(SD_FileMode_t mode);

static void SD_CopyFileInfo(const FILINFO *pFatInfo, SD_FileInfo_t *pInfo);

/* ========================================================================= */
/* Private Function Prototypes                                               */
/* ========================================================================= */

/* GPIO and validation */

static bool SD_IsValidCsPin(uint8_t csPin);

static void SD_Select(void);

static void SD_Deselect(void);

/* SPI communication */

static uint8_t SD_Transfer(uint8_t data);

static void SD_SendStartupClocks(void);

/* Command communication */

static uint8_t SD_SendCommand(uint8_t command, uint32_t argument, uint8_t crc);

static bool SD_ReadResponseBytes(uint8_t *pBuffer, uint8_t length);

static bool SD_WaitReady(void);

static bool SD_WaitDataToken(uint8_t token);

/* Initialization */

static bool SD_InitializeCard(void);

/* Address conversion */

static uint32_t SD_GetCommandAddress(uint32_t block);

/* ========================================================================= */
/* GPIO and Validation                                                       */
/* ========================================================================= */

static bool SD_IsValidCsPin(uint8_t csPin)
{
    bool bIsValid = false;

    if (ARDUINO_VALID_TRUE ==
        Arduino_HasDigitalCapability(csPin))
    {
        bIsValid = true;
    }
    else
    {
        bIsValid = false;
    }

    return bIsValid;
}

static void SD_Select(void)
{
    digitalWrite(s_u8SdCsPin, LOW);
    return;
}

static void SD_Deselect(void)
{
    digitalWrite(s_u8SdCsPin, HIGH);
    return;
}

/* ========================================================================= */
/* SPI Communication                                                         */
/* ========================================================================= */

static uint8_t SD_Transfer(uint8_t data)
{
    uint8_t u8ReceivedData = 0U;

    u8ReceivedData =
        SPI_transfer(data);

    return u8ReceivedData;
}

static void SD_SendStartupClocks(void)
{
    uint8_t u8Index = 0U;

    SD_Deselect();

    for (u8Index = 0U; u8Index < SD_STARTUP_DUMMY_BYTES; u8Index++)
    {
        (void)SD_Transfer(SD_SPI_DUMMY_BYTE);
    }

    return;
}

/* ========================================================================= */
/* Command Communication                                                     */
/* ========================================================================= */

static uint8_t SD_SendCommand(uint8_t command, uint32_t argument, uint8_t crc)
{
    uint8_t u8Response = SD_SPI_DUMMY_BYTE;
    uint32_t u32Timeout =
        SD_COMMAND_RESPONSE_TIMEOUT_COUNT;

    /*
     * Provide one idle byte before the command frame.
     */
    (void)SD_Transfer(
        SD_SPI_DUMMY_BYTE);

    /*
     * SD SPI command frame:
     *
     * Byte 0: 01 + command index
     * Byte 1: argument[31:24]
     * Byte 2: argument[23:16]
     * Byte 3: argument[15:8]
     * Byte 4: argument[7:0]
     * Byte 5: CRC7 + end bit
     */
    (void)SD_Transfer(
        (uint8_t)(0x40U | command));

    (void)SD_Transfer(
        (uint8_t)(argument >> 24U));

    (void)SD_Transfer(
        (uint8_t)(argument >> 16U));

    (void)SD_Transfer(
        (uint8_t)(argument >> 8U));

    (void)SD_Transfer(
        (uint8_t)argument);

    (void)SD_Transfer(crc);

    /*
     * R1 is valid when bit 7 becomes zero.
     */
    do
    {
        u8Response =
            SD_Transfer(
                SD_SPI_DUMMY_BYTE);

        u32Timeout--;

    } while (((u8Response & 0x80U) != 0U) && (0UL != u32Timeout));

    return u8Response;
}

static bool SD_ReadResponseBytes(uint8_t *pBuffer, uint8_t length)
{
    bool bRead = false;
    uint8_t u8Index = 0U;

    if (((uint8_t *)0 != pBuffer) &&
        (0U != length))
    {
        for (u8Index = 0U;
             u8Index < length;
             u8Index++)
        {
            pBuffer[u8Index] =
                SD_Transfer(
                    SD_SPI_DUMMY_BYTE);
        }

        bRead = true;
    }
    else
    {
        bRead = false;
    }

    return bRead;
}

static bool SD_WaitReady(void)
{
    bool bReady = false;

    uint32_t u32Timeout =
        SD_WRITE_BUSY_TIMEOUT_COUNT;

    uint8_t u8Response = 0U;

    do
    {
        u8Response =
            SD_Transfer(
                SD_SPI_DUMMY_BYTE);

        u32Timeout--;

        if (SD_SPI_DUMMY_BYTE ==
            u8Response)
        {
            bReady = true;
        }
        else
        {
            bReady = false;
        }

    } while ((false == bReady) &&
             (0UL != u32Timeout));

    return bReady;
}

static bool SD_WaitDataToken(uint8_t token)
{
    bool bTokenReceived = false;

    uint32_t u32Timeout =
        SD_DATA_TOKEN_TIMEOUT_COUNT;

    uint8_t u8Response = 0U;

    do
    {
        u8Response =
            SD_Transfer(
                SD_SPI_DUMMY_BYTE);

        u32Timeout--;

        if (token == u8Response)
        {
            bTokenReceived = true;
        }
        else
        {
            bTokenReceived = false;
        }

    } while ((false == bTokenReceived) &&
             (0UL != u32Timeout));

    return bTokenReceived;
}

/* ========================================================================= */
/* Initialization                                                            */
/* ========================================================================= */

static bool SD_InitializeCard(void)
{
    bool bInitialized = false;
    bool bCmd8Supported = false;

    uint8_t u8Response = SD_SPI_DUMMY_BYTE;

    uint8_t au8Cmd8Response[SD_CMD8_RESPONSE_SIZE] =
        {0U};

    uint8_t au8Ocr[SD_OCR_RESPONSE_SIZE] =
        {0U};

    uint32_t u32Timeout =
        SD_INITIALIZATION_TIMEOUT_COUNT;

    /*
     * Enter SPI mode.
     */
    SD_Select();

    u8Response =
        SD_SendCommand(
            SD_CMD0_GO_IDLE_STATE,
            SD_CMD0_ARGUMENT,
            SD_CMD0_CRC);

    SD_Deselect();

    (void)SD_Transfer(
        SD_SPI_DUMMY_BYTE);

    if (SD_R1_IDLE_STATE ==
        u8Response)
    {
        /*
         * Check SD version and interface condition.
         */
        SD_Select();

        u8Response =
            SD_SendCommand(
                SD_CMD8_SEND_IF_COND,
                SD_CMD8_ARGUMENT,
                SD_CMD8_CRC);

        if (SD_R1_IDLE_STATE ==
            u8Response)
        {
            (void)SD_ReadResponseBytes(
                au8Cmd8Response,
                SD_CMD8_RESPONSE_SIZE);

            if ((SD_CMD8_VOLTAGE_ACCEPTED ==
                 (au8Cmd8Response[2U] & 0x0FU)) &&
                (SD_CMD8_CHECK_PATTERN ==
                 au8Cmd8Response[3U]))
            {
                bCmd8Supported = true;
            }
            else
            {
                bCmd8Supported = false;
            }
        }
        else if (0U !=
                 (u8Response &
                  SD_R1_ILLEGAL_COMMAND_MASK))
        {
            /*
             * Legacy SDSC card.
             */
            bCmd8Supported = false;
        }
        else
        {
            bCmd8Supported = false;
        }

        SD_Deselect();

        (void)SD_Transfer(
            SD_SPI_DUMMY_BYTE);

        /*
         * Initialize the card using ACMD41.
         */
        u32Timeout =
            SD_INITIALIZATION_TIMEOUT_COUNT;

        do
        {
            SD_Select();

            u8Response =
                SD_SendCommand(
                    SD_CMD55_APP_CMD,
                    0UL,
                    SD_DEFAULT_CRC);

            SD_Deselect();

            (void)SD_Transfer(
                SD_SPI_DUMMY_BYTE);

            if ((SD_R1_IDLE_STATE ==
                 u8Response) ||
                (SD_R1_READY_STATE ==
                 u8Response))
            {
                SD_Select();

                if (true ==
                    bCmd8Supported)
                {
                    u8Response =
                        SD_SendCommand(
                            SD_ACMD41_SEND_OP_COND,
                            SD_ACMD41_HCS_ARGUMENT,
                            SD_DEFAULT_CRC);
                }
                else
                {
                    u8Response =
                        SD_SendCommand(
                            SD_ACMD41_SEND_OP_COND,
                            0UL,
                            SD_DEFAULT_CRC);
                }

                SD_Deselect();

                (void)SD_Transfer(
                    SD_SPI_DUMMY_BYTE);
            }
            else
            {
                /*
                 * CMD55 failed.
                 */
            }

            u32Timeout--;

        } while ((SD_R1_READY_STATE !=
                  u8Response) &&
                 (0UL != u32Timeout));

        if (SD_R1_READY_STATE ==
            u8Response)
        {
            /*
             * Read OCR and determine addressing mode.
             */
            SD_Select();

            u8Response =
                SD_SendCommand(
                    SD_CMD58_READ_OCR,
                    SD_CMD58_ARGUMENT,
                    SD_DEFAULT_CRC);

            if (SD_R1_READY_STATE ==
                u8Response)
            {
                (void)SD_ReadResponseBytes(
                    au8Ocr,
                    SD_OCR_RESPONSE_SIZE);

                if ((true ==
                     bCmd8Supported) &&
                    (0U !=
                     (au8Ocr[0U] &
                      SD_OCR_CCS_MASK)))
                {
                    s_bSdBlockAddressing = true;
                }
                else
                {
                    s_bSdBlockAddressing = false;
                }

                bInitialized = true;
            }
            else
            {
                bInitialized = false;
            }

            SD_Deselect();

            (void)SD_Transfer(
                SD_SPI_DUMMY_BYTE);
        }
        else
        {
            bInitialized = false;
        }

        /*
         * Byte-addressed cards require the block length to be
         * explicitly configured to 512 bytes.
         */
        if ((true == bInitialized) &&
            (false ==
             s_bSdBlockAddressing))
        {
            SD_Select();

            u8Response =
                SD_SendCommand(
                    SD_CMD16_SET_BLOCKLEN,
                    SD_CMD16_BLOCK_LENGTH_ARGUMENT,
                    SD_DEFAULT_CRC);

            SD_Deselect();

            (void)SD_Transfer(
                SD_SPI_DUMMY_BYTE);

            if (SD_R1_READY_STATE !=
                u8Response)
            {
                bInitialized = false;
            }
            else
            {
                /*
                 * 512-byte block length configured.
                 */
            }
        }
        else
        {
            /*
             * Block-addressed cards use a fixed 512-byte block size.
             */
        }
    }
    else
    {
        bInitialized = false;
    }

    return bInitialized;
}

/* ========================================================================= */
/* Address Conversion                                                        */
/* ========================================================================= */

static uint32_t SD_GetCommandAddress(uint32_t block)
{
    uint32_t u32Address = 0UL;

    if (true ==
        s_bSdBlockAddressing)
    {
        u32Address = block;
    }
    else
    {
        u32Address =
            block *
            ((uint32_t)SD_BLOCK_SIZE);
    }

    return u32Address;
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
    bool bInitialized = false;
    s_bSdInitialized = false;
    s_bSdBlockAddressing = false;
    SD_ResetFileSystemState();
    if (true ==
        SD_IsValidCsPin(csPin))
    {
        s_u8SdCsPin = csPin;
        pinMode(s_u8SdCsPin,
                OUTPUT);
        SD_Deselect();
        SPI_beginEx(
            SPI_ROLE_MASTER,
            SD_SPI_INITIAL_FREQUENCY_HZ,
            SPI_MODE0,
            SPI_MSBFIRST);
        SD_SendStartupClocks();
        bInitialized =
            SD_InitializeCard();
        if (true ==
            bInitialized)
        {
            s_bSdInitialized = true;
            SPI_setFrequency(
                SD_SPI_OPERATING_FREQUENCY_HZ);
            if (true ==
                SD_Mount())
            {
                bInitialized = true;
            }
            else
            {
                s_bSdInitialized = false;
                bInitialized = false;
            }
        }
        else
        {
            s_bSdInitialized = false;
        }
    }
    else
    {
        bInitialized = false;
    }
    return bInitialized;
}

bool SD_IsInitialized(void)
{
    return s_bSdInitialized;
}

void SD_End(void)
{
    uint8_t u8Index = 0U;

    for (u8Index = 0U;
         u8Index < SD_MAX_OPEN_FILES;
         u8Index++)
    {
        if (true ==
            s_abSdFileUsed[u8Index])
        {
            (void)f_close(
                &s_asdFilePool[u8Index]);

            s_abSdFileUsed[u8Index] =
                false;
        }
    }

    for (u8Index = 0U;
         u8Index < SD_MAX_OPEN_DIRECTORIES;
         u8Index++)
    {
        if (true ==
            s_abSdDirectoryUsed[u8Index])
        {
            (void)f_closedir(
                &s_asdDirectoryPool[u8Index]);

            s_abSdDirectoryUsed[u8Index] =
                false;
        }
    }

    (void)SD_Unmount();

    s_bSdInitialized = false;
    s_bSdBlockAddressing = false;

    SPI_end();

    return;
}

bool SD_IsMounted(void)
{
    return s_bSdMounted;
}

bool SD_Mount(void)
{
    bool bMounted = false;
    FRESULT result = FR_NOT_READY;

    if (true ==
        s_bSdInitialized)
    {
        result =
            f_mount(
                &s_sdFileSystem,
                "",
                1U);

        if (FR_OK == result)
        {
            s_bSdMounted = true;
            bMounted = true;
        }
        else
        {
            s_bSdMounted = false;
        }
    }

    return bMounted;
}

bool SD_Unmount(void)
{
    bool bUnmounted = false;
    FRESULT result = FR_NOT_READY;

    if (true ==
        s_bSdMounted)
    {
        result =
            f_mount(
                (FATFS *)0,
                "",
                0U);

        if (FR_OK == result)
        {
            s_bSdMounted = false;
            bUnmounted = true;
        }
    }
    else
    {
        bUnmounted = true;
    }

    return bUnmounted;
}

bool SD_ReadBlock(uint32_t block,
                  uint8_t *pBuffer)
{
    bool bRead = false;

    uint8_t u8Response =
        SD_SPI_DUMMY_BYTE;

    uint16_t u16Index = 0U;

    uint8_t u8CrcIndex = 0U;

    uint32_t u32Address = 0UL;

    if (((uint8_t *)0 != pBuffer) &&
        (true ==
         s_bSdInitialized))
    {
        u32Address =
            SD_GetCommandAddress(block);

        SD_Select();

        u8Response =
            SD_SendCommand(
                SD_CMD17_READ_SINGLE_BLOCK,
                u32Address,
                SD_DEFAULT_CRC);

        if (SD_R1_READY_STATE ==
            u8Response)
        {
            if (true ==
                SD_WaitDataToken(
                    SD_DATA_START_BLOCK_TOKEN))
            {
                for (u16Index = 0U;
                     u16Index <
                     SD_BLOCK_SIZE;
                     u16Index++)
                {
                    pBuffer[u16Index] =
                        SD_Transfer(
                            SD_SPI_DUMMY_BYTE);
                }

                /*
                 * CRC16 is not currently validated.
                 * Consume both CRC bytes from the card.
                 */
                for (u8CrcIndex = 0U;
                     u8CrcIndex <
                     SD_DATA_CRC_SIZE;
                     u8CrcIndex++)
                {
                    (void)SD_Transfer(
                        SD_SPI_DUMMY_BYTE);
                }

                bRead = true;
            }
            else
            {
                bRead = false;
            }
        }
        else
        {
            bRead = false;
        }

        SD_Deselect();

        (void)SD_Transfer(
            SD_SPI_DUMMY_BYTE);
    }
    else
    {
        bRead = false;
    }

    return bRead;
}

bool SD_WriteBlock(uint32_t block,
                   const uint8_t *pBuffer)
{
    bool bWritten = false;

    uint8_t u8Response =
        SD_SPI_DUMMY_BYTE;

    uint8_t u8DataResponse =
        SD_SPI_DUMMY_BYTE;

    uint16_t u16Index = 0U;

    uint32_t u32Address = 0UL;

    if (((const uint8_t *)0 != pBuffer) &&
        (true ==
         s_bSdInitialized))
    {
        u32Address =
            SD_GetCommandAddress(block);

        SD_Select();

        if (true ==
            SD_WaitReady())
        {
            u8Response =
                SD_SendCommand(
                    SD_CMD24_WRITE_SINGLE_BLOCK,
                    u32Address,
                    SD_DEFAULT_CRC);

            if (SD_R1_READY_STATE ==
                u8Response)
            {
                /*
                 * One dummy byte before the data token.
                 */
                (void)SD_Transfer(
                    SD_SPI_DUMMY_BYTE);

                (void)SD_Transfer(
                    SD_DATA_START_BLOCK_TOKEN);

                for (u16Index = 0U;
                     u16Index <
                     SD_BLOCK_SIZE;
                     u16Index++)
                {
                    (void)SD_Transfer(
                        pBuffer[u16Index]);
                }

                /*
                 * Dummy CRC16. CRC checking is disabled in SPI mode
                 * unless explicitly enabled with CMD59.
                 */
                (void)SD_Transfer(
                    SD_SPI_DUMMY_BYTE);

                (void)SD_Transfer(
                    SD_SPI_DUMMY_BYTE);

                u8DataResponse =
                    SD_Transfer(
                        SD_SPI_DUMMY_BYTE);

                if (SD_DATA_RESPONSE_ACCEPTED ==
                    (u8DataResponse &
                     SD_DATA_RESPONSE_MASK))
                {
                    if (true ==
                        SD_WaitReady())
                    {
                        bWritten = true;
                    }
                    else
                    {
                        bWritten = false;
                    }
                }
                else
                {
                    bWritten = false;
                }
            }
            else
            {
                bWritten = false;
            }
        }
        else
        {
            bWritten = false;
        }

        SD_Deselect();

        (void)SD_Transfer(
            SD_SPI_DUMMY_BYTE);
    }
    else
    {
        bWritten = false;
    }

    return bWritten;
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
