#ifndef WIRING_PWM_H
#define WIRING_PWM_H

/**
 * @file wiring_pwm.h
 * @brief Internal Arduino-layer PWM service interface.
 *
 * @details
 * This file declares the shared PWM service used by Arduino-style APIs
 * that require FTM PWM resources.
 *
 * The module is responsible for:
 * - Resolving Arduino logical pins to PWM hardware resources.
 * - Configuring pin mux for FTM output.
 * - Lazily initializing FTM instances.
 * - Configuring FTM PWM channels.
 * - Starting FTM counters.
 * - Updating PWM duty cycle.
 *
 * This module is internal to the Arduino-style API layer and is not
 * intended to be included directly by user applications.
 *
 * Hardware-specific PWM generation remains implemented by the low-level
 * FTM driver.
 */

#include <stdint.h>

/**
 * @brief Wiring PWM service status type.
 *
 * @details
 * A fixed-width integer type is used instead of enum so that storage size
 * remains explicit and consistent with the EduFramework coding rules.
 */
typedef uint8_t WiringPwm_Status_t;

/**
 * @brief PWM operation completed successfully.
 */
#define WIRING_PWM_STATUS_OK ((WiringPwm_Status_t)0U)

/**
 * @brief Invalid pin, PWM capability, or input argument.
 */
#define WIRING_PWM_STATUS_INVALID_ARGUMENT ((WiringPwm_Status_t)1U)

/**
 * @brief Underlying PORT or FTM configuration failed.
 */
#define WIRING_PWM_STATUS_DRIVER_ERROR ((WiringPwm_Status_t)2U)

/**
 * @brief Set PWM duty cycle for an Arduino-style logical pin.
 *
 * @details
 * This function resolves the logical pin to its FTM instance and channel,
 * performs one-time PWM hardware configuration when needed, and updates
 * the channel duty cycle.
 *
 * Each FTM instance is initialized lazily using the default Arduino PWM
 * configuration:
 * - 8 MHz source clock.
 * - 1 kHz PWM frequency.
 * - External FTM clock source selection.
 * - Prescaler divide-by-1.
 *
 * Each PWM channel is configured for edge-aligned low-true PWM to preserve
 * the existing analogWrite() behavior.
 *
 * @param[in] pin
 * Arduino-style logical pin identifier.
 *
 * @param[in] dutyPercent
 * PWM duty percentage from 0U to 100U.
 *
 * @return WiringPwm_Status_t
 *
 * @retval WIRING_PWM_STATUS_OK
 * Duty cycle updated successfully.
 *
 * @retval WIRING_PWM_STATUS_INVALID_ARGUMENT
 * Pin is invalid, pin does not support PWM, mapping is invalid, or duty
 * percentage is outside the supported range.
 *
 * @retval WIRING_PWM_STATUS_DRIVER_ERROR
 * PORT or FTM hardware configuration failed.
 */
WiringPwm_Status_t WiringPwm_SetDutyPercent(uint8_t pin,
                                            uint8_t dutyPercent);

/**
 * @brief Start PWM output with a specified frequency and duty cycle.
 *
 * @details
 * This function configures the selected Arduino-style PWM pin and updates
 * the frequency of its mapped FTM instance.
 *
 * The PWM frequency is shared by all channels belonging to the same FTM
 * instance. This function is intended for internal Arduino-layer services
 * such as tone().
 *
 * @param[in] pin
 * Arduino-style logical pin identifier.
 *
 * @param[in] frequencyHz
 * Requested PWM frequency in Hz.
 *
 * @param[in] dutyPercent
 * PWM duty percentage from 0U to 100U.
 *
 * @return WiringPwm_Status_t
 *
 * @retval WIRING_PWM_STATUS_OK
 * PWM output started successfully.
 *
 * @retval WIRING_PWM_STATUS_INVALID_ARGUMENT
 * Pin, frequency, or duty value is invalid.
 *
 * @retval WIRING_PWM_STATUS_DRIVER_ERROR
 * PWM hardware configuration failed.
 */
WiringPwm_Status_t WiringPwm_Start(uint8_t pin,
                                   uint32_t frequencyHz,
                                   uint8_t dutyPercent);

/**
 * @brief Disable PWM output on one Arduino-style PWM pin.
 *
 * @details
 * This function disables the active waveform on the selected channel by
 * setting its duty cycle to the inactive level.
 *
 * The FTM counter is intentionally kept running because other channels
 * belonging to the same FTM instance may still be using it.
 *
 * @param[in] pin
 * Arduino-style logical pin identifier.
 *
 * @return WiringPwm_Status_t
 *
 * @retval WIRING_PWM_STATUS_OK
 * PWM output disabled successfully.
 *
 * @retval WIRING_PWM_STATUS_INVALID_ARGUMENT
 * Pin is invalid or does not support PWM.
 *
 * @retval WIRING_PWM_STATUS_DRIVER_ERROR
 * PWM hardware update failed.
 */
WiringPwm_Status_t WiringPwm_Stop(uint8_t pin);

#endif /* WIRING_PWM_H */
