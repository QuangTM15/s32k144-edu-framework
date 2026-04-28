#include "Arduino.h"
#include "i2c_demo.h"

/* ============================================================
 * ROLE SELECT
 * 1 = master, 0 = slave
 * ============================================================ */
#define I2C_DEMO_IS_MASTER    0

/* ============================================================
 * TEST DATA
 * ============================================================ */
#define I2C_SLAVE_ADDR        0x12U
#define I2C_CMD_TOGGLE        0xA5U

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

/* ============================================================
 * Init
 * ============================================================ */

static void Demo_InitPins(void)
{
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);

    LED_AllOff();

#if I2C_DEMO_IS_MASTER
    pinMode(BTN0, INPUT_PULLUP);
#endif
}

static void Demo_InitI2C(void)
{
#if I2C_DEMO_IS_MASTER
    Wire_begin();
#else
    Wire_beginAddress(I2C_SLAVE_ADDR);
#endif
}

/* ============================================================
 * Slave callback
 * ============================================================ */

#if !I2C_DEMO_IS_MASTER

static void Demo_OnReceive(int len)
{
    uint8_t data;

    if (len <= 0)
    {
        return;
    }

    data = (uint8_t)Wire_read();

    if (data == I2C_CMD_TOGGLE)
    {
        digitalToggle(LED_RED);
    }
}

#endif

/* ============================================================
 * Master
 * ============================================================ */

#if I2C_DEMO_IS_MASTER

static void Demo_MasterHandleButton(void)
{
    if (ButtonPressed(BTN0) == 0U)
    {
        return;
    }

    Wire_beginTransmission(I2C_SLAVE_ADDR);
    Wire_write(I2C_CMD_TOGGLE);
    Wire_endTransmission();

    digitalToggle(LED_GREEN);
}

#endif

/* ============================================================
 * Main demo
 * ============================================================ */

void Demo_I2C(void)
{
    setup();

    Demo_InitPins();
    Demo_InitI2C();

#if !I2C_DEMO_IS_MASTER
    Wire_onReceive(Demo_OnReceive);
#endif

    while (1)
    {
#if I2C_DEMO_IS_MASTER
        Demo_MasterHandleButton();
#else
        /* Slave runs by interrupt callback */
#endif
    }
}
