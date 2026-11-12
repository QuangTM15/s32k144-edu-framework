/**
 * @file ina219.c
 * @brief INA219 blocking I2C driver for EduFramework (S32K144).
 * @details See ina219.h and TI SBOS448G for register definitions and math.
 */
#include "ina219.h"
#include "Wire.h"

#include <stddef.h>

/* ============================================================
 * Device Register Map and Bit Fields
 * ============================================================ */
#define INA219_REG_CONFIG (0x00U)
#define INA219_REG_SHUNT_VOLTAGE (0x01U)
#define INA219_REG_BUS_VOLTAGE (0x02U)
#define INA219_REG_POWER (0x03U)
#define INA219_REG_CURRENT (0x04U)
#define INA219_REG_CALIBRATION (0x05U)

#define INA219_CONFIG_RESET (0x8000U)
#define INA219_CONFIG_BUS_SHIFT (13U)
#define INA219_CONFIG_PGA_SHIFT (11U)
#define INA219_CONFIG_BADC_SHIFT (7U)
#define INA219_CONFIG_SADC_SHIFT (3U)
#define INA219_CONFIG_POWER_MASK (0x0007U)

#define INA219_BUS_CNVR_MASK (0x0002U)
#define INA219_BUS_OVF_MASK (0x0001U)
#define INA219_BUS_DATA_SHIFT (3U)

#define INA219_BUS_LSB_V (0.004f)
#define INA219_SHUNT_LSB_MV (0.01f)
#define INA219_CURRENT_TO_MA (1000.0f)
#define INA219_POWER_TO_MW (1000.0f)
#define INA219_CAL_CONSTANT (0.04096)
#define INA219_CAL_MAX (65534U) /* FS0 is hardwired zero. */
#define INA219_CURRENT_RAW_MAX (32767.0)

/* ============================================================
 * Default Beginner Context
 * ============================================================ */
static INA219_t g_inaDefault;

/* ============================================================
 * Private Helpers
 * ============================================================ */

/** @brief Save status to the context when it is non-NULL. */
static INA219_Status_t INA219_SetStatus(INA219_t *ina,
                                        INA219_Status_t status)
{
    if (NULL != ina)
    {
        ina->lastStatus = status;
    }
    return status;
}

/** @brief Convert the Wire error code without modifying shared Wire state. */
static INA219_Status_t INA219_MapWireStatus(uint8_t status)
{
    INA219_Status_t result = INA219_STATUS_I2C_ERROR;

    switch (status)
    {
    case WIRE_STATUS_OK:
        result = INA219_STATUS_OK;
        break;
    case WIRE_STATUS_BUSY:
        result = INA219_STATUS_BUSY;
        break;
    case WIRE_STATUS_TIMEOUT:
        result = INA219_STATUS_TIMEOUT;
        break;
    case WIRE_STATUS_NACK:
        result = INA219_STATUS_NACK;
        break;
    default:
        result = INA219_STATUS_I2C_ERROR;
        break;
    }
    return result;
}

/** @brief Convert 16-bit big-endian sensor registers from separate bytes. */
static uint16_t INA219_MakeU16(uint8_t hi, uint8_t lo)
{
    uint16_t result = ((uint16_t)hi << 8U) | (uint16_t)lo;
    return result;
}

/** @brief Convert a two's-complement raw register without signed overflow. */
static int32_t INA219_MakeS16(uint16_t raw)
{
    int32_t result = (int32_t)raw;
    if (0U != (raw & 0x8000U))
    {
        result -= 65536L;
    }
    return result;
}

/** @brief Read 16-bit register via repeated-start; the caller owns its buffer. */
static INA219_Status_t INA219_ReadRegister(uint8_t address,
                                           uint8_t reg,
                                           uint16_t *value)
{
    uint8_t rx[2] = {0U, 0U};
    INA219_Status_t result = INA219_STATUS_INVALID_ARGUMENT;

    if (NULL != value)
    {
        result = INA219_MapWireStatus(
            Wire_transferBlocking(address, &reg, 1U, rx, 2U));
        if (INA219_STATUS_OK == result)
        {
            *value = INA219_MakeU16(rx[0], rx[1]);
        }
    }
    return result;
}

