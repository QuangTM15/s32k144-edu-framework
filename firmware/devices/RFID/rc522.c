/**
 * @file rc522.c
 * @brief MFRC522 RFID device library implementation.
 *
 * @details
 * This implementation provides the following functional layers:
 *
 * - GPIO and SPI control.
 * - MFRC522 register access.
 * - Reader initialization and antenna control.
 * - Hardware CRC calculation.
 * - FIFO-based PCD communication.
 * - ISO/IEC 14443A card request.
 * - Cascade Level 1 anti-collision and selection.
 * - Four-byte UID and SAK reading.
 * - PICC halt command.
 */

#include "rc522.h"

#include "Arduino.h"
#include "arduino_pins.h"
#include "spi.h"

/* ========================================================================= */
/* Private Types                                                             */
/* ========================================================================= */

/**
 * @brief Internal operation status type.
 */
typedef uint8_t rc522_status_t;

/**
 * @brief Operation completed successfully.
 */
#define RC522_STATUS_OK                  ((rc522_status_t)0U)

/**
 * @brief General communication or protocol error.
 */
#define RC522_STATUS_ERROR               ((rc522_status_t)1U)

/**
 * @brief Operation timed out.
 */
#define RC522_STATUS_TIMEOUT             ((rc522_status_t)2U)

/**
 * @brief A card collision was reported.
 */
#define RC522_STATUS_COLLISION           ((rc522_status_t)3U)

/**
 * @brief CRC or UID BCC validation failed.
 */
#define RC522_STATUS_CRC_ERROR           ((rc522_status_t)4U)

/**
 * @brief An invalid argument was supplied.
 */
#define RC522_STATUS_INVALID_ARG         ((rc522_status_t)5U)

/**
 * @brief The receive buffer is too small.
 */
#define RC522_STATUS_NO_ROOM             ((rc522_status_t)6U)

/**
 * @brief The MFRC522 library is not initialized.
 */
#define RC522_STATUS_NOT_INITIALIZED     ((rc522_status_t)7U)

/* ========================================================================= */
/* Private Constants                                                         */
/* ========================================================================= */

/* ------------------------------------------------------------------------- */
/* SPI configuration                                                         */
/* ------------------------------------------------------------------------- */

#define RC522_SPI_FREQUENCY_HZ           (1000000UL)
#define RC522_SPI_READ_MASK              (0x80U)
#define RC522_SPI_ADDRESS_MASK           (0x7EU)
#define RC522_SPI_DUMMY_BYTE             (0x00U)

/* ------------------------------------------------------------------------- */
/* MFRC522 PCD commands                                                      */
/* ------------------------------------------------------------------------- */

#define RC522_COMMAND_IDLE               (0x00U)
#define RC522_COMMAND_CALC_CRC           (0x03U)
#define RC522_COMMAND_TRANSCEIVE         (0x0CU)
#define RC522_COMMAND_SOFT_RESET         (0x0FU)

/* ------------------------------------------------------------------------- */
/* ISO/IEC 14443A PICC commands                                              */
/* ------------------------------------------------------------------------- */

#define RC522_PICC_COMMAND_REQA          (0x26U)
#define RC522_PICC_COMMAND_HLTA          (0x50U)
#define RC522_PICC_COMMAND_SEL_CL1       (0x93U)
#define RC522_PICC_NVB_ANTICOLLISION     (0x20U)
#define RC522_PICC_NVB_SELECT            (0x70U)

/* ------------------------------------------------------------------------- */
/* MFRC522 registers                                                         */
/* ------------------------------------------------------------------------- */

#define RC522_REG_COMMAND                (0x01U)
#define RC522_REG_COM_IRQ                (0x04U)
#define RC522_REG_DIV_IRQ                (0x05U)
#define RC522_REG_ERROR                  (0x06U)
#define RC522_REG_FIFO_DATA              (0x09U)
#define RC522_REG_FIFO_LEVEL             (0x0AU)
#define RC522_REG_CONTROL                (0x0CU)
#define RC522_REG_BIT_FRAMING            (0x0DU)
#define RC522_REG_COLL                   (0x0EU)

#define RC522_REG_MODE                   (0x11U)
#define RC522_REG_TX_MODE                (0x12U)
#define RC522_REG_RX_MODE                (0x13U)
#define RC522_REG_TX_CONTROL             (0x14U)
#define RC522_REG_TX_ASK                 (0x15U)

#define RC522_REG_CRC_RESULT_H           (0x21U)
#define RC522_REG_CRC_RESULT_L           (0x22U)

#define RC522_REG_T_MODE                 (0x2AU)
#define RC522_REG_T_PRESCALER            (0x2BU)
#define RC522_REG_T_RELOAD_H             (0x2CU)
#define RC522_REG_T_RELOAD_L             (0x2DU)

#define RC522_REG_VERSION                (0x37U)

