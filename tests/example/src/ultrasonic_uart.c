/**
 * @file ultrasonic_uart.c
 * @brief HC-SR04 ultrasonic sensor UART example.
 *
 * @details
 * This example demonstrates the simplest way to use the EduFramework
 * ultrasonic device library with Serial1.
 *
 * Hardware connection:
 *
 * - HC-SR04 VCC  -> 5V
 * - HC-SR04 GND  -> GND
 * - HC-SR04 TRIG -> GPIO2
 * - HC-SR04 ECHO -> GPIO1
 *
 * The measured distance is printed to Serial1 every 500 ms.
 */

#include "Arduino.h"
#include "ultrasonic.h"
#include "ultrasonic_uart.h"

/**
 * @brief Logical pin connected to HC-SR04 TRIG.
 */
#define ULTRASONIC_UART_TRIG_PIN (GPIO2)

/**
 * @brief Logical pin connected to HC-SR04 ECHO.
 */
#define ULTRASONIC_UART_ECHO_PIN (GPIO1)

/**
 * @brief Serial baud rate used by this example.
 */
#define ULTRASONIC_UART_BAUD_RATE (9600U)

/**
 * @brief Delay between distance measurements in milliseconds.
 */
#define ULTRASONIC_UART_PERIOD_MS (500U)

/**
 * @brief Run ultrasonic sensor UART example.
 *
 * @details
 * This function initializes the Arduino-style framework layer, starts
 * Serial1, initializes the default ultrasonic sensor, then continuously
 * reads distance and prints the result.
 *
 * @return None.
 */
void Example_UltrasonicUart(void)
{
    float f32DistanceCm = ULTRASONIC_INVALID_DISTANCE_CM;

    setup();

    Serial1_begin(ULTRASONIC_UART_BAUD_RATE);

    ultrasonicBegin(ULTRASONIC_UART_TRIG_PIN,
                    ULTRASONIC_UART_ECHO_PIN);

    Serial1_println("--------------------------------");
    Serial1_println("EduFramework HC-SR04 Example");
    Serial1_println("TRIG : GPIO2");
    Serial1_println("ECHO : GPIO1");
    Serial1_println("UART : Serial1 @ 9600");
    Serial1_println("--------------------------------");

    while (1)
    {
        f32DistanceCm = ultrasonicRead();

        if (ULTRASONIC_INVALID_DISTANCE_CM != f32DistanceCm)
        {
            Serial1_print("Distance = ");
            Serial1_printFloat(f32DistanceCm);
            Serial1_println(" cm");
        }
        else
        {
            Serial1_println("Distance = Timeout");
        }

        delay(ULTRASONIC_UART_PERIOD_MS);
    }
}