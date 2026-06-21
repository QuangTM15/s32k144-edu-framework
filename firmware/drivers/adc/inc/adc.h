#ifndef ADC_H
#define ADC_H

/**
 * @file adc.h
 * @brief ADC driver public interface.
 *
 * @details
 * This file provides the public configuration types and APIs for the
 * Analog-to-Digital Converter driver used in EduFramework.
 *
 * The ADC driver is responsible for:
 * - Initializing the selected ADC instance.
 * - Configuring software-triggered single conversion mode.
 * - Running ADC calibration.
 * - Starting interrupt-based conversions.
 * - Storing and returning the latest conversion result.
 *
 * This driver is part of the low-level driver layer and accesses
 * S32K144 ADC hardware through register definitions.
 */

#include <stdint.h>
#include "S32K144.h"

/* ============================================================
 * ADC instance identifiers
 * ============================================================ */

/**
 * @brief ADC instance identifier type.
 *
 * @details
 * The S32K144 device provides ADC instances that can be selected by
 * this type. A fixed-width integer type is used instead of enum to
 * keep the storage size explicit.
 */
typedef uint8_t ADC_Instance_t;

#define IP_ADC_0 ((ADC_Instance_t)0U)
#define IP_ADC_1 ((ADC_Instance_t)1U)

/* ============================================================
 * ADC channel identifiers
 * ============================================================ */

/**
 * @brief ADC channel identifier type.
 *
 * @details
 * EduFramework currently exposes only ADC0_SE12 and ADC0_SE13 through
 * the MaaZEDU board analog header. Other ADC channels are intentionally
 * not exposed through the Arduino-style analog API at this stage.
 */
typedef uint8_t ADC_Channel_t;

#define ADC_CHANNEL_SE12 ((ADC_Channel_t)12U)
#define ADC_CHANNEL_SE13 ((ADC_Channel_t)13U)

/* ============================================================
 * ADC resolution configuration
 * ============================================================ */

/**
 * @brief ADC resolution configuration type.
 *
 * @details
 * The macro values are intended to match the S32K144 ADC CFG1 MODE
 * field encoding used by the driver implementation.
 */
typedef uint8_t ADC_Resolution_t;

#define ADC_RESOLUTION_8BIT ((ADC_Resolution_t)0U)
#define ADC_RESOLUTION_10BIT ((ADC_Resolution_t)1U)
#define ADC_RESOLUTION_12BIT ((ADC_Resolution_t)2U)

/* ============================================================
 * ADC voltage reference configuration
 * ============================================================ */

/**
 * @brief ADC voltage reference selection type.
 *
 * @details
 * The default reference uses the VREFH/VREFL pins. Alternative
 * reference selection is available for hardware configurations that
 * support it.
 */
typedef uint8_t ADC_Reference_t;

#define ADC_REF_DEFAULT ((ADC_Reference_t)0U)
#define ADC_REF_ALT ((ADC_Reference_t)1U)

/* ============================================================
 * ADC hardware averaging configuration
 * ============================================================ */

/**
 * @brief ADC hardware averaging configuration type.
 *
 * @details
 * Hardware averaging can reduce conversion noise by averaging multiple
 * samples in hardware before producing the final result.
 */
typedef uint8_t ADC_Average_t;

#define ADC_AVERAGE_DISABLED ((ADC_Average_t)0U)
#define ADC_AVERAGE_4 ((ADC_Average_t)1U)
#define ADC_AVERAGE_8 ((ADC_Average_t)2U)
#define ADC_AVERAGE_16 ((ADC_Average_t)3U)
#define ADC_AVERAGE_32 ((ADC_Average_t)4U)

/* ============================================================
 * ADC driver status
 * ============================================================ */

/**
 * @brief ADC driver status type.
 *
 * @details
 * Driver APIs return this type to report whether an operation completed
 * successfully or failed because of invalid parameters, busy state, or
 * calibration failure.
 */
typedef uint8_t ADC_Status_t;

#define ADC_STATUS_OK ((ADC_Status_t)0U)
#define ADC_STATUS_BUSY ((ADC_Status_t)1U)
#define ADC_STATUS_INVALID_INSTANCE ((ADC_Status_t)2U)
#define ADC_STATUS_INVALID_CHANNEL ((ADC_Status_t)3U)
#define ADC_STATUS_INVALID_CONFIG ((ADC_Status_t)4U)
#define ADC_STATUS_NOT_INIT ((ADC_Status_t)5U)
#define ADC_STATUS_NO_RESULT ((ADC_Status_t)6U)
#define ADC_STATUS_CALIB_FAIL ((ADC_Status_t)7U)

/* ============================================================
 * ADC configuration structure
 * ============================================================ */

/**
 * @brief ADC initialization configuration.
 *
 * @details
 * This structure contains the basic runtime configuration used by
 * ADC_Init(). The current driver is designed for software-triggered
 * single conversions. Conversion completion can optionally generate
 * an ADC interrupt.
 */
typedef struct
{
    uint32_t srcClockHz;         /**< ADC input source clock frequency in Hz. */
    ADC_Resolution_t resolution; /**< ADC conversion resolution. */
    ADC_Reference_t reference;   /**< ADC voltage reference selection. */
    uint8_t sampleTime;          /**< ADC sample time setting used by the driver. */
    ADC_Average_t average;       /**< ADC hardware averaging selection. */
    uint8_t enableInterrupt;     /**< Interrupt enable state: 0U disabled, non-zero enabled. */
} ADC_Config_t;

