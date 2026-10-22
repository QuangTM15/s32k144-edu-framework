#ifndef LCD_TFT_H
#define LCD_TFT_H

/**
 * @file lcd_tft.h
 * @brief ST7789 TFT display device library for EduFramework.
 *
 * @details
 * This module provides two API levels for ST7789 TFT displays:
 *
 * Beginner API:
 * - Uses the default EduFramework TFT hardware configuration.
 * - Hides SPI setup, display context, panel resolution, and RAM offsets.
 * - Provides Arduino-style drawing and text functions.
 *
 * Advanced API:
 * - Exposes the ST7789_t device context.
 * - Allows custom control pins, resolution, offsets, and multiple displays.
 *
 * The module is implemented on top of the EduFramework Arduino-style
 * SPI, Digital, and Time APIs.
 */

#include <stdint.h>
#include <stdbool.h>
#include "spi.h"

/* ============================================================
 * RGB565 Color Definitions
 * ============================================================ */

#define ST7789_COLOR_BLACK (0x0000U)
#define ST7789_COLOR_WHITE (0xFFFFU)
#define ST7789_COLOR_RED (0xF800U)
#define ST7789_COLOR_GREEN (0x07E0U)
#define ST7789_COLOR_BLUE (0x001FU)
#define ST7789_COLOR_YELLOW (0xFFE0U)
#define ST7789_COLOR_CYAN (0x07FFU)
#define ST7789_COLOR_MAGENTA (0xF81FU)

/**
 * @brief Beginner-friendly generic TFT color names.
 */
#define TFT_BLACK ST7789_COLOR_BLACK
#define TFT_WHITE ST7789_COLOR_WHITE
#define TFT_RED ST7789_COLOR_RED
#define TFT_GREEN ST7789_COLOR_GREEN
#define TFT_BLUE ST7789_COLOR_BLUE
#define TFT_YELLOW ST7789_COLOR_YELLOW
#define TFT_CYAN ST7789_COLOR_CYAN
#define TFT_MAGENTA ST7789_COLOR_MAGENTA

/** @brief Backlight pin omitted (e.g. permanently powered backlight). */
#define ST7789_PIN_NONE (0xFFU)

/** @brief Default clock: 8 MHz; LPSPI0 master limit is 10 MHz in current clock setup. */
#define ST7789_DEFAULT_SPI_FREQUENCY (8000000UL)

/**
 * @brief Hardware and panel configuration for one display.
 * @details The SPI bus must already be initialized as master by the application.
 * The driver owns CS/DC/RST/backlight GPIOs, but never initializes/ends SPI.
 */
typedef struct
{
    uint8_t csPin;
    uint8_t dcPin;
    uint8_t rstPin;
    uint8_t backlightPin; /**< ST7789_PIN_NONE to omit backlight control. */
    uint16_t width;
    uint16_t height;
    uint16_t xOffset;
    uint16_t yOffset;
    uint32_t spiFrequency;
} ST7789_Config_t;

/* ============================================================
 * Advanced Device Context
 * ============================================================ */

/**
 * @brief ST7789 display device context.
 *
 * @details
 * The context stores hardware configuration and text rendering state.
 * Advanced applications may create their own ST7789_t instance to use
 * custom pins, panel dimensions, or multiple displays.
 */
typedef struct
{
    uint8_t csPin;              /**< Chip Select logical pin. */
    uint8_t dcPin;              /**< Data/Command logical pin. */
    uint8_t rstPin;             /**< Hardware Reset logical pin. */
    uint8_t backlightPin;       /**< Optional backlight GPIO (ST7789_PIN_NONE allowed). */
    uint32_t spiFrequency;      /**< Per-device SPI clock target in Hz. */
    SPI_Status_t lastSpiStatus; /**< Last SPI transaction result. */
    bool initialized;           /**< Initialization completed at SPI level; not panel ACK. */

    uint16_t width;  /**< Logical display width in pixels. */
    uint16_t height; /**< Logical display height in pixels. */

    uint16_t xOffset; /**< Display RAM X offset. */
    uint16_t yOffset; /**< Display RAM Y offset. */

    uint16_t cursorX; /**< Current text cursor X position. */
    uint16_t cursorY; /**< Current text cursor Y position. */

    uint16_t textColor;      /**< Current text foreground color. */
    uint16_t textBackground; /**< Current text background color. */

    uint8_t textSize; /**< Current text scale factor. */
} ST7789_t;

