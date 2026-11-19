#ifndef INA219_H
#define INA219_H

/**
 * @file ina219.h
 * @brief INA219 bidirectional current, voltage and power monitor for EduFramework.
 *
 * @details
 * Beginner APIs manage one INA219 at address 0x40 with a 0.1-ohm shunt,
 * a nominal 2-A measurement profile and a private device context.
 * Advanced APIs support multiple 7-bit I2C addresses with independent
 * calibration and ADC configurations on one shared Wire master.
 *
 * Bus ownership: Advanced callers initialize Wire_begin() once at application
 * level. Beginner INA_Begin() calls Wire_begin() (idempotent in EduFramework).
 * This device driver does not change I2C speed or reinitialize the bus.
 * All transfers are blocking; do not call concurrently or inside an ISR.
 *
 * Measurement connections: bus voltage is measured at IN- relative to GND;
 * shunt voltage is IN+ minus IN-. Never connect a power source directly
 * across IN+ and IN- without a suitably rated load.
 *
 * Reference: Texas Instruments INA219 datasheet SBOS448G.
 */

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * Address and Beginner Profile
 * ============================================================ */
#define INA219_ADDRESS_DEFAULT (0x40U)
#define INA219_ADDRESS_MIN (0x40U)
#define INA219_ADDRESS_MAX (0x4FU)
#define INA219_DEFAULT_SHUNT_OHMS (0.1f)
#define INA219_DEFAULT_MAX_CURRENT_A (2.0f)

/* ============================================================
 * Status Codes
 * ============================================================ */
typedef uint8_t INA219_Status_t;

#define INA219_STATUS_OK ((INA219_Status_t)0U)
#define INA219_STATUS_INVALID_ARGUMENT ((INA219_Status_t)1U)
#define INA219_STATUS_NOT_INITIALIZED ((INA219_Status_t)2U)
#define INA219_STATUS_BUSY ((INA219_Status_t)3U)
#define INA219_STATUS_TIMEOUT ((INA219_Status_t)4U)
#define INA219_STATUS_NACK ((INA219_Status_t)5U)
#define INA219_STATUS_I2C_ERROR ((INA219_Status_t)6U)
#define INA219_STATUS_CALIBRATION_ERROR ((INA219_Status_t)7U)
#define INA219_STATUS_NOT_READY ((INA219_Status_t)8U)
#define INA219_STATUS_MATH_OVERFLOW ((INA219_Status_t)9U)

/* ============================================================
 * Configurations (values represent unshifted field encodings)
 * ============================================================ */
typedef uint8_t INA219_BusRange_t;
#define INA219_BUS_RANGE_16V (0U)
#define INA219_BUS_RANGE_32V (1U)

typedef uint8_t INA219_ShuntRange_t;
#define INA219_SHUNT_RANGE_40MV (0U)
#define INA219_SHUNT_RANGE_80MV (1U)
#define INA219_SHUNT_RANGE_160MV (2U)
#define INA219_SHUNT_RANGE_320MV (3U)

typedef uint8_t INA219_AdcMode_t;
#define INA219_ADC_9BIT (0x00U)
#define INA219_ADC_10BIT (0x01U)
#define INA219_ADC_11BIT (0x02U)
#define INA219_ADC_12BIT (0x03U)
#define INA219_ADC_12BIT_ALT (0x08U)
#define INA219_ADC_2_SAMPLES (0x09U)
#define INA219_ADC_4_SAMPLES (0x0AU)
#define INA219_ADC_8_SAMPLES (0x0BU)
#define INA219_ADC_16_SAMPLES (0x0CU)
#define INA219_ADC_32_SAMPLES (0x0DU)
#define INA219_ADC_64_SAMPLES (0x0EU)
#define INA219_ADC_128_SAMPLES (0x0FU)

typedef uint8_t INA219_Mode_t;
#define INA219_MODE_POWER_DOWN (0U)
#define INA219_MODE_SHUNT_TRIGGERED (1U)
#define INA219_MODE_BUS_TRIGGERED (2U)
#define INA219_MODE_BOTH_TRIGGERED (3U)
#define INA219_MODE_ADC_OFF (4U)
#define INA219_MODE_SHUNT_CONT (5U)
#define INA219_MODE_BUS_CONT (6U)
#define INA219_MODE_BOTH_CONT (7U)

/** @brief Conversion configuration for an INA219 instance. */
typedef struct
{
    INA219_BusRange_t busRange;     /**< 16V or 32V register range; IC max common mode is 26V. */
    INA219_ShuntRange_t shuntRange; /**< +/-40, 80, 160, 320 mV. */
    INA219_AdcMode_t busAdc;        /**< Bus ADC resolution / averaging field. */
    INA219_AdcMode_t shuntAdc;      /**< Shunt ADC resolution / averaging field. */
    INA219_Mode_t mode;             /**< Power-down, triggered, or continuous. */
} INA219_Config_t;

/** @brief Engineering-unit measurements (may come from successive conversions). */
typedef struct
{
    float busVoltageV;    /**< Voltage at IN- referenced to GND, in volts. */
    float shuntVoltageMv; /**< VIN+ - VIN-, in millivolts. */
    float currentMa;      /**< Signed current, in milliamperes. */
    float powerMw;        /**< Nonnegative power register value, in milliwatts. */
} INA_Data_t;

