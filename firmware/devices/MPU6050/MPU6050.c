/**
 * @file MPU6050.c
 * @brief MPU6050 device driver implementation for EduFramework.
 *
 * @details
 * This module implements beginner-friendly and advanced MPU6050 APIs
 * on top of the EduFramework Wire and Time interfaces.
 */

#include "MPU6050.h"
#include "wire.h"
#include "time.h"

#include <stddef.h>

/* ============================================================
 * Private Register Definitions
 * ============================================================ */

#define MPU6050_REG_GYRO_CONFIG (0x1BU)
#define MPU6050_REG_ACCEL_CONFIG (0x1CU)
#define MPU6050_REG_ACCEL_XOUT_H (0x3BU)
#define MPU6050_REG_PWR_MGMT_1 (0x6BU)
#define MPU6050_REG_WHO_AM_I (0x75U)

#define MPU6050_WHO_AM_I_VALUE (0x68U)
#define MPU6050_WHO_AM_I_COMPATIBLE (0x70U)

#define MPU6050_MEASUREMENT_SIZE (14U)

#define MPU6050_ACCEL_SCALE_2_G (16384.0f)
#define MPU6050_ACCEL_SCALE_4_G (8192.0f)
#define MPU6050_ACCEL_SCALE_8_G (4096.0f)
#define MPU6050_ACCEL_SCALE_16_G (2048.0f)

#define MPU6050_GYRO_SCALE_250_DEG (131.0f)
#define MPU6050_GYRO_SCALE_500_DEG (65.5f)
#define MPU6050_GYRO_SCALE_1000_DEG (32.8f)
#define MPU6050_GYRO_SCALE_2000_DEG (16.4f)

#define MPU6050_TEMP_SCALE (340.0f)
#define MPU6050_TEMP_OFFSET (36.53f)

#define MPU6050_WAKEUP_DELAY_MS (50U)
#define MPU6050_CALIBRATION_DELAY_MS (2U)

#define MPU_DEFAULT_CALIBRATION_SAMPLES (500U)

/* ============================================================
 * Beginner API Context
 * ============================================================ */

/**
 * @brief Default MPU6050 instance used by the beginner API.
 */
static MPU6050_t g_mpuDefault;

/**
 * @brief Initialization state of the default MPU6050 instance.
 */
static bool g_mpuInitialized = false;

/* ============================================================
 * Private Helper Functions
 * ============================================================ */

/**
 * @brief Initialize an MPU6050 context with default values.
 *
 * @param[out] mpu Pointer to the device context.
 *
 * @return None.
 */
static void MPU6050_ResetContext(MPU6050_t *mpu)
{
    if (mpu != NULL)
    {
        mpu->i2cAddress = MPU6050_ADDRESS_DEFAULT;

        mpu->accelRange = MPU6050_RANGE_2_G;
        mpu->gyroRange = MPU6050_RANGE_250_DEG;

        mpu->accelX = 0.0f;
        mpu->accelY = 0.0f;
        mpu->accelZ = 0.0f;

        mpu->gyroX = 0.0f;
        mpu->gyroY = 0.0f;
        mpu->gyroZ = 0.0f;

        mpu->temperature = 0.0f;

        mpu->accelOffsetX = 0.0f;
        mpu->accelOffsetY = 0.0f;
        mpu->accelOffsetZ = 0.0f;

        mpu->gyroOffsetX = 0.0f;
        mpu->gyroOffsetY = 0.0f;
        mpu->gyroOffsetZ = 0.0f;
    }
}

/**
 * @brief Check whether an accelerometer range value is valid.
 *
 * @param[in] range Accelerometer range value.
 *
 * @return true if valid; otherwise false.
 */
static bool MPU6050_IsAccelRangeValid(MPU6050_AccelRange_t range)
{
    bool bValid = false;

    if ((MPU6050_RANGE_2_G == range) ||
        (MPU6050_RANGE_4_G == range) ||
        (MPU6050_RANGE_8_G == range) ||
        (MPU6050_RANGE_16_G == range))
    {
        bValid = true;
    }

    return bValid;
}

/**
 * @brief Check whether a gyroscope range value is valid.
 *
 * @param[in] range Gyroscope range value.
 *
 * @return true if valid; otherwise false.
 */
