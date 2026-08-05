/**
 * @file st7789.c
 * @brief ST7789 TFT LCD driver implementation for EduFramework.
 *
 * @details
 * This file implements the Arduino-style display driver. It translates
 * display operations into SPI transactions and digital GPIO controls.
 */

#include "lcd_tft.h"
#include <stddef.h>

/* ========================================================================= */
/* Private Macros (Command Set)                                               */
/* ========================================================================= */

#define ST7789_CMD_SWRESET      (0x01U)
#define ST7789_CMD_SLPOUT       (0x11U)
#define ST7789_CMD_NORON        (0x13U)
#define ST7789_CMD_INVOFF       (0x20U)
#define ST7789_CMD_INVON        (0x21U)
#define ST7789_CMD_DISPON       (0x29U)
#define ST7789_CMD_CASET        (0x2AU)
#define ST7789_CMD_RASET        (0x2BU)
#define ST7789_CMD_RAMWR        (0x2CU)
#define ST7789_CMD_MADCTL       (0x36U)
#define ST7789_CMD_COLMOD       (0x3AU)

/* ========================================================================= */
/* Private Functions                                                          */
/* ========================================================================= */

/**
 * @brief Send an 8-bit command over SPI.
 *
 * @param[in] tft Pointer to the ST7789 device context.
 * @param[in] cmd Command byte to transmit.
 */
static void ST7789_WriteCommand(ST7789_t *tft, uint8_t cmd)
{
    digitalWrite(tft->dcPin, LOW);
    digitalWrite(tft->csPin, LOW);
    SPI_write(cmd);
    digitalWrite(tft->csPin, HIGH);
}

/**
 * @brief Send an 8-bit data byte over SPI.
 *
 * @param[in] tft Pointer to the ST7789 device context.
 * @param[in] data Data byte to transmit.
 */
static void ST7789_WriteData8(ST7789_t *tft, uint8_t data)
{
    digitalWrite(tft->dcPin, HIGH);
    digitalWrite(tft->csPin, LOW);
    SPI_write(data);
    digitalWrite(tft->csPin, HIGH);
}

/**
 * @brief Perform a hardware reset sequence.
 *
 * @param[in] tft Pointer to the ST7789 device context.
 */
static void ST7789_HardwareReset(ST7789_t *tft)
{
    digitalWrite(tft->rstPin, HIGH);
    delay(10);
    digitalWrite(tft->rstPin, LOW);
    delay(20);
    digitalWrite(tft->rstPin, HIGH);
    delay(120);
}

/* ========================================================================= */
/* Public API Implementation                                                  */
/* ========================================================================= */

/**
 * @copydoc ST7789_Init
 */
void ST7789_Init(ST7789_t *tft, uint8_t csPin, uint8_t dcPin, uint8_t rstPin,
                 uint16_t width, uint16_t height, uint16_t xOffset, uint16_t yOffset)
{
    if (tft == NULL)
    {
        return;
    }

    /* 1. Store configuration into the object context */
    tft->csPin   = csPin;
    tft->dcPin   = dcPin;
    tft->rstPin  = rstPin;
    tft->width   = width;
    tft->height  = height;
    tft->xOffset = xOffset;
    tft->yOffset = yOffset;

    /* 2. Configure logical Arduino pins */
    pinMode(tft->csPin, OUTPUT);
    pinMode(tft->dcPin, OUTPUT);
    pinMode(tft->rstPin, OUTPUT);

    digitalWrite(tft->csPin, HIGH);
    digitalWrite(tft->dcPin, HIGH);

    /* 3. Initialize Master SPI at 15MHz, Mode 0, MSB First */
    SPI_beginEx(SPI_ROLE_MASTER, 15000000UL, SPI_MODE0, SPI_MSBFIRST);

    /* 4. Perform Hardware Reset */
    ST7789_HardwareReset(tft);

    /* 5. Standard ST7789 Initialization Sequence */
    ST7789_WriteCommand(tft, ST7789_CMD_SWRESET);
    delay(150);

    ST7789_WriteCommand(tft, ST7789_CMD_SLPOUT);
    delay(120);

    ST7789_WriteCommand(tft, ST7789_CMD_COLMOD);
    ST7789_WriteData8(tft, 0x55U);             /* 16-bit RGB565 color format */

    ST7789_WriteCommand(tft, ST7789_CMD_MADCTL);
    ST7789_WriteData8(tft, 0x00U);             /* Default orientation */

    ST7789_WriteCommand(tft, ST7789_CMD_INVON);  /* Invert display (Required for IPS) */
    delay(10);

    ST7789_WriteCommand(tft, ST7789_CMD_NORON);
    delay(10);

    ST7789_WriteCommand(tft, ST7789_CMD_DISPON); /* Main screen turn on */
    delay(120);

    /* Clear screen to black initially */
    ST7789_FillScreen(tft, ST7789_COLOR_BLACK);
}

