/**
 * @file pn532.c
 * @brief PN532 UART/HSU transport, ISO14443A polling and MIFARE functions.
 * @details The driver uses EduFramework Serial2 only; Serial1 remains free for logs.
 *          Frames are validated for length, LCS, DCS, direction and command code.
 * @note Synchronous, single-instance, not reentrant. millis() must be operational.
 */
#include "pn532.h"
#include "Arduino.h"
#include <stddef.h>
#include <string.h>

#define PN532_PREAMBLE                 (0x00U)
#define PN532_START_CODE_1             (0x00U)
#define PN532_START_CODE_2             (0xFFU)
#define PN532_HOST_TO_PN532            (0xD4U)
#define PN532_PN532_TO_HOST            (0xD5U)
#define PN532_RF_CONFIG_FIELD          (0x01U)
#define PN532_RF_CONFIG_MAX_RETRIES    (0x05U)
#define PN532_MIFARE_AUTH_A            (0x60U)
#define PN532_MIFARE_AUTH_B            (0x61U)
#define PN532_MIFARE_READ              (0x30U)
#define PN532_MIFARE_WRITE             (0xA0U)
#define PN532_ULTRALIGHT_WRITE         (0xA2U)
#define PN532_DEFAULT_ACK_TIMEOUT_MS   (100U)
#define PN532_DEFAULT_RESP_TIMEOUT_MS  (1000U)
#define PN532_DEFAULT_RETRIES          (0U)
#define PN532_DEFAULT_COMMAND_RETRIES  (0U)
#define PN532_HSU_WAKE_LENGTH          (34U)
#define PN532_ABORT_SETTLE_DELAY_MS    (2U)
#define PN532_TX_FRAME_CAPACITY        (PN532_MAX_FRAME_DATA + 12U)

#define PN532_FRAME_TYPE_DATA          (1U)
#define PN532_FRAME_TYPE_ACK           (2U)
#define PN532_FRAME_TYPE_NACK          (3U)

static PN532_Config_t sxConfig = {PN532_DEFAULT_ACK_TIMEOUT_MS, PN532_DEFAULT_RESP_TIMEOUT_MS, PN532_DEFAULT_RETRIES, PN532_DEFAULT_COMMAND_RETRIES};
static bool sbInitialized = false;
static bool sbTargetAvailable = false;
static PN532_Card_t sxCurrentCard = {0};
static PN532_Status_t seLastError = PN532_STATUS_NOT_INITIALIZED;
static uint8_t su8LastChipStatus = 0U;
static uint8_t sau8TxFrame[PN532_TX_FRAME_CAPACITY] = {0};
static uint8_t sau8RxFrame[PN532_MAX_FRAME_DATA] = {0};
static PN532_Diagnostics_t sxDiagnostics = {0};
static char sacUIDText[PN532_UID_TEXT_LENGTH] = {0};
static char sacBlockText[(PN532_MIFARE_BLOCK_SIZE * 3U)] = {0};
static bool sbBlockValid = false;

static void PN532_InvalidateEasyCache(void)
{
    sacUIDText[0U] = '\0';
    sacBlockText[0U] = '\0';
    sbBlockValid = false;
}

static void PN532_EncodeHex(const uint8_t *pu8Data, uint8_t u8Length,
                            char *pcOutput)
{
    static const char acDigits[] = "0123456789ABCDEF";
    uint8_t u8Index = 0U;
    uint16_t u16Offset = 0U;
    for (u8Index = 0U; u8Index < u8Length; u8Index++)
    {
        if (0U != u8Index)
        {
            pcOutput[u16Offset++] = ' ';
        }
        pcOutput[u16Offset++] = acDigits[pu8Data[u8Index] >> 4U];
        pcOutput[u16Offset++] = acDigits[pu8Data[u8Index] & 0x0FU];
    }
    pcOutput[u16Offset] = '\0';
}


/* PN532 ACK frame also serves as a host-initiated abort packet (UM0701). */
static const uint8_t sau8AckFrame[6U] =
{
    0x00U, 0x00U, 0xFFU, 0x00U, 0xFFU, 0x00U
};

/** @brief Store and return a public operation's status. */
static PN532_Status_t PN532_Finish(PN532_Status_t eStatus)
{
    seLastError = eStatus;
    return eStatus;
}

/** @brief Read one byte before a whole-operation timeout expires. */
static PN532_Status_t PN532_ReadByte(uint8_t *pu8Byte, uint32_t u32StartMs, uint32_t u32TimeoutMs)
{
    PN532_Status_t eStatus = PN532_STATUS_TIMEOUT;
    uint32_t u32Read = 0U;

    while ((uint32_t)(millis() - u32StartMs) < u32TimeoutMs)
    {
        u32Read = Serial2_readBytes(pu8Byte, 1U);
        if (1U == u32Read)
        {
            if (UINT16_MAX > sxDiagnostics.u16RxByteCount)
            {
                sxDiagnostics.u16RxByteCount++;
            }
            if (PN532_DIAG_TRACE_CAPACITY > sxDiagnostics.u8RxTraceLength)
            {
                sxDiagnostics.au8RxTrace[sxDiagnostics.u8RxTraceLength++] = *pu8Byte;
            }
            eStatus = PN532_STATUS_OK;
            break;
        }
    }

    return eStatus;
}