static bool MPU6050_IsGyroRangeValid(MPU6050_GyroRange_t range)
{
    bool bValid = false;

    if ((MPU6050_RANGE_250_DEG == range) ||
        (MPU6050_RANGE_500_DEG == range) ||
        (MPU6050_RANGE_1000_DEG == range) ||
        (MPU6050_RANGE_2000_DEG == range))
    {
        bValid = true;
    }

    return bValid;
}

/**
 * @brief Write one byte to an MPU6050 register.
 *
 * @param[in] address Device I2C address.
 * @param[in] reg Register address.
 * @param[in] data Data byte.
 *
 * @return true if the I2C transaction succeeds; otherwise false.
 */
static bool MPU6050_WriteRegister(
    uint8_t address,
    uint8_t reg,
    uint8_t data)
{
    bool bSuccess = false;

    Wire_beginTransmission(address);

    (void)Wire_write(reg);
    (void)Wire_write(data);

    if (WIRE_STATUS_OK == Wire_endTransmission())
    {
        bSuccess = true;
    }

    return bSuccess;
}

/**
 * @brief Read consecutive MPU6050 registers.
 *
 * @param[in] address Device I2C address.
 * @param[in] startReg First register address.
 * @param[out] buffer Destination buffer.
 * @param[in] length Number of bytes to read.
 *
 * @return true if all requested bytes are received;
 * otherwise false.
 */
static bool MPU6050_ReadRegisters(
    uint8_t address,
    uint8_t startReg,
    uint8_t *buffer,
    uint8_t length)
{
    bool bSuccess = false;
    uint8_t index = 0U;

    if ((buffer != NULL) &&
        (0U < length))
    {
        Wire_beginTransmission(address);

        (void)Wire_write(startReg);

        if (WIRE_STATUS_OK == Wire_endTransmission())
        {
            (void)Wire_requestFrom(address, length);

            while ((0 < Wire_available()) &&
                   (index < length))
            {
                buffer[index] = (uint8_t)Wire_read();
                index++;
            }

            if (length == index)
            {
                bSuccess = true;
            }
        }
    }

    return bSuccess;
}

/**
 * @brief Convert two big-endian bytes to a signed 16-bit value.
 *
 * @param[in] highByte Most significant byte.
 * @param[in] lowByte Least significant byte.
 *
 * @return Signed 16-bit value.
 */
static int16_t MPU6050_BytesToInt16(
    uint8_t highByte,
    uint8_t lowByte)
{
    uint16_t value = 0U;
    int16_t result = 0;

    value =
        ((uint16_t)highByte << 8U) |
        (uint16_t)lowByte;

    result = (int16_t)value;

    return result;
}

/**
 * @brief Get the accelerometer scale factor.
 *
 * @param[in] range Accelerometer measurement range.
 *
 * @return Raw counts per g.
 */
static float MPU6050_GetAccelScale(
    MPU6050_AccelRange_t range)
{
    float scale = MPU6050_ACCEL_SCALE_2_G;

    switch (range)
    {
    case MPU6050_RANGE_2_G:
        scale = MPU6050_ACCEL_SCALE_2_G;
        break;

    case MPU6050_RANGE_4_G:
        scale = MPU6050_ACCEL_SCALE_4_G;
        break;

    case MPU6050_RANGE_8_G:
        scale = MPU6050_ACCEL_SCALE_8_G;
        break;

    case MPU6050_RANGE_16_G:
        scale = MPU6050_ACCEL_SCALE_16_G;
        break;

    default:
        scale = MPU6050_ACCEL_SCALE_2_G;
        break;
    }

    return scale;
}

/**
 * @brief Get the gyroscope scale factor.
 *
 * @param[in] range Gyroscope measurement range.
 *
 * @return Raw counts per degree per second.
 */
static float MPU6050_GetGyroScale(
    MPU6050_GyroRange_t range)
{
    float scale = MPU6050_GYRO_SCALE_250_DEG;

    switch (range)
    {
    case MPU6050_RANGE_250_DEG:
        scale = MPU6050_GYRO_SCALE_250_DEG;
        break;

    case MPU6050_RANGE_500_DEG:
        scale = MPU6050_GYRO_SCALE_500_DEG;
        break;

    case MPU6050_RANGE_1000_DEG:
        scale = MPU6050_GYRO_SCALE_1000_DEG;
        break;

    case MPU6050_RANGE_2000_DEG:
        scale = MPU6050_GYRO_SCALE_2000_DEG;
        break;

    default:
        scale = MPU6050_GYRO_SCALE_250_DEG;
        break;
    }

    return scale;
}

