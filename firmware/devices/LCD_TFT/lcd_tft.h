#ifndef LCD_TFT_H
#define LCD_TFT_H

/**
 * @file st7789.h
 * @brief Arduino-style ST7789 TFT LCD driver for EduFramework.
 *
 * @details
 * This module provides an Arduino-like C interface for the ST7789 display.
 * It is built entirely on top of the EduFramework high-level APIs
 * (SPI, wiring_digital, and time) and does not interact with hardware
 * registers directly.
 *
 * To support multiple displays or dynamic configurations, the hardware
 * context is stored in the ST7789_t structure, mimicking the object-oriented
 * approach of standard Arduino C++ libraries.
 */

#include "Arduino.h"
#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * Color Definitions (RGB565)
 * ============================================================ */

#define ST7789_COLOR_BLACK       0x0000U
#define ST7789_COLOR_WHITE       0xFFFFU
#define ST7789_COLOR_RED         0xF800U
#define ST7789_COLOR_GREEN       0x07E0U
#define ST7789_COLOR_BLUE        0x001FU
#define ST7789_COLOR_YELLOW      0xFFE0U
#define ST7789_COLOR_CYAN        0x07FFU
#define ST7789_COLOR_MAGENTA     0xF81FU

/* ============================================================
 * Configuration Structure (The "Object")
 * ============================================================ */

/**
 * @brief ST7789 device context structure.
 *
 * @details
 * This structure acts as an object instance for the ST7789 display.
 * It holds the logical pin mapping and resolution configurations,
 * allowing multiple displays to be controlled independently.
 */
typedef struct
{
    uint8_t csPin;        /**< Logical Arduino pin for Chip Select. */
    uint8_t dcPin;        /**< Logical Arduino pin for Data/Command. */
    uint8_t rstPin;       /**< Logical Arduino pin for Hardware Reset. */

    uint16_t width;       /**< Display physical width in pixels. */
    uint16_t height;      /**< Display physical height in pixels. */

    uint16_t xOffset;     /**< X-axis RAM offset. */
    uint16_t yOffset;     /**< Y-axis RAM offset (usually 20 for 240x280). */
} ST7789_t;

/* ============================================================
 * Public API Prototypes
 * ============================================================ */

/**
 * @brief Initialize the ST7789 display.
 *
 * @details
 * This function initializes the provided ST7789_t context, configures
 * the designated logical pins as outputs, initializes the SPI bus, and
 * sends the startup command sequence to the display.
 *
 * @param[in,out] tft
 * Pointer to the ST7789 device context.
 *
 * @param[in] csPin
 * Logical pin number for Chip Select.
 *
 * @param[in] dcPin
 * Logical pin number for Data/Command.
 *
 * @param[in] rstPin
 * Logical pin number for Hardware Reset.
 *
 * @param[in] width
 * Display width in pixels (e.g., 240).
 *
 * @param[in] height
 * Display height in pixels (e.g., 280).
 *
 * @param[in] xOffset
 * RAM column offset (usually 0).
 *
 * @param[in] yOffset
 * RAM row offset (usually 20 for 240x280 display).
 *
 * @return None.
 */
void ST7789_Init(ST7789_t *tft, uint8_t csPin, uint8_t dcPin, uint8_t rstPin,
                 uint16_t width, uint16_t height, uint16_t xOffset, uint16_t yOffset);

/**
 * @brief Set the address window for display RAM writing.
 *
 * @param[in] tft Pointer to the ST7789 device context.
 * @param[in] x0 Start column address.
 * @param[in] y0 Start row address.
 * @param[in] x1 End column address.
 * @param[in] y1 End row address.
 *
 * @return None.
 */
void ST7789_SetAddressWindow(ST7789_t *tft, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/**
 * @brief Draw a single pixel on the display.
 *
 * @param[in] tft Pointer to the ST7789 device context.
 * @param[in] x X-coordinate of the pixel.
 * @param[in] y Y-coordinate of the pixel.
 * @param[in] color 16-bit RGB565 color value.
 *
 * @return None.
 */
void ST7789_DrawPixel(ST7789_t *tft, uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief Fill the entire display with a single color.
 *
 * @param[in] tft Pointer to the ST7789 device context.
 * @param[in] color 16-bit RGB565 color value.
 *
 * @return None.
 */
void ST7789_FillScreen(ST7789_t *tft, uint16_t color);

/**
 * @brief Draw a filled rectangle on the display.
 *
 * @param[in] tft Pointer to the ST7789 device context.
 * @param[in] x X-coordinate of the top-left corner.
 * @param[in] y Y-coordinate of the top-left corner.
 * @param[in] w Width of the rectangle.
 * @param[in] h Height of the rectangle.
 * @param[in] color 16-bit RGB565 color value.
 *
 * @return None.
 */
void ST7789_FillRect(ST7789_t *tft, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

#endif /* ST7789_H */