/** @brief Parse an HSU packet with its ACK/NACK or information frame. */
static PN532_Status_t PN532_ReadPacket(uint8_t *pu8Type, uint8_t *pu8Data, uint16_t *pu16DataLength, uint32_t u32TimeoutMs)
{
    PN532_Status_t eStatus = PN532_STATUS_OK;
    uint32_t u32StartMs = millis();
    uint16_t u16Length = 0U;
    uint16_t u16Index = 0U;
    uint8_t u8Byte = 0U;
    uint8_t u8History0 = 0xFFU;
    uint8_t u8History1 = 0xFFU;
    uint8_t u8Len = 0U;
    uint8_t u8Lcs = 0U;
    uint8_t u8Dcs = 0U;
    uint8_t u8Postamble = 0U;
    uint8_t u8Checksum = 0U;

    *pu8Type = 0U;
    *pu16DataLength = 0U;

    /* Ignore stray bytes until the 00 00 FF start sequence is detected. */
    while (PN532_STATUS_OK == eStatus)
    {
        eStatus = PN532_ReadByte(&u8Byte, u32StartMs, u32TimeoutMs);
        if (PN532_STATUS_OK == eStatus)
        {
            if ((PN532_PREAMBLE == u8History0) && (PN532_START_CODE_1 == u8History1) && (PN532_START_CODE_2 == u8Byte))
            {
                break;
            }
            u8History0 = u8History1;
            u8History1 = u8Byte;
        }
    }

    if (PN532_STATUS_OK == eStatus)
    {
        eStatus = PN532_ReadByte(&u8Len, u32StartMs, u32TimeoutMs);
    }
    if (PN532_STATUS_OK == eStatus)
    {
        eStatus = PN532_ReadByte(&u8Lcs, u32StartMs, u32TimeoutMs);
    }

    if (PN532_STATUS_OK == eStatus)
    {
        if ((0x00U == u8Len) && (0xFFU == u8Lcs))
        {
            *pu8Type = PN532_FRAME_TYPE_ACK;
            eStatus = PN532_ReadByte(&u8Postamble, u32StartMs, u32TimeoutMs);
            if ((PN532_STATUS_OK == eStatus) && (0x00U != u8Postamble))
            {
                eStatus = PN532_STATUS_FRAME_ERROR;
            }
        }
        else if ((0xFFU == u8Len) && (0x00U == u8Lcs))
        {
            *pu8Type = PN532_FRAME_TYPE_NACK;
            eStatus = PN532_ReadByte(&u8Postamble, u32StartMs, u32TimeoutMs);
            if ((PN532_STATUS_OK == eStatus) && (0x00U != u8Postamble))
            {
                eStatus = PN532_STATUS_FRAME_ERROR;
            }
        }
        else
        {
            *pu8Type = PN532_FRAME_TYPE_DATA;
            if ((0xFFU == u8Len) && (0xFFU == u8Lcs))
            {
                uint8_t u8LenHi = 0U;
                uint8_t u8LenLo = 0U;
                eStatus = PN532_ReadByte(&u8LenHi, u32StartMs, u32TimeoutMs);
                if (PN532_STATUS_OK == eStatus)
                {
                    eStatus = PN532_ReadByte(&u8LenLo, u32StartMs, u32TimeoutMs);
                }
                if (PN532_STATUS_OK == eStatus)
                {
                    eStatus = PN532_ReadByte(&u8Lcs, u32StartMs, u32TimeoutMs);
                }
                if (PN532_STATUS_OK == eStatus)
                {
                    u16Length = (uint16_t)(((uint16_t)u8LenHi << 8U) | u8LenLo);
                    if (0U != (uint8_t)(u8LenHi + u8LenLo + u8Lcs))
                    {
                        eStatus = PN532_STATUS_CHECKSUM_ERROR;
                    }
                }
            }
            else
            {
                u16Length = u8Len;
                if (0U != (uint8_t)(u8Len + u8Lcs))
                {
                    eStatus = PN532_STATUS_CHECKSUM_ERROR;
                }
            }

            if ((PN532_STATUS_OK == eStatus) && (u16Length > PN532_MAX_FRAME_DATA))
            {
                eStatus = PN532_STATUS_BUFFER_TOO_SMALL;
            }
            if ((PN532_STATUS_OK == eStatus) && (0U == u16Length))
            {
                eStatus = PN532_STATUS_FRAME_ERROR;
            }

            if (PN532_STATUS_OK == eStatus)
            {
                for (u16Index = 0U; u16Index < u16Length; u16Index++)
                {
                    eStatus = PN532_ReadByte(&pu8Data[u16Index], u32StartMs, u32TimeoutMs);
                    if (PN532_STATUS_OK != eStatus)
                    {
                        break;
                    }
                    u8Checksum = (uint8_t)(u8Checksum + pu8Data[u16Index]);
                }
            }
            if (PN532_STATUS_OK == eStatus)
            {
                eStatus = PN532_ReadByte(&u8Dcs, u32StartMs, u32TimeoutMs);
            }
            if (PN532_STATUS_OK == eStatus)
            {
                eStatus = PN532_ReadByte(&u8Postamble, u32StartMs, u32TimeoutMs);
            }
            if ((PN532_STATUS_OK == eStatus) && (0U != (uint8_t)(u8Checksum + u8Dcs)))
            {
                eStatus = PN532_STATUS_CHECKSUM_ERROR;
            }
            if ((PN532_STATUS_OK == eStatus) && (0x00U != u8Postamble))
            {
                eStatus = PN532_STATUS_FRAME_ERROR;
            }
            if (PN532_STATUS_OK == eStatus)
            {
                *pu16DataLength = u16Length;
            }
        }
    }

    return eStatus;
}

