/**
 * @file spi.c
 * @brief Arduino SPI0 wrapper using LPSPI v3.1 blocking driver.
 * @details No direct LPSPI register access and no fixed slave table.
 * Device libraries own GPIO CS pins; this module serializes SPI settings
 * across transactions but is not an RTOS lock or ISR-safe mutex.
 */

#include "spi.h"
#include "S32K144.h"
#include "port.h"
#include "lpspi.h"

#define SPI_DEFAULT_FREQUENCY_HZ (1000000UL)
#define SPI_PORT_NAME (PORT_NAME_B)
#define SPI_PORT_BASE (IP_PORTB)
#define SPI_PIN_PCS0 (0U)
#define SPI_PIN_SOUT (1U)
#define SPI_PIN_SCK (2U)
#define SPI_PIN_SIN (3U)
#define SPI_PIN_MUX (PORT_MUX_ALT3)

static bool s_Initialized = false;
static bool s_TransactionActive = false;
static SPI_Role_t s_Role = SPI_ROLE_MASTER;
static SPI_Settings_t s_Settings = {SPI_MODE0, SPI_MSBFIRST, SPI_FRAME_SIZE_8, SPI_DEFAULT_FREQUENCY_HZ};
static SPI_Status_t s_LastStatus = SPI_STATUS_NOT_INIT;
static SPI_Status_t s_TransactionError = SPI_STATUS_OK;

/** @brief Validate an SPI role. */
static bool SPI_ValidRole(SPI_Role_t role)
{
    return ((role == SPI_ROLE_MASTER) || (role == SPI_ROLE_SLAVE));
}

/** @brief Validate mode, bit order and frame size without hardware accesses. */
static bool SPI_ValidSettings(const SPI_Settings_t *settings)
{
    bool valid = false;

    if (settings != (const SPI_Settings_t *)0)
    {
        valid = ((settings->mode <= SPI_MODE3) && (settings->bitOrder <= SPI_LSBFIRST) && ((settings->frameSize == SPI_FRAME_SIZE_8) || (settings->frameSize == SPI_FRAME_SIZE_16)) && (settings->frequency != 0UL));
    }

    return valid;
}

/** @brief Translate a low-level status while keeping the layers independent. */
static SPI_Status_t SPI_FromDriverStatus(lpspi_status_t status)
{
    SPI_Status_t result = SPI_STATUS_ERROR;

    switch (status)
    {
        case LPSPI_STATUS_OK:          result = SPI_STATUS_OK; break;
        case LPSPI_STATUS_ERROR:       result = SPI_STATUS_ERROR; break;
        case LPSPI_STATUS_INVALID_ARG: result = SPI_STATUS_INVALID_ARG; break;
        case LPSPI_STATUS_NOT_INIT:    result = SPI_STATUS_NOT_INIT; break;
        case LPSPI_STATUS_TIMEOUT:     result = SPI_STATUS_TIMEOUT; break;
        case LPSPI_STATUS_FIFO_ERROR:  result = SPI_STATUS_FIFO_ERROR; break;
        default:                       result = SPI_STATUS_ERROR; break;
    }

    return result;
}

/** @brief Preserve the first transfer fault until the current transaction ends. */
static void SPI_RecordTransferStatus(SPI_Status_t status)
{
    if (s_TransactionActive && (s_TransactionError == SPI_STATUS_OK) && (status != SPI_STATUS_OK))
    {
        s_TransactionError = status;
    }
}

/** @brief Configure LPSPI0 SOUT/SCK/SIN; PCS0 is for slave input only. */
static void SPI_PinInit(SPI_Role_t role)
{
    PORT_EnableClock(SPI_PORT_NAME);
    PORT_SetPinMux(SPI_PORT_BASE, SPI_PIN_SOUT, SPI_PIN_MUX);
    PORT_SetPinMux(SPI_PORT_BASE, SPI_PIN_SCK, SPI_PIN_MUX);
    PORT_SetPinMux(SPI_PORT_BASE, SPI_PIN_SIN, SPI_PIN_MUX);
    if (role == SPI_ROLE_SLAVE)
    {
        PORT_SetPinMux(SPI_PORT_BASE, SPI_PIN_PCS0, SPI_PIN_MUX);
    }
}