/* ------------------------------------------------------------------------- */
/* Register masks                                                            */
/* ------------------------------------------------------------------------- */

#define RC522_COMMAND_POWER_DOWN_MASK    (0x10U)

#define RC522_COM_IRQ_RX_MASK            (0x20U)
#define RC522_COM_IRQ_IDLE_MASK          (0x10U)
#define RC522_COM_IRQ_TIMER_MASK         (0x01U)

#define RC522_COM_IRQ_WAIT_MASK          \
    (RC522_COM_IRQ_RX_MASK |             \
     RC522_COM_IRQ_IDLE_MASK |           \
     RC522_COM_IRQ_TIMER_MASK)

#define RC522_DIV_IRQ_CRC_MASK           (0x04U)

#define RC522_ERROR_BUFFER_OVERFLOW_MASK (0x10U)
#define RC522_ERROR_COLLISION_MASK       (0x08U)
#define RC522_ERROR_CRC_MASK             (0x04U)
#define RC522_ERROR_PROTOCOL_MASK        (0x01U)

#define RC522_ERROR_FATAL_MASK           \
    (RC522_ERROR_BUFFER_OVERFLOW_MASK |  \
     RC522_ERROR_PROTOCOL_MASK)

#define RC522_FIFO_FLUSH_MASK            (0x80U)

#define RC522_BIT_FRAMING_START_SEND_MASK   (0x80U)
#define RC522_BIT_FRAMING_TX_LAST_BITS_MASK (0x07U)

#define RC522_CONTROL_RX_LAST_BITS_MASK  (0x07U)
#define RC522_COLL_VALUES_AFTER_MASK     (0x80U)
#define RC522_TX_CONTROL_ANTENNA_MASK    (0x03U)

/* ------------------------------------------------------------------------- */
/* Default configuration                                                     */
/* ------------------------------------------------------------------------- */

#define RC522_TX_MODE_DEFAULT            (0x00U)
#define RC522_RX_MODE_DEFAULT            (0x00U)
#define RC522_T_MODE_DEFAULT             (0x80U)
#define RC522_T_PRESCALER_DEFAULT        (0xA9U)
#define RC522_T_RELOAD_H_DEFAULT         (0x03U)
#define RC522_T_RELOAD_L_DEFAULT         (0xE8U)
#define RC522_TX_ASK_DEFAULT             (0x40U)
#define RC522_MODE_DEFAULT               (0x3DU)

/* ------------------------------------------------------------------------- */
/* Protocol constants                                                        */
/* ------------------------------------------------------------------------- */

#define RC522_ATQA_SIZE                  (2U)
#define RC522_ANTICOLLISION_SIZE         (5U)
#define RC522_SELECT_FRAME_SIZE          (9U)
#define RC522_SELECT_RESPONSE_SIZE       (3U)
#define RC522_HALT_FRAME_SIZE            (4U)

#define RC522_CRC_SIZE                   (2U)

#define RC522_REQA_VALID_BITS            (7U)
#define RC522_FULL_BYTE_VALID_BITS       (0U)

#define RC522_UID_BCC_INDEX              (4U)
#define RC522_SAK_INDEX                  (0U)
#define RC522_SAK_CASCADE_BIT_MASK       (0x04U)

/* ------------------------------------------------------------------------- */
/* Timing and timeout constants                                              */
/* ------------------------------------------------------------------------- */

#define RC522_HARD_RESET_LOW_DELAY_MS     (2UL)
#define RC522_HARD_RESET_STARTUP_DELAY_MS (50UL)
#define RC522_SOFT_RESET_DELAY_MS         (1UL)

#define RC522_RESET_TIMEOUT_COUNT        (1000UL)
#define RC522_CRC_TIMEOUT_COUNT          (5000UL)
#define RC522_TRANSCEIVE_TIMEOUT_COUNT   (10000UL)

/* ========================================================================= */
/* Private State                                                             */
/* ========================================================================= */

/**
 * @brief User-selected software chip-select pin.
 */
static uint8_t s_u8Rc522CsPin = GPIO0;

/**
 * @brief User-selected hardware reset pin.
 */
static uint8_t s_u8Rc522ResetPin = GPIO1;

/**
 * @brief Control-pin configuration state.
 */
static bool s_bRc522PinsConfigured = false;

/**
 * @brief Complete reader initialization state.
 */
static bool s_bRc522Initialized = false;

/* ========================================================================= */
/* Private Function Prototypes                                               */
/* ========================================================================= */

/* GPIO and validation */
static bool RC522_IsValidPins(uint8_t csPin,
                              uint8_t resetPin);

static bool RC522_IsVersionSupported(uint8_t version);

static void RC522_Select(void);
static void RC522_Deselect(void);