/** @brief Write pointer+MSB+LSB using a single write-only transaction. */
static INA219_Status_t INA219_WriteRegister(uint8_t address,
                                            uint8_t reg,
                                            uint16_t value)
{
    uint8_t tx[3] = {reg, (uint8_t)(value >> 8U), (uint8_t)value};
    INA219_Status_t result = INA219_MapWireStatus(
        Wire_transferBlocking(address, tx, 3U, NULL, 0U));
    return result;
}

/**
 * @brief Write a register and verify its readback value.
 * @note Do not use for write-only RESET bit.
 */
static INA219_Status_t INA219_WriteVerified(uint8_t address,
                                            uint8_t reg,
                                            uint16_t value)
{
    uint16_t readback = 0U;
    INA219_Status_t result = INA219_WriteRegister(address, reg, value);

    if (INA219_STATUS_OK == result)
    {
        result = INA219_ReadRegister(address, reg, &readback);
        if ((INA219_STATUS_OK == result) && (value != readback))
        {
            result = INA219_STATUS_I2C_ERROR;
        }
    }
    return result;
}

/** @brief Check the supported INA219 address space. */
static bool INA219_IsAddressValid(uint8_t address)
{
    bool valid = false;
    if ((INA219_ADDRESS_MIN <= address) && (address <= INA219_ADDRESS_MAX))
    {
        valid = true;
    }
    return valid;
}

/** @brief Check all programmable ADC field codes. */
static bool INA219_IsAdcValid(INA219_AdcMode_t adc)
{
    bool valid = false;
    if ((INA219_ADC_12BIT >= adc) ||
        ((INA219_ADC_12BIT_ALT <= adc) && (INA219_ADC_128_SAMPLES >= adc)))
    {
        valid = true;
    }
    return valid;
}

/** @brief Ensure configuration fields map to valid bit patterns. */
static bool INA219_IsConfigValid(const INA219_Config_t *config)
{
    bool valid = false;
    if ((NULL != config) &&
        (INA219_BUS_RANGE_32V >= config->busRange) &&
        (INA219_SHUNT_RANGE_320MV >= config->shuntRange) &&
        (true == INA219_IsAdcValid(config->busAdc)) &&
        (true == INA219_IsAdcValid(config->shuntAdc)) &&
        (INA219_MODE_BOTH_CONT >= config->mode))
    {
        valid = true;
    }
    return valid;
}

/** @brief Encode 16-bit Configuration register using documented field shifts. */
static uint16_t INA219_PackConfig(const INA219_Config_t *config)
{
    uint16_t result = 0U;
    if (NULL != config)
    {
        result = (uint16_t)(((uint16_t)config->busRange << INA219_CONFIG_BUS_SHIFT) |
                            ((uint16_t)config->shuntRange << INA219_CONFIG_PGA_SHIFT) |
                            ((uint16_t)config->busAdc << INA219_CONFIG_BADC_SHIFT) |
                            ((uint16_t)config->shuntAdc << INA219_CONFIG_SADC_SHIFT) |
                            (uint16_t)config->mode);
    }
    return result;
}

/** @brief Positive shunt PGA full scale, in volts. */
static double INA219_ShuntFullScaleV(INA219_ShuntRange_t range)
{
    double result = 0.0;
    switch (range)
    {
    case INA219_SHUNT_RANGE_40MV:
        result = 0.040;
        break;
    case INA219_SHUNT_RANGE_80MV:
        result = 0.080;
        break;
    case INA219_SHUNT_RANGE_160MV:
        result = 0.160;
        break;
    case INA219_SHUNT_RANGE_320MV:
        result = 0.320;
        break;
    default:
        break;
    }
    return result;
}

/** @brief Check finite positive current and shunt, and fit to PGA range. */
static bool INA219_AreCalibrationInputsValid(float shuntOhms,
                                             float maxCurrentA,
                                             INA219_ShuntRange_t range)
{
    bool valid = false;
    double drop = (double)shuntOhms * (double)maxCurrentA;

    /* Finite upper bounds reject INF and overflow; NaN fails comparisons. */
    if ((0.0f < shuntOhms) && (1000.0f >= shuntOhms) &&
        (0.0f < maxCurrentA) && (100000.0f >= maxCurrentA) &&
        (drop <= INA219_ShuntFullScaleV(range)))
    {
        valid = true;
    }
    return valid;
}

