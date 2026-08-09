/**
 * @file dc_motor.c
 * @brief DC motor control implementation.
 *
 * @details
 * This module controls one brushed DC motor through an H-bridge motor
 * driver such as the TB6612FNG.
 *
 * The public motor speed range is:
 *
 * - 0   = motor stopped.
 * - 255 = maximum motor speed.
 *
 * EduFramework currently generates low-true PWM through analogWrite().
 * The TB6612FNG PWMA input is active-high. Therefore, the requested motor
 * speed is converted internally before being passed to analogWrite().
 *
 * Application speed:
 *
 *     0 -------------------------- 255
 *     stopped                     maximum
 *
 * EduFramework PWM value:
 *
 *     255 -------------------------- 0
 *
 * This conversion is intentionally kept inside the device layer so that
 * applications can use an intuitive 0-to-255 motor speed interface.
 */

#include "dc_motor.h"

#include "Arduino.h"

/* ========================================================================= */
/* Private state                                                             */
/* ========================================================================= */

/**
 * @brief Logical pin connected to the motor driver PWM input.
 */
static uint8_t s_pwmPin = 0U;

/**
 * @brief Logical pin connected to motor driver direction input 1.
 */
static uint8_t s_in1Pin = 0U;

/**
 * @brief Logical pin connected to motor driver direction input 2.
 */
static uint8_t s_in2Pin = 0U;

/**
 * @brief Logical pin connected to the motor driver standby input.
 */
static uint8_t s_stbyPin = 0U;

/**
 * @brief Last requested motor speed.
 *
 * The value uses the public motor speed convention:
 *
 * - 0   = stopped.
 * - 255 = maximum speed.
 */
static uint8_t s_speed = DC_MOTOR_SPEED_MIN;

/* ========================================================================= */
/* Private functions                                                         */
/* ========================================================================= */

/**
 * @brief Apply a motor speed using EduFramework low-true PWM.
 *
 * @details
 * The TB6612FNG PWMA input is active-high while EduFramework analogWrite()
 * currently produces low-true PWM.
 *
 * Therefore:
 *
 * PWM value = 255 - requested motor speed
 *
 * Examples:
 *
 * - speed   0 -> analogWrite 255
 * - speed  64 -> analogWrite 191
 * - speed 128 -> analogWrite 127
 * - speed 191 -> analogWrite 64
 * - speed 255 -> analogWrite 0
 *
 * @param[in] speed Requested motor speed in the range 0 to 255.
 */
static void DCMotor_ApplySpeed(uint8_t speed)
{
    uint8_t pwmValue = 0U;

    pwmValue = (uint8_t)(DC_MOTOR_SPEED_MAX - speed);

    analogWrite(s_pwmPin, pwmValue);
}

/* ========================================================================= */
/* Public API                                                                */
/* ========================================================================= */

void DCMotor_Init(uint8_t pwmPin,
                  uint8_t in1Pin,
                  uint8_t in2Pin,
                  uint8_t stbyPin)
{
    s_pwmPin = pwmPin;
    s_in1Pin = in1Pin;
    s_in2Pin = in2Pin;
    s_stbyPin = stbyPin;

    s_speed = DC_MOTOR_SPEED_MIN;

    /*
     * Configure all motor driver control pins.
     */
    pinMode(s_pwmPin, OUTPUT);
    pinMode(s_in1Pin, OUTPUT);
    pinMode(s_in2Pin, OUTPUT);
    pinMode(s_stbyPin, OUTPUT);

    /*
     * Keep the motor driver in standby while establishing safe
     * direction and PWM states.
     */
    digitalWrite(s_stbyPin, LOW);

    digitalWrite(s_in1Pin, LOW);
    digitalWrite(s_in2Pin, LOW);

    DCMotor_ApplySpeed(DC_MOTOR_SPEED_MIN);

    /*
     * Enable the motor driver after all outputs are in a known state.
     */
    digitalWrite(s_stbyPin, HIGH);
}

void DCMotor_SetSpeed(uint8_t speed)
{
    s_speed = speed;

    DCMotor_ApplySpeed(s_speed);
}

void DCMotor_Forward(void)
{
    /*
     * TB6612FNG channel A:
     *
     * AIN1 = HIGH
     * AIN2 = LOW
     */
    digitalWrite(s_in1Pin, HIGH);
    digitalWrite(s_in2Pin, LOW);

    DCMotor_ApplySpeed(s_speed);
}

void DCMotor_Reverse(void)
{
    /*
     * TB6612FNG channel A:
     *
     * AIN1 = LOW
     * AIN2 = HIGH
     */
    digitalWrite(s_in1Pin, LOW);
    digitalWrite(s_in2Pin, HIGH);

    DCMotor_ApplySpeed(s_speed);
}

void DCMotor_Stop(void)
{
    /*
     * Remove PWM drive first.
     */
    DCMotor_ApplySpeed(DC_MOTOR_SPEED_MIN);

    /*
     * Both direction inputs low place the TB6612FNG output
     * into the coast/stop state.
     */
    digitalWrite(s_in1Pin, LOW);
    digitalWrite(s_in2Pin, LOW);
}

void DCMotor_Brake(void)
{
    /*
     * TB6612FNG short-brake state:
     *
     * AIN1 = HIGH
     * AIN2 = HIGH
     *
     * Apply maximum logical motor speed so that the low-true PWM
     * conversion produces the continuously active PWMA state.
     */
    digitalWrite(s_in1Pin, HIGH);
    digitalWrite(s_in2Pin, HIGH);

    DCMotor_ApplySpeed(DC_MOTOR_SPEED_MAX);
}

void DCMotor_Standby(uint8_t standby)
{
    if (0U == standby)
    {
        /*
         * Leave standby mode.
         */
        digitalWrite(s_stbyPin, HIGH);
    }
    else
    {
        /*
         * Remove motor drive before entering standby.
         */
        DCMotor_ApplySpeed(DC_MOTOR_SPEED_MIN);

        digitalWrite(s_in1Pin, LOW);
        digitalWrite(s_in2Pin, LOW);

        digitalWrite(s_stbyPin, LOW);
    }
}
