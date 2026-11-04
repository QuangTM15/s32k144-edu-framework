/**
 * @file ds18b20.h
 * @brief DS18B20 temperature-sensor driver for EduFramework.
 *
 * @details
 * Provides a single-sensor convenience API and a context-based API for
 * multidrop 1-Wire buses. All pin arguments use EduFramework logical pins.
 * Supported operations: ROM search/selection, CRC-checked scratchpad access,
 * 9- to 12-bit resolution, blocking/nonblocking conversion, temperature
 * decoding, alarm thresholds/search, and EEPROM copy/recall.
 *
 * @note External VDD and an external 1-Wire pull-up are required for
 *       temperature conversion and EEPROM programming with this backend.
 *       Parasite power can be detected, but strong pull-up is not implemented
 *       in the current OneWire master. Such operations return UNSUPPORTED.
 * @note Initialize the framework with setup() before using the driver.
 *       OneWire uses LPIT0 channel 2 and assumes its 40 MHz clock.
 * @warning Not thread-safe or ISR-safe. Only one outstanding conversion per
 *          bus; devices on different pins may use distinct bus contexts.
 * @warning Contexts must be zero-initialized before their first use.
 */
#ifndef DS18B20_H
#define DS18B20_H

#include <stdbool.h>
#include <stdint.h>
#include "onewire.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define DS18B20_ROM_SIZE (8U)
#define DS18B20_SCRATCHPAD_SIZE (9U)
#define DS18B20_FAMILY_CODE (0x28U)

    /* Fixed-width status codes; do not confuse these with OneWire_Status_t. */
    typedef uint8_t DS18B20_Status_t;
#define DS18B20_OK ((DS18B20_Status_t)0U)
#define DS18B20_INVALID_ARGUMENT ((DS18B20_Status_t)1U)
#define DS18B20_NOT_INITIALIZED ((DS18B20_Status_t)2U)
#define DS18B20_NO_DEVICE ((DS18B20_Status_t)3U)
#define DS18B20_BUS_ERROR ((DS18B20_Status_t)4U)
#define DS18B20_TIMER_ERROR ((DS18B20_Status_t)5U)
#define DS18B20_CRC_ERROR ((DS18B20_Status_t)6U)
#define DS18B20_UNSUPPORTED ((DS18B20_Status_t)7U)
#define DS18B20_BUSY ((DS18B20_Status_t)8U)
#define DS18B20_TIMEOUT ((DS18B20_Status_t)9U)
#define DS18B20_VERIFY_ERROR ((DS18B20_Status_t)10U)
#define DS18B20_CAPACITY_EXCEEDED ((DS18B20_Status_t)11U)
#define DS18B20_PROTOCOL_ERROR ((DS18B20_Status_t)12U)

    /* Resolution is expressed directly in bits. */
    typedef uint8_t DS18B20_Resolution_t;
#define DS18B20_RESOLUTION_9_BIT ((DS18B20_Resolution_t)9U)
#define DS18B20_RESOLUTION_10_BIT ((DS18B20_Resolution_t)10U)
#define DS18B20_RESOLUTION_11_BIT ((DS18B20_Resolution_t)11U)
#define DS18B20_RESOLUTION_12_BIT ((DS18B20_Resolution_t)12U)

    /* Power-mode values returned by DS18B20_GetPowerMode(). */
    typedef uint8_t DS18B20_PowerMode_t;