/** @brief Convert transaction parameters to low-level settings. */
static lpspi_settings_t SPI_ToDriverSettings(const SPI_Settings_t *settings)
{
    lpspi_settings_t result = {0U};

    result.clockMode = (lpspi_clock_mode_t)settings->mode;
    result.bitOrder = (lpspi_bit_order_t)settings->bitOrder;
    result.frameSize = (lpspi_frameSize_t)settings->frameSize;
    result.baudrate = settings->frequency;

    return result;
}

/** @brief Configure LPSPI0 and preserve the current role on failures. */
void SPI_beginEx(SPI_Role_t role, uint32_t frequency, SPI_Mode_t mode, SPI_BitOrder_t bitOrder)
{
    SPI_Settings_t proposed = {mode, bitOrder, SPI_FRAME_SIZE_8, frequency};
    lpspi_config_t initial = {0U};
    lpspi_settings_t driverSettings = {0U};
    lpspi_status_t status = LPSPI_STATUS_ERROR;

    if (s_TransactionActive)
    {
        s_LastStatus = SPI_STATUS_BUSY;
    }
    else if (!SPI_ValidRole(role) || (mode > SPI_MODE3) || (bitOrder > SPI_LSBFIRST))
    {
        s_LastStatus = SPI_STATUS_INVALID_ARG;
    }
    else
    {
        if (role == SPI_ROLE_MASTER)
        {
            if (frequency == 0UL)
            {
                proposed.frequency = SPI_DEFAULT_FREQUENCY_HZ;
            }
        }
        else
        {
            proposed.frequency = 0UL;
        }
        if (s_Initialized && (s_Role == SPI_ROLE_MASTER) && (role == SPI_ROLE_MASTER))
        {
            driverSettings = SPI_ToDriverSettings(&proposed);
            status = LPSPI_ApplySettings(&driverSettings);
        }
        else
        {
            SPI_PinInit(role);
            initial.mode = (role == SPI_ROLE_MASTER) ? LPSPI_MODE_MASTER : LPSPI_MODE_SLAVE;
            initial.clockMode = (lpspi_clock_mode_t)proposed.mode;
            initial.bitOrder = (lpspi_bit_order_t)proposed.bitOrder;
            initial.frameSize = LPSPI_FRAME_SIZE_8;
            initial.baudrate = proposed.frequency;
            status = LPSPI_Init(&initial);
        }
        s_LastStatus = SPI_FromDriverStatus(status);
        if (status == LPSPI_STATUS_OK)
        {
            s_Initialized = true;
            s_Role = role;
            s_Settings = proposed;
        }
    }
}

/** @brief Initialize the bus with legacy defaults. */
void SPI_begin(SPI_Role_t role)
{
    SPI_beginEx(role, SPI_DEFAULT_FREQUENCY_HZ, SPI_MODE0, SPI_MSBFIRST);
}

/** @brief Disable a bus only when it is not inside a transaction. */
void SPI_end(void)
{
    if (s_TransactionActive)
    {
        s_LastStatus = SPI_STATUS_BUSY;
    }
    else if (!s_Initialized)
    {
        s_LastStatus = SPI_STATUS_NOT_INIT;
    }
    else
    {
        LPSPI_Disable();
        s_Initialized = false;
        s_LastStatus = SPI_STATUS_OK;
    }
}

