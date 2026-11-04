/**
 * @file ds18b20.c
 * @brief DS18B20 driver implementation for EduFramework.
 *
 * @details
 * This layer implements device-specific ROM selection, memory commands,
 * conversion state and temperature decoding on the existing OneWire master.
 * ROM search uses the standard 1-Wire discrepancy-tree algorithm.
 * All functions are synchronous except StartConversion/ConversionReady.
 * No heap allocation, ISR use or implicit GPIO4/PTD12 assumption.
 *
 * @note Conversion and EEPROM programming require external VDD. The current
 *       OneWire backend does not implement parasite-power strong pull-up.
 */
#include "ds18b20.h"

#include <math.h>
#include <stddef.h>
#include <string.h>
#include "Arduino.h"

#define DS_CMD_SEARCH_ROM (0xF0U)
#define DS_CMD_ALARM_SEARCH (0xECU)
#define DS_CMD_MATCH_ROM (0x55U)
#define DS_CMD_CONVERT_T (0x44U)
#define DS_CMD_READ_SCRATCHPAD (0xBEU)
#define DS_CMD_WRITE_SCRATCHPAD (0x4EU)
#define DS_CMD_COPY_SCRATCHPAD (0x48U)
#define DS_CMD_RECALL_E2 (0xB8U)
#define DS_CMD_READ_POWER_SUPPLY (0xB4U)

#define DS_CONFIG_FIXED_BITS (0x1FU)
#define DS_CONVERSION_MARGIN_MS (50UL)
#define DS_EEPROM_COPY_WAIT_MS (20UL)
#define DS_EEPROM_RECALL_TIMEOUT_MS (100UL)

static DS18B20_Bus_t s_defaultBus = {0};
static DS18B20_Device_t s_defaultDevice = {0};
static DS18B20_Status_t s_defaultStatus = DS18B20_NOT_INITIALIZED;

/** Map low-level 1-Wire diagnostics into device-level status codes. */
static DS18B20_Status_t DS_MapOneWire(OneWire_Status_t status)
{
    DS18B20_Status_t result = DS18B20_BUS_ERROR;
    if (ONEWIRE_OK == status)
    {
        result = DS18B20_OK;
    }
    else if (ONEWIRE_INVALID_ARGUMENT == status)
    {
        result = DS18B20_INVALID_ARGUMENT;
    }
    else if (ONEWIRE_NOT_INITIALIZED == status)
    {
        result = DS18B20_NOT_INITIALIZED;
    }
    else if (ONEWIRE_NO_DEVICE == status)
    {
        result = DS18B20_NO_DEVICE;
    }
    else if (ONEWIRE_TIMER_ERROR == status)
    {
        result = DS18B20_TIMER_ERROR;
    }
    else if (ONEWIRE_BUSY == status)
    {
        result = DS18B20_BUSY;
    }
    else
    {
        result = DS18B20_BUS_ERROR;
    }
    return result;
}

static bool DS_ValidResolution(DS18B20_Resolution_t resolution)
{
    return ((DS18B20_RESOLUTION_9_BIT <= resolution) &&
            (DS18B20_RESOLUTION_12_BIT >= resolution));
}

static DS18B20_Resolution_t DS_ReadResolution(uint8_t config)
{
    return (DS18B20_Resolution_t)(9U + ((config >> 5U) & 0x03U));
}

static uint8_t DS_MakeConfig(DS18B20_Resolution_t resolution)
{
    return (uint8_t)(DS_CONFIG_FIXED_BITS |
                     ((uint8_t)(resolution - 9U) << 5U));
}

static uint32_t DS_ConversionTime(DS18B20_Resolution_t resolution)
{
    uint32_t timeMs = 750UL;
    if (DS18B20_RESOLUTION_9_BIT == resolution)
    {
        timeMs = 94UL;
    }
    else if (DS18B20_RESOLUTION_10_BIT == resolution)
    {
        timeMs = 188UL;
    }
    else if (DS18B20_RESOLUTION_11_BIT == resolution)
    {
        timeMs = 375UL;
    }
    return timeMs;
}

