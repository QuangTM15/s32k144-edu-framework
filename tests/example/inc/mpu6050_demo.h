#ifndef MPU6050_DEMO_H
#define MPU6050_DEMO_H

/**
 * @file mpu6050_demo.h
 * @brief Demonstration of the MPU6050 sensor driver.
 *
 * @details
 * This example shows how to initialize the MPU6050 on the I2C bus,
 * perform a zero-bias calibration, and continuously print the
 * accelerometer, gyroscope, and temperature readings to the serial monitor.
 */

/**
 * @brief Runs the MPU6050 sensor demonstration.
 */
void Example_MPU6050_Run(void);

#endif /* MPU6050_DEMO_H */
