#include "Arduino.h"
#include "spi_full_demo.h"

/* ============================================================
 * ROLE SELECT
 * 1 = master, 0 = slave
 * ============================================================ */
#define SPI_DEMO_IS_MASTER    1

/* ============================================================
 * TEST CASE SELECT
 * ============================================================ */
#define SPI_DEMO_CASE_8BIT    0
#define SPI_DEMO_CASE_16BIT   1
#define SPI_DEMO_CASE_BUFFER  2

#define SPI_DEMO_CASE         SPI_DEMO_CASE_BUFFER

/* ============================================================
 * TEST DATA
 * ============================================================ */
#define CMD_DATA8             0xA5U
#define CMD_DATA16            0x55AAU

#define SPI_BUF_LEN           3U
static const uint8_t g_testBuf[SPI_BUF_LEN] = {0x11U, 0x22U, 0x33U};

/* ============================================================
 * LED helpers (active low)
 * ============================================================ */
static void LED_AllOff(void)
{
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_BLUE, HIGH);
    digitalWrite(LED_GREEN, HIGH);
}

static uint8_t ButtonPressed(uint8_t pin)
{
    if (digitalRead(pin) == LOW)
    {
        while (digitalRead(pin) == LOW)
        {
        }
        delay(30);
        return 1U;
    }

    return 0U;
}

static void Demo_InitPins(void)
{
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    LED_AllOff();

#if SPI_DEMO_IS_MASTER
    pinMode(BTN0, INPUT_PULLUP);
#endif
}

static void Demo_InitSPI(void)
{
#if SPI_DEMO_CASE == SPI_DEMO_CASE_8BIT
    #if SPI_DEMO_IS_MASTER
        SPI_beginEx(SPI_ROLE_MASTER, 1000000U, SPI_MODE0, SPI_MSBFIRST);
    #else
        SPI_beginEx(SPI_ROLE_SLAVE, 0U, SPI_MODE0, SPI_MSBFIRST);
    #endif

#elif SPI_DEMO_CASE == SPI_DEMO_CASE_16BIT
    #if SPI_DEMO_IS_MASTER
        SPI_beginEx(SPI_ROLE_MASTER, 1000000U, SPI_MODE0, SPI_MSBFIRST);
    #else
        SPI_beginEx(SPI_ROLE_SLAVE, 0U, SPI_MODE0, SPI_MSBFIRST);
    #endif

#elif SPI_DEMO_CASE == SPI_DEMO_CASE_BUFFER
    #if SPI_DEMO_IS_MASTER
        SPI_beginEx(SPI_ROLE_MASTER, 1000000U, SPI_MODE0, SPI_MSBFIRST);
    #else
        SPI_beginEx(SPI_ROLE_SLAVE, 0U, SPI_MODE0, SPI_MSBFIRST);
    #endif
#endif
}

#if SPI_DEMO_IS_MASTER

static void Demo_MasterHandleButton(void)
{
    if (ButtonPressed(BTN0) == 0U)
    {
        return;
    }

#if SPI_DEMO_CASE == SPI_DEMO_CASE_8BIT
    digitalToggle(LED_RED);
    (void)SPI_transfer(CMD_DATA8);

#elif SPI_DEMO_CASE == SPI_DEMO_CASE_16BIT
    digitalToggle(LED_BLUE);
    (void)SPI_transfer16(CMD_DATA16);

#elif SPI_DEMO_CASE == SPI_DEMO_CASE_BUFFER
    digitalToggle(LED_GREEN);
    SPI_transferBuffer(g_testBuf, (uint8_t *)0, SPI_BUF_LEN);
#endif
}

#else

static void Demo_SlaveHandleReceive(void)
{
#if SPI_DEMO_CASE == SPI_DEMO_CASE_8BIT
    uint8_t data8 = 0U;

    if (!SPI_available())
    {
        return;
    }

    data8 = SPI_read();

    if (data8 == CMD_DATA8)
    {
        digitalToggle(LED_RED);
    }

#elif SPI_DEMO_CASE == SPI_DEMO_CASE_16BIT
    uint16_t data16 = 0U;

    if (!SPI_available())
    {
        return;
    }

    data16 = SPI_read16();

    if (data16 == CMD_DATA16)
    {
        digitalToggle(LED_BLUE);
    }

#elif SPI_DEMO_CASE == SPI_DEMO_CASE_BUFFER
    uint8_t buf[SPI_BUF_LEN];
    uint8_t i;

    if (!SPI_available())
    {
        return;
    }

    buf[0] = SPI_read();

    for (i = 1U; i < SPI_BUF_LEN; i++)
    {
        while (!SPI_available())
        {
        }

        buf[i] = SPI_read();
    }

    if ((buf[0] == 0x11U) &&
        (buf[1] == 0x22U) &&
        (buf[2] == 0x33U))
    {
        digitalToggle(LED_GREEN);
    }
#endif
}

#endif

void Demo_SPI_Full(void)
{
    setup();
    Demo_InitPins();
    Demo_InitSPI();

    while (1)
    {
#if SPI_DEMO_IS_MASTER
        Demo_MasterHandleButton();
#else
        Demo_SlaveHandleReceive();
#endif
    }
}