/**
 * @brief Round a positive LSB to a human-friendly 1/2/5 x 10^n value.
 * @note Input is constrained by INA219_AreCalibrationInputsValid().
 */
static double INA219_NiceRoundUp(double value)
{
    double decade = 1.0;
    double result = 0.0;

    if ((0.0 < value) && (1.0e8 >= value))
    {
        while (value < decade)
        {
            decade /= 10.0;
        }
        while (value > (10.0 * decade))
        {
            decade *= 10.0;
        }
        if (value <= decade)
        {
            result = decade;
        }
        else if (value <= (2.0 * decade))
        {
            result = 2.0 * decade;
        }
        else if (value <= (5.0 * decade))
        {
            result = 5.0 * decade;
        }
        else
        {
            result = 10.0 * decade;
        }
    }
    return result;
}

/**
 * @brief Prepare current LSB and calibration as a single valid combination.
 * @note CAL bit zero is ignored by the device, so force it to zero.
 */
static INA219_Status_t INA219_CalculateCalibration(
    float shuntOhms, float maxExpectedCurrentA,
    INA219_ShuntRange_t range, float *currentLsbA,
    float *powerLsbW, uint16_t *calibration)
{
    double minLsb = 0.0;
    double cal = 0.0;
    float calFloat = 0.0f;
    double lsb = 0.0;
    uint32_t calInt = 0U;
    INA219_Status_t result = INA219_STATUS_CALIBRATION_ERROR;

    if ((NULL == currentLsbA) || (NULL == powerLsbW) ||
        (NULL == calibration))
    {
        result = INA219_STATUS_INVALID_ARGUMENT;
    }
    else if (true == INA219_AreCalibrationInputsValid(shuntOhms,
                                                      maxExpectedCurrentA,
                                                      range))
    {
        minLsb = (double)maxExpectedCurrentA / INA219_CURRENT_RAW_MAX;
        if (minLsb < (INA219_CAL_CONSTANT /
                      ((double)INA219_CAL_MAX * (double)shuntOhms)))
        {
            minLsb = INA219_CAL_CONSTANT /
                     ((double)INA219_CAL_MAX * (double)shuntOhms);
        }
        lsb = INA219_NiceRoundUp(minLsb);
        if (0.0 < lsb)
        {
            /* Work in float here: public shunt/LSB parameters are float. */
            calFloat = 0.04096f / ((float)lsb * shuntOhms);
            cal = (double)calFloat;
            if ((2.0 <= cal) && ((double)INA219_CAL_MAX >= cal))
            {
                calInt = (uint32_t)cal;
                calInt &= ~(uint32_t)1U;
                if (0U != calInt)
                {
                    *currentLsbA = (float)lsb;
                    *powerLsbW = (float)(lsb * 20.0);
                    *calibration = (uint16_t)calInt;
                    result = INA219_STATUS_OK;
                }
            }
        }
    }
    return result;
}

/** @brief Determine whether mode performs the bus-voltage conversion. */
static bool INA219_HasBus(INA219_Mode_t mode)
{
    bool result = false;
    if ((INA219_MODE_BUS_TRIGGERED == mode) ||
        (INA219_MODE_BOTH_TRIGGERED == mode) ||
        (INA219_MODE_BUS_CONT == mode) ||
        (INA219_MODE_BOTH_CONT == mode))
    {
        result = true;
    }
    return result;
}

/** @brief Determine whether mode performs the shunt-voltage conversion. */
static bool INA219_HasShunt(INA219_Mode_t mode)
{
    bool result = false;
    if ((INA219_MODE_SHUNT_TRIGGERED == mode) ||
        (INA219_MODE_BOTH_TRIGGERED == mode) ||
        (INA219_MODE_SHUNT_CONT == mode) ||
        (INA219_MODE_BOTH_CONT == mode))
    {
        result = true;
    }
    return result;
}

/** @brief Determine whether mode runs on demand. */
static bool INA219_IsTriggered(INA219_Mode_t mode)
{
    bool result = false;
    if ((INA219_MODE_SHUNT_TRIGGERED == mode) ||
        (INA219_MODE_BUS_TRIGGERED == mode) ||
        (INA219_MODE_BOTH_TRIGGERED == mode))
    {
        result = true;
    }
    return result;
}

