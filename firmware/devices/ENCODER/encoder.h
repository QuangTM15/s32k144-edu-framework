#ifndef ENCODER_H
#define ENCODER_H

/**
 * @file encoder.h
 * @brief Quadrature encoder device interface for EduFramework.
 *
 * @details
 * This module provides interrupt-based quadrature encoder measurement
 * using two logical EduFramework GPIO pins.
 *
 * Both encoder channels are configured for either-edge interrupts.
 * Therefore, the decoder operates in quadrature x4 mode.
 *
 * The module provides:
 *
 * - Position/count measurement.
 * - Rotation direction detection.
 * - Count reset.
 * - RPM measurement.
 *
 * The encoder pins are selected by the application and must provide
 * PIN_CAP_INTERRUPT capability.
 *
 * The current implementation supports one encoder instance.
 */

#include <stdint.h>

/* ========================================================================= */
/* Direction Definitions                                                     */
/* ========================================================================= */

/**
 * @brief Encoder direction type.
 */
typedef int8_t encoder_direction_t;

/**
 * @brief Encoder is not currently moving.
 */
#define ENCODER_DIRECTION_STOPPED ((encoder_direction_t)0)

/**
 * @brief Encoder is rotating in the forward direction.
 */
#define ENCODER_DIRECTION_FORWARD ((encoder_direction_t)1)

/**
 * @brief Encoder is rotating in the reverse direction.
 */
#define ENCODER_DIRECTION_REVERSE ((encoder_direction_t) - 1)

/* ========================================================================= */
/* Public API                                                                */
/* ========================================================================= */

/**
 * @brief Initialize a quadrature encoder.
 *
 * @details
 * Both channels are configured as GPIO inputs with either-edge interrupts.
 * The encoder uses quadrature x4 decoding.
 *
 * The countsPerRevolution parameter must represent the number of decoder
 * counts generated during one complete mechanical revolution.
 *
 * For example, if an encoder produces 30 cycles per revolution and x4
 * quadrature decoding is used:
 *
 * @code
 * Encoder_Init(GPIO8, GPIO9, 120U);
 * @endcode
 *
 * Both selected logical pins must support:
 *
 * - Digital input.
 * - External interrupt.
 *
 * @param[in] channelAPin
 * Logical EduFramework pin connected to encoder channel A.
 *
 * @param[in] channelBPin
 * Logical EduFramework pin connected to encoder channel B.
 *
 * @param[in] countsPerRevolution
 * Number of decoded counts per complete mechanical revolution.
 *
 * @return uint8_t
 * @retval 1U Encoder initialized successfully.
 * @retval 0U Invalid pin, unsupported pin combination, or invalid CPR.
 */
uint8_t Encoder_Init(uint8_t channelAPin, uint8_t channelBPin, uint16_t countsPerRevolution);

/**
 * @brief Get the current accumulated encoder count.
 *
 * @details
 * Forward movement increments the count while reverse movement decrements it.
 *
 * @return Current signed encoder count.
 */
int32_t Encoder_GetCount(void);

/**
 * @brief Reset the accumulated encoder count to zero.
 *
 * @return None.
 */
void Encoder_Reset(void);

/**
 * @brief Get the most recently detected encoder direction.
 *
 * @return encoder_direction_t
 * @retval ENCODER_DIRECTION_FORWARD Forward rotation.
 * @retval ENCODER_DIRECTION_REVERSE Reverse rotation.
 * @retval ENCODER_DIRECTION_STOPPED No movement during the latest RPM window.
 */
encoder_direction_t Encoder_GetDirection(void);

/**
 * @brief Update the encoder RPM measurement.
 *
 * @details
 * This function uses the current encoder count and the EduFramework
 * millisecond time base to calculate rotational speed.
 *
 * It is non-blocking and should be called repeatedly from the application
 * main loop. A new RPM value is calculated only when the internal measurement
 * interval has elapsed.
 *
 * @return None.
 */
void Encoder_Update(void);

/**
 * @brief Get the latest calculated encoder speed.
 *
 * @details
 * RPM is returned as a positive magnitude. Rotation direction is obtained
 * separately through Encoder_GetDirection().
 *
 * @return Latest encoder speed in revolutions per minute.
 */
float Encoder_GetRpm(void);

#endif /* ENCODER_H */