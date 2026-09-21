#ifndef MPU6050_H
#define MPU6050_H

/**
 * @file MPU6050.h
 * @brief MPU6050 device driver for EduFramework.
 *
 * @details
 * This module provides beginner-friendly and advanced APIs for the
 * MPU6050 6-axis accelerometer and gyroscope.
 *
 * The beginner API hides the I2C address, Wire initialization, and
 * device context, allowing applications to initialize and read the
 * sensor with only a few function calls.
 *
 * The advanced API exposes the MPU6050 device context and configuration
 * functions for applications that require custom I2C addresses,
 * measurement ranges, calibration sample counts, or multiple instances.
 *
 * The module provides:
 * - I2C device initialization.
 * - Acceleration measurement in g.
 * - Angular velocity measurement in degrees per second.
 * - Internal sensor temperature measurement.
 * - Accelerometer and gyroscope range configuration.
 * - Zero-bias calibration.
 *
 * This module belongs to the EduFramework Device layer and is implemented
 * on top of the Arduino-style Wire and Time APIs.
 */

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * Device Addresses
 * ============================================================ */

/**
 * @brief Default MPU6050 I2C address when AD0 is connected to GND.
 */
#define MPU6050_ADDRESS_DEFAULT (0x68U)

/**
 * @brief Alternate MPU6050 I2C address when AD0 is connected to VCC.
 */
#define MPU6050_ADDRESS_ALT (0x69U)

/* ============================================================
 * Accelerometer Range Configuration
 * ============================================================ */

/**
 * @brief Accelerometer range configuration type.
 */
typedef uint8_t MPU6050_AccelRange_t;

/** @brief Accelerometer range +/-2 g. */
#define MPU6050_RANGE_2_G (0x00U)

/** @brief Accelerometer range +/-4 g. */
#define MPU6050_RANGE_4_G (0x08U)

/** @brief Accelerometer range +/-8 g. */
#define MPU6050_RANGE_8_G (0x10U)

/** @brief Accelerometer range +/-16 g. */
#define MPU6050_RANGE_16_G (0x18U)

/* ============================================================
 * Gyroscope Range Configuration
 * ============================================================ */

/**
 * @brief Gyroscope range configuration type.
 */
typedef uint8_t MPU6050_GyroRange_t;

/** @brief Gyroscope range +/-250 degrees per second. */
#define MPU6050_RANGE_250_DEG (0x00U)

/** @brief Gyroscope range +/-500 degrees per second. */
#define MPU6050_RANGE_500_DEG (0x08U)

/** @brief Gyroscope range +/-1000 degrees per second. */
#define MPU6050_RANGE_1000_DEG (0x10U)

/** @brief Gyroscope range +/-2000 degrees per second. */
#define MPU6050_RANGE_2000_DEG (0x18U)

/* ============================================================
 * Beginner Measurement Structure
 * ============================================================ */

/**
 * @brief MPU6050 measurement data.
 *
 * @details
 * This structure contains one complete MPU6050 measurement frame.
 * All values stored in the structure originate from the same sensor
 * read operation.
 */
typedef struct
{
    float accelX; /**< X-axis acceleration in g. */
    float accelY; /**< Y-axis acceleration in g. */
    float accelZ; /**< Z-axis acceleration in g. */

    float gyroX; /**< X-axis angular velocity in deg/s. */
    float gyroY; /**< Y-axis angular velocity in deg/s. */
    float gyroZ; /**< Z-axis angular velocity in deg/s. */

    float temperature; /**< Internal sensor temperature in Celsius. */
} MPU_Data_t;

/* ============================================================
 * Beginner API
 * ============================================================ */

/**
 * @brief Initialize the default MPU6050 sensor.
 *
 * @details
 * This high-level API automatically initializes the I2C bus and
 * configures the MPU6050 using:
 *
 * - I2C address: 0x68
 * - Accelerometer range: +/-2 g
 * - Gyroscope range: +/-250 deg/s
 *
 * Applications using this API do not need to create an MPU6050_t
 * context or call Wire_begin().
 *
 * @return true if initialization succeeds; otherwise false.
 */
