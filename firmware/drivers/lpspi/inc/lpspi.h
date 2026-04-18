#ifndef LPSPI_H
#define LPSPI_H

#include "S32K144.h"
#include <stdint.h>
#include <stdbool.h>

#define LPSPI_INSTANCE   IP_LPSPI0

typedef enum
{
    LPSPI_STATUS_OK = 0,
    LPSPI_STATUS_ERROR,
    LPSPI_STATUS_INVALID_ARG
} lpspi_status_t;

typedef enum
{
    LPSPI_MODE_MASTER = 0,
    LPSPI_MODE_SLAVE
} lpspi_mode_t;

typedef enum
{
    LPSPI_MODE0 = 0,
    LPSPI_MODE1,
    LPSPI_MODE2,
    LPSPI_MODE3
} lpspi_clock_mode_t;

typedef enum
{
    LPSPI_MSB_FIRST = 0,
    LPSPI_LSB_FIRST
} lpspi_bit_order_t;

typedef enum
{
    LPSPI_DATASIZE_8BIT  = 8,
    LPSPI_DATASIZE_16BIT = 16
} lpspi_datasize_t;

typedef struct
{
    lpspi_mode_t mode;
    lpspi_clock_mode_t clockMode;
    lpspi_bit_order_t bitOrder;
    lpspi_datasize_t dataSize;
    uint32_t baudrate;   /* only used in master mode */
} lpspi_config_t;

/* Init / control */
lpspi_status_t LPSPI_Init(const lpspi_config_t *config);
void LPSPI_Enable(void);
void LPSPI_Disable(void);

/* Runtime configuration */
lpspi_status_t LPSPI_SetMode(lpspi_clock_mode_t mode);
lpspi_status_t LPSPI_SetBitOrder(lpspi_bit_order_t bitOrder);
lpspi_status_t LPSPI_SetBaudRate(uint32_t baudrate);

/* Transfer */
lpspi_status_t LPSPI_Transfer8(uint8_t txData, uint8_t *rxData);
lpspi_status_t LPSPI_Transfer16(uint16_t txData, uint16_t *rxData);

lpspi_status_t LPSPI_TransferBuffer(const uint8_t *txBuf,
                                    uint8_t *rxBuf,
                                    uint32_t length);

/* Basic read / write */
lpspi_status_t LPSPI_Write(uint16_t data);
lpspi_status_t LPSPI_Read(uint16_t *data);

/* Status */
bool LPSPI_IsTxReady(void);
bool LPSPI_IsRxReady(void);
bool LPSPI_IsTransferComplete(void);

#endif /* LPSPI_H */