/* ============================================================
 * Beginner API
 * ============================================================ */

/**
 * @brief Initialize the default TFT display.
 *
 * @details
 * The default EduFramework TFT configuration is:
 *
 * - CS: GPIO0
 * - DC: GPIO1
 * - RST: GPIO2
 * - Backlight: GPIO3
 * - Resolution: 240 x 280 pixels
 * - X offset: 0
 * - Y offset: 20
 *
 * Call SPI_begin(SPI_ROLE_MASTER) once in application code BEFORE TFT_Begin().
 * Device initialization never reinitializes the shared SPI bus.
 *
 * @return true when the default display context is initialized.
 *
 * @note ST7789 displays normally use a write-only SPI interface.
 * Therefore, successful initialization indicates that the framework
 * configuration sequence has completed, not that the panel has returned
 * an acknowledgement.
 */
bool TFT_Begin(void);

/**
 * @brief Initialize the default 240x280 panel with user-chosen control pins.
 * @details Other settings (offsets and SPI clock) remain the TFT defaults.
 * Requires an already initialized SPI master bus.
 * @return true when the SPI command sequence completed successfully.
 *         Write-only displays do not acknowledge initialization.
 */
bool TFT_BeginWithPins(uint8_t csPin, uint8_t dcPin,
                       uint8_t rstPin, uint8_t backlightPin);

/** @brief Most recent SPI status for the default TFT (not panel presence). */
SPI_Status_t TFT_GetLastSpiStatus(void);

/**
 * @brief Check whether the beginner TFT API has been initialized.
 *
 * @return true if TFT_Begin() has completed; otherwise false.
 */
bool TFT_IsInitialized(void);

/**
 * @brief Turn the default TFT backlight on.
 *
 * @return None.
 */
void TFT_BacklightOn(void);

/**
 * @brief Turn the default TFT backlight off.
 *
 * @return None.
 */
void TFT_BacklightOff(void);

/**
 * @brief Fill the entire screen with one color.
 *
 * @param[in] color RGB565 color.
 *
 * @return None.
 */
void TFT_FillScreen(uint16_t color);

/**
 * @brief Draw one pixel.
 *
 * @param[in] x X coordinate.
 * @param[in] y Y coordinate.
 * @param[in] color RGB565 color.
 *
 * @return None.
 */
void TFT_DrawPixel(
    uint16_t x,
    uint16_t y,
    uint16_t color);

/**
 * @brief Draw a line between two points.
 *
 * @param[in] x0 Start X coordinate.
 * @param[in] y0 Start Y coordinate.
 * @param[in] x1 End X coordinate.
 * @param[in] y1 End Y coordinate.
 * @param[in] color RGB565 color.
 *
 * @return None.
 */
void TFT_DrawLine(
    uint16_t x0,
    uint16_t y0,
    uint16_t x1,
    uint16_t y1,
    uint16_t color);

/**
 * @brief Draw a rectangle outline.
 *
 * @param[in] x Top-left X coordinate.
 * @param[in] y Top-left Y coordinate.
 * @param[in] width Rectangle width.
 * @param[in] height Rectangle height.
 * @param[in] color RGB565 color.
 *
 * @return None.
 */
void TFT_DrawRect(
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    uint16_t color);

/**
 * @brief Draw a filled rectangle.
 *
 * @param[in] x Top-left X coordinate.
 * @param[in] y Top-left Y coordinate.
 * @param[in] width Rectangle width.
 * @param[in] height Rectangle height.
 * @param[in] color RGB565 color.
 *
 * @return None.
 */
void TFT_FillRect(
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    uint16_t color);

