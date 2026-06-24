#ifndef FTM_H
#define FTM_H

/**
 * @file ftm.h
 * @brief FlexTimer Module driver public interface.
 *
 * @details
 * This file provides the public configuration types and APIs for the
 * FlexTimer Module driver used in EduFramework.
 *
 * The FTM driver is responsible for:
 * - Initializing selected FTM instances.
 * - Starting and stopping the FTM counter.
 * - Configuring edge-aligned PWM channels.
 * - Setting PWM duty cycle by raw counter value or percentage.
 * - Updating PWM frequency by changing the modulo value.
 *
 * This driver belongs to the low-level driver layer and accesses S32K144
 * FTM hardware through register definitions.
 */

#include <stdint.h>
#include "S32K144.h"

/* ============================================================
 * FTM instance identifiers
 * ============================================================ */

/**
 * @brief FTM instance identifier type.
 *
 * @details
 * A fixed-width integer type is used instead of enum so the storage size
 * is explicit and consistent across compilers.
 */
typedef uint8_t FTM_Instance_t;

#define IP_FTM_0 ((FTM_Instance_t)0U)
#define IP_FTM_1 ((FTM_Instance_t)1U)
#define IP_FTM_2 ((FTM_Instance_t)2U)

/* ============================================================
 * FTM channel identifiers
 * ============================================================ */

/**
 * @brief FTM channel identifier type.
 *
 * @details
 * Each FTM instance provides up to eight channels in the current driver
 * scope. Channels are used by PWM output configuration.
 */
typedef uint8_t FTM_Channel_t;

#define FTM_CHANNEL_0 ((FTM_Channel_t)0U)
#define FTM_CHANNEL_1 ((FTM_Channel_t)1U)
#define FTM_CHANNEL_2 ((FTM_Channel_t)2U)
#define FTM_CHANNEL_3 ((FTM_Channel_t)3U)
#define FTM_CHANNEL_4 ((FTM_Channel_t)4U)
#define FTM_CHANNEL_5 ((FTM_Channel_t)5U)
#define FTM_CHANNEL_6 ((FTM_Channel_t)6U)
#define FTM_CHANNEL_7 ((FTM_Channel_t)7U)

/* ============================================================
 * FTM counter clock source
 * ============================================================ */

/**
 * @brief FTM counter clock source type.
 *
 * @details
 * These values are written to the FTM SC[CLKS] field by the driver when
 * starting or stopping the counter.
 */
typedef uint8_t FTM_ClockSource_t;

#define FTM_CLOCK_SOURCE_NONE ((FTM_ClockSource_t)0U)
#define FTM_CLOCK_SOURCE_SYSTEM ((FTM_ClockSource_t)1U)
#define FTM_CLOCK_SOURCE_FIXED ((FTM_ClockSource_t)2U)
#define FTM_CLOCK_SOURCE_EXTERNAL ((FTM_ClockSource_t)3U)

/* ============================================================
 * FTM prescaler
 * ============================================================ */

/**
 * @brief FTM prescaler configuration type.
 *
 * @details
 * These values are written to the FTM SC[PS] field. The driver also uses
 * this value to calculate the PWM modulo from source clock and target
 * PWM frequency.
 */
typedef uint8_t FTM_Prescaler_t;

#define FTM_PRESCALER_DIV_1 ((FTM_Prescaler_t)0U)
#define FTM_PRESCALER_DIV_2 ((FTM_Prescaler_t)1U)
#define FTM_PRESCALER_DIV_4 ((FTM_Prescaler_t)2U)
#define FTM_PRESCALER_DIV_8 ((FTM_Prescaler_t)3U)
#define FTM_PRESCALER_DIV_16 ((FTM_Prescaler_t)4U)
#define FTM_PRESCALER_DIV_32 ((FTM_Prescaler_t)5U)
#define FTM_PRESCALER_DIV_64 ((FTM_Prescaler_t)6U)
#define FTM_PRESCALER_DIV_128 ((FTM_Prescaler_t)7U)

/* ============================================================
 * FTM PWM mode
 * ============================================================ */

/**
 * @brief FTM PWM output polarity mode type.
 *
 * @details
 * The current driver supports edge-aligned PWM in high-true or low-true
 * mode. The Arduino analogWrite() path currently uses low-true mode.
 */
typedef uint8_t FTM_PwmMode_t;

#define FTM_PWM_EDGE_ALIGNED_HIGH_TRUE ((FTM_PwmMode_t)0U)
#define FTM_PWM_EDGE_ALIGNED_LOW_TRUE ((FTM_PwmMode_t)1U)

/* ============================================================
 * FTM driver status
 * ============================================================ */

/**
 * @brief FTM driver status type.
 *
 * @details
 * Driver APIs return this type to report operation result.
 */
