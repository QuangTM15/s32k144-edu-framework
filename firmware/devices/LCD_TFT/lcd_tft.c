/**
 * @file lcd_tft.c
 * @brief ST7789 TFT display implementation for EduFramework.
 *
 * @details
 * This module provides beginner-friendly TFT graphics and text APIs
 * together with an advanced ST7789 device interface.
 */

#include "lcd_tft.h"
#include "Arduino.h"

#include <stddef.h>

/* ============================================================
 * ST7789 Command Definitions
 * ============================================================ */

#define ST7789_CMD_SWRESET (0x01U)
#define ST7789_CMD_SLPOUT (0x11U)
#define ST7789_CMD_NORON (0x13U)
#define ST7789_CMD_INVON (0x21U)
#define ST7789_CMD_DISPON (0x29U)
#define ST7789_CMD_CASET (0x2AU)
#define ST7789_CMD_RASET (0x2BU)
#define ST7789_CMD_RAMWR (0x2CU)
#define ST7789_CMD_MADCTL (0x36U)
#define ST7789_CMD_COLMOD (0x3AU)

/* ============================================================
 * ST7789 Configuration
 * ============================================================ */

#define ST7789_SPI_FREQUENCY (15000000UL)

#define ST7789_RESET_HIGH_DELAY_MS (10U)
#define ST7789_RESET_LOW_DELAY_MS (20U)
#define ST7789_RESET_RECOVERY_MS (120U)

#define ST7789_SWRESET_DELAY_MS (150U)
#define ST7789_SLPOUT_DELAY_MS (120U)
#define ST7789_NORMAL_DELAY_MS (10U)
#define ST7789_DISPON_DELAY_MS (120U)

/* ============================================================
 * Default Beginner Configuration
 * ============================================================ */

#define TFT_DEFAULT_CS_PIN (GPIO0)
#define TFT_DEFAULT_DC_PIN (GPIO1)
#define TFT_DEFAULT_RST_PIN (GPIO2)
#define TFT_DEFAULT_BACKLIGHT_PIN (GPIO3)

#define TFT_DEFAULT_WIDTH (240U)
#define TFT_DEFAULT_HEIGHT (280U)

#define TFT_DEFAULT_X_OFFSET (0U)
#define TFT_DEFAULT_Y_OFFSET (20U)

#define TFT_DEFAULT_TEXT_SIZE (1U)

#define TFT_FONT_FIRST_CHAR (32)
#define TFT_FONT_LAST_CHAR (126)

#define TFT_FONT_WIDTH (5U)
#define TFT_FONT_HEIGHT (8U)
#define TFT_FONT_SPACING (1U)

/* ============================================================
 * Beginner API Context
 * ============================================================ */

static ST7789_t g_tftDefault;
static bool g_tftInitialized = false;

/* ============================================================
 * 5x7 ASCII Font
 * ============================================================ */

/**
 * @brief Minimal 5x7 ASCII font from character 32 to 126.
 */
