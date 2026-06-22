/**
 * @file spi.c
 * @brief Arduino-style SPI API implementation for EduFramework.
 *
 * @details
 * This file implements the Arduino-style SPI API on top of the low-level
 * LPSPI driver.
 *
 * Design notes:
 * - This layer owns board-level SPI pin initialization.
 * - This layer does not access LPSPI registers directly.
 * - All SPI transfers are blocking.
 * - Public API compatibility is preserved with the previous SPI module.
 */

#include "spi.h"

#include "S32K144.h"
#include "port.h"
#include "lpspi.h"

/* ========================================================================= */
/* Private Macros                                                             */
/* ========================================================================= */

/**
 * @brief Boolean-like state value used for internal initialization flag.
 */
#define SPI_STATE_UNINITIALIZED          (0U)

/**
 * @brief Boolean-like state value used for internal initialization flag.
 */
#define SPI_STATE_INITIALIZED            (1U)

/**
 * @brief Default master baudrate in Hz.
 */
#define SPI_DEFAULT_FREQUENCY_HZ         (1000000UL)

/**
 * @brief Frequency value used in slave mode.
 */
#define SPI_SLAVE_FREQUENCY_HZ           (0UL)

/**
 * @brief Default SPI clock mode.
 */
#define SPI_DEFAULT_MODE                 (SPI_MODE0)

/**
 * @brief Default SPI bit order.
 */
#define SPI_DEFAULT_BIT_ORDER            (SPI_MSBFIRST)

/**
 * @brief Default SPI frame size.
 */
#define SPI_DEFAULT_FRAME_SIZE           (LPSPI_FRAME_SIZE_8)

/**
 * @brief Error return value for 8-bit read or transfer APIs.
 */
#define SPI_ERROR_VALUE_8                (0U)

/**
 * @brief Error return value for 16-bit read or transfer APIs.
 */
#define SPI_ERROR_VALUE_16               (0U)

/**
 * @brief PORT name used by LPSPI0 pins.
 */
#define SPI_PORT_NAME                    (PORT_NAME_B)

/**
 * @brief PORT base used by LPSPI0 pins.
 */
#define SPI_PORT_BASE                    (IP_PORTB)

/**
 * @brief LPSPI0 PCS0 pin.
 */
#define SPI_PIN_PCS0                     (0U)

/**
 * @brief LPSPI0 SOUT pin.
 */
#define SPI_PIN_SOUT                     (1U)

/**
 * @brief LPSPI0 SCK pin.
 */
#define SPI_PIN_SCK                      (2U)

/**
 * @brief LPSPI0 SIN pin.
 */
#define SPI_PIN_SIN                      (3U)

/**
 * @brief PORT mux value for LPSPI0 on PTB0..PTB3.
 */
#define SPI_PIN_MUX_LPSPI0               (PORT_MUX_ALT3)

/* ========================================================================= */
/* Private Variables                                                          */
/* ========================================================================= */

/**
 * @brief Current SPI role stored by the Arduino SPI layer.
 */
static SPI_Role_t s_SpiRole = SPI_ROLE_MASTER;

/**
 * @brief SPI initialization state.
 */
static uint8_t s_u8SpiInitialized = SPI_STATE_UNINITIALIZED;

/**
 * @brief Current SPI frequency in Hz.
 */
static uint32_t s_u32SpiFrequency = SPI_DEFAULT_FREQUENCY_HZ;

/**
 * @brief Current SPI clock mode.
 */
static SPI_Mode_t s_SpiMode = SPI_DEFAULT_MODE;

/**
 * @brief Current SPI bit order.
 */
static SPI_BitOrder_t s_SpiBitOrder = SPI_DEFAULT_BIT_ORDER;

/* ========================================================================= */
/* Private Function Prototypes                                                */
/* ========================================================================= */