/* Register access */
static uint8_t RC522_BuildReadAddress(uint8_t registerAddress);
static uint8_t RC522_BuildWriteAddress(uint8_t registerAddress);

static uint8_t RC522_ReadRegister(uint8_t registerAddress);

static void RC522_WriteRegister(uint8_t registerAddress,
                                uint8_t value);

static void RC522_SetRegisterBitMask(uint8_t registerAddress,
                                     uint8_t mask);

static void RC522_ClearRegisterBitMask(uint8_t registerAddress,
                                       uint8_t mask);

static void RC522_WriteRegisterBuffer(uint8_t registerAddress,
                                      const uint8_t *pBuffer,
                                      uint8_t length);

static void RC522_ReadRegisterBuffer(uint8_t registerAddress,
                                     uint8_t *pBuffer,
                                     uint8_t length);

/* PCD configuration and communication */
static void RC522_ApplyDefaultConfiguration(void);

static rc522_status_t RC522_CalculateCRC(
    const uint8_t *pData,
    uint8_t length,
    uint8_t *pResult);

static rc522_status_t RC522_Transceive(
    const uint8_t *pTxData,
    uint8_t txLength,
    uint8_t txLastBits,
    uint8_t *pRxData,
    uint8_t *pRxLength,
    uint8_t *pRxValidBits);

/* PICC communication */
static rc522_status_t RC522_RequestA(uint8_t *pAtqa);

static rc522_status_t RC522_AntiCollisionCL1(
    uint8_t *pUidAndBcc);

static rc522_status_t RC522_SelectCL1(
    const uint8_t *pUidAndBcc,
    uint8_t *pSak);

/* ========================================================================= */
/* GPIO and Validation                                                       */
/* ========================================================================= */

static bool RC522_IsValidPins(uint8_t csPin,
                              uint8_t resetPin)
{
    bool bIsValid = false;

    if ((ARDUINO_VALID_TRUE ==
         Arduino_HasDigitalCapability(csPin)) &&
        (ARDUINO_VALID_TRUE ==
         Arduino_HasDigitalCapability(resetPin)) &&
        (csPin != resetPin))
    {
        bIsValid = true;
    }
    else
    {
        bIsValid = false;
    }

    return bIsValid;
}

static bool RC522_IsVersionSupported(uint8_t version)
{
    bool bIsSupported = false;

    if ((RC522_VERSION_COMPATIBLE_82 == version) ||
        (RC522_VERSION_CLONE_88 == version) ||
        (RC522_VERSION_1_0 == version) ||
        (RC522_VERSION_2_0 == version))
    {
        bIsSupported = true;
    }
    else
    {
        bIsSupported = false;
    }

    return bIsSupported;
}

static void RC522_Select(void)
{
    digitalWrite(s_u8Rc522CsPin, LOW);

    return;
}

static void RC522_Deselect(void)
{
    digitalWrite(s_u8Rc522CsPin, HIGH);

    return;
}

/* ========================================================================= */
/* Register Access                                                           */
/* ========================================================================= */

static uint8_t RC522_BuildReadAddress(uint8_t registerAddress)
{
    uint8_t u8Address = 0U;

    u8Address =
        (uint8_t)(RC522_SPI_READ_MASK |
                  ((registerAddress << 1U) &
                   RC522_SPI_ADDRESS_MASK));

    return u8Address;
}

static uint8_t RC522_BuildWriteAddress(uint8_t registerAddress)
{
    uint8_t u8Address = 0U;

    u8Address =
        (uint8_t)((registerAddress << 1U) &
                  RC522_SPI_ADDRESS_MASK);

    return u8Address;
}

static uint8_t RC522_ReadRegister(uint8_t registerAddress)
{
    uint8_t u8Value = 0U;

    RC522_Select();

    (void)SPI_transfer(
        RC522_BuildReadAddress(registerAddress));

    u8Value =
        SPI_transfer(RC522_SPI_DUMMY_BYTE);

    RC522_Deselect();

    return u8Value;
}

static void RC522_WriteRegister(uint8_t registerAddress,
                                uint8_t value)
{
    RC522_Select();

    (void)SPI_transfer(
        RC522_BuildWriteAddress(registerAddress));

    (void)SPI_transfer(value);

    RC522_Deselect();

    return;
}

static void RC522_SetRegisterBitMask(uint8_t registerAddress,
                                     uint8_t mask)
{
    uint8_t u8Value = 0U;

    u8Value = RC522_ReadRegister(registerAddress);
    u8Value = (uint8_t)(u8Value | mask);

    RC522_WriteRegister(registerAddress,
                        u8Value);

    return;
}

static void RC522_ClearRegisterBitMask(uint8_t registerAddress,
                                       uint8_t mask)
{
    uint8_t u8Value = 0U;

    u8Value = RC522_ReadRegister(registerAddress);
    u8Value = (uint8_t)(u8Value &
                        (uint8_t)(~mask));

    RC522_WriteRegister(registerAddress,
                        u8Value);

    return;
}