bool MPU_Begin(void);

/**
 * @brief Check whether the default MPU6050 sensor is initialized.
 *
 * @return true if MPU_Begin() completed successfully;
 * otherwise false.
 */
bool MPU_IsInitialized(void);

/**
 * @brief Read one complete MPU6050 measurement frame.
 *
 * @details
 * This function performs one sensor read and returns acceleration,
 * angular velocity, and internal temperature from the same measurement
 * frame.
 *
 * This API is recommended when an application requires multiple sensor
 * values at the same time, such as a motion monitor or sensor dashboard.
 *
 * @param[out] data Pointer to the measurement data structure.
 *
 * @return true if the complete measurement frame is read successfully;
 * otherwise false.
 */
bool MPU_ReadData(MPU_Data_t *data);

/**
 * @brief Read acceleration along the X, Y, and Z axes.
 *
 * @details
 * This function performs a new sensor read and returns acceleration
 * values in units of g.
 *
 * @param[out] x Pointer to X-axis acceleration.
 * @param[out] y Pointer to Y-axis acceleration.
 * @param[out] z Pointer to Z-axis acceleration.
 *
 * @return true if the measurement is read successfully;
 * otherwise false.
 */
bool MPU_ReadAcceleration(float *x, float *y, float *z);

/**
 * @brief Read angular velocity around the X, Y, and Z axes.
 *
 * @details
 * This function performs a new sensor read and returns angular
 * velocity values in degrees per second.
 *
 * @param[out] x Pointer to X-axis angular velocity.
 * @param[out] y Pointer to Y-axis angular velocity.
 * @param[out] z Pointer to Z-axis angular velocity.
 *
 * @return true if the measurement is read successfully;
 * otherwise false.
 */
bool MPU_ReadGyroscope(float *x, float *y, float *z);

/**
 * @brief Read the MPU6050 internal temperature.
 *
 * @details
 * This function performs a new sensor read and returns the internal
 * temperature of the MPU6050 device.
 *
 * @param[out] temperature Pointer to the temperature value in Celsius.
 *
 * @return true if the measurement is read successfully;
 * otherwise false.
 *
 * @note The MPU6050 temperature measurement represents the internal
 * sensor temperature and should not be treated as ambient temperature.
 */
bool MPU_ReadTemperature(float *temperature);

/**
 * @brief Calibrate the default MPU6050 sensor.
 *
 * @details
 * This function performs zero-bias calibration using 500 measurement
 * samples.
 *
 * The sensor must remain flat and stationary during calibration.
 * The calibration assumes that the Z axis experiences +1 g while
 * the sensor is resting flat.
 *
 * @return true if calibration completes successfully;
 * otherwise false.
 */
bool MPU_Calibrate(void);

/* ============================================================
 * Advanced Device Context
 * ============================================================ */

/**
 * @brief MPU6050 device context.
 *
 * @details
 * The context stores the device address, measurement configuration,
 * latest converted measurements, and calibration offsets.
 *
 * Applications requiring full control over the MPU6050 may create
 * and manage their own MPU6050_t instances.
 */
typedef struct
{
    uint8_t i2cAddress;

    MPU6050_AccelRange_t accelRange;
    MPU6050_GyroRange_t gyroRange;

    /* Latest converted measurements */
    float accelX;
    float accelY;
    float accelZ;

    float gyroX;
    float gyroY;
    float gyroZ;

    float temperature;

    /* Calibration offsets */
    float accelOffsetX;
    float accelOffsetY;
    float accelOffsetZ;

    float gyroOffsetX;
    float gyroOffsetY;
    float gyroOffsetZ;
} MPU6050_t;

/* ============================================================
 * Advanced API - Initialization and Configuration
 * ============================================================ */

