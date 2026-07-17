#ifndef LCD_I2C_H
#define LCD_I2C_H

/**
 * @file lcd_i2c.h
 * @brief I2C character LCD device library public interface.
 *
 * @details
 * This module provides an Arduino-style device library for HD44780-based
 * character LCDs connected through a PCF8574 I2C backpack.
 *
 * The library is implemented as a thin wrapper on top of the Arduino-style
 * Wire API provided by EduFramework.
 *
 * Typical supported modules:
 *
 * @code
 * 16x2 LCD + PCF8574
 * 20x4 LCD + PCF8574
 * @endcode
 *
 * Typical wiring:
 *
 * @code
 * LCD Module      MaaZEDU
 * VCC        ->   5V
 * GND        ->   GND
 * SDA        ->   I2C_SDA
 * SCL        ->   I2C_SCL
 * @endcode
 */

#include <stdint.h>

/* ========================================================================= */
/* Common Definitions                                                         */
/* ========================================================================= */

/**
 * @brief LCD initialized successfully.
 */
#define LCD_I2C_TRUE (1U)

/**
 * @brief LCD not initialized or invalid state.
 */
#define LCD_I2C_FALSE (0U)

/**
 * @brief Invalid LCD I2C address.
 */
#define LCD_I2C_INVALID_ADDRESS (0x00U)

/* ========================================================================= */
/* Public API                                                                 */
/* ========================================================================= */

/**
 * @brief Initialize an I2C LCD module.
 *
 * @details
 * This function initializes an HD44780-compatible LCD connected through
 * a PCF8574 I2C backpack.
 *
 * The initialization sequence configures:
 * - LCD controller
 * - Display size
 * - Entry mode
 * - Display state
 * - Backlight enabled
 *
 * Supported I2C addresses are typically:
 *
 * @code
 * 0x27
 * 0x3F
 * @endcode
 *
 * @param[in] u8Address
 * I2C slave address of the LCD module.
 *
 * @param[in] u8Columns
 * Number of display columns.
 *
 * @param[in] u8Rows
 * Number of display rows.
 *
 * @return None.
 */
void LCD_Init(uint8_t u8Address,
              uint8_t u8Columns,
              uint8_t u8Rows);

/**
 * @brief Clear the LCD display.
 *
 * @details
 * Clears all display characters and returns the cursor to the first
 * position.
 *
 * @return None.
 */
void LCD_Clear(void);

/**
 * @brief Set the LCD cursor position.
 *
 * @param[in] u8Column
 * Zero-based display column.
 *
 * @param[in] u8Row
 * Zero-based display row.
 *
 * @return None.
 */
void LCD_SetCursor(uint8_t u8Column,
                   uint8_t u8Row);

/**
 * @brief Print a null-terminated string.
 *
 * @param[in] pszString
 * Pointer to the string.
 *
 * @return None.
 */
void LCD_Print(const char *pszString);

/**
 * @brief Print one ASCII character.
 *
 * @param[in] cCharacter
 * Character to display.
 *
 * @return None.
 */
void LCD_PrintChar(char cCharacter);

/**
 * @brief Print a signed integer value.
 *
 * @param[in] s32Value
 * Integer value.
 *
 * @return None.
 */
void LCD_PrintInt(int32_t s32Value);

/**
 * @brief Print a floating-point value.
 *
 * @details
 * Floating-point values are displayed using the default precision
 * implemented by the library.
 *
 * @param[in] f32Value
 * Floating-point value.
 *
 * @return None.
 */
void LCD_PrintFloat(float f32Value);

/**
 * @brief Enable LCD backlight.
 *
 * @return None.
 */
void LCD_Backlight(void);

/**
 * @brief Disable LCD backlight.
 *
 * @return None.
 */
void LCD_NoBacklight(void);

#endif /* LCD_I2C_H */