typedef uint8_t FTM_Status_t;

#define FTM_STATUS_OK ((FTM_Status_t)0U)
#define FTM_STATUS_INVALID_INSTANCE ((FTM_Status_t)1U)
#define FTM_STATUS_INVALID_CHANNEL ((FTM_Status_t)2U)
#define FTM_STATUS_INVALID_CONFIG ((FTM_Status_t)3U)
#define FTM_STATUS_NOT_INIT ((FTM_Status_t)4U)

/* ============================================================
 * FTM configuration structures
 * ============================================================ */

/**
 * @brief FTM base counter configuration.
 *
 * @details
 * This structure is used by FTM_Init() to configure the base counter.
 */
typedef struct
{
    uint32_t srcClockHz;           /**< FTM source clock frequency in Hz. */
    FTM_ClockSource_t clockSource; /**< Counter clock source written to SC[CLKS]. */
    FTM_Prescaler_t prescaler;     /**< Counter prescaler written to SC[PS]. */
    uint16_t modulo;               /**< Counter modulo value written to MOD. */
} FTM_Config_t;

/**
 * @brief FTM PWM configuration.
 *
 * @details
 * This structure is used by FTM_InitPwm(). The driver calculates the
 * counter modulo from srcClockHz, prescaler, and pwmFreqHz.
 */
typedef struct
{
    uint32_t srcClockHz;           /**< FTM source clock frequency in Hz. */
    uint32_t pwmFreqHz;            /**< Target PWM frequency in Hz. */
    FTM_ClockSource_t clockSource; /**< Counter clock source written to SC[CLKS]. */
    FTM_Prescaler_t prescaler;     /**< Counter prescaler written to SC[PS]. */
} FTM_PwmConfig_t;

/* ============================================================
 * Core API
 * ============================================================ */

/**
 * @brief Initialize an FTM instance.
 *
 * @details
 * This function enables the selected FTM peripheral clock, disables write
 * protection, resets basic FTM control registers, configures counter
 * modulo, and stores the runtime state used by later FTM APIs.
 *
 * The counter remains stopped after initialization. Call
 * FTM_StartCounter() to start counting.
 *
 * @param[in] instance
 * FTM instance identifier.
 *
 * @param[in] config
 * Pointer to base FTM configuration.
 *
 * @return FTM_Status_t
 *
 * @retval FTM_STATUS_OK
 * FTM instance initialized successfully.
 *
 * @retval FTM_STATUS_INVALID_INSTANCE
 * Invalid FTM instance.
 *
 * @retval FTM_STATUS_INVALID_CONFIG
 * Invalid configuration pointer or field.
 */
FTM_Status_t FTM_Init(FTM_Instance_t instance, const FTM_Config_t *config);

/**
 * @brief Start an initialized FTM counter.
 *
 * @details
 * This function writes the configured clock source to SC[CLKS]. The
 * clock source is stored during FTM_Init().
 *
 * @param[in] instance
 * FTM instance identifier.
 *
 * @return FTM_Status_t
 *
 * @retval FTM_STATUS_OK
 * Counter started successfully.
 *
 * @retval FTM_STATUS_INVALID_INSTANCE
 * Invalid FTM instance.
 *
 * @retval FTM_STATUS_NOT_INIT
 * FTM instance has not been initialized.
 */
FTM_Status_t FTM_StartCounter(FTM_Instance_t instance);

/**
 * @brief Stop an initialized FTM counter.
 *
 * @details
 * This function clears SC[CLKS], which stops the FTM counter while
 * keeping the rest of the FTM configuration unchanged.
 *
 * @param[in] instance
 * FTM instance identifier.
 *
 * @return FTM_Status_t
 *
 * @retval FTM_STATUS_OK
 * Counter stopped successfully.
 *
 * @retval FTM_STATUS_INVALID_INSTANCE
 * Invalid FTM instance.
 *
 * @retval FTM_STATUS_NOT_INIT
 * FTM instance has not been initialized.
 */
FTM_Status_t FTM_StopCounter(FTM_Instance_t instance);

/* ============================================================
 * PWM API
 * ============================================================ */

/**
 * @brief Initialize an FTM instance for PWM generation.
 *
 * @details
 * This function calculates a modulo value from the requested PWM
 * frequency, source clock, and prescaler, then calls FTM_Init().
 *
 * The PWM channel mode still needs to be configured separately by
 * FTM_SetChannelModePwm().
 *
 * @param[in] instance
 * FTM instance identifier.
 *
 * @param[in] config
 * Pointer to PWM configuration.
 *
 * @return FTM_Status_t
 *
 * @retval FTM_STATUS_OK
 * PWM base timer initialized successfully.
 *
 * @retval FTM_STATUS_INVALID_INSTANCE
 * Invalid FTM instance.
 *
 * @retval FTM_STATUS_INVALID_CONFIG
 * Invalid PWM configuration.
 */