/** @brief Read current measurement flags without clearing CNVR. */
static INA219_Status_t INA219_ReadBusFlags(const INA219_t *ina,
                                           uint16_t *busRaw)
{
    INA219_Status_t result = INA219_STATUS_INVALID_ARGUMENT;
    if ((NULL != ina) && (NULL != busRaw))
    {
        result = INA219_ReadRegister(ina->i2cAddress,
                                     INA219_REG_BUS_VOLTAGE, busRaw);
    }
    return result;
}

/**
 * @brief Read conversion flags and latch CNVR for the active triggered cycle.
 * @note A completed triggered conversion remains readable even if CNVR is
 *       subsequently cleared by another register access.
 */
static INA219_Status_t INA219_CheckMathReady(INA219_t *ina,
                                             uint16_t *busRaw)
{
    INA219_Status_t result = INA219_ReadBusFlags(ina, busRaw);
    if (INA219_STATUS_OK == result)
    {
        if (0U != (*busRaw & INA219_BUS_OVF_MASK))
        {
            result = INA219_STATUS_MATH_OVERFLOW;
        }
        else if (true == INA219_IsTriggered(ina->config.mode))
        {
            if ((true == ina->conversionPending) &&
                (0U != (*busRaw & INA219_BUS_CNVR_MASK)))
            {
                ina->conversionReady = true;
            }
            if ((false == ina->conversionPending) ||
                (false == ina->conversionReady))
            {
                result = INA219_STATUS_NOT_READY;
            }
        }
    }
    return result;
}

/** @brief Mark the triggered sample as consumed after reading Power. */
static void INA219_ConsumeConversion(INA219_t *ina)
{
    if ((NULL != ina) && (true == INA219_IsTriggered(ina->config.mode)))
    {
        ina->conversionPending = false;
        ina->conversionReady = false;
    }
}

/** @brief Check a non-NULL initialized instance. */
static INA219_Status_t INA219_RequireInitialized(const INA219_t *ina)
{
    INA219_Status_t result = INA219_STATUS_INVALID_ARGUMENT;
    if (NULL != ina)
    {
        result = INA219_STATUS_NOT_INITIALIZED;
        if (true == ina->initialized)
        {
            result = INA219_STATUS_OK;
        }
    }
    return result;
}

/* ============================================================
 * Default Configuration and Initialization
 * ============================================================ */

void INA219_getDefaultConfig(INA219_Config_t *config)
{
    if (NULL != config)
    {
        config->busRange = INA219_BUS_RANGE_32V;
        config->shuntRange = INA219_SHUNT_RANGE_320MV;
        config->busAdc = INA219_ADC_12BIT;
        config->shuntAdc = INA219_ADC_12BIT;
        config->mode = INA219_MODE_BOTH_CONT;
    }
}

INA219_Status_t INA219_begin(INA219_t *ina, uint8_t address,
                             float shuntOhms, float maxExpectedCurrentA)
{
    INA219_Status_t result = INA219_STATUS_INVALID_ARGUMENT;
    float currentLsb = 0.0f;
    float powerLsb = 0.0f;
    uint16_t cal = 0U;
    uint16_t configReg = 0U;

    if (NULL != ina)
    {
        ina->initialized = false;
        ina->conversionPending = false;
        ina->conversionReady = false;
        ina->i2cAddress = address;
        ina->shuntResistanceOhms = 0.0f;
        ina->maxExpectedCurrentA = 0.0f;
        ina->currentLsbA = 0.0f;
        ina->powerLsbW = 0.0f;
        ina->calibrationValue = 0U;
        INA219_getDefaultConfig(&ina->config);

        if (true == INA219_IsAddressValid(address))
        {
            result = INA219_CalculateCalibration(
                shuntOhms, maxExpectedCurrentA, ina->config.shuntRange,
                &currentLsb, &powerLsb, &cal);
        }
        if (INA219_STATUS_OK == result)
        {
            configReg = INA219_PackConfig(&ina->config);
            result = INA219_WriteVerified(address, INA219_REG_CONFIG, configReg);
        }
        if (INA219_STATUS_OK == result)
        {
            result = INA219_WriteVerified(address, INA219_REG_CALIBRATION, cal);
        }
        if (INA219_STATUS_OK == result)
        {
            ina->shuntResistanceOhms = shuntOhms;
            ina->maxExpectedCurrentA = maxExpectedCurrentA;
            ina->currentLsbA = currentLsb;
            ina->powerLsbW = powerLsb;
            ina->calibrationValue = cal;
            ina->initialized = true;
        }
        result = INA219_SetStatus(ina, result);
    }
    return result;
}

