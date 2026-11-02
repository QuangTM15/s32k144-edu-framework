/**
 * @file ds18b20.c
 * @brief DS18B20 device driver for EduFramework.
 *
 * @note Uses the validated OneWire master driver.
 * @note One sensor, external power, 12-bit conversion.
 * @note Not reentrant; do not call from an ISR.
 */

#include "ds18b20.h"

#include <math.h>
#include "Arduino.h"
#include "onewire.h"

/* DS18B20 commands. */
#define DS18B20_FAMILY_CODE (0x28U)
#define DS18B20_CMD_READ_ROM (0x33U)
#define DS18B20_CMD_SKIP_ROM (0xCCU)
#define DS18B20_CMD_CONVERT_T (0x44U)
#define DS18B20_CMD_READ_SCRATCHPAD (0xBEU)

/* Device characteristics. */
#define DS18B20_ROM_SIZE (8U)
#define DS18B20_SCRATCHPAD_SIZE (9U)
#define DS18B20_CONVERSION_TIME_MS (750U)

/* MaaZEDU V1 wiring: GPIO4 = PTD12. */
#define DS18B20_GPIO4_PIN (12U)

/* Internal state. */
static OneWire_t g_ds18b20_bus = {0};
static bool g_ds18b20_ready = false;

/**
 * @brief Verify the ROM code of the connected sensor.
 */
static bool DS18B20_VerifyROM(void)
{
    bool valid = false;
    uint8_t rom[DS18B20_ROM_SIZE] = {0U};
    OneWire_Status_t status = ONEWIRE_OK;
    status = OneWire_Reset(&g_ds18b20_bus);
    if (ONEWIRE_OK == status)
    {
        status = OneWire_WriteByte(
            &g_ds18b20_bus,
            DS18B20_CMD_READ_ROM);
    }
    if (ONEWIRE_OK == status)
    {
        status = OneWire_Read(
            &g_ds18b20_bus,
            rom,
            DS18B20_ROM_SIZE);
    }
    if (ONEWIRE_OK == status)
    {
        if ((DS18B20_FAMILY_CODE == rom[0]) &&
            (rom[7] == OneWire_CRC8(rom, 7U)))
        {
            valid = true;
        }
    }
    return valid;
}

/**
 * @brief Reset bus and send a DS18B20 command.
 *
 * Skip ROM is valid because V1 supports one sensor.
 */
static bool DS18B20_SendCommand(uint8_t command)
{
    bool success = false;
    OneWire_Status_t status = ONEWIRE_OK;
    status = OneWire_Reset(&g_ds18b20_bus);
    if (ONEWIRE_OK == status)
    {
        status = OneWire_WriteByte(
            &g_ds18b20_bus,
            DS18B20_CMD_SKIP_ROM);
    }
    if (ONEWIRE_OK == status)
    {
        status = OneWire_WriteByte(
            &g_ds18b20_bus,
            command);
    }
    if (ONEWIRE_OK == status)
    {
        success = true;
    }
    return success;
}

/**
 * @brief Initialize and identify the sensor.
 */
bool DS18B20_begin(uint8_t pin)
{
    bool success = false;
    g_ds18b20_ready = false;
    /* Release the previous configuration if present. */
    if (0U != g_ds18b20_bus.initialized)
    {
        (void)OneWire_Deinit(&g_ds18b20_bus);
    }
    /*
     * V1 supports the verified MaaZEDU GPIO4/PTD12
     * connection. Additional pins can be mapped later.
     */
    if (GPIO4 == pin)
    {
        const OneWire_Pin_t io =
            {
                IP_PORTD,
                IP_PTD,
                DS18B20_GPIO4_PIN};
        if (ONEWIRE_OK == OneWire_Init(
                              &g_ds18b20_bus,
                              &io))
        {
            if (true == DS18B20_VerifyROM())
            {
                g_ds18b20_ready = true;
                success = true;
            }
            else
            {
                (void)OneWire_Deinit(&g_ds18b20_bus);
            }
        }
    }
    return success;
}

/**
 * @brief Convert raw temperature to Celsius.
 *
 * DS18B20 temperature is a signed 16-bit value
 * with a resolution of 1/16 degree Celsius.
 */
static float DS18B20_DecodeTemperature(
    const uint8_t *scratchpad)
{
    uint16_t raw_bits = 0U;
    int32_t signed_raw = 0L;
    uint8_t resolution = 0U;
    raw_bits = (uint16_t)((uint16_t)scratchpad[0] |
                          ((uint16_t)scratchpad[1] << 8U));
    /*
     * Configuration register bits R1:R0 indicate
     * resolution: 9, 10, 11 or 12 bits.
     */
    resolution = (uint8_t)((scratchpad[4] >> 5U) & 0x03U);
    if (0U == resolution)
    {
        raw_bits &= (uint16_t)~0x0007U;
    }
    else if (1U == resolution)
    {
        raw_bits &= (uint16_t)~0x0003U;
    }
    else if (2U == resolution)
    {
        raw_bits &= (uint16_t)~0x0001U;
    }
    /* Explicit two's-complement conversion. */
    if (0U != (raw_bits & 0x8000U))
    {
        signed_raw = (int32_t)raw_bits - 65536L;
    }
    else
    {
        signed_raw = (int32_t)raw_bits;
    }
    return (float)signed_raw / 16.0f;
}

/**
 * @brief Perform a complete temperature measurement.
 */
float DS18B20_readCelsius(void)
{
    float temperature = NAN;
    bool success = false;
    uint8_t scratchpad[DS18B20_SCRATCHPAD_SIZE] = {0U};
    if (true == g_ds18b20_ready)
    {
        /*
         * Start a new temperature conversion.
         * External power is required for this V1.
         */
        success = DS18B20_SendCommand(
            DS18B20_CMD_CONVERT_T);
        if (true == success)
        {
            /*
             * Maximum conversion time at 12-bit
             * resolution is 750 ms.
             */
            delay(DS18B20_CONVERSION_TIME_MS);
            success = DS18B20_SendCommand(
                DS18B20_CMD_READ_SCRATCHPAD);
        }
        if (true == success)
        {
            if (ONEWIRE_OK == OneWire_Read(
                                  &g_ds18b20_bus,
                                  scratchpad,
                                  DS18B20_SCRATCHPAD_SIZE))
            {
                if (scratchpad[8] ==
                    OneWire_CRC8(scratchpad, 8U))
                {
                    temperature =
                        DS18B20_DecodeTemperature(
                            scratchpad);
                }
            }
        }
    }
    return temperature;
}
