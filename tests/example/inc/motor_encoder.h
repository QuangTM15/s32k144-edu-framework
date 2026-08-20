#ifndef MOTOR_ENCODER_H
#define MOTOR_ENCODER_H

/**
 * @file motor_encoder.h
 * @brief DC motor with quadrature encoder example.
 *
 * @details
 * This example demonstrates the combined use of:
 *
 * - DC motor device driver.
 * - TB6612FNG motor driver module.
 * - Quadrature encoder device driver.
 * - PORT interrupt support.
 * - UART monitoring.
 *
 * The motor is automatically driven through several speed levels in both
 * directions while encoder count, direction, and RPM are printed to Serial1.
 */

/**
 * @brief Run the DC motor and encoder example.
 *
 * @details
 * This function initializes the motor and encoder devices and continuously
 * executes the motor test sequence.
 *
 * @return None.
 */
void MotorEncoder_Example(void);

#endif /* MOTOR_ENCODER_H */