INA219_Status_t INA219_reset(INA219_t *ina)
{
    INA219_Status_t result = INA219_RequireInitialized(ina);
    if (INA219_STATUS_OK == result)
    {
        result = INA219_WriteRegister(ina->i2cAddress, INA219_REG_CONFIG,
                                      INA219_CONFIG_RESET);
        if (INA219_STATUS_OK == result)
        {
            result = INA219_WriteVerified(ina->i2cAddress,
                                          INA219_REG_CONFIG,
                                          INA219_PackConfig(&ina->config));
        }
        if (INA219_STATUS_OK == result)
        {
            result = INA219_WriteVerified(ina->i2cAddress,
                                          INA219_REG_CALIBRATION,
                                          ina->calibrationValue);
        }
        if (INA219_STATUS_OK != result)
        {
            ina->initialized = false; /* Reset may have cleared calibration. */
        }
        ina->conversionPending = (INA219_STATUS_OK == result) &&
                                 INA219_IsTriggered(ina->config.mode);
        ina->conversionReady = false;
    }
    return INA219_SetStatus(ina, result);
}

INA219_Status_t INA219_getLastStatus(const INA219_t *ina)
{
    INA219_Status_t result = INA219_STATUS_INVALID_ARGUMENT;
    if (NULL != ina)
    {
        result = ina->lastStatus;
    }
    return result;
}

INA219_Status_t INA219_configure(INA219_t *ina,
                                 const INA219_Config_t *config)
{
    INA219_Status_t result = INA219_RequireInitialized(ina);
    if (INA219_STATUS_OK == result)
    {
        if (false == INA219_IsConfigValid(config))
        {
            result = INA219_STATUS_INVALID_ARGUMENT;
        }
        else if (false == INA219_AreCalibrationInputsValid(
                              ina->shuntResistanceOhms,
                              ina->maxExpectedCurrentA,
                              config->shuntRange))
        {
            result = INA219_STATUS_CALIBRATION_ERROR;
        }
        else
        {
            result = INA219_WriteVerified(ina->i2cAddress,
                                          INA219_REG_CONFIG,
                                          INA219_PackConfig(config));
            if (INA219_STATUS_OK == result)
            {
                ina->config = *config;
                ina->conversionPending = INA219_IsTriggered(config->mode);
                ina->conversionReady = false;
            }
            else
            {
                /* Write may have succeeded even when readback failed. */
                ina->initialized = false;
                ina->conversionPending = false;
                ina->conversionReady = false;
            }
        }
    }
    return INA219_SetStatus(ina, result);
}

INA219_Status_t INA219_setCalibration(INA219_t *ina, float shuntOhms,
                                      float maxExpectedCurrentA)
{
    INA219_Status_t result = INA219_RequireInitialized(ina);
    float newCurrentLsb = 0.0f;
    float newPowerLsb = 0.0f;
    uint16_t newCalibration = 0U;

    if (INA219_STATUS_OK == result)
    {
        result = INA219_CalculateCalibration(
            shuntOhms, maxExpectedCurrentA, ina->config.shuntRange,
            &newCurrentLsb, &newPowerLsb, &newCalibration);
        if (INA219_STATUS_OK == result)
        {
            result = INA219_WriteVerified(ina->i2cAddress,
                                          INA219_REG_CALIBRATION,
                                          newCalibration);
            if (INA219_STATUS_OK == result)
            {
                ina->shuntResistanceOhms = shuntOhms;
                ina->maxExpectedCurrentA = maxExpectedCurrentA;
                ina->currentLsbA = newCurrentLsb;
                ina->powerLsbW = newPowerLsb;
                ina->calibrationValue = newCalibration;
            }
            else
            {
                /* Do not use possibly stale scaling after a write error. */
                ina->initialized = false;
                ina->conversionPending = false;
                ina->conversionReady = false;
            }
        }
    }
    return INA219_SetStatus(ina, result);
}

