/**
 * @file hardware_serial.c
 * @brief Arduino-style hardware serial API implementation.
 *
 * @details
 * Provides Serial1 and Serial2 interfaces using the
 * EduFramework LPUART driver.
 *
 * Serial1 -> LPUART1
 * Serial2 -> LPUART2
 */

#include "hardware_serial.h"

#include "lpuart.h"
#include "irq.h"

#include <math.h>

#define HARDWARE_SERIAL_NULL_PTR ((void *)0)

#define HARDWARE_SERIAL_SRC_CLOCK_HZ (40000000U)

#define HARDWARE_SERIAL_FLOAT_SCALE (1000U)

#define HARDWARE_SERIAL_FLOAT_MAX (4294967040.0f)

#define HARDWARE_SERIAL_NEWLINE "\r\n"

#define HARDWARE_SERIAL_INT_BUFFER_SIZE (16U)

#define HARDWARE_SERIAL_LINE_BUFFER_SIZE (128U)

typedef struct
{
    char acBuffer[HARDWARE_SERIAL_LINE_BUFFER_SIZE];
    uint32_t u32Length;
    bool bOverflow;
} HardwareSerial_LineState_t;

static HardwareSerial_LineState_t sxSerial1Line = {0};
static HardwareSerial_LineState_t sxSerial2Line = {0};

/* ============================================================
 * Internal functions
 * ============================================================ */

/**
 * @brief Initialize a hardware serial instance.
 *
 * @param[in] pBase LPUART peripheral.
 * @param[in] u32BaudRate Desired baud rate.
 *
 * @return None.
 */
static void HardwareSerial_BeginInternal(
    LPUART_Type *pBase,
    uint32_t u32BaudRate)
{
    LPUART_Config_t xConfig = {0};

    if ((HARDWARE_SERIAL_NULL_PTR != pBase) &&
        (0U != u32BaudRate))
    {
        xConfig.u32BaudRate = u32BaudRate;
        xConfig.u32SrcClockHz =
            HARDWARE_SERIAL_SRC_CLOCK_HZ;

        if (LPUART_STATUS_OK == LPUART_Init(pBase, &xConfig))
        {
            if (IP_LPUART1 == pBase)
            {
                IRQ_LPUART1_RxTx_Init();
            }
            else if (IP_LPUART2 == pBase)
            {
                IRQ_LPUART2_RxTx_Init();
            }
            else
            {
                /* Unsupported instance. */
            }

            LPUART_EnableRxInterrupt(pBase);
        }
    }
}

/**
 * @brief Transmit one character.
 */
static void HardwareSerial_PrintChar(
    LPUART_Type *pBase,
    char cCharacter)
{
    if (HARDWARE_SERIAL_NULL_PTR != pBase)
    {
        LPUART_WriteChar(pBase, cCharacter);
    }
}

/**
 * @brief Transmit a null-terminated string.
 */
static void HardwareSerial_PrintString(
    LPUART_Type *pBase,
    const char *pcString)
{
    if ((HARDWARE_SERIAL_NULL_PTR != pBase) &&
        (HARDWARE_SERIAL_NULL_PTR != pcString))
    {
        LPUART_WriteString(pBase, pcString);
    }
}

/**
 * @brief Transmit a string followed by CRLF.
 */
static void HardwareSerial_PrintlnString(
    LPUART_Type *pBase,
    const char *pcString)
{
    HardwareSerial_PrintString(pBase, pcString);
    HardwareSerial_PrintString(
        pBase,
        HARDWARE_SERIAL_NEWLINE);
}

/**
 * @brief Reverse characters in a string buffer.
 */
static void HardwareSerial_ReverseBuffer(
    char *pcBuffer,
    uint32_t u32Length)
{
    uint32_t u32LeftIndex = 0U;
    uint32_t u32RightIndex = 0U;
    char cTemp = '\0';

    if ((HARDWARE_SERIAL_NULL_PTR != pcBuffer) &&
        (0U != u32Length))
    {
        u32RightIndex = u32Length - 1U;

        while (u32LeftIndex < u32RightIndex)
        {
            cTemp = pcBuffer[u32LeftIndex];

            pcBuffer[u32LeftIndex] =
                pcBuffer[u32RightIndex];

            pcBuffer[u32RightIndex] = cTemp;

            u32LeftIndex++;
            u32RightIndex--;
        }
    }
}

/**
 * @brief Convert an unsigned integer to a decimal string.
 */
