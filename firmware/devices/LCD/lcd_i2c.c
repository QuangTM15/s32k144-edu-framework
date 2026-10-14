/**
 * @file lcd_i2c.c
 * @brief HD44780 LCD via PCF8574 and EduFramework Wire.
 */
#include "lcd_i2c.h"
#include "Wire.h"
#include "time.h"
#include <stddef.h>
#define LCD_CMD_CLEAR_DISPLAY (0x01U)
#define LCD_CMD_RETURN_HOME (0x02U)
#define LCD_CMD_ENTRY_MODE_SET (0x04U)
#define LCD_CMD_DISPLAY_CONTROL (0x08U)
#define LCD_CMD_FUNCTION_SET (0x20U)
#define LCD_CMD_SET_DDRAM_ADDR (0x80U)
#define LCD_ENTRY_LEFT (0x02U)
#define LCD_DISPLAY_ON (0x04U)
#define LCD_FUNCTION_2LINE (0x08U)
#define LCD_BACKPACK_RS (0x01U)
#define LCD_BACKPACK_RW (0x02U)
#define LCD_BACKPACK_ENABLE (0x04U)
#define LCD_BACKPACK_BACKLIGHT (0x08U)
#define LCD_SEND_COMMAND (0U)
#define LCD_SEND_DATA (1U)
#define LCD_INIT_DELAY_MS (50UL)
#define LCD_COMMAND_DELAY_MS (2UL)
#define LCD_ENABLE_DELAY_US (1UL)
#define LCD_MAX_INT_BUFFER_SIZE (12U)
static uint8_t s_u8LcdAddress = LCD_I2C_INVALID_ADDRESS;
static uint8_t s_u8LcdColumns = 0U;
static uint8_t s_u8LcdRows = 0U;
static uint8_t s_u8BacklightState = LCD_BACKPACK_BACKLIGHT;
static uint8_t s_u8LcdInitialized = LCD_I2C_FALSE;
static uint8_t s_u8LcdTransferOk = LCD_I2C_FALSE;
static uint8_t LCD_IsInitialized(void)
{
    uint8_t result = LCD_I2C_FALSE;
    if ((LCD_I2C_TRUE == s_u8LcdInitialized) && (LCD_I2C_INVALID_ADDRESS != s_u8LcdAddress))
    {
        result = LCD_I2C_TRUE;
    }
    return result;
}
static void LCD_WriteExpander(uint8_t data)
{
    uint8_t value = (uint8_t)(data | s_u8BacklightState);
    Wire_beginTransmission(s_u8LcdAddress);
    (void)Wire_write(value);
    if (WIRE_STATUS_OK != Wire_endTransmission())
    {
        s_u8LcdTransferOk = LCD_I2C_FALSE;
    }
}
static void LCD_PulseEnable(uint8_t data)
{
    LCD_WriteExpander((uint8_t)(data | LCD_BACKPACK_ENABLE));
    delayMicroseconds(LCD_ENABLE_DELAY_US);
    LCD_WriteExpander((uint8_t)(data & (uint8_t)(~LCD_BACKPACK_ENABLE)));
    delayMicroseconds(LCD_ENABLE_DELAY_US);
}
static void LCD_SendNibble(uint8_t nibble, uint8_t mode)
{
    uint8_t data = (uint8_t)(nibble & 0xF0U);
    if (LCD_SEND_DATA == mode)
    {
        data = (uint8_t)(data | LCD_BACKPACK_RS);
    }
    data = (uint8_t)(data & (uint8_t)(~LCD_BACKPACK_RW));
    LCD_WriteExpander(data);
    LCD_PulseEnable(data);
}
static void LCD_SendByte(uint8_t value, uint8_t mode)
{
    LCD_SendNibble((uint8_t)(value & 0xF0U), mode);
    LCD_SendNibble((uint8_t)((value << 4U) & 0xF0U), mode);
}
static void LCD_WriteCommand(uint8_t command)
{
    if (LCD_I2C_TRUE == LCD_IsInitialized())
    {
        LCD_SendByte(command, LCD_SEND_COMMAND);
        delay(LCD_COMMAND_DELAY_MS);
    }
}
static void LCD_WriteData(uint8_t data)
{
    if (LCD_I2C_TRUE == LCD_IsInitialized())
    {
        LCD_SendByte(data, LCD_SEND_DATA);
    }
}
static void LCD_PrintUnsigned(uint32_t value)
{
    char buffer[LCD_MAX_INT_BUFFER_SIZE];
    uint8_t index = 0U;
    do
    {
        buffer[index] = (char)('0' + (value % 10U));
        value /= 10U;
        index++;
    } while ((0U < value) && (index < LCD_MAX_INT_BUFFER_SIZE));
    while (0U < index)
    {
        index--;
        LCD_PrintChar(buffer[index]);
    }
}
void LCD_Init(uint8_t u8Address, uint8_t u8Columns, uint8_t u8Rows)
{
    s_u8LcdInitialized = LCD_I2C_FALSE;
    s_u8LcdTransferOk = LCD_I2C_FALSE;
    s_u8LcdAddress = LCD_I2C_INVALID_ADDRESS;
    if ((0x08U <= u8Address) && (0x77U >= u8Address) && (0U < u8Columns) && (0U < u8Rows) && (4U >= u8Rows))
    {
        s_u8LcdAddress = u8Address;
        s_u8LcdColumns = u8Columns;
        s_u8LcdRows = u8Rows;
        s_u8BacklightState = LCD_BACKPACK_BACKLIGHT;
        Wire_begin();
        delay(LCD_INIT_DELAY_MS);
        s_u8LcdTransferOk = LCD_I2C_TRUE;
        LCD_SendNibble(0x30U, LCD_SEND_COMMAND);
        delay(5UL);
        LCD_SendNibble(0x30U, LCD_SEND_COMMAND);
        delay(5UL);
        LCD_SendNibble(0x30U, LCD_SEND_COMMAND);
        delay(1UL);
        LCD_SendNibble(0x20U, LCD_SEND_COMMAND);
        delay(1UL);
        if (LCD_I2C_TRUE == s_u8LcdTransferOk)
        {
            s_u8LcdInitialized = LCD_I2C_TRUE;
            LCD_WriteCommand((uint8_t)(LCD_CMD_FUNCTION_SET | ((1U < u8Rows) ? LCD_FUNCTION_2LINE : 0U)));
            LCD_WriteCommand((uint8_t)(LCD_CMD_DISPLAY_CONTROL | LCD_DISPLAY_ON));
            LCD_Clear();
            LCD_WriteCommand((uint8_t)(LCD_CMD_ENTRY_MODE_SET | LCD_ENTRY_LEFT));
            if (LCD_I2C_FALSE == s_u8LcdTransferOk)
            {
                s_u8LcdInitialized = LCD_I2C_FALSE;
            }
        }
    }
}
void LCD_Clear(void)
{
    LCD_WriteCommand(LCD_CMD_CLEAR_DISPLAY);
}
void LCD_SetCursor(uint8_t u8Column, uint8_t u8Row)
{
    static const uint8_t rowOffsets[4U] = {0x00U, 0x40U, 0x14U, 0x54U};
    uint8_t address = 0U;
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
        address = (uint8_t)(rowOffsets[u8Row] + u8Column);
        LCD_WriteCommand((uint8_t)(LCD_CMD_SET_DDRAM_ADDR | address));
    }
}
void LCD_Print(const char *pszString)
{
    if ((LCD_I2C_TRUE == LCD_IsInitialized()) && (NULL != pszString))
    {
        while ('\0' != *pszString)
        {
            LCD_PrintChar(*pszString);
            pszString++;
        }
    }
}
void LCD_PrintChar(char cCharacter)
{
    LCD_WriteData((uint8_t)cCharacter);
}
void LCD_PrintInt(int32_t s32Value)
{
    uint32_t magnitude = 0U;
    if (0 > s32Value)
    {
        LCD_PrintChar('-');
        magnitude = (uint32_t)(-(int64_t)s32Value);
    }
    else
    {
        magnitude = (uint32_t)s32Value;
    }
    LCD_PrintUnsigned(magnitude);
}
void LCD_PrintFloat(float f32Value)
{
    uint32_t integerPart = 0U;
    uint32_t fractionPart = 0U;
    if ((f32Value != f32Value) || (f32Value > 4294967294.0F) || (f32Value < -4294967294.0F))
    {
        LCD_Print("ERR");
    }
    else
    {
        if (0.0F > f32Value)
        {
            LCD_PrintChar('-');
            f32Value = -f32Value;
        }
        integerPart = (uint32_t)f32Value;
        fractionPart = (uint32_t)(((f32Value - (float)integerPart) * 100.0F) + 0.5F);
        if (100U <= fractionPart)
        {
            integerPart++;
            fractionPart = 0U;
        }
        LCD_PrintUnsigned(integerPart);
        LCD_PrintChar('.');
        if (10U > fractionPart)
        {
            LCD_PrintChar('0');
        }
        LCD_PrintUnsigned(fractionPart);
    }
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