static const uint8_t g_font5x7[95][5] =
    {
        {0x00, 0x00, 0x00, 0x00, 0x00},
        {0x00, 0x00, 0x5F, 0x00, 0x00},
        {0x00, 0x07, 0x00, 0x07, 0x00},
        {0x14, 0x7F, 0x14, 0x7F, 0x14},
        {0x24, 0x2A, 0x7F, 0x2A, 0x12},
        {0x23, 0x13, 0x08, 0x64, 0x62},
        {0x36, 0x49, 0x55, 0x22, 0x50},
        {0x00, 0x05, 0x03, 0x00, 0x00},
        {0x00, 0x1C, 0x22, 0x41, 0x00},
        {0x00, 0x41, 0x22, 0x1C, 0x00},
        {0x14, 0x08, 0x3E, 0x08, 0x14},
        {0x08, 0x08, 0x3E, 0x08, 0x08},
        {0x00, 0x50, 0x30, 0x00, 0x00},
        {0x08, 0x08, 0x08, 0x08, 0x08},
        {0x00, 0x60, 0x60, 0x00, 0x00},
        {0x20, 0x10, 0x08, 0x04, 0x02},

        {0x3E, 0x51, 0x49, 0x45, 0x3E},
        {0x00, 0x42, 0x7F, 0x40, 0x00},
        {0x42, 0x61, 0x51, 0x49, 0x46},
        {0x21, 0x41, 0x45, 0x4B, 0x31},
        {0x18, 0x14, 0x12, 0x7F, 0x10},
        {0x27, 0x45, 0x45, 0x45, 0x39},
        {0x3C, 0x4A, 0x49, 0x49, 0x30},
        {0x01, 0x71, 0x09, 0x05, 0x03},
        {0x36, 0x49, 0x49, 0x49, 0x36},
        {0x06, 0x49, 0x49, 0x29, 0x1E},

        {0x00, 0x36, 0x36, 0x00, 0x00},
        {0x00, 0x56, 0x36, 0x00, 0x00},
        {0x08, 0x14, 0x22, 0x41, 0x00},
        {0x14, 0x14, 0x14, 0x14, 0x14},
        {0x00, 0x41, 0x22, 0x14, 0x08},
        {0x02, 0x01, 0x51, 0x09, 0x06},
        {0x3E, 0x41, 0x5D, 0x55, 0x1E},

        {0x7E, 0x11, 0x11, 0x11, 0x7E},
        {0x7F, 0x49, 0x49, 0x49, 0x36},
        {0x3E, 0x41, 0x41, 0x41, 0x22},
        {0x7F, 0x41, 0x41, 0x22, 0x1C},
        {0x7F, 0x49, 0x49, 0x49, 0x41},
        {0x7F, 0x09, 0x09, 0x09, 0x01},
        {0x3E, 0x41, 0x49, 0x49, 0x7A},
        {0x7F, 0x08, 0x08, 0x08, 0x7F},
        {0x00, 0x41, 0x7F, 0x41, 0x00},
        {0x20, 0x40, 0x41, 0x3F, 0x01},
        {0x7F, 0x08, 0x14, 0x22, 0x41},
        {0x7F, 0x40, 0x40, 0x40, 0x40},
        {0x7F, 0x02, 0x0C, 0x02, 0x7F},
        {0x7F, 0x04, 0x08, 0x10, 0x7F},
        {0x3E, 0x41, 0x41, 0x41, 0x3E},
        {0x7F, 0x09, 0x09, 0x09, 0x06},
        {0x3E, 0x41, 0x51, 0x21, 0x5E},
        {0x7F, 0x09, 0x19, 0x29, 0x46},
        {0x46, 0x49, 0x49, 0x49, 0x31},
        {0x01, 0x01, 0x7F, 0x01, 0x01},
        {0x3F, 0x40, 0x40, 0x40, 0x3F},
        {0x1F, 0x20, 0x40, 0x20, 0x1F},
        {0x3F, 0x40, 0x38, 0x40, 0x3F},
        {0x63, 0x14, 0x08, 0x14, 0x63},
        {0x07, 0x08, 0x70, 0x08, 0x07},
        {0x61, 0x51, 0x49, 0x45, 0x43},

        {0x00, 0x7F, 0x41, 0x41, 0x00},
        {0x02, 0x04, 0x08, 0x10, 0x20},
        {0x00, 0x41, 0x41, 0x7F, 0x00},
        {0x04, 0x02, 0x01, 0x02, 0x04},
        {0x40, 0x40, 0x40, 0x40, 0x40},
        {0x00, 0x01, 0x02, 0x04, 0x00},

        {0x20, 0x54, 0x54, 0x54, 0x78},
        {0x7F, 0x48, 0x44, 0x44, 0x38},
        {0x38, 0x44, 0x44, 0x44, 0x20},
        {0x38, 0x44, 0x44, 0x48, 0x7F},
        {0x38, 0x54, 0x54, 0x54, 0x18},
        {0x08, 0x7E, 0x09, 0x01, 0x02},
        {0x0C, 0x52, 0x52, 0x52, 0x3E},
        {0x7F, 0x08, 0x04, 0x04, 0x78},
        {0x00, 0x44, 0x7D, 0x40, 0x00},
        {0x20, 0x40, 0x44, 0x3D, 0x00},
        {0x7F, 0x10, 0x28, 0x44, 0x00},
        {0x00, 0x41, 0x7F, 0x40, 0x00},
        {0x7C, 0x04, 0x18, 0x04, 0x78},
        {0x7C, 0x08, 0x04, 0x04, 0x78},
        {0x38, 0x44, 0x44, 0x44, 0x38},
        {0x7C, 0x14, 0x14, 0x14, 0x08},
        {0x08, 0x14, 0x14, 0x18, 0x7C},
        {0x7C, 0x08, 0x04, 0x04, 0x08},
        {0x48, 0x54, 0x54, 0x54, 0x20},
        {0x04, 0x3F, 0x44, 0x40, 0x20},
        {0x3C, 0x40, 0x40, 0x20, 0x7C},
        {0x1C, 0x20, 0x40, 0x20, 0x1C},
        {0x3C, 0x40, 0x30, 0x40, 0x3C},
        {0x44, 0x28, 0x10, 0x28, 0x44},
        {0x0C, 0x50, 0x50, 0x50, 0x3C},
        {0x44, 0x64, 0x54, 0x4C, 0x44},

        {0x00, 0x08, 0x36, 0x41, 0x00},
        {0x00, 0x00, 0x7F, 0x00, 0x00},
        {0x00, 0x41, 0x36, 0x08, 0x00},
        {0x08, 0x04, 0x08, 0x10, 0x08}};

/* ============================================================
 * Private ST7789 Helpers
 * ============================================================ */

