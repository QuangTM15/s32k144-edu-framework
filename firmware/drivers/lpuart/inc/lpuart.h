#ifndef LPUART_H
#define LPUART_H

#include "S32K144.h"
#include <stdint.h>
#include <stdbool.h>

/*
 * =====================================================================
 * LPUART Driver - EduFramework
 *
 * This driver provides:
 *  - Polling-based UART communication (TX/RX)
 *  - Interrupt-based RX support
 *  - Callback mechanism for upper layers
 *
 * NOTE:
 *  - This is a low-level driver (register-level)
 *  - No Arduino-style API here
 * =====================================================================
 */

/* ============================================================
 * Status type for driver functions
 * ============================================================ */
typedef enum
{
    LPUART_STATUS_OK = 0U,
    LPUART_STATUS_ERROR,
    LPUART_STATUS_INVALID_ARGUMENT,
    LPUART_STATUS_TIMEOUT
} LPUART_Status_t;

/* ============================================================
 * Configuration structure for LPUART
 * ============================================================ */
typedef struct
{
    uint32_t baudRate;     /* Desired baud rate (e.g. 9600) */
    uint32_t srcClockHz;   /* Source clock frequency (e.g. 8 MHz) */
} LPUART_Config_t;

/* ============================================================
 * Callback type for interrupt handling
 * ============================================================ */
typedef void (*LPUART_Callback_t)(void);

/* ============================================================
 * Initialization
 * ============================================================ */

/*
 * Initialize LPUART peripheral
 *
 * Parameters:
 *  - base: pointer to LPUART instance (IP_LPUART1, IP_LPUART2)
 *  - config: pointer to configuration structure
 *
 * Return:
 *  - status code
 */
LPUART_Status_t LPUART_Init(LPUART_Type *base, const LPUART_Config_t *config);

/* ============================================================
 * Polling APIs
 * ============================================================ */

/*
 * Send one character (blocking)
 */
void LPUART_WriteChar(LPUART_Type *base, char ch);

/*
 * Receive one character (blocking)
 */
char LPUART_ReadChar(LPUART_Type *base);

/*
 * Send null-terminated string
 */
void LPUART_WriteString(LPUART_Type *base, const char *str);

/*
 * Check if TX register is ready
 */
bool LPUART_IsTxReady(LPUART_Type *base);

/*
 * Check if RX data is available
 */
bool LPUART_IsRxReady(LPUART_Type *base);

/* ============================================================
 * Interrupt control APIs
 * ============================================================ */

/*
 * Enable RX interrupt
 */
void LPUART_EnableRxInterrupt(LPUART_Type *base);

/*
 * Disable RX interrupt
 */
void LPUART_DisableRxInterrupt(LPUART_Type *base);

/*
 * Enable TX interrupt
 */
void LPUART_EnableTxInterrupt(LPUART_Type *base);

/*
 * Disable TX interrupt
 */
void LPUART_DisableTxInterrupt(LPUART_Type *base);

/* ============================================================
 * Interrupt handling (Driver internal interface)
 * ============================================================ */

/*
 * This function must be called inside ISR
 *
 * It handles:
 *  - Reading received data
 *  - Clearing flags
 *  - Calling user callback
 */
void LPUART_IRQHandler(LPUART_Type *base);

/* ============================================================
 * Callback registration
 * ============================================================ */

/*
 * Register RX callback function
 */
void LPUART_SetRxCallback(LPUART_Type *base, LPUART_Callback_t callback);

/* ============================================================
 * Data access APIs (from interrupt)
 * ============================================================ */

/*
 * Get last received character (from interrupt)
 */
char LPUART_GetChar(LPUART_Type *base);

/*
 * Check if new data is available
 */
bool LPUART_IsDataAvailable(LPUART_Type *base);

#endif /* LPUART_H */
