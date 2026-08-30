#ifndef NTC_H
#define NTC_H

/**
 * @file ntc.h
 * @brief NTC thermistor device library public interface.
 *
 * @details
 * This module provides a thin educational device layer for NTC thermistor
 * temperature sensing.
 *
 * The analog temperature measurement path uses:
 * - AO pin of the NTC module.
 * - Arduino-style analog API.
 * - ADC logical pins such as ADC0_SE12 or ADC0_SE13.
 *
 * The optional digital threshold path uses:
 * - DO pin of the NTC module.
 * - Arduino-style digital API.
 * - Any digital-capable GPIO pin.
 *
 * Typical 4-pin NTC module wiring:
 *
 * @code
 * NTC module     MaaZEDU
 * VCC        ->  5V
 * GND        ->  GND
 * AO         ->  ADC0_SE12 or ADC0_SE13
 * DO         ->  GPIO0..GPIO9, optional
 * @endcode
 *
 * The analog calculation assumes a voltage divider circuit:
 *
 * @code
 * VREF ---- R_FIXED ---- ADC(AO) ---- NTC ---- GND
 * @endcode
 *
 * For the opposite divider direction, the user must adjust the configuration
 * or the implementation formula in ntc.c.
 */

#include <stdint.h>

/* ========================================================================= */
/* Common Return Values                                                       */
/* ========================================================================= */

/**
 * @brief NTC boolean-like true value.
 */
#define NTC_TRUE (1U)

/**
 * @brief NTC boolean-like false value.
 */
#define NTC_FALSE (0U)

/**
 * @brief Error value returned by int-returning NTC APIs.
 */
#define NTC_ERROR_VALUE (-1)

/* ========================================================================= */
/* Default Configuration                                                      */
/* ========================================================================= */

/**
 * @brief Default fixed series resistor value in ohms.
 *
 * @details
 * The default value assumes a common 10 kOhm resistor used with a 10 kOhm
 * NTC thermistor.
 */
#define NTC_DEFAULT_SERIES_RESISTOR_OHM (10000UL)

/**
 * @brief Default nominal NTC resistance in ohms.
 *
 * @details
 * Most educational NTC modules use a 10 kOhm thermistor measured at 25 C.
 */
#define NTC_DEFAULT_NOMINAL_RESISTANCE_OHM (10000UL)

/**
 * @brief Default nominal temperature in Celsius.
 */
#define NTC_DEFAULT_NOMINAL_TEMPERATURE_C (25.0F)

/**
 * @brief Default Beta value for common 10 kOhm NTC thermistors.
 */
#define NTC_DEFAULT_BETA_VALUE (3950UL)

/**
 * @brief Default ADC reference voltage in millivolts.
 *
 * @details
 * This must match the voltage used by analogReadMilliVolts().
 */
#define NTC_DEFAULT_REFERENCE_VOLTAGE_MV (5000UL)

/* ========================================================================= */
/* Type Definitions                                                           */
/* ========================================================================= */

/**
 * @brief NTC thermistor calculation configuration.
 *
 * @details
 * This structure stores the parameters required by the Beta equation.
 *
 * The default configuration is suitable for a common 10 kOhm B3950 NTC
 * thermistor with a 10 kOhm fixed series resistor and 5 V ADC reference.
 */
typedef struct
{
    /**
     * @brief Fixed resistor value in the voltage divider, in ohms.
     */
    uint32_t u32SeriesResistorOhm;

    /**
     * @brief Nominal NTC resistance at nominal temperature, in ohms.
     */
    uint32_t u32NominalResistanceOhm;

    /**
     * @brief Nominal temperature in Celsius, usually 25.0 C.
     */
    float f32NominalTemperatureCelsius;

    /**
     * @brief NTC Beta coefficient.
     */
    uint32_t u32BetaValue;

    /**
     * @brief ADC reference voltage in millivolts.
     */
    uint32_t u32ReferenceVoltageMv;
} NTC_Config_t;

/* ========================================================================= */
/* Public API                                                                 */
/* ========================================================================= */

