/**
 * @file mpu6050.c
 * @brief Arduino-style MPU6050 driver implementation.
 *
 * @details
 * This file implements the Arduino-style MPU6050 API on top of the
 * EduFramework Wire and Time interfaces[cite: 4]. It uses hardware floating
 * point units (FPU) for fast conversion and scaling.
 */

#include "MPU6050.h"
#include "wire.h"
#include "time.h"
#include <stddef.h>

/* ============================================================
 * Private Helper Functions
 * ============================================================ */

/**
 * @brief Write a single byte to an MPU6050 register.
 *
 * @param[in] address I2C slave address.
 * @param[in] reg Target register address.
 * @param[in] data Byte to write.
 *
 * @return None.
 */
static void MPU6050_WriteRegister(uint8_t address, uint8_t reg, uint8_t data)
{
    Wire_beginTransmission(address); //[cite: 4]
    (void)Wire_write(reg);           //[cite: 4]
    (void)Wire_write(data);          //[cite: 4]
    (void)Wire_endTransmission();    //[cite: 4]
}

/**
 * @brief Get the scaling factor for the current accelerometer range.
 *
 * @param[in] range Current accelerometer range.
 *
 * @return float Scale factor to divide raw data by.
 */
static float MPU6050_GetAccelScale(MPU6050_AccelRange_t range)
{
    float scale = 16384.0f;

    switch (range)
    {
        case MPU6050_RANGE_2_G:  scale = 16384.0f; break;
        case MPU6050_RANGE_4_G:  scale = 8192.0f;  break;
        case MPU6050_RANGE_8_G:  scale = 4096.0f;  break;
        case MPU6050_RANGE_16_G: scale = 2048.0f;  break;
        default:                 scale = 16384.0f; break;
    }

    return scale;
}

/**
 * @brief Get the scaling factor for the current gyroscope range.
 *
 * @param[in] range Current gyroscope range.
 *
 * @return float Scale factor to divide raw data by.
 */
static float MPU6050_GetGyroScale(MPU6050_GyroRange_t range)
{
    float scale = 131.0f;

    switch (range)
    {
        case MPU6050_RANGE_250_DEG:  scale = 131.0f;  break;
        case MPU6050_RANGE_500_DEG:  scale = 65.5f;   break;
        case MPU6050_RANGE_1000_DEG: scale = 32.8f;   break;
        case MPU6050_RANGE_2000_DEG: scale = 16.4f;   break;
        default:                     scale = 131.0f;  break;
    }

    return scale;
}

/* ============================================================
 * Public API Implementation
 * ============================================================ */

/**
 * @copydoc MPU6050_begin
 */
bool MPU6050_begin(MPU6050_t *mpu, uint8_t address)
{
    bool bSuccess = false;

    if (mpu != NULL)
    {
        mpu->i2cAddress = address;

        /* Reset all offsets to zero initially */
        mpu->accelOffsetX = 0.0f;
        mpu->accelOffsetY = 0.0f;
        mpu->accelOffsetZ = 0.0f;
        mpu->gyroOffsetX  = 0.0f;
        mpu->gyroOffsetY  = 0.0f;
        mpu->gyroOffsetZ  = 0.0f;

        /* 1. Verify connection by reading WHO_AM_I register */
        Wire_beginTransmission(mpu->i2cAddress); //[cite: 4]
        (void)Wire_write(MPU6050_REG_WHO_AM_I);  //[cite: 4]

        if (WIRE_STATUS_OK == Wire_endTransmission()) //[cite: 4]
        {
            (void)Wire_requestFrom(mpu->i2cAddress, 1U); //[cite: 4]

            if (0 < Wire_available()) //[cite: 4]
            {
                uint8_t whoAmI = (uint8_t)Wire_read(); //[cite: 4]

                if ((whoAmI == 0x68U) || (whoAmI == 0x70U))
                {
                    /* 2. Wake up the sensor */
                    MPU6050_WriteRegister(mpu->i2cAddress, MPU6050_REG_PWR_MGMT_1, 0x00U);
                    delay(50); //[cite: 4]

                    /* 3. Set Default Ranges */
                    MPU6050_setAccelerometerRange(mpu, MPU6050_RANGE_2_G);
                    MPU6050_setGyroRange(mpu, MPU6050_RANGE_250_DEG);

                    bSuccess = true;
                }
            }
        }
    }

    return bSuccess;
}

/**
 * @copydoc MPU6050_setAccelerometerRange
 */
void MPU6050_setAccelerometerRange(MPU6050_t *mpu, MPU6050_AccelRange_t range)
{
    if (mpu != NULL)
    {
        mpu->accelRange = range;
        MPU6050_WriteRegister(mpu->i2cAddress, MPU6050_REG_ACCEL_CONFIG, (uint8_t)range);
    }
}

/**
 * @copydoc MPU6050_setGyroRange
 */
void MPU6050_setGyroRange(MPU6050_t *mpu, MPU6050_GyroRange_t range)
{
    if (mpu != NULL)
    {
        mpu->gyroRange = range;
        MPU6050_WriteRegister(mpu->i2cAddress, MPU6050_REG_GYRO_CONFIG, (uint8_t)range);
    }
}

