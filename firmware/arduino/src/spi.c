#include "spi.h"

#include "S32K144.h"
#include "port.h"
#include "lpspi.h"

/* ============================================================
 * INTERNAL STATE
 * ============================================================ */
static SPI_Role_t g_spiRole = SPI_ROLE_MASTER;
static uint8_t g_spiInitialized = 0U;

static uint32_t g_spiFrequency = 1000000U;
static SPI_Mode_t g_spiMode = SPI_MODE0;
static SPI_BitOrder_t g_spiBitOrder = SPI_MSBFIRST;

/* ============================================================
 * INTERNAL HELPERS
 * ============================================================ */
static void SPI_PinInit(void)
{
    PORT_EnableClock(PORT_NAME_B);

    /* LPSPI0 / ALT3
     * PTB0 = PCS0
     * PTB1 = SOUT
     * PTB2 = SCK
     * PTB3 = SIN
     */
    PORT_SetPinMux(IP_PORTB, 0U, PORT_MUX_ALT3);
    PORT_SetPinMux(IP_PORTB, 1U, PORT_MUX_ALT3);
    PORT_SetPinMux(IP_PORTB, 2U, PORT_MUX_ALT3);
    PORT_SetPinMux(IP_PORTB, 3U, PORT_MUX_ALT3);
}

static lpspi_clock_mode_t SPI_ToLpspiMode(SPI_Mode_t mode)
{
    switch (mode)
    {
        case SPI_MODE0: return LPSPI_MODE0;
        case SPI_MODE1: return LPSPI_MODE1;
        case SPI_MODE2: return LPSPI_MODE2;
        case SPI_MODE3: return LPSPI_MODE3;
        default:        return LPSPI_MODE0;
    }
}

static lpspi_bit_order_t SPI_ToLpspiBitOrder(SPI_BitOrder_t bitOrder)
{
    if (bitOrder == SPI_LSBFIRST)
    {
        return LPSPI_LSB_FIRST;
    }

    return LPSPI_MSB_FIRST;
}

/* ============================================================
 * PUBLIC API
 * ============================================================ */
void SPI_begin(SPI_Role_t role)
{
    if (role == SPI_ROLE_MASTER)
    {
        SPI_beginEx(SPI_ROLE_MASTER, 1000000U, SPI_MODE0, SPI_MSBFIRST);
    }
    else
    {
        SPI_beginEx(SPI_ROLE_SLAVE, 0U, SPI_MODE0, SPI_MSBFIRST);
    }
}

void SPI_beginEx(SPI_Role_t role,
                 uint32_t frequency,
                 SPI_Mode_t mode,
                 SPI_BitOrder_t bitOrder)
{
    lpspi_config_t config;

    SPI_PinInit();

    g_spiRole = role;
    g_spiFrequency = frequency;
    g_spiMode = mode;
    g_spiBitOrder = bitOrder;

    config.mode = (role == SPI_ROLE_MASTER) ? LPSPI_MODE_MASTER : LPSPI_MODE_SLAVE;
    config.clockMode = SPI_ToLpspiMode(mode);
    config.bitOrder = SPI_ToLpspiBitOrder(bitOrder);
    config.dataSize = LPSPI_DATASIZE_8BIT;
    config.baudrate = (role == SPI_ROLE_MASTER) ? frequency : 0U;

    if (LPSPI_Init(&config) == LPSPI_STATUS_OK)
    {
        g_spiInitialized = 1U;
    }
    else
    {
        g_spiInitialized = 0U;
    }
}

void SPI_end(void)
{
    LPSPI_Disable();
    g_spiInitialized = 0U;
}

void SPI_setFrequency(uint32_t frequency)
{
    if (g_spiInitialized == 0U)
    {
        return;
    }

    g_spiFrequency = frequency;
    (void)LPSPI_SetBaudRate(frequency);
}

void SPI_setDataMode(SPI_Mode_t mode)
{
    if (g_spiInitialized == 0U)
    {
        return;
    }

    g_spiMode = mode;
    (void)LPSPI_SetMode(SPI_ToLpspiMode(mode));
}

void SPI_setBitOrder(SPI_BitOrder_t bitOrder)
{
    if (g_spiInitialized == 0U)
    {
        return;
    }

    g_spiBitOrder = bitOrder;
    (void)LPSPI_SetBitOrder(SPI_ToLpspiBitOrder(bitOrder));
}

uint8_t SPI_transfer(uint8_t data)
{
    uint8_t rxData = 0U;

    if (g_spiInitialized == 0U)
    {
        return 0U;
    }

    (void)LPSPI_Transfer8(data, &rxData);
    return rxData;
}

uint16_t SPI_transfer16(uint16_t data)
{
    uint16_t rxData = 0U;

    if (g_spiInitialized == 0U)
    {
        return 0U;
    }

    (void)LPSPI_Transfer16(data, &rxData);
    return rxData;
}

void SPI_transferBuffer(const uint8_t *txBuffer,
                        uint8_t *rxBuffer,
                        uint32_t length)
{
    if (g_spiInitialized == 0U)
    {
        return;
    }

    (void)LPSPI_TransferBuffer(txBuffer, rxBuffer, length);
}

bool SPI_available(void)
{
    if (g_spiInitialized == 0U)
    {
        return false;
    }

    return LPSPI_IsRxReady();
}

uint8_t SPI_read(void)
{
    uint16_t rxData = 0U;

    if (g_spiInitialized == 0U)
    {
        return 0U;
    }

    if (LPSPI_Read(&rxData) != LPSPI_STATUS_OK)
    {
        return 0U;
    }

    return (uint8_t)rxData;
}

uint16_t SPI_read16(void)
{
    uint16_t rxData = 0U;

    if (g_spiInitialized == 0U)
    {
        return 0U;
    }

    if (LPSPI_Read(&rxData) != LPSPI_STATUS_OK)
    {
        return 0U;
    }

    return rxData;
}

void SPI_write(uint8_t data)
{
    if (g_spiInitialized == 0U)
    {
        return;
    }

    (void)LPSPI_Write((uint16_t)data);
}

void SPI_write16(uint16_t data)
{
    if (g_spiInitialized == 0U)
    {
        return;
    }

    (void)LPSPI_Write(data);
}

SPI_Role_t SPI_getRole(void)
{
    return g_spiRole;
}