/**
 * @brief Copy MPU6050 context measurements into beginner data structure.
 *
 * @param[in] mpu Pointer to MPU6050 device context.
 * @param[out] data Pointer to destination measurement structure.
 *
 * @return None.
 */
static void MPU6050_CopyData(
    const MPU6050_t *mpu,
    MPU_Data_t *data)
{
    if ((mpu != NULL) &&
        (data != NULL))
    {
        data->accelX = mpu->accelX;
        data->accelY = mpu->accelY;
        data->accelZ = mpu->accelZ;

        data->gyroX = mpu->gyroX;
        data->gyroY = mpu->gyroY;
        data->gyroZ = mpu->gyroZ;

        data->temperature = mpu->temperature;
    }
}

/* ============================================================
 * Advanced API Implementation
 * ============================================================ */

bool MPU6050_begin(
    MPU6050_t *mpu,
    uint8_t address)
{
    bool bSuccess = false;
    uint8_t whoAmI = 0U;

    if ((mpu != NULL) &&
        ((MPU6050_ADDRESS_DEFAULT == address) ||
         (MPU6050_ADDRESS_ALT == address)))
    {
        MPU6050_ResetContext(mpu);

        mpu->i2cAddress = address;

        if (true == MPU6050_ReadRegisters(
                        mpu->i2cAddress,
                        MPU6050_REG_WHO_AM_I,
                        &whoAmI,
                        1U))
        {
            if ((MPU6050_WHO_AM_I_VALUE == whoAmI) ||
                (MPU6050_WHO_AM_I_COMPATIBLE == whoAmI))
            {
                bSuccess = MPU6050_WriteRegister(
                    mpu->i2cAddress,
                    MPU6050_REG_PWR_MGMT_1,
                    0x00U);

                if (true == bSuccess)
                {
                    delay(MPU6050_WAKEUP_DELAY_MS);

                    bSuccess = MPU6050_setAccelerometerRange(
                        mpu,
                        MPU6050_RANGE_2_G);
                }

                if (true == bSuccess)
                {
                    bSuccess = MPU6050_setGyroRange(
                        mpu,
                        MPU6050_RANGE_250_DEG);
                }
            }
        }
    }

    return bSuccess;
}

bool MPU6050_setAccelerometerRange(
    MPU6050_t *mpu,
    MPU6050_AccelRange_t range)
{
    bool bSuccess = false;

    if ((mpu != NULL) &&
        (true == MPU6050_IsAccelRangeValid(range)))
    {
        bSuccess = MPU6050_WriteRegister(
            mpu->i2cAddress,
            MPU6050_REG_ACCEL_CONFIG,
            range);

        if (true == bSuccess)
        {
            mpu->accelRange = range;
        }
    }

    return bSuccess;
}

bool MPU6050_setGyroRange(
    MPU6050_t *mpu,
    MPU6050_GyroRange_t range)
{
    bool bSuccess = false;

    if ((mpu != NULL) &&
        (true == MPU6050_IsGyroRangeValid(range)))
    {
        bSuccess = MPU6050_WriteRegister(
            mpu->i2cAddress,
            MPU6050_REG_GYRO_CONFIG,
            range);

        if (true == bSuccess)
        {
            mpu->gyroRange = range;
        }
    }

    return bSuccess;
}