static void HardwareSerial_UintToString(
    uint32_t u32Value,
    char *pcBuffer)
{
    uint32_t u32Index = 0U;
    uint32_t u32WorkingValue = u32Value;

    if (HARDWARE_SERIAL_NULL_PTR != pcBuffer)
    {
        if (0U == u32WorkingValue)
        {
            pcBuffer[0] = '0';
            pcBuffer[1] = '\0';
        }
        else
        {
            while (0U < u32WorkingValue)
            {
                pcBuffer[u32Index] =
                    (char)('0' + (u32WorkingValue % 10U));

                u32WorkingValue /= 10U;
                u32Index++;
            }

            pcBuffer[u32Index] = '\0';

            HardwareSerial_ReverseBuffer(
                pcBuffer,
                u32Index);
        }
    }
}

/**
 * @brief Print a signed 32-bit integer.
 *
 * @details
 * Handles INT32_MIN without signed integer overflow.
 */
static void HardwareSerial_PrintIntInternal(
    LPUART_Type *pBase,
    int32_t s32Value)
{
    char acBuffer[HARDWARE_SERIAL_INT_BUFFER_SIZE] = {0};
    uint32_t u32AbsValue = 0U;

    if (HARDWARE_SERIAL_NULL_PTR != pBase)
    {
        if (0 > s32Value)
        {
            HardwareSerial_PrintChar(pBase, '-');

            u32AbsValue =
                (uint32_t)(-(s32Value + 1)) + 1U;
        }
        else
        {
            u32AbsValue = (uint32_t)s32Value;
        }

        HardwareSerial_UintToString(
            u32AbsValue,
            acBuffer);

        HardwareSerial_PrintString(pBase, acBuffer);
    }
}

/**
 * @brief Print a floating-point value.
 *
 * @details
 * Prints three digits after the decimal point.
 * Handles NaN, infinity and out-of-range values.
 */
static void HardwareSerial_PrintFloatInternal(
    LPUART_Type *pBase,
    float f32Number)
{
    char acIntBuffer[HARDWARE_SERIAL_INT_BUFFER_SIZE] = {0};

    uint32_t u32IntPart = 0U;
    uint32_t u32FracPart = 0U;
    uint32_t u32Divisor = 0U;

    float f32AbsNumber = 0.0f;
    float f32Fraction = 0.0f;

    bool bValidNumber = true;

    if (HARDWARE_SERIAL_NULL_PTR != pBase)
    {
        if (isnan(f32Number))
        {
            HardwareSerial_PrintString(pBase, "nan");
            bValidNumber = false;
        }
        else if (isinf(f32Number))
        {
            if (signbit(f32Number))
            {
                HardwareSerial_PrintChar(pBase, '-');
            }

            HardwareSerial_PrintString(pBase, "inf");
            bValidNumber = false;
        }
        else
        {
            /* Finite number. */
        }

        if (true == bValidNumber)
        {
            if (signbit(f32Number))
            {
                HardwareSerial_PrintChar(pBase, '-');
                f32AbsNumber = -f32Number;
            }
            else
            {
                f32AbsNumber = f32Number;
            }

            if (HARDWARE_SERIAL_FLOAT_MAX <= f32AbsNumber)
            {
                HardwareSerial_PrintString(pBase, "ovf");
                bValidNumber = false;
            }
        }

        if (true == bValidNumber)
        {
            u32IntPart = (uint32_t)f32AbsNumber;

            f32Fraction =
                f32AbsNumber - (float)u32IntPart;

            u32FracPart = (uint32_t)((f32Fraction *
                                      (float)HARDWARE_SERIAL_FLOAT_SCALE) +
                                     0.5f);

            if (HARDWARE_SERIAL_FLOAT_SCALE <= u32FracPart)
            {
                u32IntPart++;
                u32FracPart = 0U;
            }

            HardwareSerial_UintToString(
                u32IntPart,
                acIntBuffer);

            HardwareSerial_PrintString(
                pBase,
                acIntBuffer);

            HardwareSerial_PrintChar(pBase, '.');

            u32Divisor =
                HARDWARE_SERIAL_FLOAT_SCALE / 10U;

            while (0U < u32Divisor)
            {
                HardwareSerial_PrintChar(
                    pBase,
                    (char)('0' +
                           ((u32FracPart / u32Divisor) % 10U)));

                u32Divisor /= 10U;
            }
        }
    }
}

