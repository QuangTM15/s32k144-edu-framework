/**
 * @file hardware_serial.c
 * @brief Arduino-style hardware serial API implementation.
 *
 * @details
 * This module implements user-facing Serial1 and Serial2 APIs.
 *
 * Current serial mapping:
 * - Serial1 uses LPUART1 and is intended for serial monitor/debug output
 *   through the board debug interface.
 * - Serial2 uses LPUART2 and is intended for external UART pins/header.
 *
 * This module belongs to the Arduino-style API layer and wraps the
 * low-level LPUART driver.
 */

#include "hardware_serial.h"

#include "lpuart.h"
#include "irq.h"

/**
 * @brief Null pointer macro used locally in this module.
 */
#define HARDWARE_SERIAL_NULL_PTR           ((void *)0)

/**
 * @brief LPUART source clock used by the current board configuration.
 *
 * @details
 * The LPUART driver selects SOSC_DIV2_CLK as the functional clock source.
 * In the current clock setup, this clock is 8 MHz.
 */
#define HARDWARE_SERIAL_SRC_CLOCK_HZ       (8000000U)

/**
 * @brief Number of decimal digits printed for floating-point values.
 */
#define HARDWARE_SERIAL_FLOAT_DECIMALS     (3U)

/**
 * @brief Scaling factor used to keep three decimal digits.
 */
#define HARDWARE_SERIAL_FLOAT_SCALE        (1000U)

/**
 * @brief Local newline sequence used by println APIs.
 */
#define HARDWARE_SERIAL_NEWLINE            "\r\n"

/**
 * @brief Maximum number of characters used for integer conversion.
 */
#define HARDWARE_SERIAL_INT_BUFFER_SIZE    (16U)

/**
 * @brief Print one character through an LPUART instance.
 *
 * @param[in] pBase
 * Pointer to LPUART peripheral instance.
 *
 * @param[in] cCharacter
 * Character to transmit.
 *
 * @return None.
 */
static void HardwareSerial_PrintChar(LPUART_Type *pBase, char cCharacter)
{
    if (HARDWARE_SERIAL_NULL_PTR != pBase)
    {
        LPUART_WriteChar(pBase, cCharacter);
    }
}

/**
 * @brief Print a null-terminated string through an LPUART instance.
 *
 * @param[in] pBase
 * Pointer to LPUART peripheral instance.
 *
 * @param[in] pcString
 * Pointer to null-terminated string.
 *
 * @return None.
 */
static void HardwareSerial_PrintString(LPUART_Type *pBase, const char *pcString)
{
    if ((HARDWARE_SERIAL_NULL_PTR != pBase) && (HARDWARE_SERIAL_NULL_PTR != pcString))
    {
        LPUART_WriteString(pBase, pcString);
    }
}

/**
 * @brief Print a string followed by CRLF.
 *
 * @param[in] pBase
 * Pointer to LPUART peripheral instance.
 *
 * @param[in] pcString
 * Pointer to null-terminated string.
 *
 * @return None.
 */
static void HardwareSerial_PrintlnString(LPUART_Type *pBase, const char *pcString)
{
    HardwareSerial_PrintString(pBase, pcString);
    HardwareSerial_PrintString(pBase, HARDWARE_SERIAL_NEWLINE);
}

/**
 * @brief Reverse characters inside a buffer.
 *
 * @param[in,out] pcBuffer
 * Buffer to reverse.
 *
 * @param[in] u32Length
 * Number of valid characters in the buffer.
 *
 * @return None.
 */
static void HardwareSerial_ReverseBuffer(char *pcBuffer, uint32_t u32Length)
{
    uint32_t u32LeftIndex = 0U;
    uint32_t u32RightIndex = 0U;
    char cTemp = '\0';

    if ((HARDWARE_SERIAL_NULL_PTR != pcBuffer) && (0U != u32Length))
    {
        u32LeftIndex = 0U;
        u32RightIndex = u32Length - 1U;

        while (u32LeftIndex < u32RightIndex)
        {
            cTemp = pcBuffer[u32LeftIndex];
            pcBuffer[u32LeftIndex] = pcBuffer[u32RightIndex];
            pcBuffer[u32RightIndex] = cTemp;

            u32LeftIndex++;
            u32RightIndex--;
        }
    }
}

/**
 * @brief Convert unsigned integer to decimal string.
 *
 * @param[in] u32Value
 * Unsigned integer value.
 *
 * @param[out] pcBuffer
 * Destination string buffer.
 *
 * @return None.
 */
static void HardwareSerial_UintToString(uint32_t u32Value, char *pcBuffer)
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
                pcBuffer[u32Index] = (char)('0' + (u32WorkingValue % 10U));
                u32WorkingValue = u32WorkingValue / 10U;
                u32Index++;
            }

            pcBuffer[u32Index] = '\0';
            HardwareSerial_ReverseBuffer(pcBuffer, u32Index);
        }
    }
}

