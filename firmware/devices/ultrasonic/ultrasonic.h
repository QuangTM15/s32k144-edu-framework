#ifndef ULTRASONIC_H
#define ULTRASONIC_H

/**
 * @file ultrasonic.h
 * @brief Arduino-style HC-SR04 ultrasonic sensor library for EduFramework.
 *
 * @details
 * This module provides a beginner-friendly API for the HC-SR04 ultrasonic
 * distance sensor.
 *
 * The device library belongs to the Device Layer:
 *
 * Application / Example
 *        ↓
 * Device Library
 *        ↓
 * Arduino-style API
 *        ↓
 * Low-level Drivers
 *        ↓
 * Hardware
 *
 * This module does not access MCU registers directly. It uses only
 * Arduino-style APIs such as pinMode(), digitalWrite(), digitalRead(),
 * delay(), and delayMicroseconds().
 *
 * Two API styles are provided:
 *
 * 1. Arduino-like simple API:
 *    - ultrasonicBegin()
 *    - ultrasonicRead()
 *    - ultrasonicReadFiltered()
 *
 * 2. Multi-instance API:
 *    - Ultrasonic_Begin()
 *    - Ultrasonic_ReadCm()
 *    - Ultrasonic_ReadCmFiltered()
 *
 * The simple API is recommended for beginner examples using one sensor.
 * The multi-instance API is used when more than one ultrasonic sensor is
 * required.
 */

#include <stdint.h>

/* ========================================================================= */
/* Public Constants                                                           */
/* ========================================================================= */

/**
 * @brief Default echo timeout in microseconds.
 *
 * @details
 * A timeout is required because the ECHO pin may never become HIGH or may
 * never return LOW if the sensor is disconnected or no echo is received.
 *
 * 30000 us is commonly used for HC-SR04 examples and is suitable for
 * educational distance measurement.
 */
#define ULTRASONIC_DEFAULT_TIMEOUT_US       (30000U)

/**
 * @brief Default number of samples used by ultrasonicReadFiltered().
 */
#define ULTRASONIC_DEFAULT_FILTER_SAMPLES   (20U)

/**
 * @brief Invalid distance value in centimeters.
 *
 * @details
 * Distance read APIs return this value when:
 * - Sensor pointer is invalid.
 * - Echo pulse is not detected.
 * - Echo pulse measurement times out.
 */
#define ULTRASONIC_INVALID_DISTANCE_CM      (-1.0f)

/* ========================================================================= */
/* Public Types                                                               */
/* ========================================================================= */

/**
 * @brief HC-SR04 ultrasonic sensor object.
 *
 * @details
 * This object stores all configuration data required by one ultrasonic
 * sensor instance.
 *
 * The pins are logical EduFramework/Arduino-style pins, not raw MCU pins.
 */
typedef struct
{
    /**
     * @brief Logical pin connected to the HC-SR04 TRIG pin.
     */
    uint8_t trigPin;

    /**
     * @brief Logical pin connected to the HC-SR04 ECHO pin.
     */
    uint8_t echoPin;

    /**
     * @brief Echo wait timeout in microseconds.
     */
    uint32_t timeoutUs;

} Ultrasonic_t;

/* ========================================================================= */
/* Arduino-like Simple API                                                    */
/* ========================================================================= */

/**
 * @brief Initialize the default ultrasonic sensor.
 *
 * @details
 * This function is intentionally similar to Arduino-style device APIs.
 * It is intended for simple examples using one HC-SR04 sensor.
 *
 * @param[in] trigPin
 * Logical pin connected to TRIG.
 *
 * @param[in] echoPin
 * Logical pin connected to ECHO.
 *
 * @return None.
 */
void ultrasonicBegin(uint8_t trigPin,
                     uint8_t echoPin);

/**
 * @brief Set echo timeout for the default ultrasonic sensor.
 *
 * @param[in] timeoutUs
 * Timeout value in microseconds.
 *
 * @return None.
 */