/**
 * @copydoc ST7789_SetAddressWindow
 */
void ST7789_SetAddressWindow(ST7789_t *tft, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    if (tft == NULL)
    {
        return;
    }

    uint16_t x_start = x0 + tft->xOffset;
    uint16_t x_end   = x1 + tft->xOffset;
    uint16_t y_start = y0 + tft->yOffset;
    uint16_t y_end   = y1 + tft->yOffset;

    /* Set Column Address (X) */
    ST7789_WriteCommand(tft, ST7789_CMD_CASET);
    ST7789_WriteData8(tft, (uint8_t)(x_start >> 8));
    ST7789_WriteData8(tft, (uint8_t)(x_start & 0xFFU));
    ST7789_WriteData8(tft, (uint8_t)(x_end >> 8));
    ST7789_WriteData8(tft, (uint8_t)(x_end & 0xFFU));

    /* Set Row Address (Y) */
    ST7789_WriteCommand(tft, ST7789_CMD_RASET);
    ST7789_WriteData8(tft, (uint8_t)(y_start >> 8));
    ST7789_WriteData8(tft, (uint8_t)(y_start & 0xFFU));
    ST7789_WriteData8(tft, (uint8_t)(y_end >> 8));
    ST7789_WriteData8(tft, (uint8_t)(y_end & 0xFFU));

    /* Prepare to write to RAM */
    ST7789_WriteCommand(tft, ST7789_CMD_RAMWR);
}

/**
 * @copydoc ST7789_DrawPixel
 */
void ST7789_DrawPixel(ST7789_t *tft, uint16_t x, uint16_t y, uint16_t color)
{
    if ((tft == NULL) || (x >= tft->width) || (y >= tft->height))
    {
        return;
    }

    ST7789_SetAddressWindow(tft, x, y, x, y);

    digitalWrite(tft->dcPin, HIGH);
    digitalWrite(tft->csPin, LOW);

    SPI_write16(color);

    digitalWrite(tft->csPin, HIGH);
}

/**
 * @copydoc ST7789_FillScreen
 */
void ST7789_FillScreen(ST7789_t *tft, uint16_t color)
{
    if (tft != NULL)
    {
        ST7789_FillRect(tft, 0U, 0U, tft->width, tft->height, color);
    }
}

/**
 * @copydoc ST7789_FillRect
 */
void ST7789_FillRect(ST7789_t *tft, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if ((tft == NULL) || (x >= tft->width) || (y >= tft->height))
    {
        return;
    }

    if ((x + w) > tft->width)  w = tft->width - x;
    if ((y + h) > tft->height) h = tft->height - y;

    ST7789_SetAddressWindow(tft, x, y, x + w - 1U, y + h - 1U);

    digitalWrite(tft->dcPin, HIGH);
    digitalWrite(tft->csPin, LOW);

    uint32_t totalPixels = (uint32_t)w * h;
    for (uint32_t i = 0UL; i < totalPixels; i++)
    {
        SPI_write16(color);
    }

    digitalWrite(tft->csPin, HIGH);
}