/**
 * @brief Print signed integer value.
 *
 * @param[in] pBase
 * Pointer to LPUART peripheral instance.
 *
 * @param[in] s32Value
 * Signed integer value.
 *
 * @return None.
 */
static void HardwareSerial_PrintIntInternal(LPUART_Type *pBase, int32_t s32Value)
{
    char acBuffer[HARDWARE_SERIAL_INT_BUFFER_SIZE] = {0};
    uint32_t u32AbsValue = 0U;

    if (HARDWARE_SERIAL_NULL_PTR != pBase)
    {
        if (0 > s32Value)
        {
            HardwareSerial_PrintChar(pBase, '-');
            u32AbsValue = (uint32_t)(-s32Value);
        }
        else
        {
            u32AbsValue = (uint32_t)s32Value;
        }

        HardwareSerial_UintToString(u32AbsValue, acBuffer);
        HardwareSerial_PrintString(pBase, acBuffer);
    }
}

/**
 * @brief Print floating-point value.
 *
 * @details
 * This function prints the integer part, decimal point, and three
 * fractional digits. It uses integer scaling instead of printf() to avoid
 * pulling in heavy standard library formatting code.
 *
 * @param[in] pBase
 * Pointer to LPUART peripheral instance.
 *
 * @param[in] f32Number
 * Floating-point value.
 *
 * @return None.
 */
static void HardwareSerial_PrintFloatInternal(LPUART_Type *pBase, float f32Number)
{
    char acIntBuffer[HARDWARE_SERIAL_INT_BUFFER_SIZE] = {0};
    uint32_t u32IntPart = 0U;
    uint32_t u32FracPart = 0U;
    uint32_t u32Divisor = 0U;
    float f32AbsNumber = 0.0f;

    if (HARDWARE_SERIAL_NULL_PTR != pBase)
    {
        if (0.0f > f32Number)
        {
            HardwareSerial_PrintChar(pBase, '-');
            f32AbsNumber = -f32Number;
        }
        else
        {
            f32AbsNumber = f32Number;
        }

        u32IntPart = (uint32_t)f32AbsNumber;
        u32FracPart = (uint32_t)(((f32AbsNumber - (float)u32IntPart) *
                                  (float)HARDWARE_SERIAL_FLOAT_SCALE) + 0.5f);

        if (HARDWARE_SERIAL_FLOAT_SCALE <= u32FracPart)
        {
            u32IntPart++;
            u32FracPart = 0U;
        }

        HardwareSerial_UintToString(u32IntPart, acIntBuffer);
        HardwareSerial_PrintString(pBase, acIntBuffer);
        HardwareSerial_PrintChar(pBase, '.');

        u32Divisor = HARDWARE_SERIAL_FLOAT_SCALE / 10U;

        while (0U < u32Divisor)
        {
            HardwareSerial_PrintChar(
                pBase,
                (char)('0' + ((u32FracPart / u32Divisor) % 10U)));

            u32Divisor = u32Divisor / 10U;
        }
    }
}

/**
 * @brief Read available bytes into a string buffer.
 *
 * @details
 * This function reads bytes from the software RX buffer until:
 * - No more bytes are available.
 * - Newline character is found.
 * - Destination buffer is full.
 *
 * Carriage return characters are ignored.
 *
 * @param[in] pBase
 * Pointer to LPUART peripheral instance.
 *
 * @param[out] pcBuffer
 * Destination buffer.
 *
 * @param[in] u32MaxLength
 * Maximum buffer length including null terminator.
 *
 * @return uint32_t
 * Number of characters copied into the buffer.
 */
