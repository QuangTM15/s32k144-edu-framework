/**
 * @file demo_project.c
 * @brief EduFramework integrated device demonstration.
 *
 * @details
 * This example integrates multiple EduFramework device libraries:
 *
 * - Potentiometer controls BLDC motor throttle through an ESC.
 * - HC-SR04 measures obstacle distance.
 * - Red LED indicates an obstacle within 10 cm.
 * - NTC thermistor measures temperature.
 * - ST7789 TFT displays system information and warning status.
 */

#include "demo_project.h"

#include "Arduino.h"
#include "esc.h"
#include "lcd_tft.h"
#include "ntc.h"
#include "ultrasonic.h"

#include <stdint.h>

/* ========================================================================= */
/* Hardware Configuration                                                    */
/* ========================================================================= */

#define DEMO_ESC_PIN (GPIO2)

#define DEMO_ULTRASONIC_TRIG_PIN (GPIO3)
#define DEMO_ULTRASONIC_ECHO_PIN (GPIO4)

#define DEMO_TFT_CS_PIN (GPIO5)
#define DEMO_TFT_DC_PIN (GPIO6)
#define DEMO_TFT_RST_PIN (GPIO7)

#define DEMO_POT_PIN (ADC0_SE12)
#define DEMO_NTC_PIN (ADC0_SE13)

/* ========================================================================= */
/* Demo Configuration                                                        */
/* ========================================================================= */

#define DEMO_WARNING_DISTANCE_CM (10.0F)

#define DEMO_ADC_MAX_VALUE (4095UL)
#define DEMO_THROTTLE_MAX_PERCENT (100UL)

/*
 * Motor throttle is updated continuously.
 *
 * Sensor and display tasks use millis()-based scheduling so the application
 * does not need a blocking delay inside the main loop.
 */
#define DEMO_ULTRASONIC_UPDATE_MS (60UL)
#define DEMO_NTC_UPDATE_MS (250UL)
#define DEMO_DISPLAY_UPDATE_MS (100UL)

/* TFT configuration */
#define DEMO_TFT_WIDTH (240U)
#define DEMO_TFT_HEIGHT (280U)
#define DEMO_TFT_X_OFFSET (0U)
#define DEMO_TFT_Y_OFFSET (20U)

#define DEMO_TEXT_SIZE (2U)

/* Screen layout */
#define DEMO_LINE_1_Y (10U)
#define DEMO_LINE_2_Y (50U)
#define DEMO_LINE_3_Y (85U)
#define DEMO_LINE_4_Y (120U)
#define DEMO_LINE_5_Y (170U)
#define DEMO_LINE_6_Y (210U)

/* ========================================================================= */
/* Private State                                                             */
/* ========================================================================= */

static ESC_t s_DemoEsc;
static ST7789_t s_DemoTft;

static uint8_t s_u8ThrottlePercent = 0U;

static float s_f32DistanceCm =
    ULTRASONIC_INVALID_DISTANCE_CM;

static float s_f32TemperatureC = 0.0F;

static uint8_t s_u8WarningActive = 0U;

/* Task timestamps */
static uint32_t s_u32LastUltrasonicMs = 0UL;
static uint32_t s_u32LastNtcMs = 0UL;
static uint32_t s_u32LastDisplayMs = 0UL;

/* ========================================================================= */
/* Private Function Prototypes                                               */
/* ========================================================================= */

static void Demo_InitHardware(void);

static void Demo_UpdateMotor(void);
static void Demo_UpdateDistance(void);
static void Demo_UpdateTemperature(void);
static void Demo_UpdateDisplay(void);

static void Demo_DrawStaticLayout(void);

static void Demo_DrawChar(uint16_t x,
                          uint16_t y,
                          char character,
                          uint16_t color,
                          uint16_t background,
                          uint8_t size);

static void Demo_DrawString(uint16_t x,
                            uint16_t y,
                            const char *text,
                            uint16_t color,
                            uint16_t background,
                            uint8_t size);

static void Demo_DrawUnsigned(uint16_t x,
                              uint16_t y,
                              uint32_t value,
                              uint16_t color,
                              uint8_t size);

static void Demo_DrawFloat1(uint16_t x,
                            uint16_t y,
                            float value,
                            uint16_t color,
                            uint8_t size);

/* ========================================================================= */
/* Initialization                                                            */
/* ========================================================================= */