/** @brief Encode and transmit normal or extended PN532 information frames. */
static PN532_Status_t PN532_WriteFrame(uint8_t u8Command, const uint8_t *pu8Parameters, uint16_t u16ParameterLength)
{
    PN532_Status_t eStatus = PN532_STATUS_OK;
    uint16_t u16DataLength = (uint16_t)(u16ParameterLength + 2U);
    uint16_t u16Index = 0U;
    uint16_t u16FrameLength = 0U;
    uint8_t u8Checksum = (uint8_t)(PN532_HOST_TO_PN532 + u8Command);

    sau8TxFrame[u16FrameLength++] = 0x00U;
    sau8TxFrame[u16FrameLength++] = 0x00U;
    sau8TxFrame[u16FrameLength++] = 0xFFU;

    if (0xFFU > u16DataLength)
    {
        sau8TxFrame[u16FrameLength++] = (uint8_t)u16DataLength;
        sau8TxFrame[u16FrameLength++] = (uint8_t)(0U - (uint8_t)u16DataLength);
    }
    else
    {
        sau8TxFrame[u16FrameLength++] = 0xFFU;
        sau8TxFrame[u16FrameLength++] = 0xFFU;
        sau8TxFrame[u16FrameLength++] = (uint8_t)(u16DataLength >> 8U);
        sau8TxFrame[u16FrameLength++] = (uint8_t)u16DataLength;
        sau8TxFrame[u16FrameLength++] = (uint8_t)(0U - (uint8_t)((u16DataLength >> 8U) + u16DataLength));
    }

    sau8TxFrame[u16FrameLength++] = PN532_HOST_TO_PN532;
    sau8TxFrame[u16FrameLength++] = u8Command;

    for (u16Index = 0U; u16Index < u16ParameterLength; u16Index++)
    {
        sau8TxFrame[u16FrameLength++] = pu8Parameters[u16Index];
        u8Checksum = (uint8_t)(u8Checksum + pu8Parameters[u16Index]);
    }

    sau8TxFrame[u16FrameLength++] = (uint8_t)(0U - u8Checksum);
    sau8TxFrame[u16FrameLength++] = 0x00U;

    if (u16FrameLength != Serial2_writeBytes(sau8TxFrame, u16FrameLength))
    {
        eStatus = PN532_STATUS_UART_ERROR;
    }

    return eStatus;
}

/** @brief Drop pending bytes from the last aborted transaction. */
static void PN532_ClearRx(void)
{
    uint8_t au8Discard[16U] = {0};
    uint16_t u16Count = 0U;
    while (512U > u16Count)
    {
        uint32_t u32Read = Serial2_readBytes(au8Discard, sizeof(au8Discard));
        if (0U == u32Read)
        {
            break;
        }
        u16Count = (uint16_t)(u16Count + (uint16_t)u32Read);
    }
}

/**
 * @brief Abort an unfinished PN532 operation to keep later commands in sync.
 *
 * @note This writes the host ACK frame. Do not call on a completed command.
 *       Response data may already have been generated and is discarded.
 */
static PN532_Status_t PN532_AbortCurrent(void)
{
    PN532_Status_t eStatus = PN532_STATUS_OK;

    if (sizeof(sau8AckFrame) != Serial2_writeBytes(sau8AckFrame, sizeof(sau8AckFrame)))
    {
        eStatus = PN532_STATUS_UART_ERROR;
    }
    else
    {
        delay(PN532_ABORT_SETTLE_DELAY_MS);
        PN532_ClearRx();
    }

    return eStatus;
}

