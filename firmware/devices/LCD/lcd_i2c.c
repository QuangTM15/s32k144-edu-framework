/**
 * @file lcd_i2c.c
 * @brief I2C character LCD device library implementation.
 *
 * @details
 * This file implements an Arduino-style LCD library for HD44780-compatible
 * character LCDs using a PCF8574 I2C backpack.
 */

#include "lcd_i2c.h"

#include "Wire.h"
#include "time.h"

#include <stddef.h>

/* ========================================================================= */
/* HD44780 Commands                                                           */
/* ========================================================================= */

#define LCD_CMD_CLEAR_DISPLAY (0x01U)
#define LCD_CMD_RETURN_HOME (0x02U)
#define LCD_CMD_ENTRY_MODE_SET (0x04U)
#define LCD_CMD_DISPLAY_CONTROL (0x08U)
#define LCD_CMD_FUNCTION_SET (0x20U)
#define LCD_CMD_SET_DDRAM_ADDR (0x80U)

#define LCD_ENTRY_LEFT (0x02U)
#define LCD_DISPLAY_ON (0x04U)
#define LCD_CURSOR_OFF (0x00U)
#define LCD_BLINK_OFF (0x00U)

#define LCD_FUNCTION_4BIT (0x00U)
#define LCD_FUNCTION_2LINE (0x08U)
#define LCD_FUNCTION_5X8DOTS (0x00U)

/* ========================================================================= */
/* PCF8574 Backpack Bit Mapping                                               */
/* ========================================================================= */

#define LCD_BACKPACK_RS (0x01U)
#define LCD_BACKPACK_RW (0x02U)
#define LCD_BACKPACK_ENABLE (0x04U)
#define LCD_BACKPACK_BACKLIGHT (0x08U)

#define LCD_SEND_COMMAND (0U)
#define LCD_SEND_DATA (1U)

/* ========================================================================= */
/* Local Constants                                                            */
/* ========================================================================= */

#define LCD_INIT_DELAY_MS (50UL)
#define LCD_COMMAND_DELAY_MS (2UL)
#define LCD_ENABLE_DELAY_US (1UL)

#define LCD_DECIMAL_BASE (10)
#define LCD_FLOAT_PRECISION (2U)
#define LCD_FLOAT_SCALE (100.0F)

#define LCD_MAX_INT_BUFFER_SIZE (12U)
#define LCD_MAX_FLOAT_BUFFER_SIZE (20U)

/* ========================================================================= */
/* Internal State                                                             */
/* ========================================================================= */

static uint8_t s_u8LcdAddress = LCD_I2C_INVALID_ADDRESS;
static uint8_t s_u8LcdColumns = 0U;
static uint8_t s_u8LcdRows = 0U;
static uint8_t s_u8BacklightState = LCD_BACKPACK_BACKLIGHT;
static uint8_t s_u8LcdInitialized = LCD_I2C_FALSE;

/* ========================================================================= */
/* Internal Helpers                                                           */
/* ========================================================================= */

static uint8_t LCD_IsInitialized(void)
{
    uint8_t u8Result = LCD_I2C_FALSE;

    if ((LCD_I2C_TRUE == s_u8LcdInitialized) &&
        (LCD_I2C_INVALID_ADDRESS != s_u8LcdAddress))
    {
        u8Result = LCD_I2C_TRUE;
    }

    return u8Result;
}

static void LCD_WriteExpander(uint8_t u8Data)
{
    Wire_beginTransmission(s_u8LcdAddress);
    (void)Wire_write((uint8_t)(u8Data | s_u8BacklightState));
    (void)Wire_endTransmission();
}

static void LCD_PulseEnable(uint8_t u8Data)
{
    LCD_WriteExpander((uint8_t)(u8Data | LCD_BACKPACK_ENABLE));
    delayMicroseconds(LCD_ENABLE_DELAY_US);

    LCD_WriteExpander((uint8_t)(u8Data & (uint8_t)(~LCD_BACKPACK_ENABLE)));
    delayMicroseconds(LCD_ENABLE_DELAY_US);
}

static void LCD_SendNibble(uint8_t u8Nibble, uint8_t u8Mode)
{
    uint8_t u8Data = 0U;

    u8Data = (uint8_t)(u8Nibble & 0xF0U);

    if (LCD_SEND_DATA == u8Mode)
    {
        u8Data = (uint8_t)(u8Data | LCD_BACKPACK_RS);
    }

    u8Data = (uint8_t)(u8Data & (uint8_t)(~LCD_BACKPACK_RW));

    LCD_WriteExpander(u8Data);
    LCD_PulseEnable(u8Data);
}

static void LCD_SendByte(uint8_t u8Value, uint8_t u8Mode)
{
    LCD_SendNibble((uint8_t)(u8Value & 0xF0U), u8Mode);
    LCD_SendNibble((uint8_t)((u8Value << 4U) & 0xF0U), u8Mode);
}

static void LCD_WriteCommand(uint8_t u8Command)
{
    if (LCD_I2C_TRUE == LCD_IsInitialized())
    {
        LCD_SendByte(u8Command, LCD_SEND_COMMAND);
        delay(LCD_COMMAND_DELAY_MS);
    }
}

static void LCD_WriteData(uint8_t u8Data)
{
    if (LCD_I2C_TRUE == LCD_IsInitialized())
    {
        LCD_SendByte(u8Data, LCD_SEND_DATA);
    }
}

