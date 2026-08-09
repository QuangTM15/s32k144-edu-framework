/**
 * @file dc_motor.h
 * @brief DC motor control through an H-bridge motor driver.
 *
 * @details
 * This module provides a simple Arduino-style interface for controlling
 * one brushed DC motor through an H-bridge driver such as the TB6612FNG.
 *
 * The module uses:
 *
 * - One PWM pin for motor speed control.
 * - Two digital pins for motor direction control.
 * - One digital pin for standby control.
 *
 * The implementation depends only on the public Arduino-style API and does
 * not access S32K144 peripheral registers directly.
 */

#ifndef DEVICES_DC_MOTOR_DC_MOTOR_H_
#define DEVICES_DC_MOTOR_DC_MOTOR_H_

#include <stdint.h>

/* ========================================================================= */
/* Public constants                                                          */
/* ========================================================================= */

/**
 * @brief Minimum motor speed value.
 */
#define DC_MOTOR_SPEED_MIN (0U)

/**
 * @brief Maximum motor speed value.
 */
#define DC_MOTOR_SPEED_MAX (255U)

/* ========================================================================= */
/* Public API                                                                */
/* ========================================================================= */

/**
 * @brief Initialize the DC motor interface.
 *
 * @details
 * Configures the PWM, direction, and standby pins required by the motor
 * driver. The motor is placed in a stopped state after initialization and
 * the motor driver is enabled.
 *
 * The PWM pin must support analogWrite().
 *
 * @param[in] pwmPin   Logical pin connected to the motor driver PWM input.
 * @param[in] in1Pin   Logical pin connected to motor driver input 1.
 * @param[in] in2Pin   Logical pin connected to motor driver input 2.
 * @param[in] stbyPin  Logical pin connected to the standby input.
 */
void DCMotor_Init(uint8_t pwmPin,
                  uint8_t in1Pin,
                  uint8_t in2Pin,
                  uint8_t stbyPin);

/**
 * @brief Set the motor PWM speed.
 *
 * @details
 * The speed value uses the same 8-bit range as analogWrite().
 *
 * - 0   represents 0 percent duty.
 * - 255 represents 100 percent duty.
 *
 * This function changes only the PWM duty. It does not change the current
 * motor direction.
 *
 * @param[in] speed Motor speed in the range 0 to 255.
 */
void DCMotor_SetSpeed(uint8_t speed);

/**
 * @brief Rotate the motor in the forward direction.
 *
 * @details
 * Sets the H-bridge direction inputs to the forward state. The actual
 * physical rotation direction depends on the motor wiring.
 */
void DCMotor_Forward(void);

/**
 * @brief Rotate the motor in the reverse direction.
 *
 * @details
 * Sets the H-bridge direction inputs to the reverse state. The actual
 * physical rotation direction depends on the motor wiring.
 */
void DCMotor_Reverse(void);

/**
 * @brief Stop the motor using coast mode.
 *
 * @details
 * Sets the PWM duty to zero and disables both direction inputs. The motor
 * is allowed to coast to a stop.
 */
void DCMotor_Stop(void);

/**
 * @brief Stop the motor using short-brake mode.
 *
 * @details
 * Drives both H-bridge direction inputs high while keeping the PWM input
 * active. This requests the short-brake operating state of compatible
 * H-bridge drivers such as the TB6612FNG.
 */
void DCMotor_Brake(void);

/**
 * @brief Enable or disable motor driver standby mode.
 *
 * @param[in] standby
 *            - 0: Enable the motor driver.
 *            - Non-zero: Put the motor driver into standby.
 */
void DCMotor_Standby(uint8_t standby);

#endif /* DEVICES_DC_MOTOR_DC_MOTOR_H_ */