static uint32_t HardwareSerial_ReadStringInternal(LPUART_Type *pBase,
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
        while ((true == bShouldContinue) && (true == LPUART_IsDataAvailable(pBase)))
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

/* ============================================================
 * Serial1 API -> LPUART1
 * ============================================================ */

/**
 * @brief Initialize Serial1.
 */
void Serial1_begin(uint32_t u32BaudRate)
{
    LPUART_Config_t xConfig = {0};

    xConfig.u32BaudRate = u32BaudRate;
    xConfig.u32SrcClockHz = HARDWARE_SERIAL_SRC_CLOCK_HZ;

    if (LPUART_STATUS_OK == LPUART_Init(IP_LPUART1, &xConfig))
    {
        IRQ_LPUART1_RxTx_Init();
        LPUART_EnableRxInterrupt(IP_LPUART1);
    }
}

/**
 * @brief Check whether Serial1 has received data.
 */
bool Serial1_available(void)
{
    return LPUART_IsDataAvailable(IP_LPUART1);
}

/**
 * @brief Read one character from Serial1.
 */
char Serial1_read(void)
{
    return LPUART_GetChar(IP_LPUART1);
}

/**
 * @brief Read available Serial1 data into a string buffer.
 */
uint32_t Serial1_readString(char *pcBuffer, uint32_t u32MaxLength)
{
    return HardwareSerial_ReadStringInternal(IP_LPUART1, pcBuffer, u32MaxLength);
}

/**
 * @brief Write one character to Serial1.
 */
void Serial1_write(char cCharacter)
{
    LPUART_WriteChar(IP_LPUART1, cCharacter);
}

/**
 * @brief Print a string to Serial1.
 */
void Serial1_print(const char *pcString)
{
    HardwareSerial_PrintString(IP_LPUART1, pcString);
}

/**
 * @brief Print a string to Serial1 followed by CRLF.
 */
void Serial1_println(const char *pcString)
{
    HardwareSerial_PrintlnString(IP_LPUART1, pcString);
}

/**
 * @brief Print a signed 32-bit integer to Serial1.
 */
void Serial1_printInt(int32_t s32Value)
{
    HardwareSerial_PrintIntInternal(IP_LPUART1, s32Value);
}

/**
 * @brief Print a signed 32-bit integer to Serial1 followed by CRLF.
 */
void Serial1_printlnInt(int32_t s32Value)
{
    HardwareSerial_PrintIntInternal(IP_LPUART1, s32Value);
    HardwareSerial_PrintString(IP_LPUART1, HARDWARE_SERIAL_NEWLINE);
}

/**
 * @brief Print a floating-point value to Serial1.
 */
void Serial1_printFloat(float f32Value)
{
    HardwareSerial_PrintFloatInternal(IP_LPUART1, f32Value);
}

/**
 * @brief Print a floating-point value to Serial1 followed by CRLF.
 */
void Serial1_printlnFloat(float f32Value)
{
    HardwareSerial_PrintFloatInternal(IP_LPUART1, f32Value);
    HardwareSerial_PrintString(IP_LPUART1, HARDWARE_SERIAL_NEWLINE);
}

/* ============================================================
 * Serial2 API -> LPUART2
 * ============================================================ */

/**
 * @brief Initialize Serial2.
 */
void Serial2_begin(uint32_t u32BaudRate)
{
    LPUART_Config_t xConfig = {0};

    xConfig.u32BaudRate = u32BaudRate;
    xConfig.u32SrcClockHz = HARDWARE_SERIAL_SRC_CLOCK_HZ;

    if (LPUART_STATUS_OK == LPUART_Init(IP_LPUART2, &xConfig))
    {
        IRQ_LPUART2_RxTx_Init();
        LPUART_EnableRxInterrupt(IP_LPUART2);
    }
}

/**
 * @brief Check whether Serial2 has received data.
 */
bool Serial2_available(void)
{
    return LPUART_IsDataAvailable(IP_LPUART2);
}

/**
 * @brief Read one character from Serial2.
 */
char Serial2_read(void)
{
    return LPUART_GetChar(IP_LPUART2);
}

/**
 * @brief Read available Serial2 data into a string buffer.
 */
uint32_t Serial2_readString(char *pcBuffer, uint32_t u32MaxLength)
{
    return HardwareSerial_ReadStringInternal(IP_LPUART2, pcBuffer, u32MaxLength);
}

/**
 * @brief Write one character to Serial2.
 */
void Serial2_write(char cCharacter)
{
    LPUART_WriteChar(IP_LPUART2, cCharacter);
}

/**
 * @brief Print a string to Serial2.
 */
void Serial2_print(const char *pcString)
{
    HardwareSerial_PrintString(IP_LPUART2, pcString);
}

/**
 * @brief Print a string to Serial2 followed by CRLF.
 */
void Serial2_println(const char *pcString)
{
    HardwareSerial_PrintlnString(IP_LPUART2, pcString);
}

/**
 * @brief Print a signed 32-bit integer to Serial2.
 */
void Serial2_printInt(int32_t s32Value)
{
    HardwareSerial_PrintIntInternal(IP_LPUART2, s32Value);
}

/**
 * @brief Print a signed 32-bit integer to Serial2 followed by CRLF.
 */
void Serial2_printlnInt(int32_t s32Value)
{
    HardwareSerial_PrintIntInternal(IP_LPUART2, s32Value);
    HardwareSerial_PrintString(IP_LPUART2, HARDWARE_SERIAL_NEWLINE);
}

/**
 * @brief Print a floating-point value to Serial2.
 */
void Serial2_printFloat(float f32Value)
{
    HardwareSerial_PrintFloatInternal(IP_LPUART2, f32Value);
}

/**
 * @brief Print a floating-point value to Serial2 followed by CRLF.
 */
void Serial2_printlnFloat(float f32Value)
{
    HardwareSerial_PrintFloatInternal(IP_LPUART2, f32Value);
    HardwareSerial_PrintString(IP_LPUART2, HARDWARE_SERIAL_NEWLINE);
}