#define DS18B20_POWER_EXTERNAL ((DS18B20_PowerMode_t)0U)
#define DS18B20_POWER_PARASITE ((DS18B20_PowerMode_t)1U)

    /** Caller-owned 1-Wire bus context. Keep one instance per physical DQ pin. */
    typedef struct
    {
        OneWire_t oneWire;
        uint8_t initialized;
        uint8_t converting;
        uint8_t ownerRom[DS18B20_ROM_SIZE];
        uint32_t conversionStartMs;
        uint32_t conversionDeadlineMs;
    } DS18B20_Bus_t;

    /** Caller-owned sensor context; shares a bus with other sensor contexts. */
    typedef struct
    {
        DS18B20_Bus_t *bus;
        uint8_t rom[DS18B20_ROM_SIZE];
        DS18B20_Resolution_t resolution;
        DS18B20_PowerMode_t powerMode;
        uint8_t attached;
    } DS18B20_Device_t;

    /** Nine raw scratchpad bytes, including the Dallas/Maxim CRC8 in byte 8. */
    typedef struct
    {
        uint8_t bytes[DS18B20_SCRATCHPAD_SIZE];
    } DS18B20_Scratchpad_t;

    /* -------------------------------------------------------------------------
     * Simple API: one automatically discovered DS18B20 on a selectable pin.
     * ------------------------------------------------------------------------- */
    /**
     * @brief Configure a logical GPIO and select the first discovered DS18B20.
     * @param pin Any board pin with PIN_CAP_DIGITAL capability.
     * @return true when a sensor was found and its scratchpad verified.
     * @note If multiple DS18B20s share the pin, the first ROM-search result is
     *       used. Use the advanced API to address a specific sensor.
     */
    bool DS18B20_begin(uint8_t pin);

    /** Release the simple API's bus context. */
    void DS18B20_end(void);

    /**
     * @brief Run a blocking temperature conversion and return degrees Celsius.
     * @return Celsius, or NAN on any failure. Call DS18B20_lastStatus() to
     *         determine the reason. External VDD is required.
     */
    float DS18B20_readCelsius(void);

    /** Return the status produced by the last simple API operation. */
    DS18B20_Status_t DS18B20_lastStatus(void);

    /* -------------------------------------------------------------------------
     * Advanced API: explicit bus and device management.
     * ------------------------------------------------------------------------- */
    /** Initialize a zeroed bus context using a logical digital GPIO pin. */
    DS18B20_Status_t DS18B20_BusBegin(DS18B20_Bus_t *bus, uint8_t pin);

    /** Release a bus. Attached device handles become unusable until reattached. */
    DS18B20_Status_t DS18B20_BusEnd(DS18B20_Bus_t *bus);

    /**
     * @brief Enumerate DS18B20 ROM codes by the normal 1-Wire search algorithm.
     * @param roms Output array of [capacity][8].
     * @param capacity Number of ROM slots available; must be greater than zero.
     * @param count Number stored (up to capacity).
     * @retval DS18B20_CAPACITY_EXCEEDED Additional sensors were found.
     * @note The search traverses other 1-Wire family codes but only stores 0x28.
     */
    DS18B20_Status_t DS18B20_FindDevices(DS18B20_Bus_t *bus, uint8_t (*roms)[DS18B20_ROM_SIZE], uint8_t capacity, uint8_t *count);

    /**
     * @brief Search for DS18B20s currently in the alarm state (0xEC).
     * @note Alarm state is updated by a temperature conversion. Zero matches
     *       returns DS18B20_OK with *count == 0.
     */
    DS18B20_Status_t DS18B20_FindAlarms(DS18B20_Bus_t *bus, uint8_t (*roms)[DS18B20_ROM_SIZE], uint8_t capacity, uint8_t *count);

    /**
     * @brief Select a discovered sensor and validate its scratchpad and power.
     * @param rom Valid 8-byte 1-Wire ROM (family code 0x28, CRC verified).
     */
    DS18B20_Status_t DS18B20_Attach(DS18B20_Device_t *device, DS18B20_Bus_t *bus, const uint8_t rom[DS18B20_ROM_SIZE]);

    /** Read the raw nine-byte scratchpad and validate its CRC8. */
    DS18B20_Status_t DS18B20_ReadScratchpad(DS18B20_Device_t *device, DS18B20_Scratchpad_t *scratchpad);

    /** Decode a validated scratchpad into Celsius, including negative values. */
    DS18B20_Status_t DS18B20_DecodeCelsius(
        const DS18B20_Scratchpad_t *scratchpad, float *celsius);

    /** Read the latest scratchpad temperature; does not start a conversion. */
    DS18B20_Status_t DS18B20_ReadLastCelsius(DS18B20_Device_t *device, float *celsius);

    /** Read sensor power mode (externally powered or parasite-powered). */
    DS18B20_Status_t DS18B20_GetPowerMode(DS18B20_Device_t *device, DS18B20_PowerMode_t *mode);

    /** Read the resolution currently present in the scratchpad. */
    DS18B20_Status_t DS18B20_GetResolution(DS18B20_Device_t *device, DS18B20_Resolution_t *resolution);

    /**
     * @brief Update resolution in the volatile scratchpad (9..12 bits).
     * @note TH and TL are preserved. Call CopyScratchpad to persist to EEPROM.
     */
    DS18B20_Status_t DS18B20_SetResolution(DS18B20_Device_t *device, DS18B20_Resolution_t resolution);

    /** Read signed integer Celsius alarm thresholds TH and TL. */
    DS18B20_Status_t DS18B20_GetAlarmThresholds(DS18B20_Device_t *device,
                                                int8_t *high, int8_t *low);

    /**
     * @brief Update volatile alarm thresholds; preserve current resolution.
     * @note Thresholds are signed integral Celsius values. EEPROM is unchanged.
     */
    DS18B20_Status_t DS18B20_SetAlarmThresholds(DS18B20_Device_t *device,
                                                int8_t high, int8_t low);

    /**
     * @brief Write all three configurable scratchpad bytes in one command.
     * @note Advanced operation; updates TH, TL and resolution together.
     *       Values are verified by reading back the CRC-checked scratchpad.
     */
    DS18B20_Status_t DS18B20_WriteConfiguration(DS18B20_Device_t *device,
                                                int8_t high, int8_t low,
                                                DS18B20_Resolution_t resolution);

    /**
     * @brief Copy volatile TH/TL/configuration into nonvolatile EEPROM (0x48).
     * @warning EEPROM endurance is finite. Does not work in parasite mode with
     *          this backend. Do not call routinely in the main loop.
     */
    DS18B20_Status_t DS18B20_CopyScratchpad(DS18B20_Device_t *device);

    /** Load TH/TL/configuration from nonvolatile EEPROM into scratchpad (0xB8). */
    DS18B20_Status_t DS18B20_RecallEEPROM(DS18B20_Device_t *device);

    /**
     * @brief Initiate nonblocking temperature conversion on one device.
     * @note Only one conversion is tracked per bus. Externally powered sensor
     *       required. Check ConversionReady(), then ReadLastCelsius().
     */
    DS18B20_Status_t DS18B20_StartConversion(DS18B20_Device_t *device);

    /**
     * @brief Poll whether the previously started conversion has completed.
     * @param ready Set to true only when the conversion is finished.
     * @retval DS18B20_TIMEOUT The device failed to complete within its timing
     *         budget. A timed-out conversion is released from the bus state.
     */
    DS18B20_Status_t DS18B20_ConversionReady(DS18B20_Device_t *device,
                                             bool *ready);

    /** Abort the tracked conversion and reset the bus. */
    DS18B20_Status_t DS18B20_AbortConversion(DS18B20_Device_t *device);

    /** Blocking conversion followed by CRC-checked temperature read. */
    DS18B20_Status_t DS18B20_ReadCelsius(DS18B20_Device_t *device,
                                         float *celsius);

    /** Human-readable diagnostic label for logging. */
    const char *DS18B20_StatusString(DS18B20_Status_t status);

#ifdef __cplusplus
}
#endif
#endif /* DS18B20_H */
