#ifndef WIRING_ANALOG_H
#define WIRING_ANALOG_H

/**
 * @file wiring_analog.h
 * @brief Arduino-style analog API public interface.
 *
 * @details
 * This file declares Arduino-style analog input and PWM output APIs for
 * EduFramework.
 *
 * The analog input path uses the ADC driver:
 * - analogInit()
 * - analogRead()
 * - analogStart()
 * - analogAvailable()
 * - analogGetResult()
 * - analogReadMilliVolts()
 *
 * The PWM output path uses the FTM driver:
 * - analogWrite()
 *
 * The current MaaZEDU analog input support is intentionally limited to
 * ADC0_SE12 and ADC0_SE13 because these are the analog-capable pins
 * exposed for the current educational board use case.
 */

#include <stdint.h>

/**
 * @brief Initialize the Arduino-style analog subsystem.
 *
 * @details
 * This function initializes the default ADC instance used by analogRead()
 * and the non-blocking analog APIs.
 *
 * The default configuration uses:
 * - ADC0.
 * - 12-bit conversion.
 * - Default voltage reference.
 * - Interrupt-based conversion completion.
 *
 * This function is called automatically by analogRead() and analogStart()
 * if the analog subsystem has not been initialized yet.
 *
 * @return None.
 */
void analogInit(void);

/**
 * @brief Read an analog input pin using a blocking conversion.
 *
 * @details
 * This function starts one ADC conversion and waits until the conversion
 * result is available.
 *
 * Supported analog input pins are currently limited to ADC0_SE12 and
 * ADC0_SE13.
 *
 * @param[in] pin
 * Arduino-style analog pin identifier.
 *
 * @return int
 *
 * @retval >=0
 * Raw ADC conversion result.
 *
 * @retval -1
 * Invalid pin, initialization failure, or conversion failure.
 */
int analogRead(uint8_t pin);

/**
 * @brief Start one non-blocking analog conversion.
 *
 * @details
 * This function starts one ADC conversion and returns immediately.
 * The application can later check completion using analogAvailable()
 * and read the result using analogGetResult().
 *
 * If another conversion is already active, this function does not start
 * a new conversion.
 *
 * @param[in] pin
 * Arduino-style analog pin identifier.
 *
 * @return None.
 */
void analogStart(uint8_t pin);

/**
 * @brief Check whether a non-blocking analog conversion result is ready.
 *
 * @details
 * This function checks whether the conversion started by analogStart()
 * has completed.
 *
 * @return uint8_t
 *
 * @retval 1U
 * Conversion result is ready.
 *
 * @retval 0U
 * No conversion is active or result is not ready.
 */
uint8_t analogAvailable(void);

/**
 * @brief Get the latest non-blocking analog conversion result.
 *
 * @details
 * This function reads the result of a conversion previously started by
 * analogStart().
 *
 * After a successful read, the current conversion-active state is cleared.
 *
 * @return int
 *
 * @retval >=0
 * Raw ADC conversion result.
 *
 * @retval -1
 * No result is available or the analog subsystem is not initialized.
 */
int analogGetResult(void);

/**
 * @brief Read an analog input and convert the result to millivolts.
 *
 * @details
 * This function performs a blocking analog read and scales the raw ADC
 * result using the default analog reference voltage configured in
 * wiring_analog.c.
 *
 * @param[in] pin
 * Arduino-style analog pin identifier.
 *
 * @return int
 *
 * @retval >=0
 * Converted voltage in millivolts.
 *
 * @retval -1
 * Invalid pin, initialization failure, or conversion failure.
 */
int analogReadMilliVolts(uint8_t pin);

/**
 * @brief Write PWM duty value to a PWM-capable pin.
 *
 * @details
 * This function provides Arduino-style PWM output. The input value is
 * scaled from 0..255 to a 0..100 percent PWM duty cycle.
 *
 * The underlying FTM instance and channel are selected from the Arduino
 * pin mapping table.
 *
 * @param[in] pin
 * Arduino-style pin identifier.
 *
 * @param[in] value
 * PWM value from 0U to 255U.
 *
 * @return None.
 */
void analogWrite(uint8_t pin, uint8_t value);

#endif /* WIRING_ANALOG_H */