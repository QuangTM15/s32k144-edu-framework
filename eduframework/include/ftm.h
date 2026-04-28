#ifndef FTM_H
#define FTM_H

#include <stdint.h>
#include "S32K144.h"

/* ============================================================
 * Instance
 * ============================================================ */
typedef enum
{
    IP_FTM_0 = 0U,
    IP_FTM_1 = 1U,
    IP_FTM_2 = 2U
} FTM_Instance_t;

/* ============================================================
 * Channel
 * ============================================================ */
typedef enum
{
    FTM_CHANNEL_0 = 0U,
    FTM_CHANNEL_1 = 1U,
    FTM_CHANNEL_2 = 2U,
    FTM_CHANNEL_3 = 3U,
    FTM_CHANNEL_4 = 4U,
    FTM_CHANNEL_5 = 5U,
    FTM_CHANNEL_6 = 6U,
    FTM_CHANNEL_7 = 7U
} FTM_Channel_t;

/* ============================================================
 * Clock source
 * ============================================================ */
typedef enum
{
    FTM_CLOCK_SOURCE_NONE     = 0U,
    FTM_CLOCK_SOURCE_SYSTEM   = 1U,
    FTM_CLOCK_SOURCE_FIXED    = 2U,
    FTM_CLOCK_SOURCE_EXTERNAL = 3U
} FTM_ClockSource_t;

/* ============================================================
 * Prescaler
 * ============================================================ */
typedef enum
{
    FTM_PRESCALER_DIV_1 = 0U,
    FTM_PRESCALER_DIV_2,
    FTM_PRESCALER_DIV_4,
    FTM_PRESCALER_DIV_8,
    FTM_PRESCALER_DIV_16,
    FTM_PRESCALER_DIV_32,
    FTM_PRESCALER_DIV_64,
    FTM_PRESCALER_DIV_128
} FTM_Prescaler_t;

/* ============================================================
 * PWM mode
 * ============================================================ */
typedef enum
{
    FTM_PWM_EDGE_ALIGNED_HIGH_TRUE = 0U,
    FTM_PWM_EDGE_ALIGNED_LOW_TRUE
} FTM_PwmMode_t;

/* ============================================================
 * Status
 * ============================================================ */
typedef enum
{
    FTM_STATUS_OK = 0U,
    FTM_STATUS_INVALID_INSTANCE,
    FTM_STATUS_INVALID_CHANNEL,
    FTM_STATUS_INVALID_CONFIG,
    FTM_STATUS_NOT_INIT
} FTM_Status_t;

/* ============================================================
 * Base configuration
 * ============================================================ */
typedef struct
{
    uint32_t srcClockHz;
    FTM_ClockSource_t clockSource;
    FTM_Prescaler_t prescaler;
    uint16_t modulo;
} FTM_Config_t;

/* ============================================================
 * PWM configuration
 * ============================================================ */
typedef struct
{
    uint32_t srcClockHz;
    uint32_t pwmFreqHz;
    FTM_ClockSource_t clockSource;
    FTM_Prescaler_t prescaler;
} FTM_PwmConfig_t;

/* ============================================================
 * Core API
 * ============================================================ */
FTM_Status_t FTM_Init(FTM_Instance_t instance, const FTM_Config_t *config);
FTM_Status_t FTM_StartCounter(FTM_Instance_t instance);
FTM_Status_t FTM_StopCounter(FTM_Instance_t instance);

/* ============================================================
 * PWM API
 * ============================================================ */
FTM_Status_t FTM_InitPwm(FTM_Instance_t instance, const FTM_PwmConfig_t *config);

FTM_Status_t FTM_SetChannelModePwm(FTM_Instance_t instance,
                                   FTM_Channel_t channel,
                                   FTM_PwmMode_t pwmMode);

FTM_Status_t FTM_SetPwmDuty(FTM_Instance_t instance,
                            FTM_Channel_t channel,
                            uint16_t dutyCounts);

FTM_Status_t FTM_SetPwmDutyPercent(FTM_Instance_t instance,
                                   FTM_Channel_t channel,
                                   uint8_t dutyPercent);

FTM_Status_t FTM_SetPwmFrequency(FTM_Instance_t instance, uint32_t pwmFreqHz);

#endif /* FTM_H */