PN532_Status_t PN532_Transceive(uint8_t u8Command, const uint8_t *pu8Parameters, uint16_t u16ParameterLength, uint8_t *pu8Response, uint16_t u16ResponseCapacity, uint16_t *pu16ResponseLength)
{
    PN532_Status_t eStatus = PN532_STATUS_OK;
    uint8_t u8FrameType = 0U;
    uint16_t u16DataLength = 0U;
    uint16_t u16BodyLength = 0U;
    uint16_t u16Attempt = 0U;

    if (NULL != pu16ResponseLength)
    {
        *pu16ResponseLength = 0U;
    }
    if (false == sbInitialized)
    {
        eStatus = PN532_STATUS_NOT_INITIALIZED;
    }
    else if ((NULL == pu16ResponseLength) || ((0U != u16ParameterLength) && (NULL == pu8Parameters)) || ((0U != u16ResponseCapacity) && (NULL == pu8Response)))
    {
        eStatus = PN532_STATUS_INVALID_ARGUMENT;
    }
    else if (u16ParameterLength > (PN532_MAX_FRAME_DATA - 2U))
    {
        eStatus = PN532_STATUS_BUFFER_TOO_SMALL;
    }
    else
    {
        (void)memset(&sxDiagnostics, 0, sizeof(sxDiagnostics));
        sxDiagnostics.u8Command = u8Command;
        /* Retry only while waiting for ACK; never repeat an acknowledged command. */
        for (u16Attempt = 0U; u16Attempt <= sxConfig.u8CommandRetries; u16Attempt++)
        {
            sxDiagnostics.u8Attempt = (uint8_t)u16Attempt;
            sxDiagnostics.eStage = PN532_STAGE_SEND_COMMAND;
            PN532_ClearRx();
            eStatus = PN532_WriteFrame(u8Command, pu8Parameters, u16ParameterLength);
            if (PN532_STATUS_OK == eStatus)
            {
                sxDiagnostics.eStage = PN532_STAGE_WAIT_ACK;
                eStatus = PN532_ReadPacket(&u8FrameType, sau8RxFrame, &u16DataLength, sxConfig.u32AckTimeoutMs);
                if ((PN532_STATUS_OK == eStatus) && (PN532_FRAME_TYPE_NACK == u8FrameType))
                {
                    eStatus = PN532_STATUS_NACK;
                }
                else if ((PN532_STATUS_OK == eStatus) && (PN532_FRAME_TYPE_ACK != u8FrameType))
                {
                    eStatus = PN532_STATUS_ACK_ERROR;
                }
            }
            if (PN532_STATUS_OK == eStatus)
            {
                break;
            }
            if ((PN532_STATUS_TIMEOUT != eStatus) && (PN532_STATUS_NACK != eStatus))
            {
                break;
            }
            if (u16Attempt < sxConfig.u8CommandRetries)
            {
                /* The command might have been accepted despite a lost ACK.
                 * Abort before another transmission to avoid overlapping work.
                 * For write operations, keep retries disabled (default).
                 */
                if (PN532_STATUS_OK != PN532_AbortCurrent())
                {
                    eStatus = PN532_STATUS_UART_ERROR;
                    break;
                }
            }
        }

        if (PN532_STATUS_OK == eStatus)
        {
            sxDiagnostics.eStage = PN532_STAGE_WAIT_RESPONSE;
            eStatus = PN532_ReadPacket(&u8FrameType, sau8RxFrame, &u16DataLength, sxConfig.u32ResponseTimeoutMs);
            if ((PN532_STATUS_OK == eStatus) && (PN532_FRAME_TYPE_DATA != u8FrameType))
            {
                eStatus = PN532_STATUS_PROTOCOL_ERROR;
            }
            else if ((PN532_STATUS_OK == eStatus) &&
                     (1U == u16DataLength) && (0x7FU == sau8RxFrame[0U]))
            {
                /* PN532 application-level syntax error frame. */
                eStatus = PN532_STATUS_DEVICE_ERROR;
            }
            else if ((PN532_STATUS_OK == eStatus) && (2U > u16DataLength))
            {
                eStatus = PN532_STATUS_PROTOCOL_ERROR;
            }
            else if ((PN532_STATUS_OK == eStatus) && ((PN532_PN532_TO_HOST != sau8RxFrame[0U]) || ((uint8_t)(u8Command + 1U) != sau8RxFrame[1U])))
            {
                eStatus = PN532_STATUS_PROTOCOL_ERROR;
            }
            else if (PN532_STATUS_OK == eStatus)
            {
                u16BodyLength = (uint16_t)(u16DataLength - 2U);
                *pu16ResponseLength = u16BodyLength;
                if (u16BodyLength > u16ResponseCapacity)
                {
                    eStatus = PN532_STATUS_BUFFER_TOO_SMALL;
                }
                else if (0U < u16BodyLength)
                {
                    (void)memcpy(pu8Response, &sau8RxFrame[2U], u16BodyLength);
                }
            }
        }

        /* A scan may still be running when the host-side timeout expires.
         * Explicitly abort it rather than starting another command on top.
         */
        if ((PN532_STATUS_TIMEOUT == eStatus) &&
            ((PN532_STAGE_WAIT_RESPONSE == sxDiagnostics.eStage) ||
             (PN532_STAGE_WAIT_ACK == sxDiagnostics.eStage)))
        {
            if (PN532_STATUS_OK != PN532_AbortCurrent())
            {
                eStatus = PN532_STATUS_UART_ERROR;
            }
        }
    }

    if (PN532_STATUS_OK == eStatus)
    {
        sxDiagnostics.eStage = PN532_STAGE_READY;
    }
    return PN532_Finish(eStatus);
}

PN532_Status_t PN532_Begin(void)
{
    PN532_Config_t xDefault = {PN532_DEFAULT_ACK_TIMEOUT_MS, PN532_DEFAULT_RESP_TIMEOUT_MS, PN532_DEFAULT_RETRIES, PN532_DEFAULT_COMMAND_RETRIES};
    return PN532_Init(&xDefault);
}

PN532_Status_t PN532_Init(const PN532_Config_t *pxConfig)
{
    PN532_Status_t eStatus = PN532_STATUS_OK;
    PN532_Firmware_t xFirmware = {0};
    uint8_t au8WakeUp[PN532_HSU_WAKE_LENGTH] = {0U};

    if ((NULL == pxConfig) || (0U == pxConfig->u32AckTimeoutMs) || (0U == pxConfig->u32ResponseTimeoutMs))
    {
        eStatus = PN532_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        sbInitialized = false;
        sbTargetAvailable = false;
        PN532_InvalidateEasyCache();
        su8LastChipStatus = 0U;
        (void)memset(&sxCurrentCard, 0, sizeof(sxCurrentCard));
        sxConfig = *pxConfig;
        Serial2_begin(PN532_HSU_BAUD_RATE);
        delay(50U);
        PN532_ClearRx();
        (void)memset(&sxDiagnostics, 0, sizeof(sxDiagnostics));
        sxDiagnostics.eStage = PN532_STAGE_WAKE;
        /* Tested S32K144/Elechouse HSU cold-start sequence:
         * 55 55 followed by 32 zero bytes, then GetFirmwareVersion.
         * Keep the command immediately after the preamble as in the
         * successful hardware test (43 total transmitted bytes).
         */
        au8WakeUp[0U] = 0x55U;
        au8WakeUp[1U] = 0x55U;
        if (sizeof(au8WakeUp) != Serial2_writeBytes(au8WakeUp, sizeof(au8WakeUp)))
        {
            eStatus = PN532_STATUS_UART_ERROR;
        }
        if (PN532_STATUS_OK == eStatus)
        {
            sbInitialized = true;
            eStatus = PN532_GetFirmwareVersion(&xFirmware);
        }
        if ((PN532_STATUS_OK == eStatus) && (0x32U != xFirmware.u8IC))
        {
            eStatus = PN532_STATUS_PROTOCOL_ERROR;
        }
        if (PN532_STATUS_OK == eStatus)
        {
            eStatus = PN532_SAMConfiguration();
        }
        if (PN532_STATUS_OK == eStatus)
        {
            eStatus = PN532_SetPassiveActivationRetries(sxConfig.u8PassiveActivationRetries);
        }
        if (PN532_STATUS_OK != eStatus)
        {
            sbInitialized = false;
        }
    }

    return PN532_Finish(eStatus);
}