static void Demo_InitHardware(void)
{
    /*
     * Initialize EduFramework core.
     */
    setup();

    /*
     * Configure onboard warning LED.
     *
     * MaaZEDU onboard LEDs are active-low.
     */
    pinMode(LED_RED, OUTPUT);
    digitalWrite(LED_RED, HIGH);

    /*
     * Initialize NTC.
     */
    NTC_Init();

    /*
     * Initialize ultrasonic sensor.
     */
    ultrasonicBegin(DEMO_ULTRASONIC_TRIG_PIN,
                    DEMO_ULTRASONIC_ECHO_PIN);

    /*
     * Initialize TFT.
     */
    ST7789_Init(&s_DemoTft,
                DEMO_TFT_CS_PIN,
                DEMO_TFT_DC_PIN,
                DEMO_TFT_RST_PIN,
                DEMO_TFT_WIDTH,
                DEMO_TFT_HEIGHT,
                DEMO_TFT_X_OFFSET,
                DEMO_TFT_Y_OFFSET);

    /*
     * Clear the entire display only once during initialization.
     */
    ST7789_FillScreen(&s_DemoTft,
                      ST7789_COLOR_BLACK);

    /*
     * Draw all static labels once.
     */
    Demo_DrawStaticLayout();

    /*
     * Initialize ESC.
     */
    if (true == ESC_Init(&s_DemoEsc,
                         DEMO_ESC_PIN))
    {
        /*
         * Most ESCs require minimum throttle for several seconds after
         * startup before accepting throttle commands.
         */
        ESC_Arm(&s_DemoEsc);

        ESC_SetThrottle(&s_DemoEsc, 0U);
    }
    else
    {
        /*
         * Invalid PWM pin.
         */
        digitalWrite(LED_RED, LOW);

        Demo_DrawString(10U,
                        DEMO_LINE_6_Y,
                        "ESC INIT ERROR",
                        ST7789_COLOR_RED,
                        ST7789_COLOR_BLACK,
                        DEMO_TEXT_SIZE);

        while (1)
        {
            /* Fatal configuration error. */
        }
    }

    /*
     * Obtain initial system values.
     */
    Demo_UpdateMotor();
    Demo_UpdateDistance();
    Demo_UpdateTemperature();
    Demo_UpdateDisplay();

    s_u32LastUltrasonicMs = millis();
    s_u32LastNtcMs = millis();
    s_u32LastDisplayMs = millis();
}

/* ========================================================================= */
/* Motor Control                                                             */
/* ========================================================================= */

static void Demo_UpdateMotor(void)
{
    int s32RawAdcValue = 0;
    uint32_t u32Throttle = 0UL;

    /*
     * Read potentiometer.
     */
    s32RawAdcValue = analogRead(DEMO_POT_PIN);

    if (0 <= s32RawAdcValue)
    {
        /*
         * Clamp ADC value.
         */
        if ((int)DEMO_ADC_MAX_VALUE < s32RawAdcValue)
        {
            s32RawAdcValue =
                (int)DEMO_ADC_MAX_VALUE;
        }

        /*
         * Map ADC value to throttle:
         *
         * ADC        Throttle
         * 0          0%
         * 1024       25%
         * 2048       50%
         * 3072       75%
         * 4095       100%
         */
        u32Throttle =
            ((uint32_t)s32RawAdcValue *
             DEMO_THROTTLE_MAX_PERCENT) /
            DEMO_ADC_MAX_VALUE;

        if (DEMO_THROTTLE_MAX_PERCENT <
            u32Throttle)
        {
            u32Throttle =
                DEMO_THROTTLE_MAX_PERCENT;
        }

        s_u8ThrottlePercent =
            (uint8_t)u32Throttle;

        /*
         * Apply throttle immediately.
         */
        ESC_SetThrottle(&s_DemoEsc,
                        s_u8ThrottlePercent);
    }
}

/* ========================================================================= */
/* Ultrasonic                                                                */
/* ========================================================================= */

static void Demo_UpdateDistance(void)
{
    /*
     * Read HC-SR04 distance.
     */
    s_f32DistanceCm =
        ultrasonicRead();

    /*
     * Activate warning when an obstacle is within 10 cm.
     */
    if ((ULTRASONIC_INVALID_DISTANCE_CM !=
         s_f32DistanceCm) &&
        (0.0F <= s_f32DistanceCm) &&
        (DEMO_WARNING_DISTANCE_CM >=
         s_f32DistanceCm))
    {
        s_u8WarningActive = 1U;

        /*
         * MaaZEDU LED is active-low.
         */
        digitalWrite(LED_RED, LOW);
    }
    else
    {
        s_u8WarningActive = 0U;

        digitalWrite(LED_RED, HIGH);
    }
}