/**
 * @brief Read currently available characters into a string.
 *
 * @details
 * Stops when the buffer is full, newline is received,
 * or no more characters are available.
 *
 * Carriage return is ignored.
 */
static uint32_t HardwareSerial_ReadStringInternal(
    LPUART_Type *pBase,
    char *pcBuffer,
    uint32_t u32MaxLength)
{
    uint32_t u32Index = 0U;
    char cCharacter = '\0';

    bool bShouldContinue = true;

    if ((HARDWARE_SERIAL_NULL_PTR != pBase) &&
        (HARDWARE_SERIAL_NULL_PTR != pcBuffer) &&
        (0U != u32MaxLength))
    {
        while ((true == bShouldContinue) &&
               (true == LPUART_IsDataAvailable(pBase)))
        {
            cCharacter = LPUART_GetChar(pBase);

            if ('\r' == cCharacter)
            {
                /* Ignore carriage return. */
            }
            else if ('\n' == cCharacter)
            {
                bShouldContinue = false;
            }
            else if (u32Index < (u32MaxLength - 1U))
            {
                pcBuffer[u32Index] = cCharacter;
                u32Index++;
            }
            else
            {
                bShouldContinue = false;
            }
        }

        pcBuffer[u32Index] = '\0';
    }

    return u32Index;
}

/**
 * @brief Reset line receiver state.
 *
 * @param[in,out] pxState Receiver state.
 */
static void HardwareSerial_ResetLineState(
    HardwareSerial_LineState_t *pxState)
{
    if (HARDWARE_SERIAL_NULL_PTR != pxState)
    {
        pxState->u32Length = 0U;
        pxState->bOverflow = false;
    }
}

/**
 * @brief Receive a complete newline-terminated string.
 *
 * @details
 * Non-blocking receiver. Characters are accumulated
 * across calls using persistent state.
 *
 * A line exceeding either the internal buffer capacity
 * or the caller's buffer capacity is discarded until LF.
 *
 * @param[in] pBase LPUART peripheral.
 * @param[in,out] pxState Persistent line receiver state.
 * @param[out] pcBuffer Destination buffer.
 * @param[in] u32MaxLength Destination capacity.
 *
 * @return true if a complete valid line was received.
 */
static bool HardwareSerial_ReadLineInternal(
    LPUART_Type *pBase,
    HardwareSerial_LineState_t *pxState,
    char *pcBuffer,
    uint32_t u32MaxLength)
{
    bool bLineReady = false;
    bool bContinue = true;
    char cCharacter = '\0';
    uint32_t u32Index = 0U;
    if ((HARDWARE_SERIAL_NULL_PTR != pBase) &&
        (HARDWARE_SERIAL_NULL_PTR != pxState) &&
        (HARDWARE_SERIAL_NULL_PTR != pcBuffer) &&
        (0U < u32MaxLength))
    {
        pcBuffer[0] = '\0';
        while ((true == bContinue) &&
               (true == LPUART_IsDataAvailable(pBase)))
        {
            cCharacter = LPUART_GetChar(pBase);
            if ('\n' == cCharacter)
            {
                if (false == pxState->bOverflow)
                {
                    for (u32Index = 0U;
                         u32Index < pxState->u32Length;
                         u32Index++)
                    {
                        pcBuffer[u32Index] =
                            pxState->acBuffer[u32Index];
                    }
                    pcBuffer[pxState->u32Length] = '\0';
                    bLineReady = true;
                }
                HardwareSerial_ResetLineState(pxState);
                bContinue = false;
            }
            else if ('\r' == cCharacter)
            {
                /* Ignore carriage return. */
            }
            else if (false == pxState->bOverflow)
            {
                if ((pxState->u32Length <
                     (HARDWARE_SERIAL_LINE_BUFFER_SIZE - 1U)) &&
                    (pxState->u32Length <
                     (u32MaxLength - 1U)))
                {
                    pxState->acBuffer[pxState->u32Length] =
                        cCharacter;
                    pxState->u32Length++;
                }
                else
                {
                    pxState->bOverflow = true;
                }
            }
            else
            {
                /* Discard bytes until newline. */
            }
        }
    }
    return bLineReady;
}

/* ============================================================
 * Serial1 API
 * ============================================================ */

void Serial1_begin(uint32_t u32BaudRate)
{
    HardwareSerial_ResetLineState(&sxSerial1Line);

    HardwareSerial_BeginInternal(
        IP_LPUART1,
        u32BaudRate);
}

