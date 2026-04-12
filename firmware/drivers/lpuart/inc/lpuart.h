#ifndef LPUART_H
#define LPUART_H

#include "S32K144.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    LPUART_INSTANCE_0 = 0U,
    LPUART_INSTANCE_1 = 1U,
    LPUART_INSTANCE_2 = 2U
} LPUART_Instance_t;

typedef enum
{
    LPUART_STATUS_OK = 0U,
    LPUART_STATUS_ERROR,
    LPUART_STATUS_INVALID_ARGUMENT,
    LPUART_STATUS_TIMEOUT
} LPUART_Status_t;

typedef struct
{
    uint32_t baudRate;
    uint32_t srcClockHz;
} LPUART_Config_t;

typedef void (*LPUART_Callback_t)(void);

LPUART_Status_t LPUART_Init(LPUART_Type *base, const LPUART_Config_t *config);

void LPUART_WriteChar(LPUART_Type *base, char ch);
char LPUART_ReadChar(LPUART_Type *base);
void LPUART_WriteString(LPUART_Type *base, const char *str);

bool LPUART_IsTxReady(LPUART_Type *base);
bool LPUART_IsRxReady(LPUART_Type *base);

void LPUART_EnableRxInterrupt(LPUART_Type *base);
void LPUART_DisableRxInterrupt(LPUART_Type *base);

void LPUART_EnableTxInterrupt(LPUART_Type *base);
void LPUART_DisableTxInterrupt(LPUART_Type *base);

void LPUART_SetRxCallback(LPUART_Type *base, LPUART_Callback_t callback);
void LPUART_SetTxCallback(LPUART_Type *base, LPUART_Callback_t callback);

#endif /* LPUART_H */