/* ========================================================================= */
/* Temperature                                                               */
/* ========================================================================= */

static void Demo_UpdateTemperature(void)
{
    s_f32TemperatureC =
        NTC_ReadCelsius(DEMO_NTC_PIN);
}

/* ========================================================================= */
/* Static TFT Layout                                                         */
/* ========================================================================= */

static void Demo_DrawStaticLayout(void)
{
    /*
     * Title.
     */
    Demo_DrawString(10U,
                    DEMO_LINE_1_Y,
                    "MaaZEDU DEMO",
                    ST7789_COLOR_CYAN,
                    ST7789_COLOR_BLACK,
                    DEMO_TEXT_SIZE);

    /*
     * Throttle label.
     */
    Demo_DrawString(10U,
                    DEMO_LINE_2_Y,
                    "Throttle:",
                    ST7789_COLOR_WHITE,
                    ST7789_COLOR_BLACK,
                    DEMO_TEXT_SIZE);

    /*
     * Distance label.
     */
    Demo_DrawString(10U,
                    DEMO_LINE_3_Y,
                    "Distance:",
                    ST7789_COLOR_WHITE,
                    ST7789_COLOR_BLACK,
                    DEMO_TEXT_SIZE);

    /*
     * Temperature label.
     */
    Demo_DrawString(10U,
                    DEMO_LINE_4_Y,
                    "Temp:",
                    ST7789_COLOR_WHITE,
                    ST7789_COLOR_BLACK,
                    DEMO_TEXT_SIZE);

    /*
     * Status label.
     */
    Demo_DrawString(10U,
                    DEMO_LINE_5_Y,
                    "STATUS:",
                    ST7789_COLOR_WHITE,
                    ST7789_COLOR_BLACK,
                    DEMO_TEXT_SIZE);
}

/* ========================================================================= */
/* Dynamic TFT Update                                                        */
/* ========================================================================= */

static void Demo_UpdateDisplay(void)
{
    /* --------------------------------------------------------------------- */
    /* Throttle                                                              */
    /* --------------------------------------------------------------------- */

    /*
     * Clear only the throttle value area.
     */
    ST7789_FillRect(&s_DemoTft,
                    125U,
                    DEMO_LINE_2_Y,
                    110U,
                    20U,
                    ST7789_COLOR_BLACK);

    Demo_DrawUnsigned(130U,
                      DEMO_LINE_2_Y,
                      s_u8ThrottlePercent,
                      ST7789_COLOR_GREEN,
                      DEMO_TEXT_SIZE);

    Demo_DrawString(180U,
                    DEMO_LINE_2_Y,
                    "%",
                    ST7789_COLOR_GREEN,
                    ST7789_COLOR_BLACK,
                    DEMO_TEXT_SIZE);

    /* --------------------------------------------------------------------- */
    /* Distance                                                              */
    /* --------------------------------------------------------------------- */

    /*
     * Clear only the distance value area.
     */
    ST7789_FillRect(&s_DemoTft,
                    125U,
                    DEMO_LINE_3_Y,
                    115U,
                    20U,
                    ST7789_COLOR_BLACK);

    if (ULTRASONIC_INVALID_DISTANCE_CM !=
        s_f32DistanceCm)
    {
        Demo_DrawFloat1(130U,
                        DEMO_LINE_3_Y,
                        s_f32DistanceCm,
                        ST7789_COLOR_YELLOW,
                        DEMO_TEXT_SIZE);

        Demo_DrawString(195U,
                        DEMO_LINE_3_Y,
                        "cm",
                        ST7789_COLOR_YELLOW,
                        ST7789_COLOR_BLACK,
                        DEMO_TEXT_SIZE);
    }
    else
    {
        Demo_DrawString(130U,
                        DEMO_LINE_3_Y,
                        "ERR",
                        ST7789_COLOR_RED,
                        ST7789_COLOR_BLACK,
                        DEMO_TEXT_SIZE);
    }

    /* --------------------------------------------------------------------- */
    /* Temperature                                                           */
    /* --------------------------------------------------------------------- */

    /*
     * Clear only the temperature value area.
     */
    ST7789_FillRect(&s_DemoTft,
                    85U,
                    DEMO_LINE_4_Y,
                    150U,
                    20U,
                    ST7789_COLOR_BLACK);

    Demo_DrawFloat1(90U,
                    DEMO_LINE_4_Y,
                    s_f32TemperatureC,
                    ST7789_COLOR_CYAN,
                    DEMO_TEXT_SIZE);

    Demo_DrawString(160U,
                    DEMO_LINE_4_Y,
                    "C",
                    ST7789_COLOR_CYAN,
                    ST7789_COLOR_BLACK,
                    DEMO_TEXT_SIZE);

    /* --------------------------------------------------------------------- */
    /* Status                                                                */
    /* --------------------------------------------------------------------- */

    /*
     * Clear only current status value.
     */
    ST7789_FillRect(&s_DemoTft,
                    105U,
                    DEMO_LINE_5_Y,
                    135U,
                    20U,
                    ST7789_COLOR_BLACK);

    /*
     * Clear warning message region.
     */
    ST7789_FillRect(&s_DemoTft,
                    0U,
                    DEMO_LINE_6_Y,
                    DEMO_TFT_WIDTH,
                    60U,
                    ST7789_COLOR_BLACK);

    if (0U != s_u8WarningActive)
    {
        Demo_DrawString(110U,
                        DEMO_LINE_5_Y,
                        "WARNING",
                        ST7789_COLOR_RED,
                        ST7789_COLOR_BLACK,
                        DEMO_TEXT_SIZE);

        Demo_DrawString(10U,
                        DEMO_LINE_6_Y,
                        "!!! WARNING !!!",
                        ST7789_COLOR_RED,
                        ST7789_COLOR_BLACK,
                        DEMO_TEXT_SIZE);

        Demo_DrawString(10U,
                        (uint16_t)(DEMO_LINE_6_Y +
                                   30U),
                        "OBSTACLE < 10 CM",
                        ST7789_COLOR_RED,
                        ST7789_COLOR_BLACK,
                        1U);
    }
    else
    {
        Demo_DrawString(110U,
                        DEMO_LINE_5_Y,
                        "NORMAL",
                        ST7789_COLOR_GREEN,
                        ST7789_COLOR_BLACK,
                        DEMO_TEXT_SIZE);
    }
}

