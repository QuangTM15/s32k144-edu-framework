#ifndef HARDWARE_SERIAL_H
#define HARDWARE_SERIAL_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * Arduino-style Hardware Serial API
 *
 * Mapping:
 *  - Serial1 -> LPUART1
 *  - Serial2 -> LPUART2
 * ============================================================ */

/* ============================================================
 * Serial1 API
 * ============================================================ */
void Serial1_begin(uint32_t baudRate);

bool Serial1_available(void);
char Serial1_read(void);
uint32_t Serial1_readString(char *buffer, uint32_t maxLength);

void Serial1_write(char ch);

void Serial1_print(const char *str);
void Serial1_println(const char *str);

void Serial1_printNumber(float number);
void Serial1_printlnNumber(float number);

/* ============================================================
 * Serial2 API
 * ============================================================ */
void Serial2_begin(uint32_t baudRate);

bool Serial2_available(void);
char Serial2_read(void);
uint32_t Serial2_readString(char *buffer, uint32_t maxLength);

void Serial2_write(char ch);

void Serial2_print(const char *str);
void Serial2_println(const char *str);

void Serial2_printNumber(float number);
void Serial2_printlnNumber(float number);

#endif /* HARDWARE_SERIAL_H */
