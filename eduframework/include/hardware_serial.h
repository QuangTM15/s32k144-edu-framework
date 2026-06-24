#ifndef HARDWARE_SERIAL_H
#define HARDWARE_SERIAL_H

/**
 * @file hardware_serial.h
 * @brief Arduino-style hardware serial API interface.
 *
 * @details
 * This file declares user-facing serial APIs for EduFramework.
 *
 * The Hardware Serial layer wraps the low-level LPUART driver and provides
 * simpler Arduino-style APIs for demos and learning exercises.
 *
 * Current serial mapping:
 * - Serial1 uses LPUART1 and is intended for serial monitor/debug output
 *   through the board debug interface.
 * - Serial2 uses LPUART2 and is intended for external UART pins/header.
 *
 * This module belongs to the Arduino-style API layer.
 */

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * Serial1 API
 * ============================================================ */

/**
 * @brief Initialize Serial1.
 *
 * @details
 * Serial1 is mapped to LPUART1 and is intended for serial monitor or
 * debug output through the board debug interface.
 *
 * @param[in] u32BaudRate
 * Desired baud rate.
 *
 * @return None.
 */
void Serial1_begin(uint32_t u32BaudRate);

/**
 * @brief Check whether Serial1 has received data.
 *
 * @return bool
 *
 * @retval true
 * At least one byte is available.
 *
 * @retval false
 * No data is available.
 */
bool Serial1_available(void);

/**
 * @brief Read one character from Serial1.
 *
 * @return char
 * Received character, or '\0' if no data is available.
 */
char Serial1_read(void);

/**
 * @brief Read available Serial1 data into a string buffer.
 *
 * @param[out] pcBuffer
 * Destination buffer.
 *
 * @param[in] u32MaxLength
 * Maximum buffer length including null terminator.
 *
 * @return uint32_t
 * Number of characters copied into the buffer.
 */
uint32_t Serial1_readString(char *pcBuffer, uint32_t u32MaxLength);

/**
 * @brief Write one character to Serial1.
 *
 * @param[in] cCharacter
 * Character to transmit.
 *
 * @return None.
 */
void Serial1_write(char cCharacter);

/**
 * @brief Print a string to Serial1.
 *
 * @param[in] pcString
 * Null-terminated string.
 *
 * @return None.
 */
void Serial1_print(const char *pcString);

/**
 * @brief Print a string to Serial1 followed by CRLF.
 *
 * @param[in] pcString
 * Null-terminated string.
 *
 * @return None.
 */
void Serial1_println(const char *pcString);

/**
 * @brief Print a signed 32-bit integer to Serial1.
 *
 * @param[in] s32Value
 * Signed integer value.
 *
 * @return None.
 */
void Serial1_printInt(int32_t s32Value);

/**
 * @brief Print a signed 32-bit integer to Serial1 followed by CRLF.
 *
 * @param[in] s32Value
 * Signed integer value.
 *
 * @return None.
 */
void Serial1_printlnInt(int32_t s32Value);

/**
 * @brief Print a floating-point value to Serial1.
 *
 * @details
 * The current implementation prints three digits after the decimal point.
 *
 * @param[in] f32Value
 * Floating-point value.
 *
 * @return None.
 */
void Serial1_printFloat(float f32Value);

/**
 * @brief Print a floating-point value to Serial1 followed by CRLF.
 *
 * @details
 * The current implementation prints three digits after the decimal point.
 *
 * @param[in] f32Value
 * Floating-point value.
 *
 * @return None.
 */
void Serial1_printlnFloat(float f32Value);

/* ============================================================
 * Serial2 API
 * ============================================================ */

/**
 * @brief Initialize Serial2.
 *
 * @details
 * Serial2 is mapped to LPUART2 and is intended for external UART
 * pins/header communication.
 *
 * @param[in] u32BaudRate
 * Desired baud rate.
 *
 * @return None.
 */
void Serial2_begin(uint32_t u32BaudRate);

/**
 * @brief Check whether Serial2 has received data.
 *
 * @return bool
 *
 * @retval true
 * At least one byte is available.
 *
 * @retval false
 * No data is available.
 */
bool Serial2_available(void);

/**
 * @brief Read one character from Serial2.
 *
 * @return char
 * Received character, or '\0' if no data is available.
 */
char Serial2_read(void);

/**
 * @brief Read available Serial2 data into a string buffer.
 *
 * @param[out] pcBuffer
 * Destination buffer.
 *
 * @param[in] u32MaxLength
 * Maximum buffer length including null terminator.
 *
 * @return uint32_t
 * Number of characters copied into the buffer.
 */
uint32_t Serial2_readString(char *pcBuffer, uint32_t u32MaxLength);

/**
 * @brief Write one character to Serial2.
 *
 * @param[in] cCharacter
 * Character to transmit.
 *
 * @return None.
 */
void Serial2_write(char cCharacter);

/**
 * @brief Print a string to Serial2.
 *
 * @param[in] pcString
 * Null-terminated string.
 *
 * @return None.
 */
void Serial2_print(const char *pcString);

/**
 * @brief Print a string to Serial2 followed by CRLF.
 *
 * @param[in] pcString
 * Null-terminated string.
 *
 * @return None.
 */
void Serial2_println(const char *pcString);

/**
 * @brief Print a signed 32-bit integer to Serial2.
 *
 * @param[in] s32Value
 * Signed integer value.
 *
 * @return None.
 */
void Serial2_printInt(int32_t s32Value);

/**
 * @brief Print a signed 32-bit integer to Serial2 followed by CRLF.
 *
 * @param[in] s32Value
 * Signed integer value.
 *
 * @return None.
 */
void Serial2_printlnInt(int32_t s32Value);

/**
 * @brief Print a floating-point value to Serial2.
 *
 * @details
 * The current implementation prints three digits after the decimal point.
 *
 * @param[in] f32Value
 * Floating-point value.
 *
 * @return None.
 */
void Serial2_printFloat(float f32Value);

/**
 * @brief Print a floating-point value to Serial2 followed by CRLF.
 *
 * @details
 * The current implementation prints three digits after the decimal point.
 *
 * @param[in] f32Value
 * Floating-point value.
 *
 * @return None.
 */
void Serial2_printlnFloat(float f32Value);

#endif /* HARDWARE_SERIAL_H */