static void ST7789_WriteCommand(
    ST7789_t *tft,
    uint8_t command)
{
    if (tft != NULL)
    {
        digitalWrite(tft->dcPin, LOW);
        digitalWrite(tft->csPin, LOW);

        SPI_write(command);

        digitalWrite(tft->csPin, HIGH);
    }
}

static void ST7789_WriteData8(
    ST7789_t *tft,
    uint8_t data)
{
    if (tft != NULL)
    {
        digitalWrite(tft->dcPin, HIGH);
        digitalWrite(tft->csPin, LOW);

        SPI_write(data);

        digitalWrite(tft->csPin, HIGH);
    }
}

static void ST7789_HardwareReset(ST7789_t *tft)
{
    if (tft != NULL)
    {
        digitalWrite(tft->rstPin, HIGH);
        delay(ST7789_RESET_HIGH_DELAY_MS);

        digitalWrite(tft->rstPin, LOW);
        delay(ST7789_RESET_LOW_DELAY_MS);

        digitalWrite(tft->rstPin, HIGH);
        delay(ST7789_RESET_RECOVERY_MS);
    }
}

static void ST7789_DrawFastHorizontalLine(
    ST7789_t *tft,
    int32_t x,
    int32_t y,
    int32_t width,
    uint16_t color)
{
    int32_t clippedWidth = width;

    if ((tft != NULL) &&
        (0 < clippedWidth) &&
        (0 <= y) &&
        ((int32_t)tft->height > y))
    {
        if (0 > x)
        {
            clippedWidth += x;
            x = 0;
        }

        if ((x + clippedWidth) > (int32_t)tft->width)
        {
            clippedWidth = (int32_t)tft->width - x;
        }

        if (0 < clippedWidth)
        {
            ST7789_FillRect(
                tft,
                (uint16_t)x,
                (uint16_t)y,
                (uint16_t)clippedWidth,
                1U,
                color);
        }
    }
}

static void ST7789_DrawFastVerticalLine(
    ST7789_t *tft,
    int32_t x,
    int32_t y,
    int32_t height,
    uint16_t color)
{
    int32_t clippedHeight = height;

    if ((tft != NULL) &&
        (0 < clippedHeight) &&
        (0 <= x) &&
        ((int32_t)tft->width > x))
    {
        if (0 > y)
        {
            clippedHeight += y;
            y = 0;
        }

        if ((y + clippedHeight) > (int32_t)tft->height)
        {
            clippedHeight = (int32_t)tft->height - y;
        }

        if (0 < clippedHeight)
        {
            ST7789_FillRect(
                tft,
                (uint16_t)x,
                (uint16_t)y,
                1U,
                (uint16_t)clippedHeight,
                color);
        }
    }
}

/* ============================================================
 * Advanced ST7789 Implementation
 * ============================================================ */

void ST7789_Init(
    ST7789_t *tft,
    uint8_t csPin,
    uint8_t dcPin,
    uint8_t rstPin,
    uint16_t width,
    uint16_t height,
    uint16_t xOffset,
    uint16_t yOffset)
{
    if ((tft != NULL) &&
        (0U < width) &&
        (0U < height))
    {
        tft->csPin = csPin;
        tft->dcPin = dcPin;
        tft->rstPin = rstPin;

        tft->width = width;
        tft->height = height;

        tft->xOffset = xOffset;
        tft->yOffset = yOffset;

        tft->cursorX = 0U;
        tft->cursorY = 0U;

        tft->textColor = ST7789_COLOR_WHITE;
        tft->textBackground = ST7789_COLOR_BLACK;
        tft->textSize = TFT_DEFAULT_TEXT_SIZE;

        pinMode(tft->csPin, OUTPUT);
        pinMode(tft->dcPin, OUTPUT);
        pinMode(tft->rstPin, OUTPUT);

        digitalWrite(tft->csPin, HIGH);
        digitalWrite(tft->dcPin, HIGH);

        SPI_beginEx(
            SPI_ROLE_MASTER,
            ST7789_SPI_FREQUENCY,
            SPI_MODE0,
            SPI_MSBFIRST);

        ST7789_HardwareReset(tft);

        ST7789_WriteCommand(
            tft,
            ST7789_CMD_SWRESET);

        delay(ST7789_SWRESET_DELAY_MS);

        ST7789_WriteCommand(
            tft,
            ST7789_CMD_SLPOUT);

        delay(ST7789_SLPOUT_DELAY_MS);

        ST7789_WriteCommand(
            tft,
            ST7789_CMD_COLMOD);

        ST7789_WriteData8(
            tft,
            0x55U);

        ST7789_WriteCommand(
            tft,
            ST7789_CMD_MADCTL);

        ST7789_WriteData8(
            tft,
            0x00U);

        ST7789_WriteCommand(
            tft,
            ST7789_CMD_INVON);

        delay(ST7789_NORMAL_DELAY_MS);

        ST7789_WriteCommand(
            tft,
            ST7789_CMD_NORON);

        delay(ST7789_NORMAL_DELAY_MS);

        ST7789_WriteCommand(
            tft,
            ST7789_CMD_DISPON);

        delay(ST7789_DISPON_DELAY_MS);

        ST7789_FillScreen(
            tft,
            ST7789_COLOR_BLACK);
    }
}

