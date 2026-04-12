#ifndef LPUART_H
#define LPUART_H

#include "S32K144.h"
#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * Configuration
 * ============================================================ */
#define LPUART_RX_BUFFER_SIZE    (32U)

/* ============================================================
 * Status type
 * ============================================================ */
typedef enum
{
    LPUART_STATUS_OK = 0U,
    LPUART_STATUS_ERROR,
    LPUART_STATUS_INVALID_ARGUMENT,
    LPUART_STATUS_TIMEOUT
} LPUART_Status_t;

/* ============================================================
 * LPUART configuration
 * ============================================================ */
typedef struct
{
    uint32_t baudRate;
    uint32_t srcClockHz;
} LPUART_Config_t;

/* ============================================================
 * Callback type
 * ============================================================ */
typedef void (*LPUART_Callback_t)(void);

/* ============================================================
 * Initialization
 * ============================================================ */
LPUART_Status_t LPUART_Init(LPUART_Type *base, const LPUART_Config_t *config);

/* ============================================================
 * Polling APIs
 * ============================================================ */
void LPUART_WriteChar(LPUART_Type *base, char ch);
char LPUART_ReadChar(LPUART_Type *base);
void LPUART_WriteString(LPUART_Type *base, const char *str);

bool LPUART_IsTxReady(LPUART_Type *base);
bool LPUART_IsRxReady(LPUART_Type *base);

/* ============================================================
 * Interrupt control APIs
 * ============================================================ */
void LPUART_EnableRxInterrupt(LPUART_Type *base);
void LPUART_DisableRxInterrupt(LPUART_Type *base);

/* ============================================================
 * Interrupt handler
 * ============================================================ */
void LPUART_IRQHandler(LPUART_Type *base);

/* ============================================================
 * Callback
 * ============================================================ */
void LPUART_SetRxCallback(LPUART_Type *base, LPUART_Callback_t callback);

/* ============================================================
 * RX buffer access APIs
 * ============================================================ */
char LPUART_GetChar(LPUART_Type *base);
bool LPUART_IsDataAvailable(LPUART_Type *base);

#endif /* LPUART_H */
