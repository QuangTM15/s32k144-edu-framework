#include "Arduino.h"
#include "i2c_demo.h"

#define MAX30102_ADDR          0x57U
#define MAX30102_REV_ID_REG    0xFEU
#define MAX30102_PART_ID_REG   0xFFU

static void Demo_PrintHex8(uint8_t value)
{
    const char hex[] = "0123456789ABCDEF";

    Serial1_write(hex[(value >> 4U) & 0x0FU]);
    Serial1_write(hex[value & 0x0FU]);
}

static uint8_t MAX30102_ReadRegister(uint8_t reg)
{
    uint8_t value = 0U;

    Wire_beginTransmission(MAX30102_ADDR);
    Wire_write(reg);
    Wire_endTransmission();

    Wire_requestFrom(MAX30102_ADDR, 1U);

    if (Wire_available() > 0)
    {
        value = (uint8_t)Wire_read();
    }

    return value;
}

void Demo_I2C(void)
{
    uint8_t revId;
    uint8_t partId;

    setup();

    Serial1_begin(9600);
    Wire_begin();

    Serial1_println("MAX30102 read test");

    while (1)
    {
        revId  = MAX30102_ReadRegister(MAX30102_REV_ID_REG);
        partId = MAX30102_ReadRegister(MAX30102_PART_ID_REG);

        Serial1_print("REV_ID  = 0x");
        Demo_PrintHex8(revId);
        Serial1_println("");

        Serial1_print("PART_ID = 0x");
        Demo_PrintHex8(partId);
        Serial1_println("");

        Serial1_println("");

        delay(1000);
    }
}
