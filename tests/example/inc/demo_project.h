#ifndef DEMO_PROJECT_H
#define DEMO_PROJECT_H

/**
 * @file demo_project.h
 * @brief EduFramework integrated device demonstration.
 *
 * @details
 * This example demonstrates multiple EduFramework device libraries
 * operating together in one application.
 *
 * The demonstration includes:
 *
 * - Potentiometer input for BLDC motor throttle control.
 * - ESC control for BLDC motor speed.
 * - HC-SR04 ultrasonic distance measurement.
 * - Red LED warning when an obstacle is detected within 10 cm.
 * - NTC temperature measurement.
 * - ST7789 TFT display for system information and warning status.
 *
 * Application architecture:
 *
 * @code
 * Potentiometer ----> ADC ---------> ESC ---------> BLDC Motor
 *
 * HC-SR04 ----------> GPIO/Time ---> Warning Logic ---> RED LED
 *                                         |
 *                                         v
 *                                      TFT LCD
 *
 * NTC --------------> ADC -----------------------> TFT LCD
 * @endcode
 */

/* ========================================================================= */
/* Public API                                                                */
/* ========================================================================= */

/**
 * @brief Run the integrated EduFramework demonstration.
 *
 * @details
 * This function initializes all devices used by the demonstration and then
 * continuously performs the following operations:
 *
 * 1. Reads the potentiometer value.
 * 2. Converts the potentiometer value into BLDC throttle percentage.
 * 3. Updates the ESC throttle.
 * 4. Reads distance from the ultrasonic sensor.
 * 5. Activates the red warning LED when an obstacle is within 10 cm.
 * 6. Reads temperature from the NTC sensor.
 * 7. Updates the TFT display with motor throttle, distance, temperature,
 *    and system warning status.
 *
 * This function contains the main loop and does not return during normal
 * operation.
 *
 * @return None.
 */
void Example_DemoProject_Run(void);

#endif /* DEMO_PROJECT_H */