static bool SPI_IsValidRole(SPI_Role_t Role);
static bool SPI_IsValidMode(SPI_Mode_t Mode);
static bool SPI_IsValidBitOrder(SPI_BitOrder_t BitOrder);
static bool SPI_IsInitialized(void);
static void SPI_PinInit(void);
static lpspi_mode_t SPI_ToLpspiRole(SPI_Role_t Role);
static lpspi_clock_mode_t SPI_ToLpspiMode(SPI_Mode_t Mode);
static lpspi_bit_order_t SPI_ToLpspiBitOrder(SPI_BitOrder_t BitOrder);
static lpspi_config_t SPI_BuildLpspiConfig(SPI_Role_t Role,
                                           uint32_t u32Frequency,
                                           SPI_Mode_t Mode,
                                           SPI_BitOrder_t BitOrder);

/* ========================================================================= */
/* Private Functions                                                          */
/* ========================================================================= */

/**
 * @brief Check whether a SPI role value is valid.
 *
 * @param Role SPI role value.
 *
 * @return Validity state.
 *
 * @retval true Role is valid.
 * @retval false Role is invalid.
 */
static bool SPI_IsValidRole(SPI_Role_t Role)
{
    bool bIsValid = false;

    if ((SPI_ROLE_MASTER == Role) || (SPI_ROLE_SLAVE == Role))
    {
        bIsValid = true;
    }
    else
    {
        bIsValid = false;
    }

    return bIsValid;
}

/**
 * @brief Check whether a SPI mode value is valid.
 *
 * @param Mode SPI mode value.
 *
 * @return Validity state.
 *
 * @retval true Mode is valid.
 * @retval false Mode is invalid.
 */
static bool SPI_IsValidMode(SPI_Mode_t Mode)
{
    bool bIsValid = false;

    if ((SPI_MODE0 == Mode) ||
        (SPI_MODE1 == Mode) ||
        (SPI_MODE2 == Mode) ||
        (SPI_MODE3 == Mode))
    {
        bIsValid = true;
    }
    else
    {
        bIsValid = false;
    }

    return bIsValid;
}

/**
 * @brief Check whether a SPI bit order value is valid.
 *
 * @param BitOrder SPI bit order value.
 *
 * @return Validity state.
 *
 * @retval true Bit order is valid.
 * @retval false Bit order is invalid.
 */
static bool SPI_IsValidBitOrder(SPI_BitOrder_t BitOrder)
{
    bool bIsValid = false;

    if ((SPI_MSBFIRST == BitOrder) || (SPI_LSBFIRST == BitOrder))
    {
        bIsValid = true;
    }
    else
    {
        bIsValid = false;
    }

    return bIsValid;
}

/**
 * @brief Check whether SPI has been initialized.
 *
 * @return Initialization state.
 *
 * @retval true SPI has been initialized.
 * @retval false SPI has not been initialized.
 */
static bool SPI_IsInitialized(void)
{
    bool bIsInitialized = false;

    if (SPI_STATE_INITIALIZED == s_u8SpiInitialized)
    {
        bIsInitialized = true;
    }
    else
    {
        bIsInitialized = false;
    }

    return bIsInitialized;
}

/**
 * @brief Initialize SPI pins for LPSPI0.
 *
 * @details
 * The current EduFramework SPI bus uses the tested MaaZEDU mapping:
 * - PTB0 = PCS0
 * - PTB1 = SOUT
 * - PTB2 = SCK
 * - PTB3 = SIN
 */
static void SPI_PinInit(void)
{
    PORT_EnableClock(SPI_PORT_NAME);

    PORT_SetPinMux(SPI_PORT_BASE,
                   SPI_PIN_PCS0,
                   SPI_PIN_MUX_LPSPI0);

    PORT_SetPinMux(SPI_PORT_BASE,
                   SPI_PIN_SOUT,
                   SPI_PIN_MUX_LPSPI0);

    PORT_SetPinMux(SPI_PORT_BASE,
                   SPI_PIN_SCK,
                   SPI_PIN_MUX_LPSPI0);

    PORT_SetPinMux(SPI_PORT_BASE,
                   SPI_PIN_SIN,
                   SPI_PIN_MUX_LPSPI0);

    return;
}

/**
 * @brief Convert Arduino SPI role to low-level LPSPI role.
 *
 * @param Role Arduino SPI role.
 *
 * @return Low-level LPSPI role.
 */
