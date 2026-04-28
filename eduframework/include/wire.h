#ifndef WIRE_H
#define WIRE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    WIRE_STATUS_OK = 0,
    WIRE_STATUS_BUSY,
    WIRE_STATUS_TIMEOUT,
    WIRE_STATUS_NACK,
    WIRE_STATUS_ERROR
} Wire_Status_t;

/* ============================================================
 * Init
 * ============================================================ */

/* Master mode */
void Wire_begin(void);

/* Slave mode */
void Wire_beginAddress(uint8_t address);

/* Set I2C clock (100k, 400k...) */
void Wire_setClock(uint32_t frequency);

/* ============================================================
 * Master - Write (TX)
 * ============================================================ */

void Wire_beginTransmission(uint8_t address);

uint8_t Wire_write(uint8_t data);

uint8_t Wire_writeBuffer(const uint8_t *data, uint8_t length);

/* Return status */
uint8_t Wire_endTransmission(void);

/* ============================================================
 * Master - Read (RX)
 * ============================================================ */

/* Return number of bytes received */
uint8_t Wire_requestFrom(uint8_t address, uint8_t quantity);

int Wire_available(void);

int Wire_read(void);

/* ============================================================
 * Slave API
 * ============================================================ */

void Wire_onReceive(void (*callback)(int));

void Wire_onRequest(void (*callback)(void));

/* ============================================================
 * Optional - Async (Interrupt-based)
 * ============================================================ */

/* Start async transfer (non-blocking) */
uint8_t Wire_transferAsync(uint8_t address,
                           const uint8_t *txData,
                           uint16_t txSize,
                           uint8_t *rxData,
                           uint16_t rxSize);

/* State */
uint8_t Wire_isBusy(void);
uint8_t Wire_isDone(void);

/* Get last status */
uint8_t Wire_getStatus(void);

#endif /* WIRE_H */