void ST7789_SetAddressWindow(
    ST7789_t *tft,
    uint16_t x0,
    uint16_t y0,
    uint16_t x1,
    uint16_t y1)
{
    uint16_t xStart = 0U;
    uint16_t xEnd = 0U;
    uint16_t yStart = 0U;
    uint16_t yEnd = 0U;

    if (tft != NULL)
    {
        xStart = x0 + tft->xOffset;
        xEnd = x1 + tft->xOffset;

        yStart = y0 + tft->yOffset;
        yEnd = y1 + tft->yOffset;

        ST7789_WriteCommand(
            tft,
            ST7789_CMD_CASET);

        ST7789_WriteData8(
            tft,
            (uint8_t)(xStart >> 8U));

        ST7789_WriteData8(
            tft,
            (uint8_t)(xStart & 0xFFU));

        ST7789_WriteData8(
            tft,
            (uint8_t)(xEnd >> 8U));

        ST7789_WriteData8(
            tft,
            (uint8_t)(xEnd & 0xFFU));

        ST7789_WriteCommand(
            tft,
            ST7789_CMD_RASET);

        ST7789_WriteData8(
            tft,
            (uint8_t)(yStart >> 8U));

        ST7789_WriteData8(
            tft,
            (uint8_t)(yStart & 0xFFU));

        ST7789_WriteData8(
            tft,
            (uint8_t)(yEnd >> 8U));

        ST7789_WriteData8(
            tft,
            (uint8_t)(yEnd & 0xFFU));

        ST7789_WriteCommand(
            tft,
            ST7789_CMD_RAMWR);
    }
}

void ST7789_DrawPixel(
    ST7789_t *tft,
    uint16_t x,
    uint16_t y,
    uint16_t color)
{
    if ((tft != NULL) &&
        (x < tft->width) &&
        (y < tft->height))
    {
        ST7789_SetAddressWindow(
            tft,
            x,
            y,
            x,
            y);

        digitalWrite(tft->dcPin, HIGH);
        digitalWrite(tft->csPin, LOW);

        SPI_write16(color);

        digitalWrite(tft->csPin, HIGH);
    }
}

void ST7789_FillRect(
    ST7789_t *tft,
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    uint16_t color)
{
    uint16_t clippedWidth = width;
    uint16_t clippedHeight = height;

    uint32_t totalPixels = 0UL;
    uint32_t i = 0UL;

    if ((tft != NULL) &&
        (x < tft->width) &&
        (y < tft->height) &&
        (0U < width) &&
        (0U < height))
    {
        if (((uint32_t)x + (uint32_t)clippedWidth) >
            (uint32_t)tft->width)
        {
            clippedWidth =
                (uint16_t)(tft->width - x);
        }

        if (((uint32_t)y + (uint32_t)clippedHeight) >
            (uint32_t)tft->height)
        {
            clippedHeight =
                (uint16_t)(tft->height - y);
        }

        if ((0U < clippedWidth) &&
            (0U < clippedHeight))
        {
            ST7789_SetAddressWindow(
                tft,
                x,
                y,
                (uint16_t)(x + clippedWidth - 1U),
                (uint16_t)(y + clippedHeight - 1U));

            digitalWrite(tft->dcPin, HIGH);
            digitalWrite(tft->csPin, LOW);

            totalPixels =
                (uint32_t)clippedWidth *
                (uint32_t)clippedHeight;

            for (i = 0UL;
                 i < totalPixels;
                 i++)
            {
                SPI_write16(color);
            }

            digitalWrite(tft->csPin, HIGH);
        }
    }
}

void ST7789_FillScreen(
    ST7789_t *tft,
    uint16_t color)
{
    if (tft != NULL)
    {
        ST7789_FillRect(
            tft,
            0U,
            0U,
            tft->width,
            tft->height,
            color);
    }
}