static DS18B20_Status_t DS_CheckBus(const DS18B20_Bus_t *bus)
{
    DS18B20_Status_t result = DS18B20_INVALID_ARGUMENT;
    if (NULL != bus)
    {
        result = (0U != bus->initialized) ? DS18B20_OK : DS18B20_NOT_INITIALIZED;
    }
    return result;
}

static DS18B20_Status_t DS_CheckDevice(const DS18B20_Device_t *device)
{
    DS18B20_Status_t result = DS18B20_INVALID_ARGUMENT;
    if (NULL != device)
    {
        result = DS18B20_NOT_INITIALIZED;
        if ((0U != device->attached) && (NULL != device->bus))
        {
            result = DS_CheckBus(device->bus);
            if ((DS18B20_OK == result) &&
                (0U != device->bus->converting))
            {
                result = DS18B20_BUSY;
            }
        }
    }
    return result;
}

/** Reset, select exactly one ROM and send a DS18B20 function command. */
static DS18B20_Status_t DS_SendDeviceCommand(DS18B20_Device_t *device,
                                             uint8_t command)
{
    DS18B20_Status_t result = DS_CheckDevice(device);
    OneWire_Status_t oneWireResult = ONEWIRE_OK;
    if (DS18B20_OK == result)
    {
        oneWireResult = OneWire_Reset(&device->bus->oneWire);
        if (ONEWIRE_OK == oneWireResult)
        {
            oneWireResult = OneWire_WriteByte(&device->bus->oneWire,
                                              DS_CMD_MATCH_ROM);
        }
        if (ONEWIRE_OK == oneWireResult)
        {
            oneWireResult = OneWire_Write(&device->bus->oneWire,
                                          device->rom, DS18B20_ROM_SIZE);
        }
        if (ONEWIRE_OK == oneWireResult)
        {
            oneWireResult = OneWire_WriteByte(&device->bus->oneWire,
                                              command);
        }
        result = DS_MapOneWire(oneWireResult);
    }
    return result;
}

/**
 * @brief Perform one complete pass of the Dallas/Maxim Search ROM tree.
 * @param rom In/out: previous ROM on entry, selected ROM on success.
 * @param lastDiscrepancy Previous tree branch position (0 for first pass).
 * @param nextDiscrepancy Next tree branch position (0 means final device).
 */
static DS18B20_Status_t DS_SearchNext(DS18B20_Bus_t *bus, uint8_t command,
                                      uint8_t rom[DS18B20_ROM_SIZE],
                                      uint8_t lastDiscrepancy,
                                      uint8_t *nextDiscrepancy)
{
    DS18B20_Status_t result = DS18B20_OK;
    OneWire_Status_t ow = OneWire_Reset(&bus->oneWire);
    uint8_t bitIndex = 0U;
    uint8_t zeroBranch = 0U;

    if (ONEWIRE_OK == ow)
    {
        ow = OneWire_WriteByte(&bus->oneWire, command);
    }
    for (bitIndex = 1U; (64U >= bitIndex) && (ONEWIRE_OK == ow); bitIndex++)
    {
        uint8_t idBit = 0U;
        uint8_t complementBit = 0U;
        uint8_t direction = 0U;
        uint8_t index = (uint8_t)((bitIndex - 1U) / 8U);
        uint8_t mask = (uint8_t)(1U << ((bitIndex - 1U) % 8U));

        ow = OneWire_ReadBit(&bus->oneWire, &idBit);
        if (ONEWIRE_OK == ow)
        {
            ow = OneWire_ReadBit(&bus->oneWire, &complementBit);
        }
        if (ONEWIRE_OK == ow)
        {
            if ((1U == idBit) && (1U == complementBit))
            {
                result = DS18B20_NO_DEVICE;
                break;
            }
            if (idBit != complementBit)
            {
                direction = idBit;
            }
            else
            {
                if (bitIndex < lastDiscrepancy)
                {
                    direction = ((rom[index] & mask) != 0U) ? 1U : 0U;
                }
                else if (bitIndex == lastDiscrepancy)
                {
                    direction = 1U;
                }
                else
                {
                    direction = 0U;
                }
                if (0U == direction)
                {
                    zeroBranch = bitIndex;
                }
            }
            if (0U != direction)
            {
                rom[index] |= mask;
            }
            else
            {
                rom[index] &= (uint8_t)~mask;
            }
            ow = OneWire_WriteBit(&bus->oneWire, direction);
        }
    }
    if (ONEWIRE_OK != ow)
    {
        result = DS_MapOneWire(ow);
    }
    if (DS18B20_OK == result)
    {
        *nextDiscrepancy = zeroBranch;
        if (rom[7] != OneWire_CRC8(rom, 7UL))
        {
            result = DS18B20_CRC_ERROR;
        }
    }
    return result;
}

