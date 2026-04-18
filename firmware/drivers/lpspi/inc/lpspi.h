#ifndef LPSPI_H
#define LPSPI_H

#include "S32K144.h"
#include <stdint.h>
#include <stdbool.h>

#define LPSPI_INSTANCE   IP_LPSPI0

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

void LPSPI_Init(const lpspi_config_t *config);
void LPSPI_Enable(void);
void LPSPI_Disable(void);

uint8_t  LPSPI_Transfer8(uint8_t data);
uint16_t LPSPI_Transfer16(uint16_t data);

void LPSPI_TransferBuffer(const uint8_t *txBuf,
                          uint8_t *rxBuf,
                          uint32_t length);

void     LPSPI_Write(uint16_t data);
uint16_t LPSPI_Read(void);

bool LPSPI_IsTxReady(void);
bool LPSPI_IsRxReady(void);
bool LPSPI_IsTransferComplete(void);

#endif /* LPSPI_H */