static void RC522_WriteRegisterBuffer(uint8_t registerAddress,
                                      const uint8_t *pBuffer,
                                      uint8_t length)
{
    uint8_t u8Index = 0U;

    if (((const uint8_t *)0 != pBuffer) &&
        (0U != length))
    {
        RC522_Select();

        (void)SPI_transfer(
            RC522_BuildWriteAddress(registerAddress));

        for (u8Index = 0U;
             u8Index < length;
             u8Index++)
        {
            (void)SPI_transfer(pBuffer[u8Index]);
        }

        RC522_Deselect();
    }
    else
    {
        /* Invalid buffer or zero length. */
    }

    return;
}

static void RC522_ReadRegisterBuffer(uint8_t registerAddress,
                                     uint8_t *pBuffer,
                                     uint8_t length)
{
    uint8_t u8Index = 0U;
    uint8_t u8ReadAddress = 0U;

    if (((uint8_t *)0 != pBuffer) &&
        (0U != length))
    {
        u8ReadAddress =
            RC522_BuildReadAddress(registerAddress);

        RC522_Select();

        /*
         * Begin the MFRC522 multi-byte read transaction.
         */
        (void)SPI_transfer(u8ReadAddress);

        /*
         * Retransmit the read address for all intermediate bytes.
         * This is required when sequentially reading FIFODataReg.
         */
        for (u8Index = 0U;
             u8Index < (uint8_t)(length - 1U);
             u8Index++)
        {
            pBuffer[u8Index] =
                SPI_transfer(u8ReadAddress);
        }

        /*
         * Receive the final byte while transmitting a dummy byte.
         */
        pBuffer[length - 1U] =
            SPI_transfer(RC522_SPI_DUMMY_BYTE);

        RC522_Deselect();
    }
    else
    {
        /* Invalid buffer or zero length. */
    }

    return;
}

/* ========================================================================= */
/* PCD Configuration                                                         */
/* ========================================================================= */

static void RC522_ApplyDefaultConfiguration(void)
{
    RC522_WriteRegister(RC522_REG_TX_MODE,
                        RC522_TX_MODE_DEFAULT);

    RC522_WriteRegister(RC522_REG_RX_MODE,
                        RC522_RX_MODE_DEFAULT);

    RC522_WriteRegister(RC522_REG_T_MODE,
                        RC522_T_MODE_DEFAULT);

    RC522_WriteRegister(RC522_REG_T_PRESCALER,
                        RC522_T_PRESCALER_DEFAULT);

    RC522_WriteRegister(RC522_REG_T_RELOAD_H,
                        RC522_T_RELOAD_H_DEFAULT);

    RC522_WriteRegister(RC522_REG_T_RELOAD_L,
                        RC522_T_RELOAD_L_DEFAULT);

    RC522_WriteRegister(RC522_REG_TX_ASK,
                        RC522_TX_ASK_DEFAULT);

    RC522_WriteRegister(RC522_REG_MODE,
                        RC522_MODE_DEFAULT);

    return;
}

/* ========================================================================= */
/* CRC Calculation                                                           */
/* ========================================================================= */

static rc522_status_t RC522_CalculateCRC(
    const uint8_t *pData,
    uint8_t length,
    uint8_t *pResult)
{
    rc522_status_t Status =
        RC522_STATUS_OK;

    uint32_t u32Timeout =
        RC522_CRC_TIMEOUT_COUNT;

    uint8_t u8Irq = 0U;

    if (((const uint8_t *)0 == pData) ||
        ((uint8_t *)0 == pResult) ||
        (0U == length))
    {
        Status = RC522_STATUS_INVALID_ARG;
    }
    else if (false == s_bRc522Initialized)
    {
        Status =
            RC522_STATUS_NOT_INITIALIZED;
    }
    else
    {
        RC522_WriteRegister(
            RC522_REG_COMMAND,
            RC522_COMMAND_IDLE);

        /*
         * Clear the CRC interrupt request flag.
         */
        RC522_WriteRegister(
            RC522_REG_DIV_IRQ,
            RC522_DIV_IRQ_CRC_MASK);

        RC522_SetRegisterBitMask(
            RC522_REG_FIFO_LEVEL,
            RC522_FIFO_FLUSH_MASK);

        RC522_WriteRegisterBuffer(
            RC522_REG_FIFO_DATA,
            pData,
            length);

        RC522_WriteRegister(
            RC522_REG_COMMAND,
            RC522_COMMAND_CALC_CRC);

        do
        {
            u8Irq =
                RC522_ReadRegister(
                    RC522_REG_DIV_IRQ);

            u32Timeout--;
        }
        while ((0U ==
                (u8Irq &
                 RC522_DIV_IRQ_CRC_MASK)) &&
               (0UL != u32Timeout));

        RC522_WriteRegister(
            RC522_REG_COMMAND,
            RC522_COMMAND_IDLE);

        if (0UL == u32Timeout)
        {
            Status = RC522_STATUS_TIMEOUT;
        }
        else
        {
            pResult[0U] =
                RC522_ReadRegister(
                    RC522_REG_CRC_RESULT_L);

            pResult[1U] =
                RC522_ReadRegister(
                    RC522_REG_CRC_RESULT_H);

            Status = RC522_STATUS_OK;
        }
    }

    return Status;
}