void ST7789_DrawLine(
    ST7789_t *tft,
    uint16_t x0,
    uint16_t y0,
    uint16_t x1,
    uint16_t y1,
    uint16_t color)
{
    int32_t currentX = (int32_t)x0;
    int32_t currentY = (int32_t)y0;

    int32_t targetX = (int32_t)x1;
    int32_t targetY = (int32_t)y1;

    int32_t dx = 0;
    int32_t dy = 0;

    int32_t stepX = 0;
    int32_t stepY = 0;

    int32_t error = 0;
    int32_t error2 = 0;

    bool finished = false;

    if (tft != NULL)
    {
        if (targetX >= currentX)
        {
            dx = targetX - currentX;
            stepX = 1;
        }
        else
        {
            dx = currentX - targetX;
            stepX = -1;
        }

        if (targetY >= currentY)
        {
            dy = currentY - targetY;
            stepY = 1;
        }
        else
        {
            dy = targetY - currentY;
            stepY = -1;
        }

        error = dx + dy;

        while (false == finished)
        {
            if ((0 <= currentX) &&
                (0 <= currentY) &&
                (currentX < (int32_t)tft->width) &&
                (currentY < (int32_t)tft->height))
            {
                ST7789_DrawPixel(
                    tft,
                    (uint16_t)currentX,
                    (uint16_t)currentY,
                    color);
            }

            if ((currentX == targetX) &&
                (currentY == targetY))
            {
                finished = true;
            }
            else
            {
                error2 = 2 * error;

                if (error2 >= dy)
                {
                    error += dy;
                    currentX += stepX;
                }

                if (error2 <= dx)
                {
                    error += dx;
                    currentY += stepY;
                }
            }
        }
    }
}

void ST7789_DrawRect(
    ST7789_t *tft,
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    uint16_t color)
{
    if ((tft != NULL) &&
        (0U < width) &&
        (0U < height))
    {
        ST7789_DrawFastHorizontalLine(
            tft,
            (int32_t)x,
            (int32_t)y,
            (int32_t)width,
            color);

        ST7789_DrawFastHorizontalLine(
            tft,
            (int32_t)x,
            (int32_t)y + (int32_t)height - 1,
            (int32_t)width,
            color);

        ST7789_DrawFastVerticalLine(
            tft,
            (int32_t)x,
            (int32_t)y,
            (int32_t)height,
            color);

        ST7789_DrawFastVerticalLine(
            tft,
            (int32_t)x + (int32_t)width - 1,
            (int32_t)y,
            (int32_t)height,
            color);
    }
}

void ST7789_DrawCircle(
    ST7789_t *tft,
    uint16_t x,
    uint16_t y,
    uint16_t radius,
    uint16_t color)
{
    int32_t centerX = (int32_t)x;
    int32_t centerY = (int32_t)y;

    int32_t currentX = 0;
    int32_t currentY = (int32_t)radius;

    int32_t decision = 1 - (int32_t)radius;

    if (tft != NULL)
    {
        while (currentX <= currentY)
        {
            if ((0 <= (centerX + currentX)) &&
                (0 <= (centerY + currentY)))
            {
                ST7789_DrawPixel(
                    tft,
                    (uint16_t)(centerX + currentX),
                    (uint16_t)(centerY + currentY),
                    color);
            }

            if ((0 <= (centerX - currentX)) &&
                (0 <= (centerY + currentY)))
            {
                ST7789_DrawPixel(
                    tft,
                    (uint16_t)(centerX - currentX),
                    (uint16_t)(centerY + currentY),
                    color);
            }

            if ((0 <= (centerX + currentX)) &&
                (0 <= (centerY - currentY)))
            {
                ST7789_DrawPixel(
                    tft,
                    (uint16_t)(centerX + currentX),
                    (uint16_t)(centerY - currentY),
                    color);
            }

            if ((0 <= (centerX - currentX)) &&
                (0 <= (centerY - currentY)))
            {
                ST7789_DrawPixel(
                    tft,
                    (uint16_t)(centerX - currentX),
                    (uint16_t)(centerY - currentY),
                    color);
            }

            if ((0 <= (centerX + currentY)) &&
                (0 <= (centerY + currentX)))
            {
                ST7789_DrawPixel(
                    tft,
                    (uint16_t)(centerX + currentY),
                    (uint16_t)(centerY + currentX),
                    color);
            }

            if ((0 <= (centerX - currentY)) &&
                (0 <= (centerY + currentX)))
            {
                ST7789_DrawPixel(
                    tft,
                    (uint16_t)(centerX - currentY),
                    (uint16_t)(centerY + currentX),
                    color);
            }

            if ((0 <= (centerX + currentY)) &&
                (0 <= (centerY - currentX)))
            {
                ST7789_DrawPixel(
                    tft,
                    (uint16_t)(centerX + currentY),
                    (uint16_t)(centerY - currentX),
                    color);
            }

            if ((0 <= (centerX - currentY)) &&
                (0 <= (centerY - currentX)))
            {
                ST7789_DrawPixel(
                    tft,
                    (uint16_t)(centerX - currentY),
                    (uint16_t)(centerY - currentX),
                    color);
            }

            currentX++;

            if (0 > decision)
            {
                decision +=
                    (2 * currentX) + 1;
            }
            else
            {
                currentY--;

                decision +=
                    (2 * (currentX - currentY)) + 1;
            }
        }
    }
}