/* ============================================================
 * Data Acquisition
 * ============================================================ */

INA219_Status_t INA219_readBusVoltage(INA219_t *ina, float *voltageV)
{
    INA219_Status_t result = INA219_RequireInitialized(ina);
    uint16_t raw = 0U;

    if (INA219_STATUS_OK == result)
    {
        if (NULL == voltageV)
        {
            result = INA219_STATUS_INVALID_ARGUMENT;
        }
        else if (false == INA219_HasBus(ina->config.mode))
        {
            result = INA219_STATUS_NOT_READY;
        }
        else
        {
            result = INA219_ReadBusFlags(ina, &raw);
            if ((INA219_STATUS_OK == result) &&
                (true == INA219_IsTriggered(ina->config.mode)))
            {
                if ((true == ina->conversionPending) &&
                    (0U != (raw & INA219_BUS_CNVR_MASK)))
                {
                    ina->conversionReady = true;
                }
                if ((false == ina->conversionPending) ||
                    (false == ina->conversionReady))
                {
                    result = INA219_STATUS_NOT_READY;
                }
            }
            if (INA219_STATUS_OK == result)
            {
                *voltageV = (float)(raw >> INA219_BUS_DATA_SHIFT) *
                            INA219_BUS_LSB_V;
            }
        }
    }
    return INA219_SetStatus(ina, result);
}

INA219_Status_t INA219_readShuntVoltage(INA219_t *ina, float *voltageMv)
{
    INA219_Status_t result = INA219_RequireInitialized(ina);
    uint16_t raw = 0U;
    uint16_t busRaw = 0U;

    if (INA219_STATUS_OK == result)
    {
        if (NULL == voltageMv)
        {
            result = INA219_STATUS_INVALID_ARGUMENT;
        }
        else if (false == INA219_HasShunt(ina->config.mode))
        {
            result = INA219_STATUS_NOT_READY;
        }
        else
        {
            if (true == INA219_IsTriggered(ina->config.mode))
            {
                result = INA219_CheckMathReady(ina, &busRaw);
            }
            if (INA219_STATUS_OK == result)
            {
                result = INA219_ReadRegister(ina->i2cAddress,
                                             INA219_REG_SHUNT_VOLTAGE, &raw);
                if (INA219_STATUS_OK == result)
                {
                    *voltageMv = (float)INA219_MakeS16(raw) *
                                 INA219_SHUNT_LSB_MV;
                }
            }
        }
    }
    return INA219_SetStatus(ina, result);
}

INA219_Status_t INA219_readCurrent(INA219_t *ina, float *currentMa)
{
    INA219_Status_t result = INA219_RequireInitialized(ina);
    uint16_t busRaw = 0U;
    uint16_t raw = 0U;

    if (INA219_STATUS_OK == result)
    {
        if (NULL == currentMa)
        {
            result = INA219_STATUS_INVALID_ARGUMENT;
        }
        else if (false == INA219_HasShunt(ina->config.mode))
        {
            result = INA219_STATUS_NOT_READY;
        }
        else
        {
            result = INA219_CheckMathReady(ina, &busRaw);
            if (INA219_STATUS_OK == result)
            {
                result = INA219_ReadRegister(ina->i2cAddress,
                                             INA219_REG_CURRENT, &raw);
                if (INA219_STATUS_OK == result)
                {
                    *currentMa = (float)INA219_MakeS16(raw) *
                                 ina->currentLsbA * INA219_CURRENT_TO_MA;
                }
            }
        }
    }
    return INA219_SetStatus(ina, result);
}

INA219_Status_t INA219_readPower(INA219_t *ina, float *powerMw)
{
    INA219_Status_t result = INA219_RequireInitialized(ina);
    uint16_t busRaw = 0U;
    uint16_t raw = 0U;

    if (INA219_STATUS_OK == result)
    {
        if (NULL == powerMw)
        {
            result = INA219_STATUS_INVALID_ARGUMENT;
        }
        else if ((false == INA219_HasShunt(ina->config.mode)) ||
                 (false == INA219_HasBus(ina->config.mode)))
        {
            result = INA219_STATUS_NOT_READY;
        }
        else
        {
            result = INA219_CheckMathReady(ina, &busRaw);
            if (INA219_STATUS_OK == result)
            {
                result = INA219_ReadRegister(ina->i2cAddress,
                                             INA219_REG_POWER, &raw);
                if (INA219_STATUS_OK == result)
                {
                    *powerMw = (float)raw * ina->powerLsbW *
                               INA219_POWER_TO_MW;
                    INA219_ConsumeConversion(ina);
                }
            }
        }
    }
    return INA219_SetStatus(ina, result);
}