static DS18B20_Status_t DS_Find(DS18B20_Bus_t *bus,
                                uint8_t (*roms)[DS18B20_ROM_SIZE],
                                uint8_t capacity, uint8_t *count,
                                uint8_t command)
{
    DS18B20_Status_t result = DS_CheckBus(bus);
    uint8_t rom[DS18B20_ROM_SIZE] = {0U};
    uint8_t lastDiscrepancy = 0U;
    uint8_t nextDiscrepancy = 0U;
    uint8_t stored = 0U;
    bool done = false;
    uint16_t iterations = 0U;

    if ((NULL == roms) || (NULL == count) || (0U == capacity))
    {
        result = DS18B20_INVALID_ARGUMENT;
    }
    if (DS18B20_OK == result)
    {
        *count = 0U;
        if (0U != bus->converting)
        {
            result = DS18B20_BUSY;
        }
    }
    while ((DS18B20_OK == result) && (false == done) &&
           (256U > iterations))
    {
        iterations++;
        result = DS_SearchNext(bus, command, rom, lastDiscrepancy,
                               &nextDiscrepancy);
        if (DS18B20_NO_DEVICE == result)
        {
            /* A no-alarm response is valid only on the first alarm pass. */
            if ((DS_CMD_ALARM_SEARCH == command) &&
                (1U == iterations))
            {
                result = DS18B20_OK;
            }
            else if (1U < iterations)
            {
                /* The bus changed unexpectedly during enumeration. */
                result = DS18B20_PROTOCOL_ERROR;
            }
            break;
        }
        if (DS18B20_OK == result)
        {
            if (DS18B20_FAMILY_CODE == rom[0])
            {
                if (stored < capacity)
                {
                    (void)memcpy(roms[stored], rom, DS18B20_ROM_SIZE);
                    stored++;
                }
                else
                {
                    result = DS18B20_CAPACITY_EXCEEDED;
                }
            }
            lastDiscrepancy = nextDiscrepancy;
            done = (0U == lastDiscrepancy);
        }
    }
    if ((DS18B20_OK == result) && (0U == stored) &&
        (DS_CMD_SEARCH_ROM == command))
    {
        result = DS18B20_NO_DEVICE;
    }
    if ((DS18B20_OK == result) && (256U <= iterations) &&
        (false == done))
    {
        result = DS18B20_PROTOCOL_ERROR;
    }
    if (NULL != count)
    {
        *count = stored;
    }
    return result;
}

DS18B20_Status_t DS18B20_BusBegin(DS18B20_Bus_t *bus, uint8_t pin)
{
    DS18B20_Status_t result = DS18B20_INVALID_ARGUMENT;
    if ((NULL != bus) &&
        ARDUINO_IS_VALID_PIN(pin) &&
        (ARDUINO_VALID_TRUE == Arduino_HasDigitalCapability(pin)))
    {
        if (0U != bus->initialized)
        {
            result = DS18B20_BUSY;
        }
        else
        {
            const ArduinoPinMap_t *map = &g_arduinoPinMap[pin];
            OneWire_Pin_t io = {map->portBase, map->gpioBase,
                                map->pinNumber};
            (void)memset(bus, 0, sizeof(*bus));
            result = DS_MapOneWire(OneWire_Init(&bus->oneWire, &io));
            if (DS18B20_OK == result)
            {
                bus->initialized = 1U;
            }
            else
            {
                (void)OneWire_Deinit(&bus->oneWire);
            }
        }
    }
    return result;
}

