#ifndef HARDWARE_SERIAL_H
#define HARDWARE_SERIAL_H

#include <stdint.h>
#include <stdbool.h>

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

/* Integer */
void Serial1_printInt(int value);
void Serial1_printlnInt(int value);

/* Float */
void Serial1_printFloat(float value);
void Serial1_printlnFloat(float value);

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

/* Integer */
void Serial2_printInt(int value);
void Serial2_printlnInt(int value);

/* Float */
void Serial2_printFloat(float value);
void Serial2_printlnFloat(float value);

#endif /* HARDWARE_SERIAL_H */
