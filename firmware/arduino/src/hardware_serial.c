#include "hardware_serial.h"

#include "lpuart.h"
#include "irq.h"

#define HARDWARE_SERIAL_SRC_CLOCK_HZ      (8000000U)
#define HARDWARE_SERIAL_FLOAT_SCALE       (1000U)
#define HARDWARE_SERIAL_FLOAT_DECIMALS    (3U)

static void HardwareSerial_PrintChar(LPUART_Type *base, char ch)
{
    LPUART_WriteChar(base, ch);
}

static void HardwareSerial_PrintString(LPUART_Type *base, const char *str)
{
    if (str == (const char *)0)
    {
        return;
    }

    LPUART_WriteString(base, str);
}

static void HardwareSerial_PrintlnString(LPUART_Type *base, const char *str)
{
    HardwareSerial_PrintString(base, str);
    LPUART_WriteString(base, "\r\n");
}

static void HardwareSerial_ReverseBuffer(char *buffer, uint32_t length)
{
    uint32_t i;
    uint32_t j;
    char temp;

    if (buffer == (char *)0)
    {
        return;
    }

    if (length == 0U)
    {
        return;
    }

    i = 0U;
    j = length - 1U;

    while (i < j)
    {
        temp = buffer[i];
        buffer[i] = buffer[j];
        buffer[j] = temp;
        i++;
        j--;
    }
}