/**
 * @brief Draw a circle outline.
 *
 * @param[in] x Center X coordinate.
 * @param[in] y Center Y coordinate.
 * @param[in] radius Circle radius.
 * @param[in] color RGB565 color.
 *
 * @return None.
 */
void TFT_DrawCircle(
    uint16_t x,
    uint16_t y,
    uint16_t radius,
    uint16_t color);

/**
 * @brief Draw a filled circle.
 *
 * @param[in] x Center X coordinate.
 * @param[in] y Center Y coordinate.
 * @param[in] radius Circle radius.
 * @param[in] color RGB565 color.
 *
 * @return None.
 */
void TFT_FillCircle(
    uint16_t x,
    uint16_t y,
    uint16_t radius,
    uint16_t color);

/* ============================================================
 * Beginner Text API
 * ============================================================ */

/**
 * @brief Set the text cursor position.
 *
 * @param[in] x X coordinate.
 * @param[in] y Y coordinate.
 *
 * @return None.
 */
void TFT_SetCursor(
    uint16_t x,
    uint16_t y);

/**
 * @brief Set the text foreground color.
 *
 * @param[in] color RGB565 color.
 *
 * @return None.
 */
void TFT_SetTextColor(uint16_t color);

/**
 * @brief Set the text background color.
 *
 * @param[in] color RGB565 color.
 *
 * @return None.
 */
void TFT_SetTextBackground(uint16_t color);

/**
 * @brief Set the text scale factor.
 *
 * @details
 * A text size of 1 uses the native 5x7 font.
 * Values greater than 1 scale each glyph proportionally.
 *
 * @param[in] size Text size. Values below 1 are treated as 1.
 *
 * @return None.
 */
void TFT_SetTextSize(uint8_t size);

/**
 * @brief Print a string at the current cursor position.
 *
 * @param[in] text Null-terminated ASCII string.
 *
 * @return None.
 */
void TFT_Print(const char *text);

/**
 * @brief Print a string followed by a new line.
 *
 * @param[in] text Null-terminated ASCII string.
 *
 * @return None.
 */
void TFT_Println(const char *text);

/**
 * @brief Print a signed integer.
 *
 * @param[in] value Integer value.
 *
 * @return None.
 */
void TFT_PrintInt(int32_t value);

/**
 * @brief Print a signed integer followed by a new line.
 *
 * @param[in] value Integer value.
 *
 * @return None.
 */
void TFT_PrintlnInt(int32_t value);

/**
 * @brief Print a floating-point value.
 *
 * @param[in] value Floating-point value.
 * @param[in] decimals Number of digits after the decimal point.
 *
 * @return None.
 */
void TFT_PrintFloat(
    float value,
    uint8_t decimals);

/**
 * @brief Print a floating-point value followed by a new line.
 *
 * @param[in] value Floating-point value.
 * @param[in] decimals Number of digits after the decimal point.
 *
 * @return None.
 */
void TFT_PrintlnFloat(
    float value,
    uint8_t decimals);

/* ============================================================
 * Beginner Utility API
 * ============================================================ */

/**
 * @brief Convert 8-bit RGB values to RGB565 format.
 *
 * @param[in] red Red component from 0 to 255.
 * @param[in] green Green component from 0 to 255.
 * @param[in] blue Blue component from 0 to 255.
 *
 * @return RGB565 color value.
 */
uint16_t TFT_Color565(
    uint8_t red,
    uint8_t green,
    uint8_t blue);

/**
 * @brief Get the default display width.
 *
 * @return Display width in pixels, or 0 if not initialized.
 */
uint16_t TFT_Width(void);

/**
 * @brief Get the default display height.
 *
 * @return Display height in pixels, or 0 if not initialized.
 */
uint16_t TFT_Height(void);

/* ============================================================
 * Advanced ST7789 API
 * ============================================================ */

/**
 * @brief Initialize one display with an explicit configuration.
 * @details Does not call SPI_begin()/SPI_end(). CS stays inactive outside
 * transactions; a command and all of its data share one transaction.
 * @return true if all SPI operations completed; NOT proof of panel presence.
 */