static lpspi_mode_t SPI_ToLpspiRole(SPI_Role_t Role)
{
    lpspi_mode_t LpspiRole = LPSPI_MODE_MASTER;

    if (SPI_ROLE_SLAVE == Role)
    {
        LpspiRole = LPSPI_MODE_SLAVE;
    }
    else
    {
        LpspiRole = LPSPI_MODE_MASTER;
    }

    return LpspiRole;
}

/**
 * @brief Convert Arduino SPI mode to low-level LPSPI clock mode.
 *
 * @param Mode Arduino SPI mode.
 *
 * @return Low-level LPSPI clock mode.
 */
static lpspi_clock_mode_t SPI_ToLpspiMode(SPI_Mode_t Mode)
{
    lpspi_clock_mode_t LpspiMode = LPSPI_MODE0;

    switch (Mode)
    {
        case SPI_MODE0:
            LpspiMode = LPSPI_MODE0;
            break;

        case SPI_MODE1:
            LpspiMode = LPSPI_MODE1;
            break;

        case SPI_MODE2:
            LpspiMode = LPSPI_MODE2;
            break;

        case SPI_MODE3:
            LpspiMode = LPSPI_MODE3;
            break;

        default:
            LpspiMode = LPSPI_MODE0;
            break;
    }

    return LpspiMode;
}

/**
 * @brief Convert Arduino SPI bit order to low-level LPSPI bit order.
 *
 * @param BitOrder Arduino SPI bit order.
 *
 * @return Low-level LPSPI bit order.
 */
static lpspi_bit_order_t SPI_ToLpspiBitOrder(SPI_BitOrder_t BitOrder)
{
    lpspi_bit_order_t LpspiBitOrder = LPSPI_MSB_FIRST;

    if (SPI_LSBFIRST == BitOrder)
    {
        LpspiBitOrder = LPSPI_LSB_FIRST;
    }
    else
    {
        LpspiBitOrder = LPSPI_MSB_FIRST;
    }

    return LpspiBitOrder;
}

/**
 * @brief Build low-level LPSPI configuration from Arduino SPI settings.
 *
 * @param Role Arduino SPI role.
 * @param u32Frequency SPI frequency in Hz.
 * @param Mode Arduino SPI mode.
 * @param BitOrder Arduino SPI bit order.
 *
 * @return LPSPI configuration object.
 */
static lpspi_config_t SPI_BuildLpspiConfig(SPI_Role_t Role,
                                           uint32_t u32Frequency,
                                           SPI_Mode_t Mode,
                                           SPI_BitOrder_t BitOrder)
{
    lpspi_config_t Config = {0U};

    Config.mode = SPI_ToLpspiRole(Role);
    Config.clockMode = SPI_ToLpspiMode(Mode);
    Config.bitOrder = SPI_ToLpspiBitOrder(BitOrder);
    Config.frameSize = SPI_DEFAULT_FRAME_SIZE;

    if (SPI_ROLE_MASTER == Role)
    {
        Config.baudrate = u32Frequency;
    }
    else
    {
        Config.baudrate = SPI_SLAVE_FREQUENCY_HZ;
    }

    return Config;
}

/* ========================================================================= */
/* Public Functions                                                           */
/* ========================================================================= */

/**
 * @copydoc SPI_begin
 */
void SPI_begin(SPI_Role_t role)
{
    if (SPI_ROLE_MASTER == role)
    {
        SPI_beginEx(SPI_ROLE_MASTER,
                    SPI_DEFAULT_FREQUENCY_HZ,
                    SPI_DEFAULT_MODE,
                    SPI_DEFAULT_BIT_ORDER);
    }
    else if (SPI_ROLE_SLAVE == role)
    {
        SPI_beginEx(SPI_ROLE_SLAVE,
                    SPI_SLAVE_FREQUENCY_HZ,
                    SPI_DEFAULT_MODE,
                    SPI_DEFAULT_BIT_ORDER);
    }
    else
    {
        /*
         * Invalid role. Keep API behavior safe by doing nothing.
         */
    }

    return;
}

/**
 * @copydoc SPI_beginEx
 */