INA219_Status_t INA219_readSourceVoltage(INA219_t *ina, float *voltageV)
{
    INA219_Status_t result = INA219_RequireInitialized(ina);
    uint16_t busRaw = 0U;
    uint16_t shuntRaw = 0U;
    float busV = 0.0f;
    float shuntV = 0.0f;

    if (INA219_STATUS_OK == result)
    {
        if (NULL == voltageV)
        {
            result = INA219_STATUS_INVALID_ARGUMENT;
        }
        else if ((false == INA219_HasBus(ina->config.mode)) ||
                 (false == INA219_HasShunt(ina->config.mode)))
        {
            result = INA219_STATUS_NOT_READY;
        }
        else
        {
            result = INA219_CheckMathReady(ina, &busRaw);
            if (INA219_STATUS_OK == result)
            {
                result = INA219_ReadRegister(ina->i2cAddress,
                                             INA219_REG_SHUNT_VOLTAGE,
                                             &shuntRaw);
                if (INA219_STATUS_OK == result)
                {
                    busV = (float)(busRaw >> INA219_BUS_DATA_SHIFT) *
                           INA219_BUS_LSB_V;
                    shuntV = (float)INA219_MakeS16(shuntRaw) *
                             (INA219_SHUNT_LSB_MV / 1000.0f);
                    *voltageV = busV + shuntV;
                }
            }
        }
    }
    return INA219_SetStatus(ina, result);
}

INA219_Status_t INA219_readData(INA219_t *ina, INA_Data_t *data)
{
    INA219_Status_t result = INA219_RequireInitialized(ina);
    uint16_t busRaw = 0U;
    uint16_t shuntRaw = 0U;
    uint16_t currentRaw = 0U;
    uint16_t powerRaw = 0U;
    INA_Data_t temp = {0.0f, 0.0f, 0.0f, 0.0f};

    if (INA219_STATUS_OK == result)
    {
        if (NULL == data)
        {
            result = INA219_STATUS_INVALID_ARGUMENT;
        }
        else if ((false == INA219_HasBus(ina->config.mode)) ||
                 (false == INA219_HasShunt(ina->config.mode)))
        {
            result = INA219_STATUS_NOT_READY;
        }
        else
        {
            /* Flag check must precede Power read (which clears CNVR). */
            result = INA219_CheckMathReady(ina, &busRaw);
            if (INA219_STATUS_OK == result)
            {
                result = INA219_ReadRegister(ina->i2cAddress,
                                             INA219_REG_SHUNT_VOLTAGE,
                                             &shuntRaw);
            }
            if (INA219_STATUS_OK == result)
            {
                result = INA219_ReadRegister(ina->i2cAddress,
                                             INA219_REG_CURRENT, &currentRaw);
            }
            if (INA219_STATUS_OK == result)
            {
                result = INA219_ReadRegister(ina->i2cAddress,
                                             INA219_REG_POWER, &powerRaw);
            }
            if (INA219_STATUS_OK == result)
            {
                temp.busVoltageV = (float)(busRaw >> INA219_BUS_DATA_SHIFT) *
                                   INA219_BUS_LSB_V;
                temp.shuntVoltageMv = (float)INA219_MakeS16(shuntRaw) *
                                      INA219_SHUNT_LSB_MV;
                temp.currentMa = (float)INA219_MakeS16(currentRaw) *
                                 ina->currentLsbA * INA219_CURRENT_TO_MA;
                temp.powerMw = (float)powerRaw * ina->powerLsbW *
                               INA219_POWER_TO_MW;
                *data = temp;
                INA219_ConsumeConversion(ina);
            }
        }
    }
    return INA219_SetStatus(ina, result);
}