DS18B20_Status_t DS18B20_BusEnd(DS18B20_Bus_t *bus)
{
    DS18B20_Status_t result = DS18B20_INVALID_ARGUMENT;
    if (NULL != bus)
    {
        if (0U != bus->initialized)
        {
            result = DS_MapOneWire(OneWire_Deinit(&bus->oneWire));
        }
        else
        {
            result = DS18B20_NOT_INITIALIZED;
        }
        (void)memset(bus, 0, sizeof(*bus));
    }
    return result;
}

DS18B20_Status_t DS18B20_FindDevices(DS18B20_Bus_t *bus,
                                     uint8_t (*roms)[DS18B20_ROM_SIZE],
                                     uint8_t capacity, uint8_t *count)
{
    return DS_Find(bus, roms, capacity, count, DS_CMD_SEARCH_ROM);
}

DS18B20_Status_t DS18B20_FindAlarms(DS18B20_Bus_t *bus,
                                    uint8_t (*roms)[DS18B20_ROM_SIZE],
                                    uint8_t capacity, uint8_t *count)
{
    return DS_Find(bus, roms, capacity, count, DS_CMD_ALARM_SEARCH);
}

DS18B20_Status_t DS18B20_Attach(DS18B20_Device_t *device,
                                DS18B20_Bus_t *bus,
                                const uint8_t rom[DS18B20_ROM_SIZE])
{
    DS18B20_Status_t result = DS_CheckBus(bus);
    DS18B20_Scratchpad_t scratchpad = {{0U}};
    DS18B20_PowerMode_t mode = DS18B20_POWER_EXTERNAL;
    if ((NULL == device) || (NULL == rom))
    {
        result = DS18B20_INVALID_ARGUMENT;
    }
    else if (DS18B20_FAMILY_CODE != rom[0])
    {
        result = DS18B20_INVALID_ARGUMENT;
    }
    else if (rom[7] != OneWire_CRC8(rom, 7UL))
    {
        result = DS18B20_CRC_ERROR;
    }
    if ((DS18B20_OK == result) && (0U != bus->converting))
    {
        result = DS18B20_BUSY;
    }
    if (DS18B20_OK == result)
    {
        (void)memset(device, 0, sizeof(*device));
        device->bus = bus;
        (void)memcpy(device->rom, rom, DS18B20_ROM_SIZE);
        device->attached = 1U;
        result = DS18B20_ReadScratchpad(device, &scratchpad);
        if (DS18B20_OK == result)
        {
            device->resolution = DS_ReadResolution(scratchpad.bytes[4]);
            result = DS18B20_GetPowerMode(device, &mode);
        }
        if (DS18B20_OK != result)
        {
            (void)memset(device, 0, sizeof(*device));
        }
    }
    return result;
}

DS18B20_Status_t DS18B20_ReadScratchpad(DS18B20_Device_t *device,
                                        DS18B20_Scratchpad_t *scratchpad)
{
    DS18B20_Status_t result = DS18B20_INVALID_ARGUMENT;
    OneWire_Status_t ow = ONEWIRE_OK;
    if (NULL != scratchpad)
    {
        result = DS_SendDeviceCommand(device, DS_CMD_READ_SCRATCHPAD);
        if (DS18B20_OK == result)
        {
            ow = OneWire_Read(&device->bus->oneWire, scratchpad->bytes,
                              DS18B20_SCRATCHPAD_SIZE);
            result = DS_MapOneWire(ow);
        }
        if ((DS18B20_OK == result) &&
            (scratchpad->bytes[8] != OneWire_CRC8(scratchpad->bytes, 8UL)))
        {
            result = DS18B20_CRC_ERROR;
        }
        if (DS18B20_OK == result)
        {
            device->resolution = DS_ReadResolution(scratchpad->bytes[4]);
        }
    }
    return result;
}