bool MPU6050_read(MPU6050_t *mpu)
{
    bool bSuccess = false;

    uint8_t buffer[MPU6050_MEASUREMENT_SIZE] = {0U};

    float accelScale = MPU6050_ACCEL_SCALE_2_G;
    float gyroScale = MPU6050_GYRO_SCALE_250_DEG;

    int16_t rawAccelX = 0;
    int16_t rawAccelY = 0;
    int16_t rawAccelZ = 0;

    int16_t rawTemp = 0;

    int16_t rawGyroX = 0;
    int16_t rawGyroY = 0;
    int16_t rawGyroZ = 0;

    if (mpu != NULL)
    {
        bSuccess = MPU6050_ReadRegisters(
            mpu->i2cAddress,
            MPU6050_REG_ACCEL_XOUT_H,
            buffer,
            MPU6050_MEASUREMENT_SIZE);

        if (true == bSuccess)
        {
            accelScale =
                MPU6050_GetAccelScale(mpu->accelRange);

            gyroScale =
                MPU6050_GetGyroScale(mpu->gyroRange);

            rawAccelX =
                MPU6050_BytesToInt16(buffer[0], buffer[1]);

            rawAccelY =
                MPU6050_BytesToInt16(buffer[2], buffer[3]);

            rawAccelZ =
                MPU6050_BytesToInt16(buffer[4], buffer[5]);

            rawTemp =
                MPU6050_BytesToInt16(buffer[6], buffer[7]);

            rawGyroX =
                MPU6050_BytesToInt16(buffer[8], buffer[9]);

            rawGyroY =
                MPU6050_BytesToInt16(buffer[10], buffer[11]);

            rawGyroZ =
                MPU6050_BytesToInt16(buffer[12], buffer[13]);

            mpu->accelX =
                ((float)rawAccelX / accelScale) -
                mpu->accelOffsetX;

            mpu->accelY =
                ((float)rawAccelY / accelScale) -
                mpu->accelOffsetY;

            mpu->accelZ =
                ((float)rawAccelZ / accelScale) -
                mpu->accelOffsetZ;

            mpu->temperature =
                ((float)rawTemp / MPU6050_TEMP_SCALE) +
                MPU6050_TEMP_OFFSET;

            mpu->gyroX =
                ((float)rawGyroX / gyroScale) -
                mpu->gyroOffsetX;

            mpu->gyroY =
                ((float)rawGyroY / gyroScale) -
                mpu->gyroOffsetY;

            mpu->gyroZ =
                ((float)rawGyroZ / gyroScale) -
                mpu->gyroOffsetZ;
        }
    }

    return bSuccess;
}

bool MPU6050_calibrate(
    MPU6050_t *mpu,
    uint16_t iterations)
{
    bool bSuccess = false;
    uint16_t i = 0U;

    float sumAccelX = 0.0f;
    float sumAccelY = 0.0f;
    float sumAccelZ = 0.0f;

    float sumGyroX = 0.0f;
    float sumGyroY = 0.0f;
    float sumGyroZ = 0.0f;

    float oldAccelOffsetX = 0.0f;
    float oldAccelOffsetY = 0.0f;
    float oldAccelOffsetZ = 0.0f;

    float oldGyroOffsetX = 0.0f;
    float oldGyroOffsetY = 0.0f;
    float oldGyroOffsetZ = 0.0f;

    if ((mpu != NULL) &&
        (0U < iterations))
    {
        oldAccelOffsetX = mpu->accelOffsetX;
        oldAccelOffsetY = mpu->accelOffsetY;
        oldAccelOffsetZ = mpu->accelOffsetZ;

        oldGyroOffsetX = mpu->gyroOffsetX;
        oldGyroOffsetY = mpu->gyroOffsetY;
        oldGyroOffsetZ = mpu->gyroOffsetZ;

        mpu->accelOffsetX = 0.0f;
        mpu->accelOffsetY = 0.0f;
        mpu->accelOffsetZ = 0.0f;

        mpu->gyroOffsetX = 0.0f;
        mpu->gyroOffsetY = 0.0f;
        mpu->gyroOffsetZ = 0.0f;

        bSuccess = true;

        for (i = 0U;
             (i < iterations) && (true == bSuccess);
             i++)
        {
            bSuccess = MPU6050_read(mpu);

            if (true == bSuccess)
            {
                sumAccelX += mpu->accelX;
                sumAccelY += mpu->accelY;
                sumAccelZ += mpu->accelZ;

                sumGyroX += mpu->gyroX;
                sumGyroY += mpu->gyroY;
                sumGyroZ += mpu->gyroZ;

                delay(MPU6050_CALIBRATION_DELAY_MS);
            }
        }

        if (true == bSuccess)
        {
            mpu->accelOffsetX =
                sumAccelX / (float)iterations;

            mpu->accelOffsetY =
                sumAccelY / (float)iterations;

            mpu->accelOffsetZ =
                (sumAccelZ / (float)iterations) - 1.0f;

            mpu->gyroOffsetX =
                sumGyroX / (float)iterations;

            mpu->gyroOffsetY =
                sumGyroY / (float)iterations;

            mpu->gyroOffsetZ =
                sumGyroZ / (float)iterations;
        }
        else
        {
            mpu->accelOffsetX = oldAccelOffsetX;
            mpu->accelOffsetY = oldAccelOffsetY;
            mpu->accelOffsetZ = oldAccelOffsetZ;

            mpu->gyroOffsetX = oldGyroOffsetX;
            mpu->gyroOffsetY = oldGyroOffsetY;
            mpu->gyroOffsetZ = oldGyroOffsetZ;
        }
    }

    return bSuccess;
}