bool ST7789_Begin(ST7789_t *tft, const ST7789_Config_t *config);

/** @brief Check whether the display initialization sequence was sent. */
bool ST7789_IsInitialized(const ST7789_t *tft);

/** @brief Get the last recorded SPI transaction result for this display. */
SPI_Status_t ST7789_GetLastSpiStatus(const ST7789_t *tft);

/** @brief Set an optional per-device GPIO backlight state. */
void ST7789_SetBacklight(ST7789_t *tft, bool enabled);

/**
 * @brief Initialize an ST7789 display instance.
 *
 * @param[in,out] tft Display context.
 * @param[in] csPin Chip Select logical pin.
 * @param[in] dcPin Data/Command logical pin.
 * @param[in] rstPin Reset logical pin.
 * @param[in] width Logical display width.
 * @param[in] height Logical display height.
 * @param[in] xOffset Display RAM X offset.
 * @param[in] yOffset Display RAM Y offset.
 *
 * @return None. The legacy wrapper sets no backlight pin and requires
 * an already initialized SPI bus. See ST7789_IsInitialized() for its state.
 */
void ST7789_Init(
    ST7789_t *tft,
    uint8_t csPin,
    uint8_t dcPin,
    uint8_t rstPin,
    uint16_t width,
    uint16_t height,
    uint16_t xOffset,
    uint16_t yOffset);

/**
 * @brief Set the display RAM address window.
 * @details Sends CASET/RASET/RAMWR as a complete standalone transaction.
 * For actual pixel drawing use ST7789_DrawPixel()/ST7789_FillRect(): those
 * keep the address window AND pixel bytes under a single CS assertion.
 * Do not stream raw SPI bytes after this API on a shared bus.
 *
 * @param[in] tft Display context.
 * @param[in] x0 Start X coordinate.
 * @param[in] y0 Start Y coordinate.
 * @param[in] x1 End X coordinate.
 * @param[in] y1 End Y coordinate.
 *
 * @return None.
 */
void ST7789_SetAddressWindow(
    ST7789_t *tft,
    uint16_t x0,
    uint16_t y0,
    uint16_t x1,
    uint16_t y1);

/**
 * @brief Draw one pixel.
 */
void ST7789_DrawPixel(
    ST7789_t *tft,
    uint16_t x,
    uint16_t y,
    uint16_t color);

/**
 * @brief Draw a line.
 */
void ST7789_DrawLine(
    ST7789_t *tft,
    uint16_t x0,
    uint16_t y0,
    uint16_t x1,
    uint16_t y1,
    uint16_t color);

/**
 * @brief Draw a rectangle outline.
 */
void ST7789_DrawRect(
    ST7789_t *tft,
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    uint16_t color);

/**
 * @brief Draw a filled rectangle.
 */
void ST7789_FillRect(
    ST7789_t *tft,
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    uint16_t color);

/**
 * @brief Fill the entire display.
 */
void ST7789_FillScreen(
    ST7789_t *tft,
    uint16_t color);

/**
 * @brief Draw a circle outline.
 */
void ST7789_DrawCircle(
    ST7789_t *tft,
    uint16_t x,
    uint16_t y,
    uint16_t radius,
    uint16_t color);

/**
 * @brief Draw a filled circle.
 */
void ST7789_FillCircle(
    ST7789_t *tft,
    uint16_t x,
    uint16_t y,
    uint16_t radius,
    uint16_t color);

/**
 * @brief Draw one ASCII character.
 */
void ST7789_DrawChar(
    ST7789_t *tft,
    uint16_t x,
    uint16_t y,
    char character,
    uint16_t color,
    uint16_t background,
    uint8_t size);

/**
 * @brief Draw a null-terminated ASCII string.
 */
void ST7789_DrawString(
    ST7789_t *tft,
    uint16_t x,
    uint16_t y,
    const char *text,
    uint16_t color,
    uint16_t background,
    uint8_t size);

#endif /* LCD_TFT_H */