/* ========================================================================= */
/* TFT Text Helpers                                                          */
/* ========================================================================= */

static void Demo_DrawChar(uint16_t x,
                          uint16_t y,
                          char character,
                          uint16_t color,
                          uint16_t background,
                          uint8_t size)
{
    uint8_t u8Column = 0U;
    uint8_t u8Row = 0U;
    uint8_t u8Line = 0U;

    if ((32 <= character) &&
        (122 >= character))
    {
        for (u8Column = 0U;
             u8Column < 5U;
             u8Column++)
        {
            u8Line =
                font5x7[(uint8_t)character -
                        32U][u8Column];

            for (u8Row = 0U;
                 u8Row < 8U;
                 u8Row++)
            {
                if (0U !=
                    (u8Line & 0x01U))
                {
                    if (1U == size)
                    {
                        ST7789_DrawPixel(
                            &s_DemoTft,
                            (uint16_t)(x +
                                       u8Column),
                            (uint16_t)(y +
                                       u8Row),
                            color);
                    }
                    else
                    {
                        ST7789_FillRect(
                            &s_DemoTft,
                            (uint16_t)(x +
                                       ((uint16_t)u8Column *
                                        size)),
                            (uint16_t)(y +
                                       ((uint16_t)u8Row *
                                        size)),
                            size,
                            size,
                            color);
                    }
                }
                else if (background != color)
                {
                    if (1U == size)
                    {
                        ST7789_DrawPixel(
                            &s_DemoTft,
                            (uint16_t)(x +
                                       u8Column),
                            (uint16_t)(y +
                                       u8Row),
                            background);
                    }
                    else
                    {
                        ST7789_FillRect(
                            &s_DemoTft,
                            (uint16_t)(x +
                                       ((uint16_t)u8Column *
                                        size)),
                            (uint16_t)(y +
                                       ((uint16_t)u8Row *
                                        size)),
                            size,
                            size,
                            background);
                    }
                }

                u8Line >>= 1U;
            }
        }
    }
}

static void Demo_DrawString(uint16_t x,
                            uint16_t y,
                            const char *text,
                            uint16_t color,
                            uint16_t background,
                            uint8_t size)
{
    uint16_t u16CurrentX = x;

    if ((const char *)0 != text)
    {
        while ('\0' != *text)
        {
            Demo_DrawChar(u16CurrentX,
                          y,
                          *text,
                          color,
                          background,
                          size);

            u16CurrentX =
                (uint16_t)(u16CurrentX +
                           ((uint16_t)6U *
                            size));

            text++;
        }
    }
}