FTM_Status_t FTM_InitPwm(FTM_Instance_t instance, const FTM_PwmConfig_t *config);

/**
 * @brief Configure an FTM channel for edge-aligned PWM.
 *
 * @details
 * This function configures the selected channel control register for
 * edge-aligned PWM and enables PWM output control for that channel.
 *
 * @param[in] instance
 * FTM instance identifier.
 *
 * @param[in] channel
 * FTM channel identifier.
 *
 * @param[in] pwmMode
 * PWM output polarity mode.
 *
 * @return FTM_Status_t
 *
 * @retval FTM_STATUS_OK
 * Channel configured successfully.
 *
 * @retval FTM_STATUS_INVALID_INSTANCE
 * Invalid FTM instance.
 *
 * @retval FTM_STATUS_INVALID_CHANNEL
 * Invalid FTM channel.
 *
 * @retval FTM_STATUS_INVALID_CONFIG
 * Invalid PWM mode.
 *
 * @retval FTM_STATUS_NOT_INIT
 * FTM instance has not been initialized.
 */
FTM_Status_t FTM_SetChannelModePwm(FTM_Instance_t instance,
                                   FTM_Channel_t channel,
                                   FTM_PwmMode_t pwmMode);

/**
 * @brief Set PWM duty cycle using raw counter counts.
 *
 * @details
 * This function writes the channel compare value CnV. If dutyCounts is
 * greater than the current PWM period count, it is clamped to the maximum
 * valid duty count.
 *
 * @param[in] instance
 * FTM instance identifier.
 *
 * @param[in] channel
 * FTM channel identifier.
 *
 * @param[in] dutyCounts
 * Raw compare value used for PWM duty cycle.
 *
 * @return FTM_Status_t
 *
 * @retval FTM_STATUS_OK
 * Duty cycle updated successfully.
 *
 * @retval FTM_STATUS_INVALID_INSTANCE
 * Invalid FTM instance.
 *
 * @retval FTM_STATUS_INVALID_CHANNEL
 * Invalid FTM channel.
 *
 * @retval FTM_STATUS_NOT_INIT
 * FTM instance has not been initialized.
 */
FTM_Status_t FTM_SetPwmDuty(FTM_Instance_t instance,
                            FTM_Channel_t channel,
                            uint16_t dutyCounts);

/**
 * @brief Set PWM duty cycle using percent.
 *
 * @details
 * This function converts dutyPercent from 0..100 into raw counter counts
 * based on the current modulo value, then calls FTM_SetPwmDuty().
 *
 * @param[in] instance
 * FTM instance identifier.
 *
 * @param[in] channel
 * FTM channel identifier.
 *
 * @param[in] dutyPercent
 * Duty cycle percentage from 0U to 100U.
 *
 * @return FTM_Status_t
 *
 * @retval FTM_STATUS_OK
 * Duty cycle updated successfully.
 *
 * @retval FTM_STATUS_INVALID_INSTANCE
 * Invalid FTM instance.
 *
 * @retval FTM_STATUS_INVALID_CHANNEL
 * Invalid FTM channel.
 *
 * @retval FTM_STATUS_INVALID_CONFIG
 * Duty percentage is outside valid range.
 *
 * @retval FTM_STATUS_NOT_INIT
 * FTM instance has not been initialized.
 */
FTM_Status_t FTM_SetPwmDutyPercent(FTM_Instance_t instance,
                                   FTM_Channel_t channel,
                                   uint8_t dutyPercent);

/**
 * @brief Update PWM frequency of an initialized FTM instance.
 *
 * @details
 * This function recalculates MOD from the requested PWM frequency and
 * the stored source clock/prescaler. The counter clock selection is
 * temporarily cleared while MOD is updated, then restored.
 *
 * Existing channel modes are not reconfigured by this function.
 *
 * @param[in] instance
 * FTM instance identifier.
 *
 * @param[in] pwmFreqHz
 * Target PWM frequency in Hz.
 *
 * @return FTM_Status_t
 *
 * @retval FTM_STATUS_OK
 * Frequency updated successfully.
 *
 * @retval FTM_STATUS_INVALID_INSTANCE
 * Invalid FTM instance.
 *
 * @retval FTM_STATUS_INVALID_CONFIG
 * Invalid frequency or calculated modulo.
 *
 * @retval FTM_STATUS_NOT_INIT
 * FTM instance has not been initialized.
 */
FTM_Status_t FTM_SetPwmFrequency(FTM_Instance_t instance, uint32_t pwmFreqHz);

#endif /* FTM_H */