/* ========================================================================= */
/* PCD Communication                                                         */
/* ========================================================================= */

static rc522_status_t RC522_Transceive(
    const uint8_t *pTxData,
    uint8_t txLength,
    uint8_t txLastBits,
    uint8_t *pRxData,
    uint8_t *pRxLength,
    uint8_t *pRxValidBits)
{
    rc522_status_t Status =
        RC522_STATUS_OK;

    uint32_t u32Timeout =
        RC522_TRANSCEIVE_TIMEOUT_COUNT;

    uint8_t u8Irq = 0U;
    uint8_t u8Error = 0U;
    uint8_t u8FifoLevel = 0U;
    uint8_t u8ValidBits = 0U;

    if (((const uint8_t *)0 == pTxData) ||
        (0U == txLength))
    {
        Status = RC522_STATUS_INVALID_ARG;
    }
    else if (false == s_bRc522Initialized)
    {
        Status =
            RC522_STATUS_NOT_INITIALIZED;
    }
    else
    {
        RC522_WriteRegister(
            RC522_REG_COMMAND,
            RC522_COMMAND_IDLE);

        /*
         * Clear all ComIrqReg flags.
         */
        RC522_WriteRegister(
            RC522_REG_COM_IRQ,
            0x7FU);

        RC522_SetRegisterBitMask(
            RC522_REG_FIFO_LEVEL,
            RC522_FIFO_FLUSH_MASK);

        RC522_WriteRegisterBuffer(
            RC522_REG_FIFO_DATA,
            pTxData,
            txLength);

        RC522_WriteRegister(
            RC522_REG_BIT_FRAMING,
            (uint8_t)(txLastBits &
                      RC522_BIT_FRAMING_TX_LAST_BITS_MASK));

        RC522_WriteRegister(
            RC522_REG_COMMAND,
            RC522_COMMAND_TRANSCEIVE);

        RC522_SetRegisterBitMask(
            RC522_REG_BIT_FRAMING,
            RC522_BIT_FRAMING_START_SEND_MASK);

        do
        {
            u8Irq =
                RC522_ReadRegister(
                    RC522_REG_COM_IRQ);

            u32Timeout--;
        }
        while ((0U ==
                (u8Irq &
                 RC522_COM_IRQ_WAIT_MASK)) &&
               (0UL != u32Timeout));

        RC522_ClearRegisterBitMask(
            RC522_REG_BIT_FRAMING,
            RC522_BIT_FRAMING_START_SEND_MASK);

        if ((0UL == u32Timeout) ||
            (0U !=
             (u8Irq &
              RC522_COM_IRQ_TIMER_MASK)))
        {
            Status = RC522_STATUS_TIMEOUT;
        }
        else
        {
            u8Error =
                RC522_ReadRegister(
                    RC522_REG_ERROR);

            if (0U !=
                (u8Error &
                 RC522_ERROR_COLLISION_MASK))
            {
                Status =
                    RC522_STATUS_COLLISION;
            }
            else if (0U !=
                     (u8Error &
                      RC522_ERROR_CRC_MASK))
            {
                Status =
                    RC522_STATUS_CRC_ERROR;
            }
            else if (0U !=
                     (u8Error &
                      RC522_ERROR_FATAL_MASK))
            {
                Status =
                    RC522_STATUS_ERROR;
            }
            else if (((uint8_t *)0 != pRxData) &&
                     ((uint8_t *)0 != pRxLength))
            {
                u8FifoLevel =
                    RC522_ReadRegister(
                        RC522_REG_FIFO_LEVEL);

                if (*pRxLength < u8FifoLevel)
                {
                    Status =
                        RC522_STATUS_NO_ROOM;
                }
                else
                {
                    RC522_ReadRegisterBuffer(
                        RC522_REG_FIFO_DATA,
                        pRxData,
                        u8FifoLevel);

                    *pRxLength = u8FifoLevel;

                    u8ValidBits =
                        (uint8_t)(
                            RC522_ReadRegister(
                                RC522_REG_CONTROL) &
                            RC522_CONTROL_RX_LAST_BITS_MASK);

                    if ((uint8_t *)0 !=
                        pRxValidBits)
                    {
                        *pRxValidBits =
                            u8ValidBits;
                    }
                    else
                    {
                        /* Valid-bit output is optional. */
                    }

                    Status = RC522_STATUS_OK;
                }
            }
            else
            {
                /*
                 * A receive buffer is optional for commands such as HLTA.
                 */
                Status = RC522_STATUS_OK;
            }
        }
    }

    return Status;
}