/** @brief Per-device state; do not modify fields directly after initialization. */
typedef struct
{
    uint8_t i2cAddress;
    float shuntResistanceOhms;
    float maxExpectedCurrentA;
    float currentLsbA;
    float powerLsbW;
    uint16_t calibrationValue;
    INA219_Config_t config;
    bool initialized;
    bool conversionPending; /**< Triggered conversion requested and not consumed. */
    bool conversionReady;   /**< CNVR observed since latest trigger. */
    INA219_Status_t lastStatus;
} INA219_t;

/* ============================================================
 * Beginner APIs - fixed address 0x40 and R100 shunt
 * ============================================================ */

/** @brief Initialize Wire (if needed) and default 0x40 sensor. */
bool INA_Begin(void);

/** @brief Return true only after successful beginner initialization. */
bool INA_IsInitialized(void);

/** @brief Read bus voltage, shunt voltage, current and power. */
bool INA_ReadData(INA_Data_t *data);

/** @brief Read voltage measured at IN- (V). */
bool INA_ReadBusVoltage(float *voltageV);

/** @brief Read shunt voltage VIN+ - VIN- (mV). */
bool INA_ReadShuntVoltage(float *voltageMv);

/** @brief Read calibrated signed current (mA). */
bool INA_ReadCurrent(float *currentMa);

/** @brief Read calibrated power register (mW). */
bool INA_ReadPower(float *powerMw);

/** @brief Return the last status of the default context. */
INA219_Status_t INA_GetLastStatus(void);

/* ============================================================
 * Advanced APIs - independent instance per physical device
 * ============================================================ */

/**
 * @brief Program defaults and calibration for an INA219 at a 7-bit address.
 * @param[out] ina Device context; its previous content is overwritten.
 * @param[in] address INA219 address from 0x40 to 0x4F.
 * @param[in] shuntOhms Actual shunt resistance in ohms (>0).
 * @param[in] maxExpectedCurrentA Positive expected peak current (A); must
 *            fit the selected shunt PGA range and Current register scaling.
 * @return INA219_STATUS_*.
 * @note Caller must have initialized the shared Wire bus.
 */
INA219_Status_t INA219_begin(INA219_t *ina, uint8_t address,
                             float shuntOhms, float maxExpectedCurrentA);

/** @brief Software-reset this sensor; restore its stored config/calibration. */
INA219_Status_t INA219_reset(INA219_t *ina);

/** @brief Retrieve the last result for this context. NULL -> INVALID_ARGUMENT. */
INA219_Status_t INA219_getLastStatus(const INA219_t *ina);

/**
 * @brief Read all four quantities; write *data only on full success.
 * @note Requires both shunt and bus ADC enabled. For triggered mode,
 *       returns NOT_READY until conversion completion is observed. Reading
 *       power consumes the latched completion state.
 *       Registers are read sequentially, not an atomic sample snapshot.
 */
INA219_Status_t INA219_readData(INA219_t *ina, INA_Data_t *data);

/** @brief Read VBUS at IN- in volts. Requires bus ADC mode. */
INA219_Status_t INA219_readBusVoltage(INA219_t *ina, float *voltageV);

/** @brief Read signed shunt voltage in millivolts. Requires shunt ADC mode. */
INA219_Status_t INA219_readShuntVoltage(INA219_t *ina, float *voltageMv);

/** @brief Read calibrated signed current in mA. Requires shunt ADC mode. */
INA219_Status_t INA219_readCurrent(INA219_t *ina, float *currentMa);

/** @brief Read calibrated power in mW; reading clears CNVR. Requires both ADCs. */
INA219_Status_t INA219_readPower(INA219_t *ina, float *powerMw);

/** @brief Calculate voltage at VIN+ (V) = VBUS + VSHUNT. */
INA219_Status_t INA219_readSourceVoltage(INA219_t *ina, float *voltageV);

/** @brief Populate config with INA219 power-on configuration (0x399F). */
void INA219_getDefaultConfig(INA219_Config_t *config);

/**
 * @brief Write configuration; store it only if the transaction succeeds.
 * @note Expected maximum current must fit the new PGA shunt range.
 *       A failed configuration I2C write/readback invalidates the context;
 *       call INA219_begin() again before measuring.
 */
INA219_Status_t INA219_configure(INA219_t *ina, const INA219_Config_t *config);

/**
 * @brief Calculate and write calibration for real shunt and peak current.
 * @details Chooses a rounded-up 1/2/5*10^n CurrentLSB, respecting the
 *          16-bit calibration register; power LSB = 20*current LSB.
 * @note Calibration determines scale, not the physical current rating of
 *       the shunt resistor, PCB traces, connectors or load wiring.
 *       A failed calibration I2C write/readback invalidates the context.
 */
INA219_Status_t INA219_setCalibration(INA219_t *ina, float shuntOhms,
                                      float maxExpectedCurrentA);

/** @brief Restart a conversion; only valid in triggered measurement modes. */
INA219_Status_t INA219_triggerConversion(INA219_t *ina);

/** @brief Read CNVR and OVF flags; a ready observation is latched for the active trigger. */
INA219_Status_t INA219_getMeasurementStatus(INA219_t *ina,
                                            bool *ready, bool *overflow);

/** @brief Read one of the six registers (0x00-0x05); primarily for diagnosis. */
INA219_Status_t INA219_readRawRegister(INA219_t *ina, uint8_t reg,
                                       uint16_t *value);

#endif /* INA219_H */