DS18B20_Status_t DS18B20_DecodeCelsius(
    const DS18B20_Scratchpad_t *scratchpad, float *celsius)
{
    DS18B20_Status_t result = DS18B20_INVALID_ARGUMENT;
    if ((NULL != scratchpad) && (NULL != celsius))
    {
        uint16_t bits = 0U;
        int32_t raw = 0L;
        DS18B20_Resolution_t resolution = 0U;
        result = DS18B20_CRC_ERROR;
        if (scratchpad->bytes[8] == OneWire_CRC8(scratchpad->bytes, 8UL))
        {
            resolution = DS_ReadResolution(scratchpad->bytes[4]);
            bits = (uint16_t)((uint16_t)scratchpad->bytes[0] |
                              ((uint16_t)scratchpad->bytes[1] << 8U));
            if (DS18B20_RESOLUTION_9_BIT == resolution)
            {
                bits &= (uint16_t)~0x0007U;
            }
            else if (DS18B20_RESOLUTION_10_BIT == resolution)
            {
                bits &= (uint16_t)~0x0003U;
            }
            else if (DS18B20_RESOLUTION_11_BIT == resolution)
            {
                bits &= (uint16_t)~0x0001U;
            }
            raw = ((bits & 0x8000U) != 0U) ? ((int32_t)bits - 65536L) : (int32_t)bits;
            *celsius = (float)raw / 16.0f;
            result = DS18B20_OK;
        }
    }
    return result;
}

DS18B20_Status_t DS18B20_ReadLastCelsius(DS18B20_Device_t *device,
                                         float *celsius)
{
    DS18B20_Status_t result = DS18B20_INVALID_ARGUMENT;
    DS18B20_Scratchpad_t scratchpad = {{0U}};
    if (NULL != celsius)
    {
        result = DS18B20_ReadScratchpad(device, &scratchpad);
        if (DS18B20_OK == result)
        {
            result = DS18B20_DecodeCelsius(&scratchpad, celsius);
        }
    }
    return result;
}

DS18B20_Status_t DS18B20_GetPowerMode(DS18B20_Device_t *device,
                                      DS18B20_PowerMode_t *mode)
{
    DS18B20_Status_t result = DS18B20_INVALID_ARGUMENT;
    uint8_t bit = 0U;
    if (NULL != mode)
    {
        result = DS_SendDeviceCommand(device, DS_CMD_READ_POWER_SUPPLY);
        if (DS18B20_OK == result)
        {
            result = DS_MapOneWire(OneWire_ReadBit(&device->bus->oneWire,
                                                   &bit));
        }
        if (DS18B20_OK == result)
        {
            *mode = (0U == bit) ? DS18B20_POWER_PARASITE : DS18B20_POWER_EXTERNAL;
            device->powerMode = *mode;
        }
    }
    return result;
}

DS18B20_Status_t DS18B20_GetResolution(DS18B20_Device_t *device,
                                       DS18B20_Resolution_t *resolution)
{
    DS18B20_Status_t result = DS18B20_INVALID_ARGUMENT;
    DS18B20_Scratchpad_t scratchpad = {{0U}};
    if (NULL != resolution)
    {
        result = DS18B20_ReadScratchpad(device, &scratchpad);
        if (DS18B20_OK == result)
        {
            *resolution = DS_ReadResolution(scratchpad.bytes[4]);
        }
    }
    return result;
}

DS18B20_Status_t DS18B20_GetAlarmThresholds(DS18B20_Device_t *device,
                                            int8_t *high, int8_t *low)
{
    DS18B20_Status_t result = DS18B20_INVALID_ARGUMENT;
    DS18B20_Scratchpad_t scratchpad = {{0U}};
    if ((NULL != high) && (NULL != low))
    {
        result = DS18B20_ReadScratchpad(device, &scratchpad);
        if (DS18B20_OK == result)
        {
            /* Explicit sign extension from 8-bit two's-complement bytes. */
            *high = (scratchpad.bytes[2] >= 128U) ? (int8_t)((int16_t)scratchpad.bytes[2] - 256) : (int8_t)scratchpad.bytes[2];
            *low = (scratchpad.bytes[3] >= 128U) ? (int8_t)((int16_t)scratchpad.bytes[3] - 256) : (int8_t)scratchpad.bytes[3];
        }
    }
    return result;
}

