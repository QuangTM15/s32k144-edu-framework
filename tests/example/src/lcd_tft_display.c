/**
 * @file tft_spi_display.c
 * @brief Hello World text rendering example for ST7789 display.
 */

#include "lcd_tft_display.h"
#include "Arduino.h"
#include "lcd_tft.h"

/* Global display object instance */
static ST7789_t tftDisplay;


/**
 * @brief Runs the Hello World demo on the ST7789 screen.
 */
void Example_TFT_HelloWorld(void)
{
    /* 1. Mandatory framework initialization for Clock and Time */
    setup();

    /* 2. Configure backlight (If BLK pin is connected to GPIO3 instead of 3V3) */
    pinMode(GPIO3, OUTPUT);      /*[cite: 4] */
    digitalWrite(GPIO3, HIGH);   /* Turn on backlight[cite: 4] */

    /* 3. Initialize display object (Customize CS, DC, RST pins for your hardware) */
    ST7789_Init(&tftDisplay, GPIO0, GPIO1, GPIO2, 240U, 280U, 0U, 20U);

    /* 4. Clear screen to Black */
    ST7789_FillScreen(&tftDisplay, ST7789_COLOR_BLACK);

    /* 5. Print "HELLO WORLD!" (Size x3, Yellow text, Black background) */
    DrawString(&tftDisplay, 10U, 80U, "HELLO WORLD!", ST7789_COLOR_YELLOW, ST7789_COLOR_BLACK, 3U);

    /* 6. Print "EduFramework" below (Size x2, Cyan text, Black background) */
    DrawString(&tftDisplay, 10U, 120U, "EduFramework", ST7789_COLOR_CYAN, ST7789_COLOR_BLACK, 2U);

    /* 7. Blink the built-in LED to indicate the program is running[cite: 4] */
    pinMode(LED_BUILTIN, OUTPUT); /*[cite: 4] */

    while (1)
    {
        digitalToggle(LED_BUILTIN); /*[cite: 4] */
        delay(500);                 /*[cite: 4] */
    }
}
