#ifndef MPU6050_H
#define MPU6050_H

/**
 * @file mpu6050.h
 * @brief Arduino-style MPU6050 I2C sensor driver for EduFramework.
 *
 * @details
 * This file provides the public configuration types and APIs for the
 * MPU6050 6-DOF accelerometer and gyroscope.
 *
 * The MPU6050 driver is responsible for:
 * - Initializing the sensor via the I2C bus.
 * - Configuring measurement ranges for both accelerometer and gyroscope.
 * - Calculating and applying zero-bias offsets (Calibration).
 * - Reading and converting raw sensor data into physical units.
 *
 * This driver belongs to the Hardware Abstraction Layer (HAL) and relies
 * on the Arduino-style Wire and Time APIs
 */

#include "Arduino.h"
#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * MPU6050 Constants and Registers
 * ============================================================ */

/**
 * @brief Default I2C address for MPU6050 (AD0 pin connected to GND).
 */
#define MPU6050_ADDRESS_DEFAULT         (0x68U)

/**
 * @brief Alternate I2C address for MPU6050 (AD0 pin connected to VCC).
 */
#define MPU6050_ADDRESS_ALT             (0x69U)

#define MPU6050_REG_SMPLRT_DIV          (0x19U)
#define MPU6050_REG_CONFIG              (0x1AU)
#define MPU6050_REG_GYRO_CONFIG         (0x1BU)
#define MPU6050_REG_ACCEL_CONFIG        (0x1CU)
#define MPU6050_REG_ACCEL_XOUT_H        (0x3BU)
#define MPU6050_REG_TEMP_OUT_H          (0x41U)
#define MPU6050_REG_GYRO_XOUT_H         (0x43U)
#define MPU6050_REG_PWR_MGMT_1          (0x6BU)
#define MPU6050_REG_WHO_AM_I            (0x75U)

/* ============================================================
 * Configuration Types
 * ============================================================ */

/**
 * @brief Accelerometer range configuration type.
 */
typedef enum
{
    MPU6050_RANGE_2_G  = 0x00U,  /**< +/- 2g range */
    MPU6050_RANGE_4_G  = 0x08U,  /**< +/- 4g range */
    MPU6050_RANGE_8_G  = 0x10U,  /**< +/- 8g range */
    MPU6050_RANGE_16_G = 0x18U   /**< +/- 16g range */
} MPU6050_AccelRange_t;

/**
 * @brief Gyroscope range configuration type.
 */
typedef enum
{
    MPU6050_RANGE_250_DEG  = 0x00U,  /**< +/- 250 deg/s range */
    MPU6050_RANGE_500_DEG  = 0x08U,  /**< +/- 500 deg/s range */
    MPU6050_RANGE_1000_DEG = 0x10U,  /**< +/- 1000 deg/s range */
    MPU6050_RANGE_2000_DEG = 0x18U   /**< +/- 2000 deg/s range */
} MPU6050_GyroRange_t;

/* ============================================================
 * Object Context Structure
 * ============================================================ */

/**
 * @brief MPU6050 device context structure.
 *
 * @details
 * This structure acts as an object instance for the MPU6050 sensor.
 * It stores the I2C address, current configurations, zero-bias offsets,
 * and the latest scaled data read from the sensor.
 */
typedef struct
{
    uint8_t i2cAddress;                  /**< I2C address of the device. */

    MPU6050_AccelRange_t accelRange;     /**< Current accelerometer range. */
    MPU6050_GyroRange_t gyroRange;       /**< Current gyroscope range. */

    /* Scaled Data (Offsets applied) */
    float accelX;                        /**< Acceleration X in g. */
    float accelY;                        /**< Acceleration Y in g. */
    float accelZ;                        /**< Acceleration Z in g. */

    float gyroX;                         /**< Gyroscope X in deg/s. */
    float gyroY;                         /**< Gyroscope Y in deg/s. */
    float gyroZ;                         /**< Gyroscope Z in deg/s. */

    float temperature;                   /**< Temperature in Celsius. */

    /* Zero-bias Offsets */
    float accelOffsetX;                  /**< Acceleration X offset. */
    float accelOffsetY;                  /**< Acceleration Y offset. */
    float accelOffsetZ;                  /**< Acceleration Z offset. */

    float gyroOffsetX;                   /**< Gyroscope X offset. */
    float gyroOffsetY;                   /**< Gyroscope Y offset. */
    float gyroOffsetZ;                   /**< Gyroscope Z offset. */
} MPU6050_t;

/* ============================================================
 * Public API Prototypes
 * ============================================================ */

/**
 * @brief Initialize the MPU6050 sensor.
 *
 * @details
 * This function verifies communication by reading the WHO_AM_I register,
 * wakes up the sensor from sleep mode, and applies the default ranges
 * (+/- 2g for Accel, +/- 250 deg/s for Gyro).
 *
 * @param[in,out] mpu Pointer to the MPU6050 context.
 * @param[in] address I2C address of the sensor.
 *
 * @return bool
 * @retval true Sensor initialized successfully.
 * @retval false Sensor not found or communication failed.
 */
bool MPU6050_begin(MPU6050_t *mpu, uint8_t address);

/**
 * @brief Set the accelerometer measurement range.
 *
 * @param[in,out] mpu Pointer to the MPU6050 context.
 * @param[in] range Desired accelerometer range.
 *
 * @return None.
 */
void MPU6050_setAccelerometerRange(MPU6050_t *mpu, MPU6050_AccelRange_t range);

/**
 * @brief Set the gyroscope measurement range.
 *
 * @param[in,out] mpu Pointer to the MPU6050 context.
 * @param[in] range Desired gyroscope range.
 *
 * @return None.
 */
void MPU6050_setGyroRange(MPU6050_t *mpu, MPU6050_GyroRange_t range);

/**
 * @brief Calibrate the sensor by calculating zero-bias offsets.
 *
 * @details
 * The sensor must be placed flat and absolutely still before calling this
 * function. It will take multiple samples, average them, and store the
 * offsets internally to be subtracted during normal reads.
 *
 * @param[in,out] mpu Pointer to the MPU6050 context.
 * @param[in] iterations Number of samples to average (e.g., 500).
 *
 * @return None.
 */
void MPU6050_calibrate(MPU6050_t *mpu, uint16_t iterations);

/**
 * @brief Fetch the latest data from the sensor.
 *
 * @details
 * This function reads 14 bytes of raw data via I2C, converts them into
 * physical units (g, deg/s, Celsius), subtracts the stored offsets,
 * and updates the MPU6050 context.
 *
 * @param[in,out] mpu Pointer to the MPU6050 context.
 *
 * @return None.
 */
void MPU6050_read(MPU6050_t *mpu);

/* ============================================================
 * Individual Data Getters
 * ============================================================ */

float MPU6050_getAccelerationX(const MPU6050_t *mpu);
float MPU6050_getAccelerationY(const MPU6050_t *mpu);
float MPU6050_getAccelerationZ(const MPU6050_t *mpu);

float MPU6050_getGyroX(const MPU6050_t *mpu);
float MPU6050_getGyroY(const MPU6050_t *mpu);
float MPU6050_getGyroZ(const MPU6050_t *mpu);

float MPU6050_getTemperature(const MPU6050_t *mpu);

#endif /* MPU6050_H */