static void Demo_DrawUnsigned(uint16_t x,
                              uint16_t y,
                              uint32_t value,
                              uint16_t color,
                              uint8_t size)
{
    char acBuffer[11U] = {0};

    uint8_t u8Index = 0U;
    uint16_t u16CurrentX = x;

    if (0UL == value)
    {
        Demo_DrawChar(x,
                      y,
                      '0',
                      color,
                      ST7789_COLOR_BLACK,
                      size);
    }
    else
    {
        while ((0UL != value) &&
               (u8Index < 10U))
        {
            acBuffer[u8Index] =
                (char)('0' +
                       (value % 10UL));

            value /= 10UL;

            u8Index++;
        }

        while (0U != u8Index)
        {
            u8Index--;

            Demo_DrawChar(u16CurrentX,
                          y,
                          acBuffer[u8Index],
                          color,
                          ST7789_COLOR_BLACK,
                          size);

            u16CurrentX =
                (uint16_t)(u16CurrentX +
                           ((uint16_t)6U *
                            size));
        }
    }
}

static void Demo_DrawFloat1(uint16_t x,
                            uint16_t y,
                            float value,
                            uint16_t color,
                            uint8_t size)
{
    uint32_t u32IntegerPart = 0UL;
    uint32_t u32DecimalPart = 0UL;

    uint16_t u16DecimalX = 0U;

    if (0.0F > value)
    {
        Demo_DrawChar(x,
                      y,
                      '-',
                      color,
                      ST7789_COLOR_BLACK,
                      size);

        x =
            (uint16_t)(x +
                       ((uint16_t)6U *
                        size));

        value = -value;
    }

    u32IntegerPart =
        (uint32_t)value;

    u32DecimalPart =
        (uint32_t)(((value -
                     (float)u32IntegerPart) *
                    10.0F) +
                   0.5F);

    if (10UL <= u32DecimalPart)
    {
        u32IntegerPart++;
        u32DecimalPart = 0UL;
    }

    Demo_DrawUnsigned(x,
                      y,
                      u32IntegerPart,
                      color,
                      size);

    if (100UL <= u32IntegerPart)
    {
        u16DecimalX =
            (uint16_t)(x +
                       ((uint16_t)18U *
                        size));
    }
    else if (10UL <= u32IntegerPart)
    {
        u16DecimalX =
            (uint16_t)(x +
                       ((uint16_t)12U *
                        size));
    }
    else
    {
        u16DecimalX =
            (uint16_t)(x +
                       ((uint16_t)6U *
                        size));
    }

    Demo_DrawChar(u16DecimalX,
                  y,
                  '.',
                  color,
                  ST7789_COLOR_BLACK,
                  size);

    Demo_DrawUnsigned(
        (uint16_t)(u16DecimalX +
                   ((uint16_t)6U *
                    size)),
        y,
        u32DecimalPart,
        color,
        size);
}

/* ========================================================================= */
/* Public Example                                                            */
/* ========================================================================= */

void Example_DemoProject_Run(void)
{
    uint32_t u32CurrentMs = 0UL;

    Demo_InitHardware();

    while (1)
    {
        /*
         * Potentiometer -> BLDC.
         *
         * Update continuously for maximum throttle response.
         */
        Demo_UpdateMotor();

        /*
         * Get current system time once per loop.
         */
        u32CurrentMs = millis();

        /*
         * Ultrasonic task.
         */
        if (DEMO_ULTRASONIC_UPDATE_MS <=
            (u32CurrentMs -
             s_u32LastUltrasonicMs))
        {
            s_u32LastUltrasonicMs =
                u32CurrentMs;

            Demo_UpdateDistance();
        }

        /*
         * NTC task.
         */
        if (DEMO_NTC_UPDATE_MS <=
            (u32CurrentMs -
             s_u32LastNtcMs))
        {
            s_u32LastNtcMs =
                u32CurrentMs;

            Demo_UpdateTemperature();
        }

        /*
         * TFT task.
         */
        if (DEMO_DISPLAY_UPDATE_MS <=
            (u32CurrentMs -
             s_u32LastDisplayMs))
        {
            s_u32LastDisplayMs =
                u32CurrentMs;

            Demo_UpdateDisplay();
        }
    }
}