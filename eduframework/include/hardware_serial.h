#ifndef HARDWARE_SERIAL_H
#define HARDWARE_SERIAL_H

/**
 * @file hardware_serial.h
 * @brief Arduino-style hardware serial API interface.
 *
 * @details
 * This module provides user-facing serial APIs for EduFramework.
 *
 * Serial mapping:
 * - Serial1: LPUART1, serial monitor and debug interface.
 * - Serial2: LPUART2, external UART communication.
 *
 * Both instances use the SPLL_DIV2 clock source at 40 MHz
 * with the current EduFramework system clock configuration.
 *
 * @note The system clock must be initialized before calling
 * Serial1_begin() or Serial2_begin().
 */

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * Serial1 API
 * ============================================================ */

/**
 * @brief Initialize Serial1.
 *
 * @param[in] u32BaudRate Desired baud rate.
 *
 * @return None.
 *
 * @note Initialization failures are not reported through
 * this legacy API.
 */
void Serial1_begin(uint32_t u32BaudRate);

/**
 * @brief Check whether Serial1 has received data.
 *
 * @retval true At least one byte is available.
 * @retval false No data is available.
 */
bool Serial1_available(void);

/**
 * @brief Read one character from Serial1.
 *
 * @return Received character, or '\0' if no data is available.
 *
 * @note The return value cannot distinguish an empty buffer
 * from a received binary zero byte.
 */
char Serial1_read(void);

/**
 * @brief Read available Serial1 data into a string buffer.
 *
 * @details
 * Reads until no more data is available, newline is found,
 * or the destination buffer is full.
 *
 * Carriage return characters are ignored.
 *
 * This function is non-blocking and does not wait for
 * a complete line.
 *
 * @param[out] pcBuffer Destination buffer.
 * @param[in] u32MaxLength Buffer capacity including '\0'.
 *
 * @return Number of characters copied.
 */
uint32_t Serial1_readString(char *pcBuffer, uint32_t u32MaxLength);

/**
 * @brief Transmit one character through Serial1.
 *
 * @param[in] cCharacter Character to transmit.
 */
void Serial1_write(char cCharacter);

/**
 * @brief Print a null-terminated string through Serial1.
 *
 * @param[in] pcString String to transmit.
 */
void Serial1_print(const char *pcString);

/**
 * @brief Print a string followed by CRLF.
 *
 * @param[in] pcString String to transmit.
 */
void Serial1_println(const char *pcString);

/**
 * @brief Print a signed 32-bit integer.
 *
 * @param[in] s32Value Integer value.
 */
void Serial1_printInt(int32_t s32Value);

/**
 * @brief Print a signed 32-bit integer followed by CRLF.
 *
 * @param[in] s32Value Integer value.
 */
void Serial1_printlnInt(int32_t s32Value);

/**
 * @brief Print a floating-point value with three decimals.
 *
 * @param[in] f32Value Floating-point value.
 */
void Serial1_printFloat(float f32Value);

/**
 * @brief Print a floating-point value followed by CRLF.
 *
 * @param[in] f32Value Floating-point value.
 */
void Serial1_printlnFloat(float f32Value);

/**
 * @brief Read a complete line from Serial1.
 *
 * @details
 * Accumulates received characters across calls.
 * Returns true only when a newline character is received.
 *
 * Carriage return characters are ignored.
 * The destination is always null-terminated when a line
 * is successfully returned.
 *
 * If a line exceeds the internal capacity or the supplied
 * buffer capacity, the entire line is discarded.
 *
 * @param[out] pcBuffer Destination string buffer.
 * @param[in] u32MaxLength Buffer capacity including terminator.
 *
 * @retval true A complete line was received.
 * @retval false No complete valid line is available.
 */
bool Serial1_readLine(char *pcBuffer, uint32_t u32MaxLength);

/**
 * @brief Transmit a binary data buffer through Serial1.
 *
 * @param[in] pu8Data Pointer to the source data buffer.
 * @param[in] u32Length Number of bytes to transmit.
 *
 * @return Number of bytes submitted for transmission.
 */
