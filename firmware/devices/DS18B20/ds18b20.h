
/**
 * @file ds18b20.h
 * @brief Simple DS18B20 temperature sensor API.
 *
 * @note One externally powered DS18B20 is supported.
 * @note V1 supports GPIO4 (PTD12) on MaaZEDU.
 * @note readCelsius() is blocking.
 */

#ifndef DS18B20_H
#define DS18B20_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Initialize the DS18B20 sensor.
     *
     * @param pin Logical board pin (GPIO4 in V1).
     * @return true if a valid DS18B20 is detected.
     */
    bool DS18B20_begin(uint8_t pin);

    /**
     * @brief Measure temperature in degrees Celsius.
     *
     * Performs temperature conversion, waits for completion,
     * reads the scratchpad and verifies CRC.
     *
     * @return Temperature in Celsius, or NAN on error.
     */
    float DS18B20_readCelsius(void);

#ifdef __cplusplus
}
#endif
#endif /* DS18B20_H */