bool Serial1_available(void)
{
    return LPUART_IsDataAvailable(IP_LPUART1);
}

char Serial1_read(void)
{
    return LPUART_GetChar(IP_LPUART1);
}

uint32_t Serial1_readString(
    char *pcBuffer,
    uint32_t u32MaxLength)
{
    return HardwareSerial_ReadStringInternal(
        IP_LPUART1,
        pcBuffer,
        u32MaxLength);
}

void Serial1_write(char cCharacter)
{
    HardwareSerial_PrintChar(
        IP_LPUART1,
        cCharacter);
}

void Serial1_print(const char *pcString)
{
    HardwareSerial_PrintString(
        IP_LPUART1,
        pcString);
}

void Serial1_println(const char *pcString)
{
    HardwareSerial_PrintlnString(
        IP_LPUART1,
        pcString);
}

void Serial1_printInt(int32_t s32Value)
{
    HardwareSerial_PrintIntInternal(
        IP_LPUART1,
        s32Value);
}

void Serial1_printlnInt(int32_t s32Value)
{
    HardwareSerial_PrintIntInternal(
        IP_LPUART1,
        s32Value);

    HardwareSerial_PrintString(
        IP_LPUART1,
        HARDWARE_SERIAL_NEWLINE);
}

void Serial1_printFloat(float f32Value)
{
    HardwareSerial_PrintFloatInternal(
        IP_LPUART1,
        f32Value);
}

void Serial1_printlnFloat(float f32Value)
{
    HardwareSerial_PrintFloatInternal(
        IP_LPUART1,
        f32Value);

    HardwareSerial_PrintString(
        IP_LPUART1,
        HARDWARE_SERIAL_NEWLINE);
}

/**
 * @brief Read a complete line from Serial1.
 */
bool Serial1_readLine(char *pcBuffer, uint32_t u32MaxLength)
{
    return HardwareSerial_ReadLineInternal(
        IP_LPUART1,
        &sxSerial1Line,
        pcBuffer,
        u32MaxLength);
}

/* ============================================================
 * Serial2 API
 * ============================================================ */

void Serial2_begin(uint32_t u32BaudRate)
{
    HardwareSerial_ResetLineState(&sxSerial2Line);

    HardwareSerial_BeginInternal(
        IP_LPUART2,
        u32BaudRate);
}

bool Serial2_available(void)
{
    return LPUART_IsDataAvailable(IP_LPUART2);
}

char Serial2_read(void)
{
    return LPUART_GetChar(IP_LPUART2);
}

uint32_t Serial2_readString(
    char *pcBuffer,
    uint32_t u32MaxLength)
{
    return HardwareSerial_ReadStringInternal(
        IP_LPUART2,
        pcBuffer,
        u32MaxLength);
}

void Serial2_write(char cCharacter)
{
    HardwareSerial_PrintChar(
        IP_LPUART2,
        cCharacter);
}

void Serial2_print(const char *pcString)
{
    HardwareSerial_PrintString(
        IP_LPUART2,
        pcString);
}

void Serial2_println(const char *pcString)
{
    HardwareSerial_PrintlnString(
        IP_LPUART2,
        pcString);
}

void Serial2_printInt(int32_t s32Value)
{
    HardwareSerial_PrintIntInternal(
        IP_LPUART2,
        s32Value);
}

void Serial2_printlnInt(int32_t s32Value)
{
    HardwareSerial_PrintIntInternal(
        IP_LPUART2,
        s32Value);

    HardwareSerial_PrintString(
        IP_LPUART2,
        HARDWARE_SERIAL_NEWLINE);
}

void Serial2_printFloat(float f32Value)
{
    HardwareSerial_PrintFloatInternal(
        IP_LPUART2,
        f32Value);
}

void Serial2_printlnFloat(float f32Value)
{
    HardwareSerial_PrintFloatInternal(
        IP_LPUART2,
        f32Value);

    HardwareSerial_PrintString(
        IP_LPUART2,
        HARDWARE_SERIAL_NEWLINE);
}

/**
 * @brief Read a complete line from Serial2.
 */
bool Serial2_readLine(char *pcBuffer, uint32_t u32MaxLength)
{
    return HardwareSerial_ReadLineInternal(
        IP_LPUART2,
        &sxSerial2Line,
        pcBuffer,
        u32MaxLength);
}