/* ============================================================
 * Trigger, Flags and Register Diagnostics
 * ============================================================ */

INA219_Status_t INA219_triggerConversion(INA219_t *ina)
{
    INA219_Status_t result = INA219_RequireInitialized(ina);
    if (INA219_STATUS_OK == result)
    {
        if (false == INA219_IsTriggered(ina->config.mode))
        {
            result = INA219_STATUS_INVALID_ARGUMENT;
        }
        else
        {
            result = INA219_WriteVerified(ina->i2cAddress,
                                          INA219_REG_CONFIG,
                                          INA219_PackConfig(&ina->config));
            if (INA219_STATUS_OK == result)
            {
                ina->conversionPending = true;
                ina->conversionReady = false;
            }
            else
            {
                ina->initialized = false;
                ina->conversionPending = false;
                ina->conversionReady = false;
            }
        }
    }
    return INA219_SetStatus(ina, result);
}

INA219_Status_t INA219_getMeasurementStatus(INA219_t *ina,
                                            bool *ready, bool *overflow)
{
    INA219_Status_t result = INA219_RequireInitialized(ina);
    uint16_t raw = 0U;
    if (INA219_STATUS_OK == result)
    {
        if ((NULL == ready) || (NULL == overflow))
        {
            result = INA219_STATUS_INVALID_ARGUMENT;
        }
        else
        {
            result = INA219_ReadBusFlags(ina, &raw);
            if (INA219_STATUS_OK == result)
            {
                *overflow = (0U != (raw & INA219_BUS_OVF_MASK));
                if ((true == INA219_IsTriggered(ina->config.mode)) &&
                    (true == ina->conversionPending))
                {
                    if (0U != (raw & INA219_BUS_CNVR_MASK))
                    {
                        ina->conversionReady = true;
                    }
                    *ready = ina->conversionReady;
                }
                else
                {
                    *ready = (0U != (raw & INA219_BUS_CNVR_MASK));
                }
            }
        }
    }
    return INA219_SetStatus(ina, result);
}

INA219_Status_t INA219_readRawRegister(INA219_t *ina, uint8_t reg,
                                       uint16_t *value)
{
    INA219_Status_t result = INA219_RequireInitialized(ina);
    if (INA219_STATUS_OK == result)
    {
        if ((NULL == value) || (INA219_REG_CALIBRATION < reg))
        {
            result = INA219_STATUS_INVALID_ARGUMENT;
        }
        else
        {
            result = INA219_ReadRegister(ina->i2cAddress, reg, value);
        }
    }
    return INA219_SetStatus(ina, result);
}

/* ============================================================
 * Beginner API Wrappers
 * ============================================================ */

bool INA_Begin(void)
{
    bool result = false;
    Wire_begin();
    if (INA219_STATUS_OK == INA219_begin(&g_inaDefault,
                                         INA219_ADDRESS_DEFAULT,
                                         INA219_DEFAULT_SHUNT_OHMS,
                                         INA219_DEFAULT_MAX_CURRENT_A))
    {
        result = true;
    }
    return result;
}

bool INA_IsInitialized(void)
{
    bool result = g_inaDefault.initialized;
    return result;
}

bool INA_ReadData(INA_Data_t *data)
{
    bool result = (INA219_STATUS_OK == INA219_readData(&g_inaDefault, data));
    return result;
}

bool INA_ReadBusVoltage(float *voltageV)
{
    bool result = (INA219_STATUS_OK ==
                   INA219_readBusVoltage(&g_inaDefault, voltageV));
    return result;
}

bool INA_ReadShuntVoltage(float *voltageMv)
{
    bool result = (INA219_STATUS_OK ==
                   INA219_readShuntVoltage(&g_inaDefault, voltageMv));
    return result;
}

bool INA_ReadCurrent(float *currentMa)
{
    bool result = (INA219_STATUS_OK ==
                   INA219_readCurrent(&g_inaDefault, currentMa));
    return result;
}

bool INA_ReadPower(float *powerMw)
{
    bool result = (INA219_STATUS_OK ==
                   INA219_readPower(&g_inaDefault, powerMw));
    return result;
}

INA219_Status_t INA_GetLastStatus(void)
{
    INA219_Status_t result = g_inaDefault.lastStatus;
    return result;
}
