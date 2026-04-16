#ifndef SPI_HW_H
#define SPI_HW_H

#include <stdint.h>
#include <stdbool.h>

/* =========================================
 * STATUS
 * ========================================= */

typedef enum
{
    SPI_HW_STATUS_OK = 0,
    SPI_HW_STATUS_ERROR,
    SPI_HW_STATUS_TIMEOUT
} SPI_HW_Status_t;

/* =========================================
 * CONFIG TYPES
 * ========================================= */

typedef enum
{
    SPI_HW_MODE0 = 0,
    SPI_HW_MODE1,
    SPI_HW_MODE2,
    SPI_HW_MODE3
} SPI_HW_Mode_t;

typedef enum
{
    SPI_HW_MSBFIRST = 0,
    SPI_HW_LSBFIRST
} SPI_HW_BitOrder_t;

typedef struct
{
    uint32_t srcClockHz;     /* Source clock of LPSPI (Hz) */
    uint32_t baudRate;       /* Desired SPI clock (Hz) */
    SPI_HW_Mode_t mode;      /* CPOL + CPHA */
    SPI_HW_BitOrder_t bitOrder;
} SPI_HW_MasterConfig_t;

typedef struct
{
    SPI_HW_Mode_t mode;           /* CPOL + CPHA */
    SPI_HW_BitOrder_t bitOrder;
} SPI_HW_SlaveConfig_t;


/* =========================================
 * MASTER CONTROL
 * ========================================= */

/* Init LPSPI1 as MASTER */
SPI_HW_Status_t SPI_HW_MasterInit(const SPI_HW_MasterConfig_t *config);

/* Deinit (disable module) */
void SPI_HW_MasterDeinit(void);

/* =========================================
 * MASTER TRANSFER (POLLING)
 * ========================================= */

/* Transfer 8-bit */
SPI_HW_Status_t SPI_HW_MasterTransfer8(uint8_t txData, uint8_t *rxData);

/* Transfer 16-bit */
SPI_HW_Status_t SPI_HW_MasterTransfer16(uint16_t txData, uint16_t *rxData);

/* Transfer buffer */
SPI_HW_Status_t SPI_HW_MasterTransferBuffer(
    const uint8_t *txBuffer,
    uint8_t *rxBuffer,
    uint32_t length);

/* =========================================
 * RX UTILITIES
 * ========================================= */

/* Check RX FIFO has data */
bool SPI_HW_RxAvailable(void);

/* Read 8-bit from RX FIFO */
uint8_t SPI_HW_Read8(void);

/* Read 16-bit from RX FIFO */
uint16_t SPI_HW_Read16(void);

/* Change SPI mode (CPOL/CPHA) */
SPI_HW_Status_t SPI_HW_SetMode(SPI_HW_Mode_t mode);

/* Change bit order */
SPI_HW_Status_t SPI_HW_SetBitOrder(SPI_HW_BitOrder_t bitOrder);

/* Change baud rate */
SPI_HW_Status_t SPI_HW_SetBaudRate(uint32_t baudRate);

/* =========================================
 * SLAVE CONTROL
 * ========================================= */

/* Init LPSPI1 as SLAVE */
SPI_HW_Status_t SPI_HW_SlaveInit(const SPI_HW_SlaveConfig_t *config);

/* Deinit (disable module) */
void SPI_HW_SlaveDeinit(void);

/* =========================================
 * SLAVE DATA (POLLING)
 * ========================================= */

/* Check RX FIFO has data */
bool SPI_HW_SlaveRxAvailable(void);

/* Read 8-bit from RX FIFO */
SPI_HW_Status_t SPI_HW_SlaveRead8(uint8_t *rxData);

/* Write 8-bit to TX FIFO before master clocks data out */
SPI_HW_Status_t SPI_HW_SlaveWrite8(uint8_t txData);


#endif
