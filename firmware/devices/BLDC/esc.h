#ifndef ESC_H
#define ESC_H

/**
 * @file esc.h
 * @brief Arduino-style ESC (Electronic Speed Controller) device library.
 *
 * @details
 * This library generates a standard 50 Hz RC PWM signal for controlling
 * Brushless DC (BLDC) motor ESCs.
 *
 * The default throttle pulse range is 1000 us to 2000 us.
 */

#include "Arduino.h"
#include <stdbool.h>
#include <stdint.h>

/* ========================================================================= */
/* Configuration Constants                                                   */
/* ========================================================================= */

#define ESC_DEFAULT_MIN_PULSE_US (1000U)
#define ESC_DEFAULT_MAX_PULSE_US (2000U)

/* ========================================================================= */
/* Arduino-style API Prototypes                                              */
/* ========================================================================= */

/**
 * @brief Initialize the ESC on a PWM-capable logical pin.
 *
 * @details
 * Configures the selected pin and its associated FTM channel to generate
 * a 50 Hz RC PWM signal.
 *
 * The ESC device context is managed internally by the library.
 *
 * @param[in] pin Logical pin used for the ESC signal.
 *
 * @return true if initialization is successful.
 * @return false if the selected pin does not support PWM.
 */
bool ESC_Init(uint8_t pin);

/**
 * @brief Set the ESC throttle pulse range.
 *
 * @details
 * The minimum pulse represents 0% throttle and the maximum pulse represents
 * 100% throttle.
 *
 * @param[in] minUs Minimum throttle pulse width in microseconds.
 * @param[in] maxUs Maximum throttle pulse width in microseconds.
 *
 * @return true if the pulse range is valid.
 * @return false if the ESC is not initialized or the range is invalid.
 */
bool ESC_SetPulseRange(uint16_t minUs, uint16_t maxUs);

/**
 * @brief Arm the ESC.
 *
 * @details
 * Sends the configured minimum throttle pulse for 3 seconds.
 * This function is blocking during the arming period.
 */
void ESC_Arm(void);

/**
 * @brief Set the ESC throttle percentage.
 *
 * @details
 * The throttle value is mapped from the configured minimum pulse at 0%
 * to the configured maximum pulse at 100%.
 *
 * Values greater than 100% are limited to 100%.
 *
 * @param[in] percent Throttle value from 0U to 100U.
 */
void ESC_SetThrottle(uint8_t percent);

/**
 * @brief Send a raw pulse width to the ESC.
 *
 * @details
 * This is an advanced API that directly converts the requested pulse width
 * into FTM timer counts.
 *
 * @param[in] us Pulse width in microseconds.
 */
void ESC_SetMicroseconds(uint16_t us);

#endif /* ESC_H */