/**
 * @copydoc MPU6050_calibrate
 */
void MPU6050_calibrate(MPU6050_t *mpu, uint16_t iterations)
{
    if ((mpu != NULL) && (iterations > 0U))
    {
        float sumAccelX = 0.0f;
        float sumAccelY = 0.0f;
        float sumAccelZ = 0.0f;
        float sumGyroX = 0.0f;
        float sumGyroY = 0.0f;
        float sumGyroZ = 0.0f;

        /* Temporarily remove offsets to read raw scaled data */
        mpu->accelOffsetX = 0.0f;
        mpu->accelOffsetY = 0.0f;
        mpu->accelOffsetZ = 0.0f;
        mpu->gyroOffsetX  = 0.0f;
        mpu->gyroOffsetY  = 0.0f;
        mpu->gyroOffsetZ  = 0.0f;

        for (uint16_t i = 0U; i < iterations; i++)
        {
            MPU6050_read(mpu);

            sumAccelX += mpu->accelX;
            sumAccelY += mpu->accelY;
            sumAccelZ += mpu->accelZ;

            sumGyroX += mpu->gyroX;
            sumGyroY += mpu->gyroY;
            sumGyroZ += mpu->gyroZ;

            delay(2); /* Short delay between samples[cite: 4] */
        }

        /* Calculate averages */
        mpu->accelOffsetX = sumAccelX / (float)iterations;
        mpu->accelOffsetY = sumAccelY / (float)iterations;

        /* Assuming the sensor is flat, Z-axis experiences 1g of gravity */
        mpu->accelOffsetZ = (sumAccelZ / (float)iterations) - 1.0f;

        mpu->gyroOffsetX = sumGyroX / (float)iterations;
        mpu->gyroOffsetY = sumGyroY / (float)iterations;
        mpu->gyroOffsetZ = sumGyroZ / (float)iterations;
    }
}

/**
 * @copydoc MPU6050_read
 */
void MPU6050_read(MPU6050_t *mpu)
{
    if (mpu != NULL)
    {
        uint8_t buffer[14];
        uint8_t index = 0U;

        Wire_beginTransmission(mpu->i2cAddress); //[cite: 4]
        (void)Wire_write(MPU6050_REG_ACCEL_XOUT_H); //[cite: 4]
        (void)Wire_endTransmission(); //[cite: 4]

        (void)Wire_requestFrom(mpu->i2cAddress, 14U); //[cite: 4]

        while ((0 < Wire_available()) && (index < 14U)) //[cite: 4]
        {
            buffer[index] = (uint8_t)Wire_read(); //[cite: 4]
            index++;
        }

        if (14U == index)
        {
            float aScale = MPU6050_GetAccelScale(mpu->accelRange);
            float gScale = MPU6050_GetGyroScale(mpu->gyroRange);

            /* Accel data: Scale to g and remove offset */
            mpu->accelX = ((float)((int16_t)((buffer[0] << 8) | buffer[1])) / aScale) - mpu->accelOffsetX;
            mpu->accelY = ((float)((int16_t)((buffer[2] << 8) | buffer[3])) / aScale) - mpu->accelOffsetY;
            mpu->accelZ = ((float)((int16_t)((buffer[4] << 8) | buffer[5])) / aScale) - mpu->accelOffsetZ;

            /* Temperature data */
            int16_t rawTemp = (int16_t)((buffer[6] << 8) | buffer[7]);
            mpu->temperature = ((float)rawTemp / 340.0f) + 36.53f;

            /* Gyro data: Scale to deg/s and remove offset */
            mpu->gyroX = ((float)((int16_t)((buffer[8] << 8) | buffer[9])) / gScale) - mpu->gyroOffsetX;
            mpu->gyroY = ((float)((int16_t)((buffer[10] << 8) | buffer[11])) / gScale) - mpu->gyroOffsetY;
            mpu->gyroZ = ((float)((int16_t)((buffer[12] << 8) | buffer[13])) / gScale) - mpu->gyroOffsetZ;
        }
    }
}

/* ============================================================
 * Getters
 * ============================================================ */

float MPU6050_getAccelerationX(const MPU6050_t *mpu)
{
    return (mpu != NULL) ? mpu->accelX : 0.0f;
}

float MPU6050_getAccelerationY(const MPU6050_t *mpu)
{
    return (mpu != NULL) ? mpu->accelY : 0.0f;
}

float MPU6050_getAccelerationZ(const MPU6050_t *mpu)
{
    return (mpu != NULL) ? mpu->accelZ : 0.0f;
}

float MPU6050_getGyroX(const MPU6050_t *mpu)
{
    return (mpu != NULL) ? mpu->gyroX : 0.0f;
}

float MPU6050_getGyroY(const MPU6050_t *mpu)
{
    return (mpu != NULL) ? mpu->gyroY : 0.0f;
}

float MPU6050_getGyroZ(const MPU6050_t *mpu)
{
    return (mpu != NULL) ? mpu->gyroZ : 0.0f;
}

float MPU6050_getTemperature(const MPU6050_t *mpu)
{
    return (mpu != NULL) ? mpu->temperature : 0.0f;
}