void ST7789_FillCircle(
    ST7789_t *tft,
    uint16_t x,
    uint16_t y,
    uint16_t radius,
    uint16_t color)
{
    int32_t centerX = (int32_t)x;
    int32_t centerY = (int32_t)y;

    int32_t currentX = 0;
    int32_t currentY = (int32_t)radius;

    int32_t decision = 1 - (int32_t)radius;

    if (tft != NULL)
    {
        ST7789_DrawFastVerticalLine(
            tft,
            centerX,
            centerY - (int32_t)radius,
            ((int32_t)radius * 2) + 1,
            color);

        while (currentX <= currentY)
        {
            ST7789_DrawFastVerticalLine(
                tft,
                centerX + currentX,
                centerY - currentY,
                (currentY * 2) + 1,
                color);

            ST7789_DrawFastVerticalLine(
                tft,
                centerX - currentX,
                centerY - currentY,
                (currentY * 2) + 1,
                color);

            ST7789_DrawFastVerticalLine(
                tft,
                centerX + currentY,
                centerY - currentX,
                (currentX * 2) + 1,
                color);

            ST7789_DrawFastVerticalLine(
                tft,
                centerX - currentY,
                centerY - currentX,
                (currentX * 2) + 1,
                color);

            currentX++;

            if (0 > decision)
            {
                decision +=
                    (2 * currentX) + 1;
            }
            else
            {
                currentY--;

                decision +=
                    (2 * (currentX - currentY)) + 1;
            }
        }
    }
}

void ST7789_DrawChar(
    ST7789_t *tft,
    uint16_t x,
    uint16_t y,
    char character,
    uint16_t color,
    uint16_t background,
    uint8_t size)
{
    uint8_t column = 0U;
    uint8_t row = 0U;

    uint8_t fontData = 0U;
    uint8_t scale = size;

    int32_t characterIndex = 0;

    if (0U == scale)
    {
        scale = 1U;
    }

    if ((tft != NULL) &&
        ((int32_t)character >= TFT_FONT_FIRST_CHAR) &&
        ((int32_t)character <= TFT_FONT_LAST_CHAR))
    {
        characterIndex =
            (int32_t)character -
            TFT_FONT_FIRST_CHAR;

        for (column = 0U;
             column < TFT_FONT_WIDTH;
             column++)
        {
            fontData =
                g_font5x7[characterIndex][column];

            for (row = 0U;
                 row < TFT_FONT_HEIGHT;
                 row++)
            {
                if (0U != (fontData & 0x01U))
                {
                    if (1U == scale)
                    {
                        ST7789_DrawPixel(
                            tft,
                            (uint16_t)(x + column),
                            (uint16_t)(y + row),
                            color);
                    }
                    else
                    {
                        ST7789_FillRect(
                            tft,
                            (uint16_t)(x +
                                       ((uint16_t)column * scale)),
                            (uint16_t)(y +
                                       ((uint16_t)row * scale)),
                            scale,
                            scale,
                            color);
                    }
                }
                else
                {
                    if (1U == scale)
                    {
                        ST7789_DrawPixel(
                            tft,
                            (uint16_t)(x + column),
                            (uint16_t)(y + row),
                            background);
                    }
                    else
                    {
                        ST7789_FillRect(
                            tft,
                            (uint16_t)(x +
                                       ((uint16_t)column * scale)),
                            (uint16_t)(y +
                                       ((uint16_t)row * scale)),
                            scale,
                            scale,
                            background);
                    }
                }

                fontData >>= 1U;
            }
        }

        ST7789_FillRect(
            tft,
            (uint16_t)(x +
                       ((uint16_t)TFT_FONT_WIDTH * scale)),
            y,
            scale,
            (uint16_t)(TFT_FONT_HEIGHT * scale),
            background);
    }
}

void ST7789_DrawString(
    ST7789_t *tft,
    uint16_t x,
    uint16_t y,
    const char *text,
    uint16_t color,
    uint16_t background,
    uint8_t size)
{
    uint16_t currentX = x;
    uint16_t currentY = y;

    uint16_t characterWidth = 0U;
    uint16_t lineHeight = 0U;

    uint8_t scale = size;

    if (0U == scale)
    {
        scale = 1U;
    }

    characterWidth =
        (uint16_t)((TFT_FONT_WIDTH +
                    TFT_FONT_SPACING) *
                   scale);

    lineHeight =
        (uint16_t)(TFT_FONT_HEIGHT * scale);

    if ((tft != NULL) &&
        (text != NULL))
    {
        while ('\0' != *text)
        {
            if ('\n' == *text)
            {
                currentX = x;
                currentY =
                    (uint16_t)(currentY + lineHeight);
            }
            else if ('\r' != *text)
            {
                ST7789_DrawChar(
                    tft,
                    currentX,
                    currentY,
                    *text,
                    color,
                    background,
                    scale);

                currentX =
                    (uint16_t)(currentX +
                               characterWidth);
            }

            text++;
        }
    }
}

/* ============================================================
 * Beginner Text Helpers
 * ============================================================ */