PN532_Status_t PN532_GetFirmwareVersion(PN532_Firmware_t *pxFirmware)
{
    PN532_Status_t eStatus = PN532_STATUS_OK;
    uint8_t au8Response[4U] = {0};
    uint16_t u16Length = 0U;

    if (NULL == pxFirmware)
    {
        eStatus = PN532_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        eStatus = PN532_Transceive(PN532_CMD_GET_FIRMWARE, NULL, 0U, au8Response, sizeof(au8Response), &u16Length);
        if ((PN532_STATUS_OK == eStatus) && (4U != u16Length))
        {
            eStatus = PN532_STATUS_PROTOCOL_ERROR;
        }
        if (PN532_STATUS_OK == eStatus)
        {
            pxFirmware->u8IC = au8Response[0U];
            pxFirmware->u8Version = au8Response[1U];
            pxFirmware->u8Revision = au8Response[2U];
            pxFirmware->u8Support = au8Response[3U];
        }
    }

    return PN532_Finish(eStatus);
}

PN532_Status_t PN532_SAMConfiguration(void)
{
    uint8_t au8Parameters[3U] = {0x01U, 0x14U, 0x01U};
    uint16_t u16Length = 0U;
    PN532_Status_t eStatus = PN532_Transceive(PN532_CMD_SAM_CONFIGURATION, au8Parameters, sizeof(au8Parameters), NULL, 0U, &u16Length);

    if ((PN532_STATUS_OK == eStatus) && (0U != u16Length))
    {
        eStatus = PN532_STATUS_PROTOCOL_ERROR;
    }

    return PN532_Finish(eStatus);
}

PN532_Status_t PN532_SetRFField(bool bEnabled)
{
    uint8_t au8Parameters[2U] = {PN532_RF_CONFIG_FIELD, 0U};
    uint16_t u16Length = 0U;
    PN532_Status_t eStatus = PN532_STATUS_OK;

    if (true == bEnabled)
    {
        au8Parameters[1U] = 0x01U;
    }
    eStatus = PN532_Transceive(PN532_CMD_RF_CONFIGURATION, au8Parameters, sizeof(au8Parameters), NULL, 0U, &u16Length);
    if ((PN532_STATUS_OK == eStatus) && (0U != u16Length))
    {
        eStatus = PN532_STATUS_PROTOCOL_ERROR;
    }
    if ((PN532_STATUS_OK == eStatus) && (false == bEnabled))
    {
        sbTargetAvailable = false;
    }
    return PN532_Finish(eStatus);
}

PN532_Status_t PN532_SetPassiveActivationRetries(uint8_t u8Retries)
{
    uint8_t au8Parameters[4U] = {PN532_RF_CONFIG_MAX_RETRIES, 0xFFU, 0x01U, u8Retries};
    uint16_t u16Length = 0U;
    PN532_Status_t eStatus = PN532_Transceive(PN532_CMD_RF_CONFIGURATION, au8Parameters, sizeof(au8Parameters), NULL, 0U, &u16Length);

    if ((PN532_STATUS_OK == eStatus) && (0U != u16Length))
    {
        eStatus = PN532_STATUS_PROTOCOL_ERROR;
    }
    if (PN532_STATUS_OK == eStatus)
    {
        sxConfig.u8PassiveActivationRetries = u8Retries;
    }
    return PN532_Finish(eStatus);
}

PN532_Status_t PN532_ReadPassiveTarget(PN532_Card_t *pxCard)
{
    PN532_Status_t eStatus = PN532_STATUS_OK;
    uint8_t au8Parameters[2U] = {0x01U, 0x00U};
    uint8_t au8Response[PN532_MAX_FRAME_DATA - 2U] = {0};
    uint16_t u16Length = 0U;
    uint8_t u8UIDLength = 0U;

    if (NULL == pxCard)
    {
        eStatus = PN532_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        sbTargetAvailable = false;
        PN532_InvalidateEasyCache();
        (void)memset(pxCard, 0, sizeof(*pxCard));
        eStatus = PN532_Transceive(PN532_CMD_IN_LIST_PASSIVE, au8Parameters, sizeof(au8Parameters), au8Response, sizeof(au8Response), &u16Length);
        if ((PN532_STATUS_OK == eStatus) && (1U > u16Length))
        {
            eStatus = PN532_STATUS_PROTOCOL_ERROR;
        }
        else if ((PN532_STATUS_OK == eStatus) && (0U == au8Response[0U]))
        {
            eStatus = PN532_STATUS_NO_CARD;
        }
        else if ((PN532_STATUS_OK == eStatus) && ((1U != au8Response[0U]) || (6U > u16Length) || (0U == au8Response[1U])))
        {
            eStatus = PN532_STATUS_PROTOCOL_ERROR;
        }
        else if (PN532_STATUS_OK == eStatus)
        {
            u8UIDLength = au8Response[5U];
            if ((0U == u8UIDLength) || (PN532_UID_MAX_LENGTH < u8UIDLength) || ((uint16_t)(6U + u8UIDLength) > u16Length))
            {
                eStatus = PN532_STATUS_PROTOCOL_ERROR;
            }
            else
            {
                pxCard->u8TargetNumber = au8Response[1U];
                pxCard->u16ATQA = (uint16_t)(((uint16_t)au8Response[2U] << 8U) | au8Response[3U]);
                pxCard->u8SAK = au8Response[4U];
                pxCard->u8UIDLength = u8UIDLength;
                (void)memcpy(pxCard->au8UID, &au8Response[6U], u8UIDLength);
                sxCurrentCard = *pxCard;
                sbTargetAvailable = true;
                PN532_EncodeHex(pxCard->au8UID, pxCard->u8UIDLength,
                                sacUIDText);
            }
        }
    }

    return PN532_Finish(eStatus);
}

