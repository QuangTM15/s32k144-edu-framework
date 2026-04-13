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
/* Board currently supports SE12, SE13 */
typedef enum
{
    ADC_CHANNEL_SE12 = 12U,
    ADC_CHANNEL_SE13 = 13U
} ADC_Channel_t;

/* ===================== RESOLUTION ===================== */
typedef enum
{
    ADC_RESOLUTION_8BIT,
    ADC_RESOLUTION_10BIT,
    ADC_RESOLUTION_12BIT
} ADC_Resolution_t;

/* ===================== REFERENCE ===================== */
typedef enum
{
    ADC_REF_DEFAULT,   /* VREFH/VREFL */
    ADC_REF_ALT
} ADC_Reference_t;

/* ===================== AVERAGE ===================== */
typedef enum
{
    ADC_AVERAGE_DISABLED = 0U,
    ADC_AVERAGE_4        = 4U,
    ADC_AVERAGE_8        = 8U,
    ADC_AVERAGE_16       = 16U,
    ADC_AVERAGE_32       = 32U
} ADC_Average_t;

/* ===================== STATUS ===================== */
typedef enum
{
    ADC_STATUS_OK = 0U,
    ADC_STATUS_BUSY,
    ADC_STATUS_INVALID,
    ADC_STATUS_NOT_INIT,
    ADC_STATUS_NO_RESULT,
    ADC_STATUS_CALIB_FAIL
} ADC_Status_t;

/* ===================== CONFIG ===================== */
typedef struct
{
    uint32_t srcClockHz;
    ADC_Resolution_t resolution;
    ADC_Reference_t reference;
    uint8_t sampleTime;
    ADC_Average_t average;
    uint8_t enableInterrupt;
} ADC_Config_t;

/* ===================== API ===================== */

/* Init & calibration */
ADC_Status_t ADC_Init(ADC_Instance_t instance, const ADC_Config_t *config);
ADC_Status_t ADC_Calibrate(ADC_Instance_t instance);

/* Conversion (interrupt-based) */
ADC_Status_t ADC_StartConversion_IT(ADC_Instance_t instance, ADC_Channel_t channel);
uint8_t      ADC_IsDone(ADC_Instance_t instance);
ADC_Status_t ADC_GetResult(ADC_Instance_t instance, uint16_t *result);

/* IRQ handler (call inside ISR) */
void ADC_IRQHandler(ADC_Instance_t instance);

#endif /* ADC_H */