/**
 * @brief Initialize the NTC device layer.
 *
 * @details
 * This function initializes the internal NTC configuration to the default
 * 10 kOhm B3950 setup.
 *
 * The ADC subsystem is still initialized lazily by analogRead() or
 * analogReadMilliVolts().
 *
 * @return None.
 */
void NTC_Init(void);

/**
 * @brief Set custom NTC calculation configuration.
 *
 * @details
 * Use this function when the NTC value, fixed resistor value, Beta value,
 * or reference voltage is different from the default configuration.
 *
 * If pConfig is NULL or contains invalid zero-valued resistance, Beta, or
 * reference voltage fields, the configuration is not changed.
 *
 * @param[in] pConfig
 * Pointer to user configuration.
 *
 * @return None.
 */
void NTC_SetConfig(const NTC_Config_t *pConfig);

/**
 * @brief Get the current NTC calculation configuration.
 *
 * @param[out] pConfig
 * Pointer to destination configuration structure.
 *
 * @return None.
 */
void NTC_GetConfig(NTC_Config_t *pConfig);

/**
 * @brief Read raw ADC value from the NTC analog output pin.
 *
 * @param[in] u8AnalogPin
 * Arduino-style analog pin, such as ADC0_SE12 or ADC0_SE13.
 *
 * @return int
 *
 * @retval >=0
 * Raw ADC conversion result.
 *
 * @retval NTC_ERROR_VALUE
 * Invalid pin or ADC conversion failure.
 */
int NTC_ReadRaw(uint8_t u8AnalogPin);

/**
 * @brief Read voltage from the NTC analog output pin.
 *
 * @param[in] u8AnalogPin
 * Arduino-style analog pin, such as ADC0_SE12 or ADC0_SE13.
 *
 * @return int
 *
 * @retval >=0
 * Voltage in millivolts.
 *
 * @retval NTC_ERROR_VALUE
 * Invalid pin or ADC conversion failure.
 */
int NTC_ReadMilliVolts(uint8_t u8AnalogPin);

/**
 * @brief Read calculated NTC resistance.
 *
 * @details
 * The calculation assumes this divider:
 *
 * @code
 * VREF ---- R_FIXED ---- ADC(AO) ---- NTC ---- GND
 * @endcode
 *
 * @param[in] u8AnalogPin
 * Arduino-style analog pin connected to AO.
 *
 * @return float
 *
 * @retval >=0.0F
 * NTC resistance in ohms.
 *
 * @retval -1.0F
 * Invalid pin, invalid voltage, or ADC conversion failure.
 */
float NTC_ReadResistance(uint8_t u8AnalogPin);

/**
 * @brief Read calculated temperature in Celsius.
 *
 * @details
 * This function uses the Beta equation:
 *
 * @code
 * 1/T = 1/T0 + ln(R/R0) / B
 * @endcode
 *
 * Temperatures are calculated in Kelvin internally and converted to Celsius.
 *
 * @param[in] u8AnalogPin
 * Arduino-style analog pin connected to AO.
 *
 * @return float
 *
 * @retval Temperature
 * Calculated temperature in Celsius.
 *
 * @retval -273.15F
 * Error value when measurement or calculation fails.
 */
float NTC_ReadCelsius(uint8_t u8AnalogPin);

/**
 * @brief Read calculated temperature in Fahrenheit.
 *
 * @param[in] u8AnalogPin
 * Arduino-style analog pin connected to AO.
 *
 * @return float
 * Calculated temperature in Fahrenheit, or an error-derived value if the
 * Celsius read fails.
 */
float NTC_ReadFahrenheit(uint8_t u8AnalogPin);

/**
 * @brief Read optional digital threshold output from the NTC module.
 *
 * @details
 * This function is a thin wrapper around digitalRead().
 *
 * It is intended for 4-pin NTC modules that expose a comparator output
 * pin named DO. The threshold is adjusted by the trimmer potentiometer
 * on the module.
 *
 * @param[in] u8DigitalPin
 * Arduino-style digital pin connected to DO.
 *
 * @return uint8_t
 *
 * @retval 1U
 * Digital input is HIGH.
 *
 * @retval 0U
 * Digital input is LOW or the pin is invalid.
 */
uint8_t NTC_ReadThreshold(uint8_t u8DigitalPin);

#endif /* NTC_H */