/** @brief Apply a device profile and reserve the current transaction. */
SPI_Status_t SPI_beginTransaction(const SPI_Settings_t *settings)
{
    lpspi_settings_t driverSettings = {0U};
    lpspi_status_t status = LPSPI_STATUS_ERROR;
    SPI_Status_t result = SPI_STATUS_OK;

    if (s_TransactionActive)
    {
        result = SPI_STATUS_BUSY;
    }
    else if (!s_Initialized)
    {
        result = SPI_STATUS_NOT_INIT;
    }
    else if (s_Role != SPI_ROLE_MASTER)
    {
        result = SPI_STATUS_UNSUPPORTED;
    }
    else if (!SPI_ValidSettings(settings))
    {
        result = SPI_STATUS_INVALID_ARG;
    }
    else
    {
        driverSettings = SPI_ToDriverSettings(settings);
        status = LPSPI_ApplySettings(&driverSettings);
        result = SPI_FromDriverStatus(status);
        if (result == SPI_STATUS_OK)
        {
            s_Settings = *settings;
            s_TransactionActive = true;
            s_TransactionError = SPI_STATUS_OK;
        }
    }
    s_LastStatus = result;

    return result;
}

/** @brief Wait for SPI hardware to finish while external CS remains asserted. */
SPI_Status_t SPI_endTransaction(void)
{
    SPI_Status_t result = SPI_STATUS_NO_TRANSACTION;

    if (s_TransactionActive)
    {
        result = s_TransactionError;
        if (result == SPI_STATUS_OK)
        {
            result = SPI_FromDriverStatus(LPSPI_WaitIdle());
        }
        s_TransactionActive = false;
        s_TransactionError = SPI_STATUS_OK;
    }
    s_LastStatus = result;

    return result;
}

/** @brief Query whether another beginTransaction is allowed. */
bool SPI_isTransactionActive(void)
{
    return s_TransactionActive;
}

/** @brief Get the latest wrapper status. */
SPI_Status_t SPI_getLastStatus(void)
{
    return s_LastStatus;
}

/** @brief Get the current actual clock rate. */
uint32_t SPI_getActualFrequency(void)
{
    uint32_t result = 0UL;

    if (s_Initialized && (s_Role == SPI_ROLE_MASTER))
    {
        result = LPSPI_GetActualBaudRate();
    }

    return result;
}

/** @brief Update frequency when not inside an active transaction. */
void SPI_setFrequency(uint32_t frequency)
{
    if (s_TransactionActive)
    {
        s_LastStatus = SPI_STATUS_BUSY;
    }
    else if (!s_Initialized)
    {
        s_LastStatus = SPI_STATUS_NOT_INIT;
    }
    else if ((s_Role != SPI_ROLE_MASTER) || (frequency == 0UL))
    {
        s_LastStatus = SPI_STATUS_INVALID_ARG;
    }
    else
    {
        s_LastStatus = SPI_FromDriverStatus(LPSPI_SetBaudRate(frequency));
        if (s_LastStatus == SPI_STATUS_OK)
        {
            s_Settings.frequency = frequency;
        }
    }
}

/** @brief Update clock mode when the bus is free. */
void SPI_setDataMode(SPI_Mode_t mode)
{
    if (s_TransactionActive)
    {
        s_LastStatus = SPI_STATUS_BUSY;
    }
    else if (!s_Initialized)
    {
        s_LastStatus = SPI_STATUS_NOT_INIT;
    }
    else if (mode > SPI_MODE3)
    {
        s_LastStatus = SPI_STATUS_INVALID_ARG;
    }
    else
    {
        s_LastStatus = SPI_FromDriverStatus(LPSPI_SetMode((lpspi_clock_mode_t)mode));
        if (s_LastStatus == SPI_STATUS_OK)
        {
            s_Settings.mode = mode;
        }
    }
}

/** @brief Update bit order when the bus is free. */
void SPI_setBitOrder(SPI_BitOrder_t bitOrder)
{
    if (s_TransactionActive)
    {
        s_LastStatus = SPI_STATUS_BUSY;
    }
    else if (!s_Initialized)
    {
        s_LastStatus = SPI_STATUS_NOT_INIT;
    }
    else if (bitOrder > SPI_LSBFIRST)
    {
        s_LastStatus = SPI_STATUS_INVALID_ARG;
    }
    else
    {
        s_LastStatus = SPI_FromDriverStatus(LPSPI_SetBitOrder((lpspi_bit_order_t)bitOrder));
        if (s_LastStatus == SPI_STATUS_OK)
        {
            s_Settings.bitOrder = bitOrder;
        }
    }
}