PN532_Status_t PN532_ReadUID(uint8_t *pu8UID, uint8_t u8Capacity, uint8_t *pu8UIDLength)
{
    PN532_Status_t eStatus = PN532_STATUS_OK;
    PN532_Card_t xCard = {0};

    if (NULL != pu8UIDLength)
    {
        *pu8UIDLength = 0U;
    }
    if ((NULL == pu8UID) || (NULL == pu8UIDLength) || (0U == u8Capacity))
    {
        eStatus = PN532_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        eStatus = PN532_ReadPassiveTarget(&xCard);
        if ((PN532_STATUS_OK == eStatus) && (xCard.u8UIDLength > u8Capacity))
        {
            eStatus = PN532_STATUS_BUFFER_TOO_SMALL;
        }
        if (PN532_STATUS_OK == eStatus)
        {
            (void)memcpy(pu8UID, xCard.au8UID, xCard.u8UIDLength);
            *pu8UIDLength = xCard.u8UIDLength;
        }
    }

    return PN532_Finish(eStatus);
}

bool PN532_IsCardPresent(void)
{
    PN532_Card_t xCard = {0};
    return (PN532_STATUS_OK == PN532_ReadPassiveTarget(&xCard));
}

PN532_Status_t PN532_DataExchange(const uint8_t *pu8TxData, uint16_t u16TxLength, uint8_t *pu8RxData, uint16_t u16RxCapacity, uint16_t *pu16RxLength)
{
    PN532_Status_t eStatus = PN532_STATUS_OK;
    uint8_t au8Parameters[PN532_MAX_FRAME_DATA - 2U] = {0};
    uint8_t au8Response[PN532_MAX_FRAME_DATA - 2U] = {0};
    uint16_t u16Length = 0U;
    uint16_t u16DataLength = 0U;

    if (NULL != pu16RxLength)
    {
        *pu16RxLength = 0U;
    }
    if ((NULL == pu16RxLength) || ((0U != u16TxLength) && (NULL == pu8TxData)) || ((0U != u16RxCapacity) && (NULL == pu8RxData)))
    {
        eStatus = PN532_STATUS_INVALID_ARGUMENT;
    }
    else if (false == sbTargetAvailable)
    {
        eStatus = PN532_STATUS_NO_TARGET;
    }
    else if (u16TxLength > (sizeof(au8Parameters) - 1U))
    {
        eStatus = PN532_STATUS_BUFFER_TOO_SMALL;
    }
    else
    {
        su8LastChipStatus = 0U;
        au8Parameters[0U] = sxCurrentCard.u8TargetNumber;
        if (0U != u16TxLength)
        {
            (void)memcpy(&au8Parameters[1U], pu8TxData, u16TxLength);
        }
        eStatus = PN532_Transceive(PN532_CMD_IN_DATA_EXCHANGE, au8Parameters, (uint16_t)(u16TxLength + 1U), au8Response, sizeof(au8Response), &u16Length);
        if ((PN532_STATUS_OK == eStatus) && (1U > u16Length))
        {
            eStatus = PN532_STATUS_PROTOCOL_ERROR;
        }
        else if (PN532_STATUS_OK == eStatus)
        {
            su8LastChipStatus = au8Response[0U];
            if (0U != (uint8_t)(su8LastChipStatus & 0x3FU))
            {
                eStatus = PN532_STATUS_DEVICE_ERROR;
            }
            else
            {
                u16DataLength = (uint16_t)(u16Length - 1U);
                *pu16RxLength = u16DataLength;
                if (u16DataLength > u16RxCapacity)
                {
                    eStatus = PN532_STATUS_BUFFER_TOO_SMALL;
                }
                else
                {
                    if (0U < u16DataLength)
                    {
                        (void)memcpy(pu8RxData, &au8Response[1U], u16DataLength);
                    }
                    if (0U != (uint8_t)(su8LastChipStatus & 0x40U))
                    {
                        eStatus = PN532_STATUS_MORE_DATA;
                    }
                }
            }
        }
    }

    return PN532_Finish(eStatus);
}

PN532_Status_t PN532_MifareAuthenticate(uint8_t u8Block, bool bKeyB, const uint8_t au8Key[PN532_MIFARE_KEY_LENGTH])
{
    PN532_Status_t eStatus = PN532_STATUS_OK;
    uint8_t au8Command[2U + PN532_MIFARE_KEY_LENGTH + PN532_UID_MAX_LENGTH] = {0};
    uint16_t u16Length = 0U;

    if (NULL == au8Key)
    {
        eStatus = PN532_STATUS_INVALID_ARGUMENT;
    }
    else if (false == sbTargetAvailable)
    {
        eStatus = PN532_STATUS_NO_TARGET;
    }
    else
    {
        au8Command[0U] = (true == bKeyB) ? PN532_MIFARE_AUTH_B : PN532_MIFARE_AUTH_A;
        au8Command[1U] = u8Block;
        (void)memcpy(&au8Command[2U], au8Key, PN532_MIFARE_KEY_LENGTH);
        /* MIFARE Classic authentication uses the final four NFCID1 bytes. */
        if (4U > sxCurrentCard.u8UIDLength)
        {
            eStatus = PN532_STATUS_PROTOCOL_ERROR;
        }
        else
        {
            (void)memcpy(&au8Command[2U + PN532_MIFARE_KEY_LENGTH],
                         &sxCurrentCard.au8UID[sxCurrentCard.u8UIDLength - 4U], 4U);
            eStatus = PN532_DataExchange(au8Command,
                                         (uint16_t)(2U + PN532_MIFARE_KEY_LENGTH + 4U),
                                         NULL, 0U, &u16Length);
        }
        if ((PN532_STATUS_OK == eStatus) && (0U != u16Length))
        {
            eStatus = PN532_STATUS_PROTOCOL_ERROR;
        }
    }

    return PN532_Finish(eStatus);
}