void ultrasonicSetTimeout(uint32_t timeoutUs);

/**
 * @brief Read echo pulse duration from the default ultrasonic sensor.
 *
 * @return uint32_t
 * Echo pulse duration in microseconds.
 *
 * @retval 0U
 * Timeout or invalid measurement.
 */
uint32_t ultrasonicReadDuration(void);

/**
 * @brief Read distance from the default ultrasonic sensor.
 *
 * @details
 * This function returns distance in centimeters by default.
 *
 * @return float
 * Distance in centimeters, or ULTRASONIC_INVALID_DISTANCE_CM on timeout.
 */
float ultrasonicRead(void);

/**
 * @brief Read distance in inches from the default ultrasonic sensor.
 *
 * @return float
 * Distance in inches, or ULTRASONIC_INVALID_DISTANCE_CM on timeout.
 */
float ultrasonicReadInch(void);

/**
 * @brief Read filtered distance from the default ultrasonic sensor.
 *
 * @details
 * This function collects multiple samples, sorts them, removes the smallest
 * and largest noisy groups, then averages the middle samples.
 *
 * @return float
 * Filtered distance in centimeters, or ULTRASONIC_INVALID_DISTANCE_CM.
 */
float ultrasonicReadFiltered(void);

/* ========================================================================= */
/* Multi-instance API                                                         */
/* ========================================================================= */

/**
 * @brief Initialize an ultrasonic sensor instance.
 *
 * @param[in,out] sensor
 * Pointer to ultrasonic sensor object.
 *
 * @param[in] trigPin
 * Logical pin connected to TRIG.
 *
 * @param[in] echoPin
 * Logical pin connected to ECHO.
 *
 * @return None.
 */
void Ultrasonic_Begin(Ultrasonic_t *sensor,
                      uint8_t trigPin,
                      uint8_t echoPin);

/**
 * @brief Set timeout for an ultrasonic sensor instance.
 *
 * @param[in,out] sensor
 * Pointer to ultrasonic sensor object.
 *
 * @param[in] timeoutUs
 * Timeout value in microseconds.
 *
 * @return None.
 */
void Ultrasonic_SetTimeout(Ultrasonic_t *sensor,
                           uint32_t timeoutUs);

/**
 * @brief Read echo pulse duration in microseconds.
 *
 * @param[in] sensor
 * Pointer to ultrasonic sensor object.
 *
 * @return uint32_t
 * Echo pulse duration in microseconds.
 *
 * @retval 0U
 * Timeout or invalid measurement.
 */
uint32_t Ultrasonic_ReadDurationUs(Ultrasonic_t *sensor);

/**
 * @brief Read distance in centimeters.
 *
 * @param[in] sensor
 * Pointer to ultrasonic sensor object.
 *
 * @return float
 * Distance in centimeters, or ULTRASONIC_INVALID_DISTANCE_CM.
 */
float Ultrasonic_ReadCm(Ultrasonic_t *sensor);

/**
 * @brief Read distance in inches.
 *
 * @param[in] sensor
 * Pointer to ultrasonic sensor object.
 *
 * @return float
 * Distance in inches, or ULTRASONIC_INVALID_DISTANCE_CM.
 */
float Ultrasonic_ReadInch(Ultrasonic_t *sensor);

/**
 * @brief Read filtered distance in centimeters.
 *
 * @param[in] sensor
 * Pointer to ultrasonic sensor object.
 *
 * @param[in,out] buffer
 * Buffer used to store distance samples.
 *
 * @param[in] sampleCount
 * Number of samples to collect.
 *
 * @return float
 * Filtered distance in centimeters, or ULTRASONIC_INVALID_DISTANCE_CM.
 */
float Ultrasonic_ReadCmFiltered(Ultrasonic_t *sensor,
                                float *buffer,
                                uint8_t sampleCount);

#endif /* ULTRASONIC_H */