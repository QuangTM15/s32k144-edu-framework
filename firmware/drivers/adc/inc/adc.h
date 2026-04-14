#ifndef ADC_H
#define ADC_H

#include <stdint.h>
#include "S32K144.h"

/* ===================== INSTANCE ===================== */
typedef enum
{
    IP_ADC_0 = 0U,
    IP_ADC_1 = 1U
} ADC_Instance_t;

/* ===================== CHANNEL ===================== */
/* Current board usage is focused on ADC0_SE12 and ADC0_SE13 */
typedef enum
{
    ADC_CHANNEL_SE12 = 12U,
    ADC_CHANNEL_SE13 = 13U
} ADC_Channel_t;

/* ===================== RESOLUTION ===================== */
typedef enum
{
    ADC_RESOLUTION_8BIT = 0U,
    ADC_RESOLUTION_10BIT,
    ADC_RESOLUTION_12BIT
} ADC_Resolution_t;

/* ===================== REFERENCE ===================== */
typedef enum
{
    ADC_REF_DEFAULT = 0U, /* VREFH / VREFL */
    ADC_REF_ALT           /* Alternative reference */
} ADC_Reference_t;

/* ===================== HARDWARE AVERAGE ===================== */
typedef enum
{
    ADC_AVERAGE_DISABLED = 0U,
    ADC_AVERAGE_4,
    ADC_AVERAGE_8,
    ADC_AVERAGE_16,
    ADC_AVERAGE_32
} ADC_Average_t;

/* ===================== STATUS ===================== */
typedef enum
{
    ADC_STATUS_OK = 0U,
    ADC_STATUS_BUSY,
    ADC_STATUS_INVALID_INSTANCE,
    ADC_STATUS_INVALID_CHANNEL,
    ADC_STATUS_INVALID_CONFIG,
    ADC_STATUS_NOT_INIT,
    ADC_STATUS_NO_RESULT,
    ADC_STATUS_CALIB_FAIL
} ADC_Status_t;

/* ===================== CONFIG ===================== */
typedef struct
{
    uint32_t srcClockHz;                 /* ADC input source clock frequency */
    ADC_Resolution_t resolution;         /* 8-bit / 10-bit / 12-bit */
    ADC_Reference_t reference;           /* Voltage reference selection */
    uint8_t sampleTime;                  /* Driver-defined sample time setting */
    ADC_Average_t average;               /* Hardware average selection */
    uint8_t enableInterrupt;             /* 0: disable interrupt, 1: enable interrupt */
} ADC_Config_t;

/* ===================== DRIVER API ===================== */
/*
 * ADC_Init:
 * - Enable/configure ADC peripheral clock
 * - Configure ADC operating mode for software-triggered single conversion
 * - Configure resolution, reference, sample time, averaging
 * - Initialize internal driver state
 */
ADC_Status_t ADC_Init(ADC_Instance_t instance, const ADC_Config_t *config);

/*
 * ADC_Calibrate:
 * - Run ADC calibration sequence
 * - Must be called after ADC_Init() and before normal conversions
 */
ADC_Status_t ADC_Calibrate(ADC_Instance_t instance);

/*
 * ADC_StartConversion_IT:
 * - Start one software-triggered conversion
 * - Conversion completion is handled by ADC interrupt
 * - Result is latched into driver internal state inside ADC_IRQHandler()
 */
ADC_Status_t ADC_StartConversion_IT(ADC_Instance_t instance, ADC_Channel_t channel);

/*
 * ADC_IsDone:
 * - Return 1 if a conversion result is available
 * - Return 0 if conversion is still running or no result is ready
 */
uint8_t ADC_IsDone(ADC_Instance_t instance);

/*
 * ADC_GetResult:
 * - Read the last conversion result captured by interrupt handler
 * - Returns ADC_STATUS_NO_RESULT if conversion is not completed yet
 */
ADC_Status_t ADC_GetResult(ADC_Instance_t instance, uint16_t *result);

/*
 * ADC_IRQHandler:
 * - Driver ISR helper
 * - Must be called from real MCU ISR in irq.c
 */
void ADC_IRQHandler(ADC_Instance_t instance);

#endif /* ADC_H */