PN532_Status_t PN532_MifareReadBlock(uint8_t u8Block, uint8_t au8Data[PN532_MIFARE_BLOCK_SIZE])
{
    PN532_Status_t eStatus = PN532_STATUS_OK;
    uint8_t au8Command[2U] = {PN532_MIFARE_READ, u8Block};
    uint16_t u16Length = 0U;

    if (NULL == au8Data)
    {
        eStatus = PN532_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        eStatus = PN532_DataExchange(au8Command, sizeof(au8Command), au8Data, PN532_MIFARE_BLOCK_SIZE, &u16Length);
        if ((PN532_STATUS_OK == eStatus) && (PN532_MIFARE_BLOCK_SIZE != u16Length))
        {
            eStatus = PN532_STATUS_PROTOCOL_ERROR;
        }
    }
    return PN532_Finish(eStatus);
}

PN532_Status_t PN532_MifareWriteBlock(uint8_t u8Block, const uint8_t au8Data[PN532_MIFARE_BLOCK_SIZE])
{
    PN532_Status_t eStatus = PN532_STATUS_OK;
    uint8_t au8Command[2U + PN532_MIFARE_BLOCK_SIZE] = {PN532_MIFARE_WRITE, u8Block};
    uint16_t u16Length = 0U;

    if (NULL == au8Data)
    {
        eStatus = PN532_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        (void)memcpy(&au8Command[2U], au8Data, PN532_MIFARE_BLOCK_SIZE);
        eStatus = PN532_DataExchange(au8Command, sizeof(au8Command), NULL, 0U, &u16Length);
        if ((PN532_STATUS_OK == eStatus) && (0U != u16Length))
        {
            eStatus = PN532_STATUS_PROTOCOL_ERROR;
        }
    }
    return PN532_Finish(eStatus);
}

PN532_Status_t PN532_UltralightReadPages(uint8_t u8FirstPage, uint8_t au8Data[PN532_MIFARE_BLOCK_SIZE])
{
    PN532_Status_t eStatus = PN532_STATUS_OK;
    uint8_t au8Command[2U] = {PN532_MIFARE_READ, u8FirstPage};
    uint16_t u16Length = 0U;

    if (NULL == au8Data)
    {
        eStatus = PN532_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        eStatus = PN532_DataExchange(au8Command, sizeof(au8Command), au8Data, PN532_MIFARE_BLOCK_SIZE, &u16Length);
        if ((PN532_STATUS_OK == eStatus) && (PN532_MIFARE_BLOCK_SIZE != u16Length))
        {
            eStatus = PN532_STATUS_PROTOCOL_ERROR;
        }
    }
    return PN532_Finish(eStatus);
}

PN532_Status_t PN532_UltralightWritePage(uint8_t u8Page, const uint8_t au8Data[PN532_ULTRALIGHT_PAGE_SIZE])
{
    PN532_Status_t eStatus = PN532_STATUS_OK;
    uint8_t au8Command[2U + PN532_ULTRALIGHT_PAGE_SIZE] = {PN532_ULTRALIGHT_WRITE, u8Page};
    uint16_t u16Length = 0U;

    if (NULL == au8Data)
    {
        eStatus = PN532_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        (void)memcpy(&au8Command[2U], au8Data, PN532_ULTRALIGHT_PAGE_SIZE);
        eStatus = PN532_DataExchange(au8Command, sizeof(au8Command), NULL, 0U, &u16Length);
        if ((PN532_STATUS_OK == eStatus) && (0U != u16Length))
        {
            eStatus = PN532_STATUS_PROTOCOL_ERROR;
        }
    }
    return PN532_Finish(eStatus);
}

PN532_Status_t PN532_ReleaseTarget(void)
{
    PN532_Status_t eStatus = PN532_STATUS_OK;
    uint8_t u8Target = sxCurrentCard.u8TargetNumber;
    uint8_t au8Response[1U] = {0};
    uint16_t u16Length = 0U;

    if (false == sbTargetAvailable)
    {
        eStatus = PN532_STATUS_NO_TARGET;
    }
    else
    {
        eStatus = PN532_Transceive(PN532_CMD_IN_RELEASE, &u8Target, 1U, au8Response, sizeof(au8Response), &u16Length);
        if ((PN532_STATUS_OK == eStatus) && (1U != u16Length))
        {
            eStatus = PN532_STATUS_PROTOCOL_ERROR;
        }
        if ((PN532_STATUS_OK == eStatus) && (0U != (uint8_t)(au8Response[0U] & 0x3FU)))
        {
            su8LastChipStatus = au8Response[0U];
            eStatus = PN532_STATUS_DEVICE_ERROR;
        }
        sbTargetAvailable = false;
        PN532_InvalidateEasyCache();
    }
    return PN532_Finish(eStatus);
}

PN532_Status_t PN532_GetLastError(void)
{
    return seLastError;
}

uint8_t PN532_GetLastChipStatus(void)
{
    return su8LastChipStatus;
}

void PN532_GetDiagnostics(PN532_Diagnostics_t *pxDiagnostics)
{
    if (NULL != pxDiagnostics)
    {
        *pxDiagnostics = sxDiagnostics;
    }
}

const char *PN532_StageString(PN532_Stage_t eStage)
{
    const char *pcText = "UNKNOWN";
    switch (eStage)
    {
        case PN532_STAGE_IDLE: pcText = "IDLE"; break;
        case PN532_STAGE_WAKE: pcText = "WAKE"; break;
        case PN532_STAGE_SEND_COMMAND: pcText = "SEND_COMMAND"; break;
        case PN532_STAGE_WAIT_ACK: pcText = "WAIT_ACK"; break;
        case PN532_STAGE_WAIT_RESPONSE: pcText = "WAIT_RESPONSE"; break;
        case PN532_STAGE_READY: pcText = "READY"; break;
        default: break;
    }
    return pcText;
}