/* ============================================================
 * Advanced Measurement Getters
 * ============================================================ */

float MPU6050_getAccelerationX(const MPU6050_t *mpu)
{
    float value = 0.0f;

    if (mpu != NULL)
    {
        value = mpu->accelX;
    }

    return value;
}

float MPU6050_getAccelerationY(const MPU6050_t *mpu)
{
    float value = 0.0f;

    if (mpu != NULL)
    {
        value = mpu->accelY;
    }

    return value;
}

float MPU6050_getAccelerationZ(const MPU6050_t *mpu)
{
    float value = 0.0f;

    if (mpu != NULL)
    {
        value = mpu->accelZ;
    }

    return value;
}

float MPU6050_getGyroX(const MPU6050_t *mpu)
{
    float value = 0.0f;

    if (mpu != NULL)
    {
        value = mpu->gyroX;
    }

    return value;
}

float MPU6050_getGyroY(const MPU6050_t *mpu)
{
    float value = 0.0f;

    if (mpu != NULL)
    {
        value = mpu->gyroY;
    }

    return value;
}

float MPU6050_getGyroZ(const MPU6050_t *mpu)
{
    float value = 0.0f;

    if (mpu != NULL)
    {
        value = mpu->gyroZ;
    }

    return value;
}

float MPU6050_getTemperature(const MPU6050_t *mpu)
{
    float value = 0.0f;

    if (mpu != NULL)
    {
        value = mpu->temperature;
    }

    return value;
}

/* ============================================================
 * Beginner API Implementation
 * ============================================================ */

bool MPU_Begin(void)
{
    bool bSuccess = false;

    g_mpuInitialized = false;

    /*
     * Initialize I2C automatically so beginner applications
     * do not need to call Wire_begin().
     */
    Wire_begin();

    bSuccess = MPU6050_begin(
        &g_mpuDefault,
        MPU6050_ADDRESS_DEFAULT);

    if (true == bSuccess)
    {
        g_mpuInitialized = true;
    }

    return bSuccess;
}

bool MPU_IsInitialized(void)
{
    return g_mpuInitialized;
}

bool MPU_ReadData(MPU_Data_t *data)
{
    bool bSuccess = false;

    if ((true == g_mpuInitialized) &&
        (data != NULL))
    {
        bSuccess = MPU6050_read(&g_mpuDefault);

        if (true == bSuccess)
        {
            MPU6050_CopyData(
                &g_mpuDefault,
                data);
        }
    }

    return bSuccess;
}

bool MPU_ReadAcceleration(
    float *x,
    float *y,
    float *z)
{
    bool bSuccess = false;
    MPU_Data_t data = {0};

    if ((x != NULL) &&
        (y != NULL) &&
        (z != NULL))
    {
        bSuccess = MPU_ReadData(&data);

        if (true == bSuccess)
        {
            *x = data.accelX;
            *y = data.accelY;
            *z = data.accelZ;
        }
    }

    return bSuccess;
}

bool MPU_ReadGyroscope(
    float *x,
    float *y,
    float *z)
{
    bool bSuccess = false;
    MPU_Data_t data = {0};

    if ((x != NULL) &&
        (y != NULL) &&
        (z != NULL))
    {
        bSuccess = MPU_ReadData(&data);

        if (true == bSuccess)
        {
            *x = data.gyroX;
            *y = data.gyroY;
            *z = data.gyroZ;
        }
    }

    return bSuccess;
}

bool MPU_ReadTemperature(float *temperature)
{
    bool bSuccess = false;
    MPU_Data_t data = {0};

    if (temperature != NULL)
    {
        bSuccess = MPU_ReadData(&data);

        if (true == bSuccess)
        {
            *temperature = data.temperature;
        }
    }

    return bSuccess;
}

bool MPU_Calibrate(void)
{
    bool bSuccess = false;

    if (true == g_mpuInitialized)
    {
        bSuccess = MPU6050_calibrate(
            &g_mpuDefault,
            MPU_DEFAULT_CALIBRATION_SAMPLES);
    }

    return bSuccess;
}
