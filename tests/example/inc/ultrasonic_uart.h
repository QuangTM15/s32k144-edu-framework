#ifndef ULTRASONIC_UART_H
#define ULTRASONIC_UART_H

/**
 * @file ultrasonic_uart.h
 * @brief HC-SR04 ultrasonic sensor UART example.
 *
 * @details
 * This example demonstrates how to use the Arduino-style ultrasonic device
 * library together with the Arduino-style Serial1 API.
 *
 * The measured distance is printed to the serial terminal every 500 ms.
 */

/**
 * @brief Run the ultrasonic UART example.
 *
 * @details
 * This function continuously reads distance from an HC-SR04 ultrasonic
 * sensor and prints the result to Serial1.
 *
 * @return None.
 */
void Example_UltrasonicUart(void);

#endif /* ULTRASONIC_UART_H */