/**
 * @brief Initialize an MPU6050 device instance.
 *
 * @details
 * The function verifies communication with the sensor, wakes the
 * device from sleep mode, clears calibration offsets, and configures:
 *
 * - Accelerometer: +/-2 g
 * - Gyroscope: +/-250 deg/s
 *
 * @note Wire_begin() must be called before this function when using
 * the advanced API.
 *
 * @param[in,out] mpu Pointer to the MPU6050 device context.
 * @param[in] address I2C address of the sensor.
 *
 * @return true if initialization succeeds; otherwise false.
 */
bool MPU6050_begin(MPU6050_t *mpu, uint8_t address);

/**
 * @brief Configure the accelerometer measurement range.
 *
 * @param[in,out] mpu Pointer to the MPU6050 device context.
 * @param[in] range Accelerometer measurement range.
 *
 * @return true if the configuration is written successfully;
 * otherwise false.
 */
bool MPU6050_setAccelerometerRange(
    MPU6050_t *mpu,
    MPU6050_AccelRange_t range);

/**
 * @brief Configure the gyroscope measurement range.
 *
 * @param[in,out] mpu Pointer to the MPU6050 device context.
 * @param[in] range Gyroscope measurement range.
 *
 * @return true if the configuration is written successfully;
 * otherwise false.
 */
bool MPU6050_setGyroRange(
    MPU6050_t *mpu,
    MPU6050_GyroRange_t range);

/* ============================================================
 * Advanced API - Measurement and Calibration
 * ============================================================ */

/**
 * @brief Read the latest sensor measurements.
 *
 * @details
 * This function reads acceleration, temperature, and angular velocity
 * in one 14-byte sensor transaction sequence.
 *
 * Raw measurements are converted to:
 *
 * - Acceleration in g.
 * - Angular velocity in degrees per second.
 * - Temperature in Celsius.
 *
 * Stored calibration offsets are applied to acceleration and
 * gyroscope measurements.
 *
 * @param[in,out] mpu Pointer to the MPU6050 device context.
 *
 * @return true if a complete measurement frame is received;
 * otherwise false.
 */
bool MPU6050_read(MPU6050_t *mpu);

/**
 * @brief Calibrate accelerometer and gyroscope zero-bias offsets.
 *
 * @details
 * The sensor must remain flat and stationary during calibration.
 * The requested number of measurements is averaged to determine
 * the sensor offsets.
 *
 * The calibration assumes that the Z axis experiences +1 g while
 * the sensor is resting flat.
 *
 * @param[in,out] mpu Pointer to the MPU6050 device context.
 * @param[in] iterations Number of calibration samples.
 *
 * @return true if all calibration samples are read successfully;
 * otherwise false.
 */
bool MPU6050_calibrate(
    MPU6050_t *mpu,
    uint16_t iterations);

/* ============================================================
 * Advanced API - Measurement Getters
 * ============================================================ */

/**
 * @brief Get acceleration along the X axis.
 *
 * @return Acceleration in g.
 */
float MPU6050_getAccelerationX(const MPU6050_t *mpu);

/**
 * @brief Get acceleration along the Y axis.
 *
 * @return Acceleration in g.
 */
float MPU6050_getAccelerationY(const MPU6050_t *mpu);

/**
 * @brief Get acceleration along the Z axis.
 *
 * @return Acceleration in g.
 */
float MPU6050_getAccelerationZ(const MPU6050_t *mpu);

/**
 * @brief Get angular velocity around the X axis.
 *
 * @return Angular velocity in degrees per second.
 */
float MPU6050_getGyroX(const MPU6050_t *mpu);

/**
 * @brief Get angular velocity around the Y axis.
 *
 * @return Angular velocity in degrees per second.
 */
float MPU6050_getGyroY(const MPU6050_t *mpu);

/**
 * @brief Get angular velocity around the Z axis.
 *
 * @return Angular velocity in degrees per second.
 */
float MPU6050_getGyroZ(const MPU6050_t *mpu);

/**
 * @brief Get the latest measured sensor temperature.
 *
 * @return Internal sensor temperature in Celsius.
 */
float MPU6050_getTemperature(const MPU6050_t *mpu);

#endif /* MPU6050_H */
