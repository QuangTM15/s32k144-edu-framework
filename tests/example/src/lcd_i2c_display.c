/**
 * @file lcd_i2c_display.c
 * @brief I2C LCD display example.
 *
 * @details
 * This example demonstrates the simplest way to use the EduFramework
 * I2C LCD device library.
 *
 * Hardware connection:
 *
 * - LCD VCC -> 5V
 * - LCD GND -> GND
 * - LCD SDA -> I2C_SDA
 * - LCD SCL -> I2C_SCL
 *
 * The LCD continuously displays a counter that increments every second.
 */

#include "Arduino.h"
#include "lcd_i2c.h"
#include "lcd_i2c_display.h"

/**
 * @brief LCD I2C slave address.
 *
 * @details
 * Most PCF8574 LCD modules use address 0x27.
 * Some modules may use 0x3F.
 */
#define LCD_I2C_EXAMPLE_ADDRESS      (0x27U)

/**
 * @brief LCD column count.
 */
#define LCD_I2C_EXAMPLE_COLUMNS      (16U)

/**
 * @brief LCD row count.
 */
#define LCD_I2C_EXAMPLE_ROWS         (2U)

/**
 * @brief Display update period in milliseconds.
 */
#define LCD_I2C_EXAMPLE_PERIOD_MS    (1000U)

/**
 * @brief Run LCD I2C display example.
 *
 * @details
 * This function initializes the Arduino-style framework layer,
 * initializes the LCD, then continuously updates a counter once
 * every second.
 *
 * @return None.
 */
void Example_LcdI2cDisplay(void)
{
    int32_t s32Counter = 0;

    setup();

    LCD_Init(LCD_I2C_EXAMPLE_ADDRESS,
             LCD_I2C_EXAMPLE_COLUMNS,
             LCD_I2C_EXAMPLE_ROWS);

    LCD_Clear();

    while (1)
    {
        LCD_SetCursor(0U, 0U);
        LCD_Print("EduFramework");

        LCD_SetCursor(0U, 1U);
        LCD_Print("Count: ");
        LCD_PrintInt(s32Counter);

        s32Counter++;

        delay(LCD_I2C_EXAMPLE_PERIOD_MS);
    }
}