/* ========================================================================= */
/* PICC Request and Selection                                                */
/* ========================================================================= */

static rc522_status_t RC522_RequestA(uint8_t *pAtqa)
{
    rc522_status_t Status =
        RC522_STATUS_OK;

    uint8_t au8Command[1U] =
    {
        RC522_PICC_COMMAND_REQA
    };

    uint8_t u8ResponseLength =
        RC522_ATQA_SIZE;

    uint8_t u8ValidBits = 0U;

    if ((uint8_t *)0 == pAtqa)
    {
        Status =
            RC522_STATUS_INVALID_ARG;
    }
    else
    {
        RC522_ClearRegisterBitMask(
            RC522_REG_COLL,
            RC522_COLL_VALUES_AFTER_MASK);

        Status = RC522_Transceive(
            au8Command,
            1U,
            RC522_REQA_VALID_BITS,
            pAtqa,
            &u8ResponseLength,
            &u8ValidBits);

        if (RC522_STATUS_OK == Status)
        {
            if ((RC522_ATQA_SIZE !=
                 u8ResponseLength) ||
                (0U != u8ValidBits))
            {
                Status =
                    RC522_STATUS_ERROR;
            }
            else
            {
                /* A valid ATQA response was received. */
            }
        }
    }

    return Status;
}

static rc522_status_t RC522_AntiCollisionCL1(
    uint8_t *pUidAndBcc)
{
    rc522_status_t Status =
        RC522_STATUS_OK;

    uint8_t au8Frame[2U] =
    {
        RC522_PICC_COMMAND_SEL_CL1,
        RC522_PICC_NVB_ANTICOLLISION
    };

    uint8_t u8ResponseLength =
        RC522_ANTICOLLISION_SIZE;

    uint8_t u8ValidBits = 0U;
    uint8_t u8Bcc = 0U;

    if ((uint8_t *)0 == pUidAndBcc)
    {
        Status =
            RC522_STATUS_INVALID_ARG;
    }
    else
    {
        RC522_ClearRegisterBitMask(
            RC522_REG_COLL,
            RC522_COLL_VALUES_AFTER_MASK);

        Status = RC522_Transceive(
            au8Frame,
            2U,
            RC522_FULL_BYTE_VALID_BITS,
            pUidAndBcc,
            &u8ResponseLength,
            &u8ValidBits);

        if (RC522_STATUS_OK == Status)
        {
            if ((RC522_ANTICOLLISION_SIZE !=
                 u8ResponseLength) ||
                (0U != u8ValidBits))
            {
                Status =
                    RC522_STATUS_ERROR;
            }
            else
            {
                u8Bcc =
                    (uint8_t)(
                        pUidAndBcc[0U] ^
                        pUidAndBcc[1U] ^
                        pUidAndBcc[2U] ^
                        pUidAndBcc[3U]);

                if (u8Bcc !=
                    pUidAndBcc[
                        RC522_UID_BCC_INDEX])
                {
                    Status =
                        RC522_STATUS_CRC_ERROR;
                }
                else
                {
                    /* UID BCC validation succeeded. */
                }
            }
        }
    }

    return Status;
}

