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

/* ========================================================================= */
/* Private Constants                                                         */
/* ========================================================================= */

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

static uint8_t SD_SendCommand(uint8_t command,
                              uint32_t argument,
                              uint8_t crc);

static bool SD_ReadResponseBytes(uint8_t *pBuffer,
                                 uint8_t length);

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
    digitalWrite(s_u8SdCsPin,
                 LOW);

    return;
}

static void SD_Deselect(void)
{
    digitalWrite(s_u8SdCsPin,
                 HIGH);

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

    for (u8Index = 0U;
         u8Index < SD_STARTUP_DUMMY_BYTES;
         u8Index++)
    {
        (void)SD_Transfer(
            SD_SPI_DUMMY_BYTE);
    }

    return;
}

/* ========================================================================= */
/* Command Communication                                                     */
/* ========================================================================= */

static uint8_t SD_SendCommand(uint8_t command,
                              uint32_t argument,
                              uint8_t crc)
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

    } while (((u8Response & 0x80U) != 0U) &&
             (0UL != u32Timeout));

    return u8Response;
}

static bool SD_ReadResponseBytes(uint8_t *pBuffer,
                                 uint8_t length)
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
/* Public API                                                                */
/* ========================================================================= */

bool SD_Begin(uint8_t csPin)
{
    bool bInitialized = false;

    s_bSdInitialized = false;
    s_bSdBlockAddressing = false;

    if (true ==
        SD_IsValidCsPin(csPin))
    {
        s_u8SdCsPin = csPin;

        pinMode(s_u8SdCsPin,
                OUTPUT);

        SD_Deselect();

        /*
         * SD cards must be initialized at a low SPI clock frequency.
         */
        SPI_beginEx(
            SPI_ROLE_MASTER,
            SD_SPI_INITIAL_FREQUENCY_HZ,
            SPI_MODE0,
            SPI_MSBFIRST);

        /*
         * Provide at least 74 clock cycles while CS is high.
         */
        SD_SendStartupClocks();

        bInitialized =
            SD_InitializeCard();

        if (true ==
            bInitialized)
        {
            s_bSdInitialized = true;

            /*
             * Initialization is complete. Increase SPI frequency
             * for normal data transfers.
             */
            SPI_setFrequency(
                SD_SPI_OPERATING_FREQUENCY_HZ);
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