static void HardwareSerial_UintToString(uint32_t value, char *buffer)
{
    uint32_t index = 0U;

    if (buffer == (char *)0)
    {
        return;
    }

    if (value == 0U)
    {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    while (value > 0U)
    {
        buffer[index] = (char)('0' + (value % 10U));
        value /= 10U;
        index++;
    }

    buffer[index] = '\0';
    HardwareSerial_ReverseBuffer(buffer, index);
}

static void HardwareSerial_PrintIntInternal(LPUART_Type *base, int value)
{
    char buffer[16];
    uint32_t absValue;

    if (value < 0)
    {
        HardwareSerial_PrintChar(base, '-');
        absValue = (uint32_t)(-value);
    }
    else
    {
        absValue = (uint32_t)value;
    }

    HardwareSerial_UintToString(absValue, buffer);
    HardwareSerial_PrintString(base, buffer);
}

static void HardwareSerial_PrintFloatInternal(LPUART_Type *base, float number)
{
    char intBuffer[16];
    char fracBuffer[16];
    uint32_t intPart;
    uint32_t fracPart;
    uint32_t scale = HARDWARE_SERIAL_FLOAT_SCALE;
    uint32_t divisor;
    float absNumber;

    if (number < 0.0f)
    {
        HardwareSerial_PrintChar(base, '-');
        absNumber = -number;
    }
    else
    {
        absNumber = number;
    }

    intPart = (uint32_t)absNumber;
    fracPart = (uint32_t)((absNumber - (float)intPart) * (float)scale + 0.5f);

    if (fracPart >= scale)
    {
        intPart += 1U;
        fracPart = 0U;
    }

    HardwareSerial_UintToString(intPart, intBuffer);
    HardwareSerial_PrintString(base, intBuffer);

    HardwareSerial_PrintChar(base, '.');

    divisor = scale / 10U;
    while (divisor > 0U)
    {
        HardwareSerial_PrintChar(base, (char)('0' + ((fracPart / divisor) % 10U)));
        divisor /= 10U;
    }

    (void)fracBuffer;
}

static uint32_t HardwareSerial_ReadStringInternal(LPUART_Type *base, char *buffer, uint32_t maxLength)
{
    uint32_t index = 0U;
    char ch;

    if ((buffer == (char *)0) || (maxLength == 0U))
    {
        return 0U;
    }

    while (LPUART_IsDataAvailable(base))
    {
        ch = LPUART_GetChar(base);

        if (ch == '\r')
        {
            continue;
        }

        if (ch == '\n')
        {
            break;
        }

        if (index < (maxLength - 1U))
        {
            buffer[index] = ch;
            index++;
        }
        else
        {
            break;
        }
    }

    buffer[index] = '\0';
    return index;
}

/* ============================================================
 * Serial1 API -> LPUART1
 * ============================================================ */

void Serial1_begin(uint32_t baudRate)
{
    LPUART_Config_t config;

    config.baudRate = baudRate;
    config.srcClockHz = HARDWARE_SERIAL_SRC_CLOCK_HZ;

    if (LPUART_Init(IP_LPUART1, &config) == LPUART_STATUS_OK)
    {
        IRQ_LPUART1_RxTx_Init();
        LPUART_EnableRxInterrupt(IP_LPUART1);
    }
}

bool Serial1_available(void)
{
    return LPUART_IsDataAvailable(IP_LPUART1);
}

char Serial1_read(void)
{
    return LPUART_GetChar(IP_LPUART1);
}

uint32_t Serial1_readString(char *buffer, uint32_t maxLength)
{
    return HardwareSerial_ReadStringInternal(IP_LPUART1, buffer, maxLength);
}

void Serial1_write(char ch)
{
    LPUART_WriteChar(IP_LPUART1, ch);
}

void Serial1_print(const char *str)
{
    HardwareSerial_PrintString(IP_LPUART1, str);
}

void Serial1_println(const char *str)
{
    HardwareSerial_PrintlnString(IP_LPUART1, str);
}

void Serial1_printInt(int value)
{
    HardwareSerial_PrintIntInternal(IP_LPUART1, value);
}

void Serial1_printlnInt(int value)
{
    HardwareSerial_PrintIntInternal(IP_LPUART1, value);
    LPUART_WriteString(IP_LPUART1, "\r\n");
}

void Serial1_printFloat(float value)
{
    HardwareSerial_PrintFloatInternal(IP_LPUART1, value);
}

void Serial1_printlnFloat(float value)
{
    HardwareSerial_PrintFloatInternal(IP_LPUART1, value);
    LPUART_WriteString(IP_LPUART1, "\r\n");
}

/* ============================================================
 * Serial2 API -> LPUART2
 * ============================================================ */

void Serial2_begin(uint32_t baudRate)
{
    LPUART_Config_t config;

    config.baudRate = baudRate;
    config.srcClockHz = HARDWARE_SERIAL_SRC_CLOCK_HZ;

    if (LPUART_Init(IP_LPUART2, &config) == LPUART_STATUS_OK)
    {
        IRQ_LPUART2_RxTx_Init();
        LPUART_EnableRxInterrupt(IP_LPUART2);
    }
}

bool Serial2_available(void)
{
    return LPUART_IsDataAvailable(IP_LPUART2);
}

char Serial2_read(void)
{
    return LPUART_GetChar(IP_LPUART2);
}

uint32_t Serial2_readString(char *buffer, uint32_t maxLength)
{
    return HardwareSerial_ReadStringInternal(IP_LPUART2, buffer, maxLength);
}

void Serial2_write(char ch)
{
    LPUART_WriteChar(IP_LPUART2, ch);
}

void Serial2_print(const char *str)
{
    HardwareSerial_PrintString(IP_LPUART2, str);
}

void Serial2_println(const char *str)
{
    HardwareSerial_PrintlnString(IP_LPUART2, str);
}

void Serial2_printInt(int value)
{
    HardwareSerial_PrintIntInternal(IP_LPUART2, value);
}

void Serial2_printlnInt(int value)
{
    HardwareSerial_PrintIntInternal(IP_LPUART2, value);
    LPUART_WriteString(IP_LPUART2, "\r\n");
}

void Serial2_printFloat(float value)
{
    HardwareSerial_PrintFloatInternal(IP_LPUART2, value);
}

void Serial2_printlnFloat(float value)
{
    HardwareSerial_PrintFloatInternal(IP_LPUART2, value);
    LPUART_WriteString(IP_LPUART2, "\r\n");
}