DS18B20_Status_t DS18B20_WriteConfiguration(
    DS18B20_Device_t *device, int8_t high, int8_t low,
    DS18B20_Resolution_t resolution)
{
    DS18B20_Status_t result = DS18B20_INVALID_ARGUMENT;
    DS18B20_Scratchpad_t verify = {{0U}};
    OneWire_Status_t ow = ONEWIRE_OK;
    if (true == DS_ValidResolution(resolution))
    {
        result = DS_SendDeviceCommand(device, DS_CMD_WRITE_SCRATCHPAD);
        if (DS18B20_OK == result)
        {
            ow = OneWire_WriteByte(&device->bus->oneWire, (uint8_t)high);
            if (ONEWIRE_OK == ow)
            {
                ow = OneWire_WriteByte(&device->bus->oneWire, (uint8_t)low);
            }
            if (ONEWIRE_OK == ow)
            {
                ow = OneWire_WriteByte(&device->bus->oneWire,
                                       DS_MakeConfig(resolution));
            }
            result = DS_MapOneWire(ow);
        }
        if (DS18B20_OK == result)
        {
            result = DS18B20_ReadScratchpad(device, &verify);
        }
        if ((DS18B20_OK == result) &&
            ((verify.bytes[2] != (uint8_t)high) ||
             (verify.bytes[3] != (uint8_t)low) ||
             (DS_ReadResolution(verify.bytes[4]) != resolution)))
        {
            result = DS18B20_VERIFY_ERROR;
        }
    }
    return result;
}

DS18B20_Status_t DS18B20_SetResolution(DS18B20_Device_t *device,
                                       DS18B20_Resolution_t resolution)
{
    DS18B20_Status_t result = DS18B20_INVALID_ARGUMENT;
    int8_t high = 0;
    int8_t low = 0;
    if (true == DS_ValidResolution(resolution))
    {
        result = DS18B20_GetAlarmThresholds(device, &high, &low);
        if (DS18B20_OK == result)
        {
            result = DS18B20_WriteConfiguration(device, high, low,
                                                resolution);
        }
    }
    return result;
}

DS18B20_Status_t DS18B20_SetAlarmThresholds(DS18B20_Device_t *device,
                                            int8_t high, int8_t low)
{
    DS18B20_Status_t result = DS18B20_INVALID_ARGUMENT;
    DS18B20_Resolution_t resolution = DS18B20_RESOLUTION_12_BIT;
    result = DS18B20_GetResolution(device, &resolution);
    if (DS18B20_OK == result)
    {
        result = DS18B20_WriteConfiguration(device, high, low, resolution);
    }
    return result;
}

DS18B20_Status_t DS18B20_CopyScratchpad(DS18B20_Device_t *device)
{
    DS18B20_Status_t result = DS18B20_INVALID_ARGUMENT;
    DS18B20_PowerMode_t mode = DS18B20_POWER_PARASITE;
    DS18B20_Scratchpad_t original = {{0U}};
    DS18B20_Scratchpad_t verify = {{0U}};
    result = DS18B20_GetPowerMode(device, &mode);
    if ((DS18B20_OK == result) && (DS18B20_POWER_EXTERNAL != mode))
    {
        result = DS18B20_UNSUPPORTED;
    }
    if (DS18B20_OK == result)
    {
        result = DS18B20_ReadScratchpad(device, &original);
    }
    if (DS18B20_OK == result)
    {
        result = DS_SendDeviceCommand(device, DS_CMD_COPY_SCRATCHPAD);
    }
    if (DS18B20_OK == result)
    {
        /* External VDD allows a normal pull-up while EEPROM is programmed. */
        delay(DS_EEPROM_COPY_WAIT_MS);
        result = DS18B20_RecallEEPROM(device);
    }
    if (DS18B20_OK == result)
    {
        result = DS18B20_ReadScratchpad(device, &verify);
    }
    if ((DS18B20_OK == result) &&
        (0 != memcmp(&original.bytes[2], &verify.bytes[2], 3U)))
    {
        result = DS18B20_VERIFY_ERROR;
    }
    return result;
}