/* ============================================================
 * ADC driver API
 * ============================================================ */

/**
 * @brief Initialize an ADC instance.
 *
 * @details
 * This function enables and configures the selected ADC peripheral for
 * software-triggered single conversion mode.
 *
 * The configuration includes:
 * - ADC functional clock selection.
 * - Conversion resolution.
 * - Voltage reference selection.
 * - Sample time.
 * - Optional hardware averaging.
 * - Optional conversion complete interrupt.
 *
 * ADC_Calibrate() should be called after successful initialization and
 * before normal conversions are started.
 *
 * @param[in] instance
 * ADC instance identifier.
 *
 * @param[in] config
 * Pointer to ADC configuration structure.
 *
 * @return ADC_Status_t
 *
 * @retval ADC_STATUS_OK
 * ADC instance initialized successfully.
 *
 * @retval ADC_STATUS_INVALID_INSTANCE
 * Invalid ADC instance.
 *
 * @retval ADC_STATUS_INVALID_CONFIG
 * Configuration pointer or configuration field is invalid.
 */
ADC_Status_t ADC_Init(ADC_Instance_t instance, const ADC_Config_t *config);

/**
 * @brief Run ADC calibration.
 *
 * @details
 * This function starts the ADC calibration sequence for the selected
 * instance and waits until the hardware reports calibration completion.
 *
 * Calibration should be executed after ADC_Init() and before the first
 * normal conversion to improve conversion accuracy.
 *
 * @param[in] instance
 * ADC instance identifier.
 *
 * @return ADC_Status_t
 *
 * @retval ADC_STATUS_OK
 * Calibration completed successfully.
 *
 * @retval ADC_STATUS_INVALID_INSTANCE
 * Invalid ADC instance.
 *
 * @retval ADC_STATUS_NOT_INIT
 * ADC instance has not been initialized.
 *
 * @retval ADC_STATUS_CALIB_FAIL
 * ADC hardware reported calibration failure.
 */
ADC_Status_t ADC_Calibrate(ADC_Instance_t instance);

/**
 * @brief Start one interrupt-based ADC conversion.
 *
 * @details
 * This function starts one software-triggered ADC conversion on the
 * requested ADC channel.
 *
 * Conversion completion is handled by the ADC interrupt. The interrupt
 * handler stores the conversion result inside the ADC driver internal
 * state. The result can later be checked with ADC_IsDone() and read by
 * ADC_GetResult().
 *
 * @param[in] instance
 * ADC instance identifier.
 *
 * @param[in] channel
 * ADC channel identifier.
 *
 * @return ADC_Status_t
 *
 * @retval ADC_STATUS_OK
 * Conversion started successfully.
 *
 * @retval ADC_STATUS_BUSY
 * A conversion is already active.
 *
 * @retval ADC_STATUS_INVALID_INSTANCE
 * Invalid ADC instance.
 *
 * @retval ADC_STATUS_INVALID_CHANNEL
 * Unsupported ADC channel.
 *
 * @retval ADC_STATUS_NOT_INIT
 * ADC instance has not been initialized.
 */
ADC_Status_t ADC_StartConversion_IT(ADC_Instance_t instance, ADC_Channel_t channel);

/**
 * @brief Check whether the latest ADC conversion is complete.
 *
 * @details
 * This function checks the ADC driver internal result-ready flag. It is
 * intended for non-blocking analog read flows.
 *
 * @param[in] instance
 * ADC instance identifier.
 *
 * @return uint8_t
 *
 * @retval 1U
 * Conversion result is available.
 *
 * @retval 0U
 * Conversion is not complete, no conversion has been started, or the
 * instance is invalid/not initialized.
 */
uint8_t ADC_IsDone(ADC_Instance_t instance);

/**
 * @brief Read the latest ADC conversion result.
 *
 * @details
 * This function copies the latest conversion result stored by the ADC
 * interrupt handler to the user-provided result pointer.
 *
 * Reading the result consumes the ready state in the driver
 * implementation.
 *
 * @param[in] instance
 * ADC instance identifier.
 *
 * @param[out] result
 * Pointer used to receive the raw ADC conversion result.
 *
 * @return ADC_Status_t
 *
 * @retval ADC_STATUS_OK
 * Result copied successfully.
 *
 * @retval ADC_STATUS_INVALID_INSTANCE
 * Invalid ADC instance.
 *
 * @retval ADC_STATUS_INVALID_CONFIG
 * Result pointer is invalid.
 *
 * @retval ADC_STATUS_NOT_INIT
 * ADC instance has not been initialized.
 *
 * @retval ADC_STATUS_NO_RESULT
 * No unread conversion result is available.
 */
ADC_Status_t ADC_GetResult(ADC_Instance_t instance, uint16_t *result);

/**
 * @brief ADC driver interrupt helper.
 *
 * @details
 * This function is called from the real MCU ADC ISR in irq.c. It checks
 * the ADC conversion-complete flag, reads the hardware result register,
 * and updates the ADC driver internal state.
 *
 * Application code should not call this function directly.
 *
 * @param[in] instance
 * ADC instance identifier.
 *
 * @return None.
 */
void ADC_IRQHandler(ADC_Instance_t instance);

#endif /* ADC_H */