static void LCD_PrintUnsigned(uint32_t u32Value)
{
    char acBuffer[LCD_MAX_INT_BUFFER_SIZE] = {0};
    uint8_t u8Index = 0U;
    uint8_t u8WriteIndex = 0U;

    if (0UL == u32Value)
    {
        LCD_PrintChar('0');
    }
    else
    {
        while ((0UL < u32Value) && (u8Index < LCD_MAX_INT_BUFFER_SIZE))
        {
            acBuffer[u8Index] = (char)('0' + (u32Value % (uint32_t)LCD_DECIMAL_BASE));
            u32Value = u32Value / (uint32_t)LCD_DECIMAL_BASE;
            u8Index++;
        }

        while (0U < u8Index)
        {
            u8Index--;
            LCD_PrintChar(acBuffer[u8Index]);
            u8WriteIndex++;
        }

        (void)u8WriteIndex;
    }
}

/* ========================================================================= */
/* Public API                                                                 */
/* ========================================================================= */

void LCD_Init(uint8_t u8Address,
              uint8_t u8Columns,
              uint8_t u8Rows)
{
    s_u8LcdInitialized = LCD_I2C_FALSE;

    if ((LCD_I2C_INVALID_ADDRESS != u8Address) &&
        (0U != u8Columns) &&
        (0U != u8Rows))
    {
        s_u8LcdAddress = u8Address;
        s_u8LcdColumns = u8Columns;
        s_u8LcdRows = u8Rows;
        s_u8BacklightState = LCD_BACKPACK_BACKLIGHT;

        Wire_begin();
        delay(LCD_INIT_DELAY_MS);

        /*
         * HD44780 4-bit initialization sequence.
         * These are sent as high nibbles only before normal 4-bit mode.
         */
        LCD_SendNibble(0x30U, LCD_SEND_COMMAND);
        delay(5UL);
        LCD_SendNibble(0x30U, LCD_SEND_COMMAND);
        delay(5UL);
        LCD_SendNibble(0x30U, LCD_SEND_COMMAND);
        delay(1UL);
        LCD_SendNibble(0x20U, LCD_SEND_COMMAND);
        delay(1UL);

        s_u8LcdInitialized = LCD_I2C_TRUE;

        LCD_WriteCommand((uint8_t)(LCD_CMD_FUNCTION_SET |
                                   LCD_FUNCTION_4BIT |
                                   LCD_FUNCTION_2LINE |
                                   LCD_FUNCTION_5X8DOTS));

        LCD_WriteCommand((uint8_t)(LCD_CMD_DISPLAY_CONTROL |
                                   LCD_DISPLAY_ON |
                                   LCD_CURSOR_OFF |
                                   LCD_BLINK_OFF));

        LCD_Clear();

        LCD_WriteCommand((uint8_t)(LCD_CMD_ENTRY_MODE_SET |
                                   LCD_ENTRY_LEFT));
    }
}

void LCD_Clear(void)
{
    LCD_WriteCommand(LCD_CMD_CLEAR_DISPLAY);
    delay(LCD_COMMAND_DELAY_MS);
}

void LCD_SetCursor(uint8_t u8Column,
                   uint8_t u8Row)
{
    static const uint8_t au8RowOffsets[4U] = {0x00U, 0x40U, 0x14U, 0x54U};
    uint8_t u8Address = 0U;

    if (LCD_I2C_TRUE == LCD_IsInitialized())
    {
        if (s_u8LcdRows <= u8Row)
        {
            u8Row = (uint8_t)(s_u8LcdRows - 1U);
        }

        if (s_u8LcdColumns <= u8Column)
        {
            u8Column = (uint8_t)(s_u8LcdColumns - 1U);
        }

        if (4U > u8Row)
        {
            u8Address = (uint8_t)(au8RowOffsets[u8Row] + u8Column);
            LCD_WriteCommand((uint8_t)(LCD_CMD_SET_DDRAM_ADDR | u8Address));
        }
    }
}

void LCD_Print(const char *pszString)
{
    const char *pszCurrent = pszString;

    if ((LCD_I2C_TRUE == LCD_IsInitialized()) &&
        (NULL != pszString))
    {
        while ('\0' != *pszCurrent)
        {
            LCD_PrintChar(*pszCurrent);
            pszCurrent++;
        }
    }
}

void LCD_PrintChar(char cCharacter)
{
    LCD_WriteData((uint8_t)cCharacter);
}

void LCD_PrintInt(int32_t s32Value)
{
    uint32_t u32Value = 0UL;

    if (0 > s32Value)
    {
        LCD_PrintChar('-');
        u32Value = (uint32_t)(-s32Value);
    }
    else
    {
        u32Value = (uint32_t)s32Value;
    }

    LCD_PrintUnsigned(u32Value);
}

void LCD_PrintFloat(float f32Value)
{
    int32_t s32IntegerPart = 0;
    int32_t s32FractionPart = 0;
    float f32ScaledFraction = 0.0F;

    if (0.0F > f32Value)
    {
        LCD_PrintChar('-');
        f32Value = -f32Value;
    }

    s32IntegerPart = (int32_t)f32Value;
    f32ScaledFraction = (f32Value - (float)s32IntegerPart) * LCD_FLOAT_SCALE;
    s32FractionPart = (int32_t)(f32ScaledFraction + 0.5F);

    LCD_PrintInt(s32IntegerPart);
    LCD_PrintChar('.');

    if (10 > s32FractionPart)
    {
        LCD_PrintChar('0');
    }

    LCD_PrintInt(s32FractionPart);

    (void)LCD_FLOAT_PRECISION;
}

void LCD_Backlight(void)
{
    if (LCD_I2C_TRUE == LCD_IsInitialized())
    {
        s_u8BacklightState = LCD_BACKPACK_BACKLIGHT;
        LCD_WriteExpander(0U);
    }
}

void LCD_NoBacklight(void)
{
    if (LCD_I2C_TRUE == LCD_IsInitialized())
    {
        s_u8BacklightState = 0U;
        LCD_WriteExpander(0U);
    }
}