DS18B20_Status_t DS18B20_RecallEEPROM(DS18B20_Device_t *device)
{
    DS18B20_Status_t result = DS_SendDeviceCommand(device,
                                                   DS_CMD_RECALL_E2);
    DS18B20_Scratchpad_t scratchpad = {{0U}};
    uint32_t startMs = millis();
    bool ready = false;
    uint8_t bit = 0U;
    while ((DS18B20_OK == result) && (false == ready) &&
           ((uint32_t)(millis() - startMs) <=
            DS_EEPROM_RECALL_TIMEOUT_MS))
    {
        result = DS_MapOneWire(OneWire_ReadBit(&device->bus->oneWire,
                                               &bit));
        ready = ((DS18B20_OK == result) && (1U == bit));
        if ((DS18B20_OK == result) && (false == ready))
        {
            delay(1U);
        }
    }
    if ((DS18B20_OK == result) && (false == ready))
    {
        result = DS18B20_TIMEOUT;
    }
    if (DS18B20_OK == result)
    {
        result = DS18B20_ReadScratchpad(device, &scratchpad);
    }
    return result;
}

DS18B20_Status_t DS18B20_StartConversion(DS18B20_Device_t *device)
{
    DS18B20_Status_t result = DS_CheckDevice(device);
    DS18B20_PowerMode_t mode = DS18B20_POWER_PARASITE;
    if (DS18B20_OK == result)
    {
        result = DS18B20_GetPowerMode(device, &mode);
    }
    if ((DS18B20_OK == result) && (DS18B20_POWER_EXTERNAL != mode))
    {
        result = DS18B20_UNSUPPORTED;
    }
    if (DS18B20_OK == result)
    {
        result = DS_SendDeviceCommand(device, DS_CMD_CONVERT_T);
    }
    if (DS18B20_OK == result)
    {
        DS18B20_Bus_t *bus = device->bus;
        bus->conversionStartMs = millis();
        bus->conversionDeadlineMs =
            DS_ConversionTime(device->resolution) + DS_CONVERSION_MARGIN_MS;
        (void)memcpy(bus->ownerRom, device->rom, DS18B20_ROM_SIZE);
        bus->converting = 1U;
    }
    return result;
}

DS18B20_Status_t DS18B20_ConversionReady(DS18B20_Device_t *device,
                                         bool *ready)
{
    DS18B20_Status_t result = DS18B20_INVALID_ARGUMENT;
    if ((NULL != device) && (NULL != ready))
    {
        DS18B20_Bus_t *bus = device->bus;
        *ready = false;
        result = DS18B20_NOT_INITIALIZED;
        if ((0U != device->attached) && (NULL != bus) &&
            (0U != bus->initialized))
        {
            result = DS18B20_INVALID_ARGUMENT;
            if ((0U != bus->converting) &&
                (0 == memcmp(bus->ownerRom, device->rom, DS18B20_ROM_SIZE)))
            {
                uint8_t bit = 0U;
                result = DS_MapOneWire(OneWire_ReadBit(&bus->oneWire, &bit));
                if ((DS18B20_OK == result) && (1U == bit))
                {
                    *ready = true;
                    bus->converting = 0U;
                }
                else if ((DS18B20_OK == result) &&
                         ((uint32_t)(millis() - bus->conversionStartMs) >
                          bus->conversionDeadlineMs))
                {
                    bus->converting = 0U;
                    result = DS18B20_TIMEOUT;
                }
            }
        }
    }
    return result;
}

DS18B20_Status_t DS18B20_AbortConversion(DS18B20_Device_t *device)
{
    DS18B20_Status_t result = DS18B20_INVALID_ARGUMENT;
    if ((NULL != device) && (NULL != device->bus) &&
        (0U != device->bus->initialized) && (0U != device->attached))
    {
        DS18B20_Bus_t *bus = device->bus;
        if ((0U == bus->converting) ||
            (0 != memcmp(bus->ownerRom, device->rom, DS18B20_ROM_SIZE)))
        {
            result = DS18B20_INVALID_ARGUMENT;
        }
        else
        {
            result = DS_MapOneWire(OneWire_Reset(&bus->oneWire));
            bus->converting = 0U;
        }
    }
    return result;
}

