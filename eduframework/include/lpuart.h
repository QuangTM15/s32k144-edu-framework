#ifndef LPUART_H
#define LPUART_H

/**
 * @file lpuart.h
 * @brief Low Power UART driver interface.
 *
 * @details
 * This file declares the public APIs for the LPUART driver used in
 * EduFramework.
 *
 * The LPUART driver is a low-level register driver. It is responsible for:
 * - Initializing LPUART peripheral instances.
 * - Configuring baud rate.
 * - Sending and receiving characters in polling mode.
 * - Enabling and disabling RX interrupts.
 * - Handling received data through an internal RX ring buffer.
 * - Registering an optional RX callback.
 *
 * This driver belongs to the Driver Layer and must not depend on the
 * Arduino-style API layer.
 */

#include "S32K144.h"

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief LPUART RX ring buffer size.
 *
 * @details
 * This value defines the number of bytes stored by the internal RX buffer
 * for each supported LPUART instance.
 */
#define LPUART_RX_BUFFER_SIZE                (32U)

/**
 * @brief LPUART status type.
 *
 * @details
 * The status type uses an explicitly sized integer instead of enum so that
 * its storage size is controlled by the project.
 */
typedef uint8_t LPUART_Status_t;

/**
 * @brief Operation completed successfully.
 */
#define LPUART_STATUS_OK                     ((LPUART_Status_t)0U)

/**
 * @brief General operation error.
 */
#define LPUART_STATUS_ERROR                  ((LPUART_Status_t)1U)

/**
 * @brief Invalid input argument.
 */
#define LPUART_STATUS_INVALID_ARGUMENT       ((LPUART_Status_t)2U)

/**
 * @brief Operation timeout.
 */
#define LPUART_STATUS_TIMEOUT                ((LPUART_Status_t)3U)

/**
 * @brief LPUART initialization configuration.
 *
 * @details
 * This structure provides the baud rate and source clock required to
 * configure the LPUART baud-rate generator.
 */
typedef struct
{
    /**
     * @brief Desired UART baud rate.
     *
     * @details
     * Example values are 9600, 19200, 115200.
     */
    uint32_t u32BaudRate;

    /**
     * @brief LPUART functional clock frequency in Hz.
     *
     * @details
     * This value must match the clock source selected for the LPUART
     * peripheral in the driver implementation.
     */
    uint32_t u32SrcClockHz;
} LPUART_Config_t;

/**
 * @brief LPUART callback function type.
 *
 * @details
 * A callback registered with LPUART_SetRxCallback() is executed from
 * interrupt context when a new byte is received.
 *
 * Callback functions must be short and must not call blocking APIs.
 */
typedef void (*LPUART_Callback_t)(void);

/**
 * @brief Initialize an LPUART peripheral instance.
 *
 * @details
 * This function configures the selected LPUART instance, including pins,
 * module clock, baud-rate generator, transmitter, receiver, and internal
 * RX buffer state.
 *
 * @param[in] pBase
 * Pointer to LPUART peripheral instance.
 *
 * @param[in] pConfig
 * Pointer to LPUART configuration structure.
 *
 * @return LPUART_Status_t
 *
 * @retval LPUART_STATUS_OK
 * Initialization successful.
 *
 * @retval LPUART_STATUS_INVALID_ARGUMENT
 * Invalid peripheral base pointer or configuration parameter.
 */
LPUART_Status_t LPUART_Init(LPUART_Type *pBase, const LPUART_Config_t *pConfig);

/**
 * @brief Check whether the transmit data register is ready.
 *
 * @param[in] pBase
 * Pointer to LPUART peripheral instance.
 *
 * @return bool
 *
 * @retval true
 * Transmit data register is ready.
 *
 * @retval false
 * Transmit data register is not ready.
 */
bool LPUART_IsTxReady(LPUART_Type *pBase);

/**
 * @brief Check whether received data is available in the hardware register.
 *
 * @param[in] pBase
 * Pointer to LPUART peripheral instance.
 *
 * @return bool
 *
 * @retval true
 * Hardware receive data flag is set.
 *
 * @retval false
 * No received data is available in the hardware register.
 */
bool LPUART_IsRxReady(LPUART_Type *pBase);

/**
 * @brief Write one character using polling mode.
 *
 * @details
 * This function waits until the transmit data register is ready, then writes
 * one character to the LPUART data register.
 *
 * @param[in] pBase
 * Pointer to LPUART peripheral instance.
 *
 * @param[in] cCharacter
 * Character to transmit.
 *
 * @return None.
 */
void LPUART_WriteChar(LPUART_Type *pBase, char cCharacter);

/**
 * @brief Read one character using polling mode.
 *
 * @details
 * This function waits until the receive data flag is set, then reads one
 * character from the LPUART data register.
 *
 * @param[in] pBase
 * Pointer to LPUART peripheral instance.
 *
 * @return char
 * Received character.
 */
char LPUART_ReadChar(LPUART_Type *pBase);

/**
 * @brief Write a null-terminated string using polling mode.
 *
 * @details
 * This function transmits each character until the null terminator is found.
 * A null string pointer is ignored by the implementation.
 *
 * @param[in] pBase
 * Pointer to LPUART peripheral instance.
 *
 * @param[in] pcString
 * Pointer to null-terminated string.
 *
 * @return None.
 */
void LPUART_WriteString(LPUART_Type *pBase, const char *pcString);

/**
 * @brief Enable LPUART receive interrupt.
 *
 * @param[in] pBase
 * Pointer to LPUART peripheral instance.
 *
 * @return None.
 */
void LPUART_EnableRxInterrupt(LPUART_Type *pBase);

/**
 * @brief Disable LPUART receive interrupt.
 *
 * @param[in] pBase
 * Pointer to LPUART peripheral instance.
 *
 * @return None.
 */
void LPUART_DisableRxInterrupt(LPUART_Type *pBase);

/**
 * @brief LPUART interrupt handler called by the IRQ layer.
 *
 * @details
 * This function handles receive interrupts by reading received data from
 * the hardware data register and storing it into the internal RX ring buffer.
 *
 * @param[in] pBase
 * Pointer to LPUART peripheral instance.
 *
 * @return None.
 */
void LPUART_IRQHandler(LPUART_Type *pBase);

/**
 * @brief Register RX callback for an LPUART instance.
 *
 * @details
 * The callback is executed from interrupt context when a new byte is received.
 * Passing a null callback disables callback execution.
 *
 * @param[in] pBase
 * Pointer to LPUART peripheral instance.
 *
 * @param[in] pfCallback
 * Pointer to RX callback function.
 *
 * @return None.
 */
void LPUART_SetRxCallback(LPUART_Type *pBase, LPUART_Callback_t pfCallback);

/**
 * @brief Get one character from the internal RX ring buffer.
 *
 * @details
 * This function reads one byte from the software RX ring buffer.
 * If the buffer is empty, the function returns '\0'.
 *
 * @param[in] pBase
 * Pointer to LPUART peripheral instance.
 *
 * @return char
 * Received character from the software RX buffer, or '\0' if empty.
 */
char LPUART_GetChar(LPUART_Type *pBase);

/**
 * @brief Check whether software RX buffer contains data.
 *
 * @param[in] pBase
 * Pointer to LPUART peripheral instance.
 *
 * @return bool
 *
 * @retval true
 * At least one byte is available in the RX buffer.
 *
 * @retval false
 * RX buffer is empty.
 */
bool LPUART_IsDataAvailable(LPUART_Type *pBase);

#endif /* LPUART_H */