uint32_t Serial1_writeBytes(const uint8_t *pu8Data, uint32_t u32Length);

/**
 * @brief Read available binary data from Serial1.
 *
 * @details
 * This function is non-blocking and returns immediately
 * when no more received data is available.
 *
 * @param[out] pu8Buffer Destination data buffer.
 * @param[in] u32MaxLength Maximum number of bytes to read.
 *
 * @return Number of bytes copied into the destination buffer.
 */
uint32_t Serial1_readBytes(uint8_t *pu8Buffer, uint32_t u32MaxLength);

/* ============================================================
 * Serial2 API
 * ============================================================ */

/**
 * @brief Initialize Serial2.
 *
 * @param[in] u32BaudRate Desired baud rate.
 *
 * @return None.
 */
void Serial2_begin(uint32_t u32BaudRate);

/**
 * @brief Check whether Serial2 has received data.
 *
 * @retval true At least one byte is available.
 * @retval false No data is available.
 */
bool Serial2_available(void);

/**
 * @brief Read one character from Serial2.
 *
 * @return Received character, or '\0' if no data is available.
 */
char Serial2_read(void);

/**
 * @brief Read available Serial2 data into a string buffer.
 *
 * @param[out] pcBuffer Destination buffer.
 * @param[in] u32MaxLength Buffer capacity including '\0'.
 *
 * @return Number of characters copied.
 */
uint32_t Serial2_readString(char *pcBuffer, uint32_t u32MaxLength);

/**
 * @brief Transmit one character through Serial2.
 *
 * @param[in] cCharacter Character to transmit.
 */
void Serial2_write(char cCharacter);

/**
 * @brief Print a null-terminated string through Serial2.
 *
 * @param[in] pcString String to transmit.
 */
void Serial2_print(const char *pcString);

/**
 * @brief Print a string followed by CRLF.
 *
 * @param[in] pcString String to transmit.
 */
void Serial2_println(const char *pcString);

/**
 * @brief Print a signed 32-bit integer.
 *
 * @param[in] s32Value Integer value.
 */
void Serial2_printInt(int32_t s32Value);

/**
 * @brief Print a signed 32-bit integer followed by CRLF.
 *
 * @param[in] s32Value Integer value.
 */
void Serial2_printlnInt(int32_t s32Value);

/**
 * @brief Print a floating-point value with three decimals.
 *
 * @param[in] f32Value Floating-point value.
 */
void Serial2_printFloat(float f32Value);

/**
 * @brief Print a floating-point value followed by CRLF.
 *
 * @param[in] f32Value Floating-point value.
 */
void Serial2_printlnFloat(float f32Value);

/**
 * @brief Read a complete line from Serial2.
 *
 * @details
 * Non-blocking line receiver with independent state.
 * See Serial1_readLine() for detailed behavior.
 *
 * @param[out] pcBuffer Destination string buffer.
 * @param[in] u32MaxLength Buffer capacity including terminator.
 *
 * @retval true A complete line was received.
 * @retval false No complete valid line is available.
 */
bool Serial2_readLine(char *pcBuffer, uint32_t u32MaxLength);

/**
 * @brief Transmit binary data through Serial2.
 *
 * @param[in] pu8Data Pointer to the source data buffer.
 * @param[in] u32Length Number of bytes to transmit.
 *
 * @return Number of bytes submitted for transmission.
 */
uint32_t Serial2_writeBytes(const uint8_t *pu8Data, uint32_t u32Length);

/**
 * @brief Read available binary data from Serial2.
 *
 * @details
 * This function is non-blocking and preserves all byte values,
 * including 0x00, 0x0A and 0xFF.
 *
 * @param[out] pu8Buffer Destination buffer.
 * @param[in] u32MaxLength Maximum number of bytes to read.
 *
 * @return Number of bytes copied into the destination buffer.
 */
uint32_t Serial2_readBytes(uint8_t *pu8Buffer, uint32_t u32MaxLength);

#endif /* HARDWARE_SERIAL_H */
