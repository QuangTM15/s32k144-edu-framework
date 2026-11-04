#ifndef LCD_I2C_H
#define LCD_I2C_H
/**
 * @file lcd_i2c.h
 * @brief HD44780 character LCD with PCF8574 I2C backpack.
 * @note Use a safe 3.3 V I2C bus or a level shifter if backpack pull-ups use 5 V.
 */
#include <stdint.h>
#define LCD_I2C_TRUE (1U)
#define LCD_I2C_FALSE (0U)
#define LCD_I2C_INVALID_ADDRESS (0x00U)
void LCD_Init(uint8_t u8Address, uint8_t u8Columns, uint8_t u8Rows);
void LCD_Clear(void);
void LCD_SetCursor(uint8_t u8Column, uint8_t u8Row);
void LCD_Print(const char *pszString);
void LCD_PrintChar(char cCharacter);
void LCD_PrintInt(int32_t s32Value);
void LCD_PrintFloat(float f32Value);
void LCD_Backlight(void);
void LCD_NoBacklight(void);
#endif /* LCD_I2C_H */
