#ifndef LPSPI_H
#define LPSPI_H

/**
 * @file lpspi.h
 * @brief Blocking LPSPI0 driver for NXP S32K144 / EduFramework.
 * @details v3.1: Workarounds for S32K144 ERR051472 and ERR010655.
 *
 * @details
 * SPI bus and clock configuration are owned by this driver. GPIO chip-select
 * lines are owned by the SPI wrapper or device layer, never by this driver.
 * Logical slave count is not limited by this interface. The practical limit
 * comes from GPIO availability, wiring, loading and bus arbitration.
 *
 * This is a polling implementation (no DMA or IRQ transfers). It is not
 * reentrant; only one application context may access LPSPI0 at a time.
 * PORT pin mux must be configured before use by the board/SPI layer.
 * The LPSPI0 functional clock must be SPLL_DIV2 = 40 MHz in this build.
 */

#include <stdbool.h>
#include <stdint.h>
#include "S32K144.h"

#define LPSPI_INSTANCE IP_LPSPI0

typedef uint8_t lpspi_status_t;
#define LPSPI_STATUS_OK          ((lpspi_status_t)0U)
#define LPSPI_STATUS_ERROR       ((lpspi_status_t)1U)
#define LPSPI_STATUS_INVALID_ARG ((lpspi_status_t)2U)
#define LPSPI_STATUS_NOT_INIT    ((lpspi_status_t)3U)
#define LPSPI_STATUS_TIMEOUT     ((lpspi_status_t)4U)
#define LPSPI_STATUS_FIFO_ERROR  ((lpspi_status_t)5U)

typedef uint8_t lpspi_mode_t;
#define LPSPI_MODE_MASTER ((lpspi_mode_t)0U)
#define LPSPI_MODE_SLAVE  ((lpspi_mode_t)1U)

typedef uint8_t lpspi_clock_mode_t;
#define LPSPI_MODE0 ((lpspi_clock_mode_t)0U)
#define LPSPI_MODE1 ((lpspi_clock_mode_t)1U)
#define LPSPI_MODE2 ((lpspi_clock_mode_t)2U)
#define LPSPI_MODE3 ((lpspi_clock_mode_t)3U)

typedef uint8_t lpspi_bit_order_t;
#define LPSPI_MSB_FIRST ((lpspi_bit_order_t)0U)
#define LPSPI_LSB_FIRST ((lpspi_bit_order_t)1U)

typedef uint8_t lpspi_frameSize_t;
#define LPSPI_FRAME_SIZE_8  ((lpspi_frameSize_t)8U)
#define LPSPI_FRAME_SIZE_16 ((lpspi_frameSize_t)16U)

/** @brief Peripheral mode and initial SPI settings. */
typedef struct
{
    lpspi_mode_t mode;
    lpspi_clock_mode_t clockMode;
    lpspi_bit_order_t bitOrder;
    lpspi_frameSize_t frameSize;
    uint32_t baudrate;
} lpspi_config_t;

/** @brief Independent settings for one logical SPI transaction. */
typedef struct
{
    lpspi_clock_mode_t clockMode;
    lpspi_bit_order_t bitOrder;
    lpspi_frameSize_t frameSize;
    uint32_t baudrate;
} lpspi_settings_t;

/** @brief Initialize LPSPI0 once; requires configured functional clock and pin mux. */
lpspi_status_t LPSPI_Init(const lpspi_config_t *config);

/** @brief Enable or disable the initialized peripheral without discarding settings. */
void LPSPI_Enable(void);
void LPSPI_Disable(void);

/**
 * @brief Change all bus settings for the next SPI transaction.
 * @details Must be called with every external GPIO CS inactive. Validates
 * every parameter before touching hardware; preserves prior enable state.
 * In master mode, the requested SCK is rounded down to a supported rate
 * and capped at 10 MHz for the current S32K144 RUN clock configuration.
 */
lpspi_status_t LPSPI_ApplySettings(const lpspi_settings_t *settings);

/** @brief Wait until the command queue is empty and hardware is idle. */
lpspi_status_t LPSPI_WaitIdle(void);

/**
 * @brief Clear a fault by resetting both FIFOs and reloading the SPI command.
 * @details This aborts any unfinished transaction. Caller must release CS
 * and retry from the beginning of the device transaction after recovery.
 */
lpspi_status_t LPSPI_Recover(void);

/** @brief Return the nearest supported master SCK frequency in Hz. */
uint32_t LPSPI_GetActualBaudRate(void);

/** @brief Legacy runtime settings APIs (kept source-compatible). */
lpspi_status_t LPSPI_SetMode(lpspi_clock_mode_t mode);
lpspi_status_t LPSPI_SetBitOrder(lpspi_bit_order_t bitOrder);
lpspi_status_t LPSPI_SetBaudRate(uint32_t baudrate);
void LPSPI_SetFrameSize(lpspi_frameSize_t frameSize);
lpspi_status_t LPSPI_SetFrameSizeChecked(lpspi_frameSize_t frameSize);

/** @brief Blocking full-duplex transfers; always consume receive FIFO data. */
lpspi_status_t LPSPI_Transfer8(uint8_t txData, uint8_t *rxData);
lpspi_status_t LPSPI_Transfer16(uint16_t txData, uint16_t *rxData);
lpspi_status_t LPSPI_TransferBuffer(const uint8_t *txBuf, uint8_t *rxBuf, uint32_t length);

/** @brief Blocking write/read helpers; master reads clock out dummy ones. */
lpspi_status_t LPSPI_Write8(uint8_t data);
lpspi_status_t LPSPI_Write16(uint16_t data);
lpspi_status_t LPSPI_Read8(uint8_t *data);
lpspi_status_t LPSPI_Read16(uint16_t *data);

/** @brief Raw state queries (legacy API). */
bool LPSPI_IsTxReady(void);
bool LPSPI_IsRxReady(void);
bool LPSPI_IsTransferComplete(void);

/** @brief Latest REF/TEF snapshot, preserved when LPSPI_Recover() clears flags. */
uint32_t LPSPI_GetLastFaultSR(void);
uint32_t LPSPI_GetLastFaultFSR(void);
uint32_t LPSPI_GetLastFaultTCR(void);
uint32_t LPSPI_GetLastFaultCR(void);
uint32_t LPSPI_GetLastFaultCFGR1(void);

#endif /* LPSPI_H */
