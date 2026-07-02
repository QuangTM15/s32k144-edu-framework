/**
 * @file ultrasonic_uart.c
 * @brief HC-SR04 ultrasonic sensor example with UART output.
 *
 * @details
 * This example demonstrates how to use the EduFramework ultrasonic device
 * library together with the Arduino-style Serial1 API.
 *
 * Hardware connection example:
 *
 * - HC-SR04 TRIG -> GPIO0
 * - HC-SR04 ECHO -> GPIO1
 * - HC-SR04 VCC  -> 5V
 * - HC-SR04 GND  -> GND
 *
 * The example sends distance data to Serial1 every 500 ms.
 */

#include "Arduino.h"
#include "ultrasonic.h"
#include "ultrasonic_uart.h"

/**
 * @brief Logical pin connected to HC-SR04 TRIG.
 */
#define ULTRASONIC_UART_TRIG_PIN        (GPIO0)

/**
 * @brief Logical pin connected to HC-SR04 ECHO.
 */
#define ULTRASONIC_UART_ECHO_PIN        (GPIO1)

/**
 * @brief Serial baud rate used by this example.
 */
#define ULTRASONIC_UART_BAUD_RATE       (9600U)

/**
 * @brief Delay between distance measurements in milliseconds.
 */
#define ULTRASONIC_UART_PERIOD_MS       (500U)

/**
 * @brief Run ultrasonic sensor UART example.
 *
 * @details
 * This function initializes the EduFramework Arduino-style layer, starts
 * Serial1, initializes the default ultrasonic sensor, then continuously
 * reads distance and prints the result.
 *
 * @return None.
 */
void Example_UltrasonicUart(void)
{
    float f32DistanceCm = 0.0f;

    setup();

    Serial1_begin(ULTRASONIC_UART_BAUD_RATE);

    ultrasonicBegin(ULTRASONIC_UART_TRIG_PIN,
                    ULTRASONIC_UART_ECHO_PIN);

    Serial1_println("--------------------------------");
    Serial1_println("EduFramework Ultrasonic UART");
    Serial1_println("TRIG = GPIO0, ECHO = GPIO1");
    Serial1_println("--------------------------------");

    while (1)
    {
        f32DistanceCm = ultrasonicRead();

        if (0.0f <= f32DistanceCm)
        {
            Serial1_print("Distance: ");
            Serial1_printFloat(f32DistanceCm);
            Serial1_println(" cm");
        }
        else
        {
            Serial1_println("Distance: Timeout");
        }

        delay(ULTRASONIC_UART_PERIOD_MS);
    }
}