static void TFT_NewLine(void)
{
    uint16_t lineHeight = 0U;

    if (true == g_tftInitialized)
    {
        lineHeight =
            (uint16_t)(TFT_FONT_HEIGHT *
                       g_tftDefault.textSize);

        g_tftDefault.cursorX = 0U;

        g_tftDefault.cursorY =
            (uint16_t)(g_tftDefault.cursorY +
                       lineHeight);
    }
}

static void TFT_WriteCharacter(char character)
{
    uint16_t characterWidth = 0U;
    uint16_t lineHeight = 0U;

    if (true == g_tftInitialized)
    {
        characterWidth =
            (uint16_t)((TFT_FONT_WIDTH +
                        TFT_FONT_SPACING) *
                       g_tftDefault.textSize);

        lineHeight =
            (uint16_t)(TFT_FONT_HEIGHT *
                       g_tftDefault.textSize);

        if ('\n' == character)
        {
            TFT_NewLine();
        }
        else if ('\r' != character)
        {
            if ((g_tftDefault.cursorX +
                 characterWidth) >
                g_tftDefault.width)
            {
                g_tftDefault.cursorX = 0U;

                g_tftDefault.cursorY =
                    (uint16_t)(g_tftDefault.cursorY +
                               lineHeight);
            }

            ST7789_DrawChar(
                &g_tftDefault,
                g_tftDefault.cursorX,
                g_tftDefault.cursorY,
                character,
                g_tftDefault.textColor,
                g_tftDefault.textBackground,
                g_tftDefault.textSize);

            g_tftDefault.cursorX =
                (uint16_t)(g_tftDefault.cursorX +
                           characterWidth);
        }
    }
}

static void TFT_PrintUnsignedInternal(uint32_t value)
{
    char buffer[11];
    uint8_t index = 0U;
    uint8_t i = 0U;

    if (0UL == value)
    {
        TFT_WriteCharacter('0');
    }
    else
    {
        while ((0UL < value) &&
               (index < 10U))
        {
            buffer[index] =
                (char)('0' +
                       (value % 10UL));

            value /= 10UL;
            index++;
        }

        for (i = index;
             0U < i;
             i--)
        {
            TFT_WriteCharacter(
                buffer[i - 1U]);
        }
    }
}

/* ============================================================
 * Beginner API Implementation
 * ============================================================ */

bool TFT_Begin(void)
{
    g_tftInitialized = false;

    pinMode(
        TFT_DEFAULT_BACKLIGHT_PIN,
        OUTPUT);

    digitalWrite(
        TFT_DEFAULT_BACKLIGHT_PIN,
        HIGH);

    ST7789_Init(
        &g_tftDefault,
        TFT_DEFAULT_CS_PIN,
        TFT_DEFAULT_DC_PIN,
        TFT_DEFAULT_RST_PIN,
        TFT_DEFAULT_WIDTH,
        TFT_DEFAULT_HEIGHT,
        TFT_DEFAULT_X_OFFSET,
        TFT_DEFAULT_Y_OFFSET);

    g_tftDefault.cursorX = 0U;
    g_tftDefault.cursorY = 0U;

    g_tftDefault.textColor =
        TFT_WHITE;

    g_tftDefault.textBackground =
        TFT_BLACK;

    g_tftDefault.textSize =
        TFT_DEFAULT_TEXT_SIZE;

    g_tftInitialized = true;

    return g_tftInitialized;
}

bool TFT_IsInitialized(void)
{
    return g_tftInitialized;
}

void TFT_BacklightOn(void)
{
    if (true == g_tftInitialized)
    {
        digitalWrite(
            TFT_DEFAULT_BACKLIGHT_PIN,
            HIGH);
    }
}

void TFT_BacklightOff(void)
{
    if (true == g_tftInitialized)
    {
        digitalWrite(
            TFT_DEFAULT_BACKLIGHT_PIN,
            LOW);
    }
}

void TFT_FillScreen(uint16_t color)
{
    if (true == g_tftInitialized)
    {
        ST7789_FillScreen(
            &g_tftDefault,
            color);
    }
}

void TFT_DrawPixel(
    uint16_t x,
    uint16_t y,
    uint16_t color)
{
    if (true == g_tftInitialized)
    {
        ST7789_DrawPixel(
            &g_tftDefault,
            x,
            y,
            color);
    }
}

void TFT_DrawLine(
    uint16_t x0,
    uint16_t y0,
    uint16_t x1,
    uint16_t y1,
    uint16_t color)
{
    if (true == g_tftInitialized)
    {
        ST7789_DrawLine(
            &g_tftDefault,
            x0,
            y0,
            x1,
            y1,
            color);
    }
}

void TFT_DrawRect(
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    uint16_t color)
{
    if (true == g_tftInitialized)
    {
        ST7789_DrawRect(
            &g_tftDefault,
            x,
            y,
            width,
            height,
            color);
    }
}

void TFT_FillRect(
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    uint16_t color)
{
    if (true == g_tftInitialized)
    {
        ST7789_FillRect(
            &g_tftDefault,
            x,
            y,
            width,
            height,
            color);
    }
}