static rc522_status_t RC522_SelectCL1(
    const uint8_t *pUidAndBcc,
    uint8_t *pSak)
{
    rc522_status_t Status =
        RC522_STATUS_OK;

    uint8_t au8Frame[
        RC522_SELECT_FRAME_SIZE] = {0U};

    uint8_t au8Response[
        RC522_SELECT_RESPONSE_SIZE] = {0U};

    uint8_t au8CalculatedCrc[
        RC522_CRC_SIZE] = {0U};

    uint8_t u8ResponseLength =
        RC522_SELECT_RESPONSE_SIZE;

    uint8_t u8ValidBits = 0U;
    uint8_t u8Index = 0U;

    if (((const uint8_t *)0 ==
         pUidAndBcc) ||
        ((uint8_t *)0 == pSak))
    {
        Status =
            RC522_STATUS_INVALID_ARG;
    }
    else
    {
        au8Frame[0U] =
            RC522_PICC_COMMAND_SEL_CL1;

        au8Frame[1U] =
            RC522_PICC_NVB_SELECT;

        for (u8Index = 0U;
             u8Index <
             RC522_ANTICOLLISION_SIZE;
             u8Index++)
        {
            au8Frame[u8Index + 2U] =
                pUidAndBcc[u8Index];
        }

        Status = RC522_CalculateCRC(
            au8Frame,
            7U,
            &au8Frame[7U]);

        if (RC522_STATUS_OK == Status)
        {
            Status = RC522_Transceive(
                au8Frame,
                RC522_SELECT_FRAME_SIZE,
                RC522_FULL_BYTE_VALID_BITS,
                au8Response,
                &u8ResponseLength,
                &u8ValidBits);
        }
        else
        {
            /* CRC calculation failed. */
        }

        if (RC522_STATUS_OK == Status)
        {
            if ((RC522_SELECT_RESPONSE_SIZE !=
                 u8ResponseLength) ||
                (0U != u8ValidBits))
            {
                Status =
                    RC522_STATUS_ERROR;
            }
            else
            {
                Status = RC522_CalculateCRC(
                    au8Response,
                    1U,
                    au8CalculatedCrc);
            }
        }

        if (RC522_STATUS_OK == Status)
        {
            if ((au8CalculatedCrc[0U] !=
                 au8Response[1U]) ||
                (au8CalculatedCrc[1U] !=
                 au8Response[2U]))
            {
                Status =
                    RC522_STATUS_CRC_ERROR;
            }
            else
            {
                *pSak =
                    au8Response[RC522_SAK_INDEX];

                if (0U !=
                    (*pSak &
                     RC522_SAK_CASCADE_BIT_MASK))
                {
                    /*
                     * Cascade Level 2 is required, but the current library
                     * only supports four-byte UIDs.
                     */
                    Status =
                        RC522_STATUS_ERROR;
                }
                else
                {
                    /* Single-size UID selection succeeded. */
                }
            }
        }
    }

    return Status;
}

/* ========================================================================= */
/* Public PCD API                                                            */
/* ========================================================================= */

bool RC522_PCD_Init(uint8_t csPin,
                    uint8_t resetPin)
{
    bool bInitialized = false;
    bool bResetCompleted = false;
    uint8_t u8Version =
        RC522_VERSION_INVALID;

    s_bRc522PinsConfigured = false;
    s_bRc522Initialized = false;

    if (true ==
        RC522_IsValidPins(csPin,
                          resetPin))
    {
        s_u8Rc522CsPin = csPin;
        s_u8Rc522ResetPin = resetPin;

        pinMode(s_u8Rc522CsPin,
                OUTPUT);

        pinMode(s_u8Rc522ResetPin,
                OUTPUT);

        RC522_Deselect();

        digitalWrite(s_u8Rc522ResetPin,
                     HIGH);

        s_bRc522PinsConfigured = true;

        SPI_beginEx(SPI_ROLE_MASTER,
                    RC522_SPI_FREQUENCY_HZ,
                    SPI_MODE0,
                    SPI_MSBFIRST);

        bResetCompleted =
            RC522_PCD_Reset();

        if (true == bResetCompleted)
        {
            RC522_ApplyDefaultConfiguration();
            RC522_PCD_AntennaOn();

            u8Version =
                RC522_ReadRegister(
                    RC522_REG_VERSION);

            if (true ==
                RC522_IsVersionSupported(
                    u8Version))
            {
                s_bRc522Initialized = true;
                bInitialized = true;
            }
            else
            {
                s_bRc522Initialized = false;
                bInitialized = false;
            }
        }
        else
        {
            bInitialized = false;
        }
    }
    else
    {
        bInitialized = false;
    }

    return bInitialized;
}

bool RC522_PCD_Reset(void)
{
    bool bResetCompleted = false;

    uint32_t u32Timeout =
        RC522_RESET_TIMEOUT_COUNT;

    uint8_t u8Command = 0U;

    /*
     * A raw reset invalidates the active configuration.
     */
    s_bRc522Initialized = false;

    if (true == s_bRc522PinsConfigured)
    {
        RC522_Deselect();

        digitalWrite(s_u8Rc522ResetPin,
                     LOW);

        delay(RC522_HARD_RESET_LOW_DELAY_MS);

        digitalWrite(s_u8Rc522ResetPin,
                     HIGH);

        delay(
            RC522_HARD_RESET_STARTUP_DELAY_MS);

        RC522_WriteRegister(
            RC522_REG_COMMAND,
            RC522_COMMAND_SOFT_RESET);

        delay(RC522_SOFT_RESET_DELAY_MS);

        do
        {
            u8Command =
                RC522_ReadRegister(
                    RC522_REG_COMMAND);

            u32Timeout--;
        }
        while ((0U !=
                (u8Command &
                 RC522_COMMAND_POWER_DOWN_MASK)) &&
               (0UL != u32Timeout));

        if (0UL != u32Timeout)
        {
            RC522_WriteRegister(
                RC522_REG_COMMAND,
                RC522_COMMAND_IDLE);

            bResetCompleted = true;
        }
        else
        {
            bResetCompleted = false;
        }
    }
    else
    {
        bResetCompleted = false;
    }

    return bResetCompleted;
}

