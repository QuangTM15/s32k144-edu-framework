#ifndef ESC_H
#define ESC_H

/**
 * @file esc.h
 * @brief Arduino-style ESC device library for EduFramework.
 *
 * @details
 * This library provides a simple interface for controlling an Electronic
 * Speed Controller (ESC) using a 50 Hz RC PWM signal.
 *
 * The API is designed for educational and automotive/embedded applications
 * and hides the underlying FTM timer configuration from the application.
 *
 * The default throttle pulse range is:
 * - 1000 us at 0% throttle.
 * - 2000 us at 100% throttle.
 *
 * The pulse range can be changed with ESC_SetPulseRange().
 *
 * EduFramework currently supports one ESC instance through this API.
 */

#include "Arduino.h"

#include <stdbool.h>
#include <stdint.h>

/* ========================================================================= */
/* Public Constants                                                           */
/* ========================================================================= */

/**
 * @brief Default pulse width representing 0% throttle.
 */
#define ESC_DEFAULT_MIN_PULSE_US (1000U)

/**
 * @brief Default pulse width representing 100% throttle.
 */
#define ESC_DEFAULT_MAX_PULSE_US (2000U)

/* ========================================================================= */
/* Public API                                                                 */
/* ========================================================================= */

/**
 * @brief Initialize the ESC on a PWM-capable logical pin.
 *
 * @details
 * This function configures the selected logical pin and its associated
 * FTM channel to generate a 50 Hz RC PWM signal.
 *
 * The output is initially disabled until a valid ESC command is issued.
 *
 * The default pulse range is 1000 us to 2000 us.
 *
 * @param[in] pin Logical PWM-capable pin used for the ESC signal.
 *
 * @return Initialization state.
 *
 * @retval true ESC initialized successfully.
 * @retval false The selected pin does not support PWM or initialization failed.
 */
bool ESC_Init(uint8_t pin);

/**
 * @brief Stop the ESC output and deinitialize the ESC device.
 *
 * @details
 * This function disables the PWM output managed by the ESC library and
 * resets the internal ESC state.
 *
 * ESC_Init() must be called again before the ESC can be controlled.
 */
void ESC_End(void);

/**
 * @brief Check whether the ESC device is initialized.
 *
 * @return Initialization state.
 *
 * @retval true ESC is initialized.
 * @retval false ESC is not initialized.
 */
bool ESC_IsInitialized(void);

/**
 * @brief Configure the ESC throttle pulse range.
 *
 * @details
 * The minimum pulse represents 0% throttle and the maximum pulse represents
 * 100% throttle.
 *
 * Changing the pulse range does not immediately change the current output.
 * The new range is used by subsequent ESC_SetThrottle() and
 * ESC_SetMicroseconds() calls.
 *
 * @param[in] minUs Minimum throttle pulse width in microseconds.
 * @param[in] maxUs Maximum throttle pulse width in microseconds.
 *
 * @return Configuration state.
 *
 * @retval true Pulse range updated successfully.
 * @retval false ESC is not initialized or the supplied range is invalid.
 */
bool ESC_SetPulseRange(uint16_t minUs, uint16_t maxUs);

/**
 * @brief Arm the ESC using the configured minimum throttle pulse.
 *
 * @details
 * This function sends the configured minimum throttle pulse and holds it
 * for the EduFramework arming period.
 *
 * The function is blocking during the arming period.
 *
 * The exact arming behavior required by an ESC may vary between devices.
 */
void ESC_Arm(void);

/**
 * @brief Set the commanded ESC throttle.
 *
 * @details
 * The throttle percentage is mapped linearly between the configured minimum
 * and maximum pulse widths.
 *
 * A value of 0% produces the configured minimum pulse.
 * A value of 100% produces the configured maximum pulse.
 *
 * Values greater than 100% are limited to 100%.
 *
 * @param[in] percent Commanded throttle percentage.
 */
void ESC_SetThrottle(uint8_t percent);

/**
 * @brief Get the most recently commanded throttle percentage.
 *
 * @details
 * This function returns the last throttle command stored by the library.
 * It does not measure the actual motor speed.
 *
 * @return Last commanded throttle percentage from 0U to 100U.
 */
uint8_t ESC_GetThrottle(void);

/**
 * @brief Set the ESC pulse width directly.
 *
 * @details
 * This advanced API directly controls the commanded RC PWM pulse width.
 *
 * Values below the configured minimum pulse are limited to the minimum.
 * Values above the configured maximum pulse are limited to the maximum.
 *
 * Calling this function also updates the stored throttle value according
 * to the configured pulse range.
 *
 * @param[in] us Requested pulse width in microseconds.
 */
void ESC_SetMicroseconds(uint16_t us);

/**
 * @brief Get the most recently commanded ESC pulse width.
 *
 * @details
 * This function returns the pulse width most recently commanded by the
 * library. It does not measure the physical ESC signal.
 *
 * @return Last commanded pulse width in microseconds.
 */
uint16_t ESC_GetMicroseconds(void);

#endif /* ESC_H */