void TFT_DrawCircle(
    uint16_t x,
    uint16_t y,
    uint16_t radius,
    uint16_t color)
{
    if (true == g_tftInitialized)
    {
        ST7789_DrawCircle(
            &g_tftDefault,
            x,
            y,
            radius,
            color);
    }
}

void TFT_FillCircle(
    uint16_t x,
    uint16_t y,
    uint16_t radius,
    uint16_t color)
{
    if (true == g_tftInitialized)
    {
        ST7789_FillCircle(
            &g_tftDefault,
            x,
            y,
            radius,
            color);
    }
}

void TFT_SetCursor(
    uint16_t x,
    uint16_t y)
{
    if (true == g_tftInitialized)
    {
        g_tftDefault.cursorX = x;
        g_tftDefault.cursorY = y;
    }
}

void TFT_SetTextColor(uint16_t color)
{
    if (true == g_tftInitialized)
    {
        g_tftDefault.textColor = color;
    }
}

void TFT_SetTextBackground(uint16_t color)
{
    if (true == g_tftInitialized)
    {
        g_tftDefault.textBackground = color;
    }
}

void TFT_SetTextSize(uint8_t size)
{
    if (true == g_tftInitialized)
    {
        if (0U == size)
        {
            g_tftDefault.textSize = 1U;
        }
        else
        {
            g_tftDefault.textSize = size;
        }
    }
}

void TFT_Print(const char *text)
{
    if ((true == g_tftInitialized) &&
        (text != NULL))
    {
        while ('\0' != *text)
        {
            TFT_WriteCharacter(*text);
            text++;
        }
    }
}

void TFT_Println(const char *text)
{
    if ((true == g_tftInitialized) &&
        (text != NULL))
    {
        TFT_Print(text);
        TFT_NewLine();
    }
}

void TFT_PrintInt(int32_t value)
{
    uint32_t magnitude = 0UL;

    if (true == g_tftInitialized)
    {
        if (0 > value)
        {
            TFT_WriteCharacter('-');

            magnitude =
                (uint32_t)(-(value + 1)) + 1UL;
        }
        else
        {
            magnitude =
                (uint32_t)value;
        }

        TFT_PrintUnsignedInternal(
            magnitude);
    }
}

void TFT_PrintlnInt(int32_t value)
{
    if (true == g_tftInitialized)
    {
        TFT_PrintInt(value);
        TFT_NewLine();
    }
}

void TFT_PrintFloat(
    float value,
    uint8_t decimals)
{
    uint8_t decimalCount = decimals;
    uint8_t i = 0U;

    uint32_t multiplier = 1UL;
    uint32_t integerPart = 0UL;
    uint32_t fractionalPart = 0UL;

    float positiveValue = value;
    float fraction = 0.0f;

    if (true == g_tftInitialized)
    {
        if (6U < decimalCount)
        {
            decimalCount = 6U;
        }

        if (0.0f > positiveValue)
        {
            TFT_WriteCharacter('-');
            positiveValue = -positiveValue;
        }

        for (i = 0U;
             i < decimalCount;
             i++)
        {
            multiplier *= 10UL;
        }

        integerPart =
            (uint32_t)positiveValue;

        fraction =
            positiveValue -
            (float)integerPart;

        fractionalPart =
            (uint32_t)((fraction *
                        (float)multiplier) +
                       0.5f);

        if ((0U < decimalCount) &&
            (fractionalPart >= multiplier))
        {
            integerPart++;
            fractionalPart = 0UL;
        }

        TFT_PrintUnsignedInternal(
            integerPart);

        if (0U < decimalCount)
        {
            TFT_WriteCharacter('.');

            multiplier /= 10UL;

            while (0UL < multiplier)
            {
                TFT_WriteCharacter(
                    (char)('0' +
                           ((fractionalPart /
                             multiplier) %
                            10UL)));

                multiplier /= 10UL;
            }
        }
    }
}

void TFT_PrintlnFloat(
    float value,
    uint8_t decimals)
{
    if (true == g_tftInitialized)
    {
        TFT_PrintFloat(
            value,
            decimals);

        TFT_NewLine();
    }
}

uint16_t TFT_Color565(
    uint8_t red,
    uint8_t green,
    uint8_t blue)
{
    uint16_t color = 0U;

    color =
        (uint16_t)((((uint16_t)red & 0xF8U) << 8U) |
                   (((uint16_t)green & 0xFCU) << 3U) |
                   ((uint16_t)blue >> 3U));

    return color;
}

uint16_t TFT_Width(void)
{
    uint16_t width = 0U;

    if (true == g_tftInitialized)
    {
        width = g_tftDefault.width;
    }

    return width;
}

uint16_t TFT_Height(void)
{
    uint16_t height = 0U;

    if (true == g_tftInitialized)
    {
        height = g_tftDefault.height;
    }

    return height;
}