void RC522_PCD_AntennaOn(void)
{
    uint8_t u8Value = 0U;

    if (true == s_bRc522PinsConfigured)
    {
        u8Value =
            RC522_ReadRegister(
                RC522_REG_TX_CONTROL);

        if (RC522_TX_CONTROL_ANTENNA_MASK !=
            (u8Value &
             RC522_TX_CONTROL_ANTENNA_MASK))
        {
            RC522_SetRegisterBitMask(
                RC522_REG_TX_CONTROL,
                RC522_TX_CONTROL_ANTENNA_MASK);
        }
        else
        {
            /* Antenna is already enabled. */
        }
    }
    else
    {
        /* Control pins are not configured. */
    }

    return;
}

void RC522_PCD_AntennaOff(void)
{
    if (true == s_bRc522PinsConfigured)
    {
        RC522_ClearRegisterBitMask(
            RC522_REG_TX_CONTROL,
            RC522_TX_CONTROL_ANTENNA_MASK);
    }
    else
    {
        /* Control pins are not configured. */
    }

    return;
}

uint8_t RC522_PCD_GetVersion(void)
{
    uint8_t u8Version =
        RC522_VERSION_INVALID;

    if (true == s_bRc522Initialized)
    {
        u8Version =
            RC522_ReadRegister(
                RC522_REG_VERSION);
    }
    else
    {
        u8Version =
            RC522_VERSION_INVALID;
    }

    return u8Version;
}

bool RC522_PCD_IsInitialized(void)
{
    return s_bRc522Initialized;
}

/* ========================================================================= */
/* Public PICC API                                                           */
/* ========================================================================= */

bool RC522_PICC_IsNewCardPresent(void)
{
    bool bCardPresent = false;

    uint8_t au8Atqa[
        RC522_ATQA_SIZE] = {0U};

    if (true == s_bRc522Initialized)
    {
        if (RC522_STATUS_OK ==
            RC522_RequestA(au8Atqa))
        {
            bCardPresent = true;
        }
        else
        {
            bCardPresent = false;
        }
    }
    else
    {
        bCardPresent = false;
    }

    return bCardPresent;
}

bool RC522_PICC_ReadCardSerial(
    rc522_uid_t *pUid)
{
    bool bUidRead = false;

    rc522_status_t Status =
        RC522_STATUS_OK;

    uint8_t au8UidAndBcc[
        RC522_ANTICOLLISION_SIZE] =
    {
        0U
    };

    uint8_t u8Sak = 0U;
    uint8_t u8Index = 0U;

    if (((rc522_uid_t *)0 != pUid) &&
        (true == s_bRc522Initialized))
    {
        /*
         * Clear output fields before attempting communication.
         */
        for (u8Index = 0U;
             u8Index < RC522_UID_MAX_SIZE;
             u8Index++)
        {
            pUid->bytes[u8Index] = 0U;
        }

        pUid->size = 0U;
        pUid->sak = 0U;

        Status =
            RC522_AntiCollisionCL1(
                au8UidAndBcc);

        if (RC522_STATUS_OK == Status)
        {
            Status =
                RC522_SelectCL1(
                    au8UidAndBcc,
                    &u8Sak);
        }
        else
        {
            /* Anti-collision failed. */
        }

        if (RC522_STATUS_OK == Status)
        {
            for (u8Index = 0U;
                 u8Index <
                 RC522_UID_SINGLE_SIZE;
                 u8Index++)
            {
                pUid->bytes[u8Index] =
                    au8UidAndBcc[u8Index];
            }

            pUid->size =
                RC522_UID_SINGLE_SIZE;

            pUid->sak = u8Sak;

            bUidRead = true;
        }
        else
        {
            bUidRead = false;
        }
    }
    else
    {
        bUidRead = false;
    }

    return bUidRead;
}

void RC522_PICC_HaltA(void)
{
    rc522_status_t Status =
        RC522_STATUS_OK;

    uint8_t au8Frame[
        RC522_HALT_FRAME_SIZE] =
    {
        RC522_PICC_COMMAND_HLTA,
        0x00U,
        0x00U,
        0x00U
    };

    if (true == s_bRc522Initialized)
    {
        Status = RC522_CalculateCRC(
            au8Frame,
            2U,
            &au8Frame[2U]);

        if (RC522_STATUS_OK == Status)
        {
            /*
             * A correctly halted PICC does not return a normal response.
             * The resulting transceive timeout is intentionally ignored.
             */
            (void)RC522_Transceive(
                au8Frame,
                RC522_HALT_FRAME_SIZE,
                RC522_FULL_BYTE_VALID_BITS,
                (uint8_t *)0,
                (uint8_t *)0,
                (uint8_t *)0);
        }
        else
        {
            /* CRC calculation failed. */
        }
    }
    else
    {
        /* Reader is not initialized. */
    }

    return;
}
