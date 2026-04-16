#ifndef SPI_H
#define SPI_H

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    SPI_ROLE_MASTER = 0,
    SPI_ROLE_SLAVE
} SPI_Role_t;

typedef enum
{
    SPI_MODE0 = 0,
    SPI_MODE1,
    SPI_MODE2,
    SPI_MODE3
} SPI_Mode_t;

typedef enum
{
    SPI_MSBFIRST = 0,
    SPI_LSBFIRST
} SPI_BitOrder_t;

void SPI_begin(SPI_Role_t role);
void SPI_beginEx(SPI_Role_t role,
                 uint32_t frequency,
                 SPI_Mode_t mode,
                 SPI_BitOrder_t bitOrder);
void SPI_end(void);

void SPI_setFrequency(uint32_t frequency);
void SPI_setDataMode(SPI_Mode_t mode);
void SPI_setBitOrder(SPI_BitOrder_t bitOrder);

uint8_t SPI_transfer(uint8_t data);
uint16_t SPI_transfer16(uint16_t data);
void SPI_transferBuffer(const uint8_t *txBuffer,
                        uint8_t *rxBuffer,
                        uint32_t length);

bool SPI_available(void);
uint8_t SPI_read(void);
uint16_t SPI_read16(void);

void SPI_write(uint8_t data);
void SPI_write16(uint16_t data);

SPI_Role_t SPI_getRole(void);

#endif