void SPI_beginEx(SPI_Role_t role,
                 uint32_t frequency,
                 SPI_Mode_t mode,
                 SPI_BitOrder_t bitOrder)
{
    lpspi_config_t Config = {0U};
    lpspi_status_t Status = LPSPI_STATUS_ERROR;
    uint32_t u32EffectiveFrequency = frequency;

    if ((true == SPI_IsValidRole(role)) &&
        (true == SPI_IsValidMode(mode)) &&
        (true == SPI_IsValidBitOrder(bitOrder)))
    {
        if (SPI_ROLE_MASTER == role)
        {
            if (0UL == frequency)
            {
                u32EffectiveFrequency = SPI_DEFAULT_FREQUENCY_HZ;
            }
            else
            {
                u32EffectiveFrequency = frequency;
            }
        }
        else
        {
            u32EffectiveFrequency = SPI_SLAVE_FREQUENCY_HZ;
        }

        if (true == SPI_IsInitialized())
        {
            SPI_end();
        }
        else
        {
            /* SPI is not initialized yet. No need to disable it. */
        }

        SPI_PinInit();

        Config = SPI_BuildLpspiConfig(role,
                                      u32EffectiveFrequency,
                                      mode,
                                      bitOrder);

        Status = LPSPI_Init(&Config);

        if (LPSPI_STATUS_OK == Status)
        {
            s_SpiRole = role;
            s_u32SpiFrequency = u32EffectiveFrequency;
            s_SpiMode = mode;
            s_SpiBitOrder = bitOrder;
            s_u8SpiInitialized = SPI_STATE_INITIALIZED;
        }
        else
        {
            s_u8SpiInitialized = SPI_STATE_UNINITIALIZED;
        }
    }
    else
    {
        /*
         * Invalid configuration. Keep previous SPI state unchanged.
         */
    }

    return;
}

/**
 * @copydoc SPI_end
 */
void SPI_end(void)
{
    if (true == SPI_IsInitialized())
    {
        LPSPI_Disable();
        s_u8SpiInitialized = SPI_STATE_UNINITIALIZED;
    }
    else
    {
        /* SPI is already disabled from the Arduino layer point of view. */
    }

    return;
}

/**
 * @copydoc SPI_setFrequency
 */
void SPI_setFrequency(uint32_t frequency)
{
    lpspi_status_t Status = LPSPI_STATUS_ERROR;

    if ((true == SPI_IsInitialized()) &&
        (SPI_ROLE_MASTER == s_SpiRole) &&
        (0UL != frequency))
    {
        Status = LPSPI_SetBaudRate(frequency);

        if (LPSPI_STATUS_OK == Status)
        {
            s_u32SpiFrequency = frequency;
        }
        else
        {
            /*
             * Keep previous frequency value when the low-level driver rejects
             * the requested baudrate.
             */
        }
    }
    else
    {
        /*
         * Frequency is only configurable in initialized master mode.
         */
    }

    return;
}

/**
 * @copydoc SPI_setDataMode
 */
void SPI_setDataMode(SPI_Mode_t mode)
{
    lpspi_status_t Status = LPSPI_STATUS_ERROR;

    if ((true == SPI_IsInitialized()) && (true == SPI_IsValidMode(mode)))
    {
        Status = LPSPI_SetMode(SPI_ToLpspiMode(mode));

        if (LPSPI_STATUS_OK == Status)
        {
            s_SpiMode = mode;
        }
        else
        {
            /*
             * Keep previous mode when the low-level driver rejects the request.
             */
        }
    }
    else
    {
        /*
         * SPI is not initialized or mode is invalid.
         */
    }

    return;
}

/**
 * @copydoc SPI_setBitOrder
 */
void SPI_setBitOrder(SPI_BitOrder_t bitOrder)
{
    lpspi_status_t Status = LPSPI_STATUS_ERROR;

    if ((true == SPI_IsInitialized()) && (true == SPI_IsValidBitOrder(bitOrder)))
    {
        Status = LPSPI_SetBitOrder(SPI_ToLpspiBitOrder(bitOrder));

        if (LPSPI_STATUS_OK == Status)
        {
            s_SpiBitOrder = bitOrder;
        }
        else
        {
            /*
             * Keep previous bit order when the low-level driver rejects the request.
             */
        }
    }
    else
    {
        /*
         * SPI is not initialized or bit order is invalid.
         */
    }

    return;
}

/**
 * @copydoc SPI_transfer
 */
