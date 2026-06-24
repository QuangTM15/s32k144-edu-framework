#ifndef WIRING_DIGITAL_H
#define WIRING_DIGITAL_H

/**
 * @file wiring_digital.h
 * @brief Arduino-style digital I/O API for EduFramework.
 *
 * @details
 * This file exposes beginner-friendly digital I/O functions similar to the
 * Arduino programming model.
 *
 * User code should call these APIs instead of directly calling GPIO or PORT
 * drivers when writing normal examples and educational applications.
 *
 * This layer translates logical Arduino pin names into low-level PORT/GPIO
 * driver calls through the Arduino pin mapping module.
 */

#include <stdint.h>
#include <stdbool.h>

/* ========================================================================= */
/* Public API Prototypes                                                      */
/* ========================================================================= */

/**
 * @brief Configure a logical Arduino pin as input or output.
 *
 * @details
 * This function configures a logical pin for digital I/O usage.
 *
 * It performs the following operations:
 *
 * 1. Validate logical pin number.
 * 2. Check whether the pin supports digital capability.
 * 3. Enable the corresponding PORT clock.
 * 4. Configure PORT mux to GPIO mode.
 * 5. Configure pull resistor based on selected mode.
 * 6. Configure GPIO pin direction.
 * 7. Enable passive filter automatically for on-board button pins.
 *
 * Supported modes:
 *
 * - INPUT
 * - OUTPUT
 * - INPUT_PULLUP
 * - INPUT_PULLDOWN
 *
 * @param[in] pin
 * Logical Arduino pin number.
 *
 * @param[in] mode
 * Digital pin mode.
 *
 * @return None.
 */
void pinMode(uint8_t pin,
             uint8_t mode);

/**
 * @brief Write a digital logic level to a logical Arduino pin.
 *
 * @details
 * This function writes HIGH or LOW to a logical pin configured as a digital
 * output. The actual register write is performed by the GPIO driver.
 *
 * @param[in] pin
 * Logical Arduino pin number.
 *
 * @param[in] value
 * Digital logic level. Use HIGH or LOW.
 *
 * @return None.
 */
void digitalWrite(uint8_t pin,
                  uint8_t value);

/**
 * @brief Read a digital logic level from a logical Arduino pin.
 *
 * @details
 * This function reads the current input level of a logical digital pin.
 *
 * If the pin is invalid or does not support digital capability, the function
 * returns false as LOW-equivalent value because the Arduino-style API does
 * not expose an error code.
 *
 * @param[in] pin
 * Logical Arduino pin number.
 *
 * @return bool
 * @retval true   Pin level is HIGH.
 * @retval false  Pin level is LOW, pin is invalid, or pin is not digital-capable.
 */
bool digitalRead(uint8_t pin);

/**
 * @brief Toggle a logical Arduino digital output pin.
 *
 * @details
 * This function toggles the current output state of a logical digital pin.
 * The actual register write is performed by the GPIO driver.
 *
 * @param[in] pin
 * Logical Arduino pin number.
 *
 * @return None.
 */
void digitalToggle(uint8_t pin);

#endif /* WIRING_DIGITAL_H */