/** @brief Exchange one byte in the current bus settings. */
uint8_t SPI_transfer(uint8_t data)
{
    uint8_t result = 0U;

    if (!s_Initialized)
    {
        s_LastStatus = SPI_STATUS_NOT_INIT;
    }
    else
    {
        s_LastStatus = SPI_FromDriverStatus(LPSPI_Transfer8(data, &result));
        SPI_RecordTransferStatus(s_LastStatus);
        if (s_LastStatus != SPI_STATUS_OK)
        {
            result = 0U;
        }
    }

    return result;
}

/** @brief Exchange one 16-bit word in the current bus settings. */
uint16_t SPI_transfer16(uint16_t data)
{
    uint16_t result = 0U;

    if (!s_Initialized)
    {
        s_LastStatus = SPI_STATUS_NOT_INIT;
    }
    else
    {
        s_LastStatus = SPI_FromDriverStatus(LPSPI_Transfer16(data, &result));
        SPI_RecordTransferStatus(s_LastStatus);
        if (s_LastStatus != SPI_STATUS_OK)
        {
            result = 0U;
        }
    }

    return result;
}

/** @brief Exchange byte arrays; null TX means transmit 0xFF dummy data. */
void SPI_transferBuffer(const uint8_t *txBuffer, uint8_t *rxBuffer, uint32_t length)
{
    if (!s_Initialized)
    {
        s_LastStatus = SPI_STATUS_NOT_INIT;
    }
    else
    {
        s_LastStatus = SPI_FromDriverStatus(LPSPI_TransferBuffer(txBuffer, rxBuffer, length));
        SPI_RecordTransferStatus(s_LastStatus);
    }
}

/** @brief Read FIFO readiness, normally false after blocking master calls. */
bool SPI_available(void)
{
    bool result = false;

    if (s_Initialized)
    {
        result = LPSPI_IsRxReady();
    }

    return result;
}

/** @brief Receive a byte with dummy transmit clock in master mode. */
uint8_t SPI_read(void)
{
    uint8_t result = 0U;

    if (!s_Initialized)
    {
        s_LastStatus = SPI_STATUS_NOT_INIT;
    }
    else
    {
        s_LastStatus = SPI_FromDriverStatus(LPSPI_Read8(&result));
        SPI_RecordTransferStatus(s_LastStatus);
        if (s_LastStatus != SPI_STATUS_OK)
        {
            result = 0U;
        }
    }

    return result;
}

/** @brief Receive a 16-bit frame using dummy transmit data in master mode. */
uint16_t SPI_read16(void)
{
    uint16_t result = 0U;

    if (!s_Initialized)
    {
        s_LastStatus = SPI_STATUS_NOT_INIT;
    }
    else
    {
        s_LastStatus = SPI_FromDriverStatus(LPSPI_Read16(&result));
        SPI_RecordTransferStatus(s_LastStatus);
        if (s_LastStatus != SPI_STATUS_OK)
        {
            result = 0U;
        }
    }

    return result;
}

/** @brief Transmit a byte; discard the simultaneous received byte. */
void SPI_write(uint8_t data)
{
    if (!s_Initialized)
    {
        s_LastStatus = SPI_STATUS_NOT_INIT;
    }
    else
    {
        s_LastStatus = SPI_FromDriverStatus(LPSPI_Write8(data));
        SPI_RecordTransferStatus(s_LastStatus);
    }
}

/** @brief Transmit a 16-bit frame; discard received data. */
void SPI_write16(uint16_t data)
{
    if (!s_Initialized)
    {
        s_LastStatus = SPI_STATUS_NOT_INIT;
    }
    else
    {
        s_LastStatus = SPI_FromDriverStatus(LPSPI_Write16(data));
        SPI_RecordTransferStatus(s_LastStatus);
    }
}

/** @brief Return the most recently initialized SPI role. */
SPI_Role_t SPI_getRole(void)
{
    return s_Role;
}