uint8_t SPI_transfer(uint8_t data)
{
    uint8_t u8RxData = SPI_ERROR_VALUE_8;
    lpspi_status_t Status = LPSPI_STATUS_ERROR;

    if (true == SPI_IsInitialized())
    {
        Status = LPSPI_Transfer8(data, &u8RxData);

        if (LPSPI_STATUS_OK != Status)
        {
            u8RxData = SPI_ERROR_VALUE_8;
        }
        else
        {
            /* u8RxData already contains the received byte. */
        }
    }
    else
    {
        u8RxData = SPI_ERROR_VALUE_8;
    }

    return u8RxData;
}

/**
 * @copydoc SPI_transfer16
 */
uint16_t SPI_transfer16(uint16_t data)
{
    uint16_t u16RxData = SPI_ERROR_VALUE_16;
    lpspi_status_t Status = LPSPI_STATUS_ERROR;

    if (true == SPI_IsInitialized())
    {
        Status = LPSPI_Transfer16(data, &u16RxData);

        if (LPSPI_STATUS_OK != Status)
        {
            u16RxData = SPI_ERROR_VALUE_16;
        }
        else
        {
            /* u16RxData already contains the received frame. */
        }
    }
    else
    {
        u16RxData = SPI_ERROR_VALUE_16;
    }

    return u16RxData;
}

/**
 * @copydoc SPI_transferBuffer
 */
void SPI_transferBuffer(const uint8_t *txBuffer,
                        uint8_t *rxBuffer,
                        uint32_t length)
{
    if (true == SPI_IsInitialized())
    {
        (void)LPSPI_TransferBuffer(txBuffer, rxBuffer, length);
    }
    else
    {
        /*
         * SPI is not initialized. Keep Arduino-style behavior: do nothing.
         */
    }

    return;
}

/**
 * @copydoc SPI_available
 */
bool SPI_available(void)
{
    bool bAvailable = false;

    if (true == SPI_IsInitialized())
    {
        bAvailable = LPSPI_IsRxReady();
    }
    else
    {
        bAvailable = false;
    }

    return bAvailable;
}

/**
 * @copydoc SPI_read
 */
uint8_t SPI_read(void)
{
    uint8_t u8RxData = SPI_ERROR_VALUE_8;
    lpspi_status_t Status = LPSPI_STATUS_ERROR;

    if (true == SPI_IsInitialized())
    {
        Status = LPSPI_Read8(&u8RxData);

        if (LPSPI_STATUS_OK != Status)
        {
            u8RxData = SPI_ERROR_VALUE_8;
        }
        else
        {
            /* u8RxData already contains the received byte. */
        }
    }
    else
    {
        u8RxData = SPI_ERROR_VALUE_8;
    }

    return u8RxData;
}

/**
 * @copydoc SPI_read16
 */
uint16_t SPI_read16(void)
{
    uint16_t u16RxData = SPI_ERROR_VALUE_16;
    lpspi_status_t Status = LPSPI_STATUS_ERROR;

    if (true == SPI_IsInitialized())
    {
        Status = LPSPI_Read16(&u16RxData);

        if (LPSPI_STATUS_OK != Status)
        {
            u16RxData = SPI_ERROR_VALUE_16;
        }
        else
        {
            /* u16RxData already contains the received frame. */
        }
    }
    else
    {
        u16RxData = SPI_ERROR_VALUE_16;
    }

    return u16RxData;
}

/**
 * @copydoc SPI_write
 */
void SPI_write(uint8_t data)
{
    if (true == SPI_IsInitialized())
    {
        (void)LPSPI_Write8(data);
    }
    else
    {
        /*
         * SPI is not initialized. Keep Arduino-style behavior: do nothing.
         */
    }

    return;
}

/**
 * @copydoc SPI_write16
 */
void SPI_write16(uint16_t data)
{
    if (true == SPI_IsInitialized())
    {
        (void)LPSPI_Write16(data);
    }
    else
    {
        /*
         * SPI is not initialized. Keep Arduino-style behavior: do nothing.
         */
    }

    return;
}

/**
 * @copydoc SPI_getRole
 */
SPI_Role_t SPI_getRole(void)
{
    return s_SpiRole;
}