const char *PN532_StatusString(PN532_Status_t eStatus)
{
    const char *pcText = "UNKNOWN";
    switch (eStatus)
    {
        case PN532_STATUS_OK: pcText = "OK"; break;
        case PN532_STATUS_INVALID_ARGUMENT: pcText = "INVALID_ARGUMENT"; break;
        case PN532_STATUS_NOT_INITIALIZED: pcText = "NOT_INITIALIZED"; break;
        case PN532_STATUS_UART_ERROR: pcText = "UART_ERROR"; break;
        case PN532_STATUS_TIMEOUT: pcText = "TIMEOUT"; break;
        case PN532_STATUS_NACK: pcText = "NACK"; break;
        case PN532_STATUS_ACK_ERROR: pcText = "ACK_ERROR"; break;
        case PN532_STATUS_FRAME_ERROR: pcText = "FRAME_ERROR"; break;
        case PN532_STATUS_CHECKSUM_ERROR: pcText = "CHECKSUM_ERROR"; break;
        case PN532_STATUS_BUFFER_TOO_SMALL: pcText = "BUFFER_TOO_SMALL"; break;
        case PN532_STATUS_PROTOCOL_ERROR: pcText = "PROTOCOL_ERROR"; break;
        case PN532_STATUS_DEVICE_ERROR: pcText = "DEVICE_ERROR"; break;
        case PN532_STATUS_NO_CARD: pcText = "NO_CARD"; break;
        case PN532_STATUS_NO_TARGET: pcText = "NO_TARGET"; break;
        case PN532_STATUS_MORE_DATA: pcText = "MORE_DATA"; break;
        default: break;
    }
    return pcText;
}


/* =========================== EASY API ============================ */

const char *PN532_GetUID(void)
{
    return sacUIDText;
}

uint8_t PN532_GetUIDLength(void)
{
    return (true == sbTargetAvailable) ? sxCurrentCard.u8UIDLength : 0U;
}

uint16_t PN532_GetATQA(void)
{
    return (true == sbTargetAvailable) ? sxCurrentCard.u16ATQA : 0U;
}

uint8_t PN532_GetSAK(void)
{
    return (true == sbTargetAvailable) ? sxCurrentCard.u8SAK : 0U;
}

const char *PN532_GetCardType(void)
{
    const char *pcType = "NO_CARD";
    if (true == sbTargetAvailable)
    {
        pcType = "ISO14443A / UNKNOWN";
        if ((0x08U == sxCurrentCard.u8SAK) &&
            (0x0004U == sxCurrentCard.u16ATQA))
        {
            pcType = "MIFARE Classic 1K (candidate)";
        }
        else if (0x00U == sxCurrentCard.u8SAK)
        {
            pcType = "Type 2 / Ultralight / NTAG (candidate)";
        }
    }
    return pcType;
}

static int8_t PN532_HexNibble(char c)
{
    int8_t i8Value = -1;
    if ((c >= '0') && (c <= '9'))
    {
        i8Value = (int8_t)(c - '0');
    }
    else if ((c >= 'A') && (c <= 'F'))
    {
        i8Value = (int8_t)(c - 'A' + 10);
    }
    else if ((c >= 'a') && (c <= 'f'))
    {
        i8Value = (int8_t)(c - 'a' + 10);
    }
    return i8Value;
}

bool PN532_Authenticate(uint8_t u8Block, const char *pcKeyHex)
{
    PN532_Status_t eStatus = PN532_STATUS_OK;
    uint8_t au8Key[PN532_MIFARE_KEY_LENGTH] = {0};
    uint8_t u8Index = 0U;
    int8_t i8Hi = 0;
    int8_t i8Lo = 0;

    if (NULL == pcKeyHex)
    {
        eStatus = PN532_STATUS_INVALID_ARGUMENT;
    }
    else if (12U != strlen(pcKeyHex))
    {
        eStatus = PN532_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        for (u8Index = 0U; u8Index < PN532_MIFARE_KEY_LENGTH; u8Index++)
        {
            i8Hi = PN532_HexNibble(pcKeyHex[2U * u8Index]);
            i8Lo = PN532_HexNibble(pcKeyHex[2U * u8Index + 1U]);
            if ((0 > i8Hi) || (0 > i8Lo))
            {
                eStatus = PN532_STATUS_INVALID_ARGUMENT;
                break;
            }
            au8Key[u8Index] = (uint8_t)((((uint8_t)i8Hi) << 4U) |
                                         (uint8_t)i8Lo);
        }
        if (PN532_STATUS_OK == eStatus)
        {
            eStatus = PN532_MifareAuthenticate(u8Block, false, au8Key);
        }
    }
    return (PN532_STATUS_OK == PN532_Finish(eStatus));
}

bool PN532_ReadBlock(uint8_t u8Block)
{
    uint8_t au8Data[PN532_MIFARE_BLOCK_SIZE] = {0};
    PN532_Status_t eStatus = PN532_STATUS_OK;
    sbBlockValid = false;
    sacBlockText[0U] = '\0';
    eStatus = PN532_MifareReadBlock(u8Block, au8Data);
    if (PN532_STATUS_OK == eStatus)
    {
        PN532_EncodeHex(au8Data, PN532_MIFARE_BLOCK_SIZE, sacBlockText);
        sbBlockValid = true;
    }
    return (PN532_STATUS_OK == PN532_Finish(eStatus));
}

const char *PN532_GetBlockHex(void)
{
    return (true == sbBlockValid) ? sacBlockText : "";
}

bool PN532_End(void)
{
    PN532_Status_t eStatus = PN532_ReleaseTarget();
    return (PN532_STATUS_OK == eStatus);
}
