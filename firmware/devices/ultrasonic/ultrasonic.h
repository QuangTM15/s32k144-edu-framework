#ifndef ULTRASONIC_H
#define ULTRASONIC_H

/**
 * @file ultrasonic.h
 * @brief Arduino-style HC-SR04 ultrasonic sensor library for EduFramework.
 *
 * @details
 * This module provides a beginner-friendly device API for the HC-SR04
 * ultrasonic distance sensor.
 *
 * The ultrasonic device library belongs to the Device Layer:
 *
 * Application / Example
 *        |
 *        v
 * Device Library
 *        |
 *        v
 * Arduino-style API
 *        |
 *        v
 * Low-level Drivers
 *        |
 *        v
 * Hardware
 *
 * This module does not access MCU registers directly. It uses only
 * Arduino-style APIs such as pinMode(), digitalWrite(), digitalRead(),
 * delay(), delayMicroseconds(), and micros().
 */

#include <stdint.h>

/* ========================================================================= */
/* Public Constants                                                           */
/* ========================================================================= */

/**
 * @brief Default echo timeout in microseconds.
 *
 * @details
 * The timeout prevents the program from blocking forever when:
 * - The sensor is disconnected.
 * - No object is detected.
 * - The ECHO pin never becomes HIGH.
 * - The ECHO pin never returns LOW.
 */
#define ULTRASONIC_DEFAULT_TIMEOUT_US (30000U)

/**
 * @brief Default number of samples used by ultrasonicReadFiltered().
 *
 * @details
 * The default filtered read collects 20 samples, sorts them, removes noisy
 * low-end and high-end samples, then averages the middle samples.
 */
#define ULTRASONIC_DEFAULT_FILTER_SAMPLES (20U)

/**
 * @brief Invalid distance value in centimeters.
 *
 * @details
 * Distance read functions return this value when a valid echo pulse cannot
 * be measured.
 */
#define ULTRASONIC_INVALID_DISTANCE_CM (-1.0f)

/* ========================================================================= */
/* Public Types                                                               */
/* ========================================================================= */

/**
 * @brief HC-SR04 ultrasonic sensor object.
 *
 * @details
 * This object stores the logical TRIG pin, logical ECHO pin, and timeout
 * value for one ultrasonic sensor instance.
 *
 * The pins are EduFramework logical pins, not raw MCU port/pin numbers.
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
     * @brief Echo measurement timeout in microseconds.
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
 * This API is intended for simple applications using one HC-SR04 sensor.
 * It configures the TRIG pin as output and the ECHO pin as input.
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
 * @brief Set timeout for the default ultrasonic sensor.
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
 * @details
 * This function sends a trigger pulse and measures how long the ECHO pin
 * stays HIGH.
 *
 * @return uint32_t
 * Echo pulse duration in microseconds.
 *
 * @retval 0U
 * Timeout or invalid measurement.
 */
uint32_t ultrasonicReadDuration(void);

/**
 * @brief Read distance in centimeters from the default ultrasonic sensor.
 *
 * @details
 * This is the main beginner-friendly read API. It returns distance in
 * centimeters by default.
 *
 * @return float
 * Distance in centimeters.
 *
 * @retval ULTRASONIC_INVALID_DISTANCE_CM
 * Timeout or invalid measurement.
 */
float ultrasonicRead(void);

/**
 * @brief Read distance in inches from the default ultrasonic sensor.
 *
 * @return float
 * Distance in inches.
 *
 * @retval ULTRASONIC_INVALID_DISTANCE_CM
 * Timeout or invalid measurement.
 */
float ultrasonicReadInch(void);

/**
 * @brief Read filtered distance in centimeters from the default sensor.
 *
 * @details
 * This function uses an internal static sample buffer. It is simple to use,
 * but it is intended for one default sensor only.
 *
 * For multi-sensor applications, use Ultrasonic_ReadCmFiltered() with a
 * user-provided buffer.
 *
 * @return float
 * Filtered distance in centimeters.
 *
 * @retval ULTRASONIC_INVALID_DISTANCE_CM
 * Timeout or invalid measurement.
 */
float ultrasonicReadFiltered(void);

/* ========================================================================= */
/* Multi-instance API                                                         */
/* ========================================================================= */

/**
 * @brief Initialize an ultrasonic sensor instance.
 *
 * @details
 * This API is used when an application needs more than one HC-SR04 sensor.
 *
 * @param[in,out] sensor
 * Pointer to an ultrasonic sensor object.
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
 * Pointer to an ultrasonic sensor object.
 *
 * @param[in] timeoutUs
 * Timeout value in microseconds.
 *
 * @return None.
 */
void Ultrasonic_SetTimeout(Ultrasonic_t *sensor,
                           uint32_t timeoutUs);

/**
 * @brief Read echo pulse duration from an ultrasonic sensor instance.
 *
 * @param[in] sensor
 * Pointer to an ultrasonic sensor object.
 *
 * @return uint32_t
 * Echo pulse duration in microseconds.
 *
 * @retval 0U
 * Timeout, invalid sensor pointer, or invalid measurement.
 */
uint32_t Ultrasonic_ReadDurationUs(Ultrasonic_t *sensor);

/**
 * @brief Read distance in centimeters from an ultrasonic sensor instance.
 *
 * @param[in] sensor
 * Pointer to an ultrasonic sensor object.
 *
 * @return float
 * Distance in centimeters.
 *
 * @retval ULTRASONIC_INVALID_DISTANCE_CM
 * Timeout, invalid sensor pointer, or invalid measurement.
 */
float Ultrasonic_ReadCm(Ultrasonic_t *sensor);

/**
 * @brief Read distance in inches from an ultrasonic sensor instance.
 *
 * @param[in] sensor
 * Pointer to an ultrasonic sensor object.
 *
 * @return float
 * Distance in inches.
 *
 * @retval ULTRASONIC_INVALID_DISTANCE_CM
 * Timeout, invalid sensor pointer, or invalid measurement.
 */
float Ultrasonic_ReadInch(Ultrasonic_t *sensor);

/**
 * @brief Read filtered distance in centimeters from an ultrasonic sensor.
 *
 * @details
 * This function collects multiple samples into the user-provided buffer,
 * sorts the samples, removes noisy low-end and high-end samples, and returns
 * the average of the middle samples.
 *
 * The buffer must have at least sampleCount elements.
 *
 * @param[in] sensor
 * Pointer to an ultrasonic sensor object.
 *
 * @param[in,out] buffer
 * Sample buffer provided by the caller.
 *
 * @param[in] sampleCount
 * Number of samples to collect.
 *
 * @return float
 * Filtered distance in centimeters.
 *
 * @retval ULTRASONIC_INVALID_DISTANCE_CM
 * Invalid input, timeout, or no valid samples.
 */
float Ultrasonic_ReadCmFiltered(Ultrasonic_t *sensor,
                                float *buffer,
                                uint8_t sampleCount);

#endif /* ULTRASONIC_H */