/**
 * @file motor_encoder.c
 * @brief DC motor with quadrature encoder demonstration.
 *
 * @details
 * This example demonstrates:
 *
 * - DC motor forward control.
 * - DC motor reverse control.
 * - PWM speed control.
 * - Motor stop.
 * - Encoder count.
 * - Encoder direction.
 * - Encoder RPM measurement.
 *
 * Hardware connection:
 *
 * TB6612FNG:
 * - PWMA -> GPIO2
 * - AIN1 -> GPIO3
 * - AIN2 -> GPIO4
 * - STBY -> GPIO5
 * - VCC  -> 3.3V
 * - GND  -> GND
 * - VM   -> External motor supply
 * - AO1  -> Motor pin 5
 * - AO2  -> Motor pin 6
 *
 * Encoder:
 * - Pin 1 / Channel A -> GPIO8
 * - Pin 2 / Channel B -> GPIO9
 * - Pin 3 / VCC       -> 3.3V
 * - Pin 4 / GND       -> GND
 */

#include "motor_encoder.h"
#include "Arduino.h"
#include "hardware_serial.h"
#include "dc_motor.h"
#include "encoder.h"

/* ========================================================================= */
/* Pin Configuration                                                         */
/* ========================================================================= */

#define MOTOR_PWM_PIN GPIO2
#define MOTOR_IN1_PIN GPIO3
#define MOTOR_IN2_PIN GPIO4
#define MOTOR_STBY_PIN GPIO5

#define ENCODER_A_PIN GPIO8
#define ENCODER_B_PIN GPIO9

/*
 * Encoder = 30 pulses/revolution.
 * Encoder driver uses x4 quadrature decoding.
 *
 * 30 x 4 = 120 counts/revolution.
 */
#define ENCODER_CPR (120U)

/* ========================================================================= */
/* Motor Speed                                                               */
/* ========================================================================= */

#define MOTOR_SPEED_50 (128U)
#define MOTOR_SPEED_75 (191U)
#define MOTOR_SPEED_100 (255U)

/* ========================================================================= */
/* Private Functions                                                         */
/* ========================================================================= */

/**
 * @brief Print encoder information periodically.
 *
 * @param[in] u32DurationMs
 * Monitoring duration in milliseconds.
 */
static void MotorEncoder_Monitor(uint32_t u32DurationMs)
{
    uint32_t u32StartTime;
    uint32_t u32LastPrintTime;

    u32StartTime = millis();
    u32LastPrintTime = u32StartTime;

    while ((millis() - u32StartTime) < u32DurationMs)
    {
        Encoder_Update();

        if ((millis() - u32LastPrintTime) >= 200U)
        {
            u32LastPrintTime = millis();

            Serial1_print("Count: ");
            Serial1_printInt(Encoder_GetCount());

            Serial1_print(" | Direction: ");

            if (ENCODER_DIRECTION_FORWARD == Encoder_GetDirection())
            {
                Serial1_print("FORWARD");
            }
            else if (ENCODER_DIRECTION_REVERSE == Encoder_GetDirection())
            {
                Serial1_print("REVERSE");
            }
            else
            {
                Serial1_print("STOP");
            }

            Serial1_print(" | RPM: ");
            Serial1_printFloat(Encoder_GetRpm());

            Serial1_println("");
        }
    }
}

/* ========================================================================= */
/* Main Example Function                                                     */
/* ========================================================================= */

/**
 * @copydoc Example_MotorEncoder_Run
 */
void MotorEncoder_Example(void)
{
    /* 1. Mandatory framework initialization. */
    setup();

    /* 2. UART monitoring. */
    Serial1_begin(9600U);

    Serial1_println("");
    Serial1_println("=== DC Motor + Encoder Demo ===");

    /* 3. Initialize DC motor. */
    DCMotor_Init(
        MOTOR_PWM_PIN,
        MOTOR_IN1_PIN,
        MOTOR_IN2_PIN,
        MOTOR_STBY_PIN);

    Serial1_println("Motor initialized.");

    /* 4. Initialize encoder. */
    if (0U == Encoder_Init(
                  ENCODER_A_PIN,
                  ENCODER_B_PIN,
                  ENCODER_CPR))
    {
        Serial1_println("ERROR: Encoder initialization failed!");

        DCMotor_Stop();

        while (1)
        {
            delay(100U);
        }
    }

    Serial1_println("Encoder initialized.");

    Encoder_Reset();

    /* 5. Main functional test. */
    while (1)
    {
        /* ============================================================= */
        /* Forward 50%                                                   */
        /* ============================================================= */

        Serial1_println("");
        Serial1_println("FORWARD - 50%");

        DCMotor_SetSpeed(MOTOR_SPEED_50);
        DCMotor_Forward();

        MotorEncoder_Monitor(3000U);

        /* ============================================================= */
        /* Forward 75%                                                   */
        /* ============================================================= */

        Serial1_println("");
        Serial1_println("FORWARD - 75%");

        DCMotor_SetSpeed(MOTOR_SPEED_75);

        MotorEncoder_Monitor(3000U);

        /* ============================================================= */
        /* Forward 100%                                                  */
        /* ============================================================= */

        Serial1_println("");
        Serial1_println("FORWARD - 100%");

        DCMotor_SetSpeed(MOTOR_SPEED_100);

        MotorEncoder_Monitor(3000U);

        /* ============================================================= */
        /* Stop                                                          */
        /* ============================================================= */

        Serial1_println("");
        Serial1_println("STOP");

        DCMotor_Stop();

        MotorEncoder_Monitor(2000U);

        /* ============================================================= */
        /* Reverse 50%                                                   */
        /* ============================================================= */

        Serial1_println("");
        Serial1_println("REVERSE - 50%");

        DCMotor_SetSpeed(MOTOR_SPEED_50);
        DCMotor_Reverse();

        MotorEncoder_Monitor(3000U);

        /* ============================================================= */
        /* Reverse 75%                                                   */
        /* ============================================================= */

        Serial1_println("");
        Serial1_println("REVERSE - 75%");

        DCMotor_SetSpeed(MOTOR_SPEED_75);

        MotorEncoder_Monitor(3000U);

        /* ============================================================= */
        /* Reverse 100%                                                  */
        /* ============================================================= */

        Serial1_println("");
        Serial1_println("REVERSE - 100%");

        DCMotor_SetSpeed(MOTOR_SPEED_100);

        MotorEncoder_Monitor(3000U);

        /* ============================================================= */
        /* Stop                                                          */
        /* ============================================================= */

        Serial1_println("");
        Serial1_println("STOP");

        DCMotor_Stop();

        MotorEncoder_Monitor(2000U);
    }
}