DS18B20_Status_t DS18B20_ReadCelsius(DS18B20_Device_t *device,
                                     float *celsius)
{
    DS18B20_Status_t result = DS18B20_INVALID_ARGUMENT;
    bool ready = false;
    if (NULL != celsius)
    {
        result = DS18B20_StartConversion(device);
    }
    while ((DS18B20_OK == result) && (false == ready))
    {
        result = DS18B20_ConversionReady(device, &ready);
        if ((DS18B20_OK == result) && (false == ready))
        {
            delay(1U);
        }
    }
    if (DS18B20_OK == result)
    {
        result = DS18B20_ReadLastCelsius(device, celsius);
    }
    return result;
}

bool DS18B20_begin(uint8_t pin)
{
    DS18B20_Status_t result = DS18B20_OK;
    uint8_t roms[1U][DS18B20_ROM_SIZE] = {{0U}};
    uint8_t count = 0U;
    DS18B20_end();
    result = DS18B20_BusBegin(&s_defaultBus, pin);
    if (DS18B20_OK == result)
    {
        result = DS18B20_FindDevices(&s_defaultBus, roms, 1U, &count);
        /* A full search may return CAPACITY_EXCEEDED with the first valid ROM. */
        if ((DS18B20_CAPACITY_EXCEEDED == result) && (1U == count))
        {
            result = DS18B20_OK;
        }
    }
    if ((DS18B20_OK == result) && (1U == count))
    {
        result = DS18B20_Attach(&s_defaultDevice, &s_defaultBus, roms[0]);
    }
    if (DS18B20_OK != result)
    {
        (void)DS18B20_BusEnd(&s_defaultBus);
        (void)memset(&s_defaultDevice, 0, sizeof(s_defaultDevice));
    }
    s_defaultStatus = result;
    return (DS18B20_OK == result);
}

void DS18B20_end(void)
{
    if (0U != s_defaultBus.initialized)
    {
        (void)DS18B20_BusEnd(&s_defaultBus);
    }
    (void)memset(&s_defaultDevice, 0, sizeof(s_defaultDevice));
    s_defaultStatus = DS18B20_NOT_INITIALIZED;
}

float DS18B20_readCelsius(void)
{
    float temperature = NAN;
    s_defaultStatus = DS18B20_ReadCelsius(&s_defaultDevice, &temperature);
    return temperature;
}

DS18B20_Status_t DS18B20_lastStatus(void)
{
    return s_defaultStatus;
}

const char *DS18B20_StatusString(DS18B20_Status_t status)
{
    const char *text = "UNKNOWN";
    switch (status)
    {
    case DS18B20_OK:
        text = "OK";
        break;
    case DS18B20_INVALID_ARGUMENT:
        text = "INVALID_ARGUMENT";
        break;
    case DS18B20_NOT_INITIALIZED:
        text = "NOT_INITIALIZED";
        break;
    case DS18B20_NO_DEVICE:
        text = "NO_DEVICE";
        break;
    case DS18B20_BUS_ERROR:
        text = "BUS_ERROR";
        break;
    case DS18B20_TIMER_ERROR:
        text = "TIMER_ERROR";
        break;
    case DS18B20_CRC_ERROR:
        text = "CRC_ERROR";
        break;
    case DS18B20_UNSUPPORTED:
        text = "UNSUPPORTED";
        break;
    case DS18B20_BUSY:
        text = "BUSY";
        break;
    case DS18B20_TIMEOUT:
        text = "TIMEOUT";
        break;
    case DS18B20_VERIFY_ERROR:
        text = "VERIFY_ERROR";
        break;
    case DS18B20_CAPACITY_EXCEEDED:
        text = "CAPACITY_EXCEEDED";
        break;
    case DS18B20_PROTOCOL_ERROR:
        text = "PROTOCOL_ERROR";
        break;
    default:
        break;
    }
    return text;
}
