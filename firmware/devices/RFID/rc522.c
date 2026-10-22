/**
 * @file rc522.c
 * @brief MFRC522 register and ISO14443A CL1 protocol over shared SPI.
 * @details One SPI transaction per register/buffer access. No malloc, no
 * ISR use, no nested transactions. CS is released after endTransaction().
 */
#include "rc522.h"
#include "Arduino.h"
#include "arduino_pins.h"
#include <stddef.h>

#define RC522_READ_MASK (0x80U)
#define RC522_ADDRESS_MASK (0x7EU)
#define RC522_DUMMY (0x00U)
#define RC522_CMD_IDLE (0x00U)
#define RC522_CMD_CRC (0x03U)
#define RC522_CMD_TRANSCEIVE (0x0CU)
#define RC522_CMD_RESET (0x0FU)
#define RC522_PICC_REQA (0x26U)
#define RC522_PICC_HLTA (0x50U)
#define RC522_PICC_SEL_CL1 (0x93U)
#define RC522_REG_COMMAND (0x01U)
#define RC522_REG_COM_IRQ (0x04U)
#define RC522_REG_DIV_IRQ (0x05U)
#define RC522_REG_ERROR (0x06U)
#define RC522_REG_FIFO_DATA (0x09U)
#define RC522_REG_FIFO_LEVEL (0x0AU)
#define RC522_REG_CONTROL (0x0CU)
#define RC522_REG_BIT_FRAMING (0x0DU)
#define RC522_REG_COLL (0x0EU)
#define RC522_REG_MODE (0x11U)
#define RC522_REG_TX_MODE (0x12U)
#define RC522_REG_RX_MODE (0x13U)
#define RC522_REG_TX_CONTROL (0x14U)
#define RC522_REG_TX_ASK (0x15U)
#define RC522_REG_CRC_H (0x21U)
#define RC522_REG_CRC_L (0x22U)
#define RC522_REG_T_MODE (0x2AU)
#define RC522_REG_T_PRESCALER (0x2BU)
#define RC522_REG_T_RELOAD_H (0x2CU)
#define RC522_REG_T_RELOAD_L (0x2DU)
#define RC522_REG_VERSION (0x37U)
#define RC522_IRQ_CRC (0x04U)
#define RC522_IRQ_WAIT (0x31U)
#define RC522_IRQ_TIMER (0x01U)
#define RC522_ERROR_COLLISION (0x08U)
#define RC522_ERROR_CRC (0x04U)
#define RC522_ERROR_FATAL (0x11U)
#define RC522_FIFO_FLUSH (0x80U)
#define RC522_START_SEND (0x80U)
#define RC522_ANTENNA_MASK (0x03U)
#define RC522_RESET_POLL_LIMIT (1000UL)
#define RC522_CRC_POLL_LIMIT (5000UL)
#define RC522_RF_POLL_LIMIT (10000UL)

typedef enum
{
    RC522_OK = 0,
    RC522_ERROR,
    RC522_TIMEOUT,
    RC522_COLLISION,
    RC522_CRC_ERROR,
    RC522_INVALID_ARG,
    RC522_NO_ROOM,
    RC522_NOT_READY,
    RC522_SPI_ERROR
} rc522_status_t;

static uint8_t s_csPin = GPIO4;
static uint8_t s_resetPin = GPIO5;
static bool s_pinsReady = false;
static bool s_initialized = false;
static SPI_Status_t s_lastSpi = SPI_STATUS_NOT_INIT;
static const SPI_Settings_t s_spiSettings = {SPI_MODE0, SPI_MSBFIRST, SPI_FRAME_SIZE_8, RC522_SPI_FREQUENCY_HZ};

static bool RC522_ValidPins(uint8_t cs, uint8_t rst)
{
    return (cs != rst) && (ARDUINO_VALID_TRUE == Arduino_HasDigitalCapability(cs)) &&
           (ARDUINO_VALID_TRUE == Arduino_HasDigitalCapability(rst));
}

static bool RC522_ValidVersion(uint8_t version)
{
    return (RC522_VERSION_COMPATIBLE_82 == version) || (RC522_VERSION_CLONE_88 == version) ||
           (RC522_VERSION_1_0 == version) || (RC522_VERSION_2_0 == version);
}

/** @brief Acquire SPI profile first, then assert CS. */
static rc522_status_t RC522_BeginAccess(void)
{
    rc522_status_t result = RC522_NOT_READY;
    if (s_pinsReady)
    {
        s_lastSpi = SPI_beginTransaction(&s_spiSettings);
        if (SPI_STATUS_OK == s_lastSpi)
        {
            digitalWrite(s_csPin, LOW);
            result = RC522_OK;
        }
        else
        {
            result = RC522_SPI_ERROR;
        }
    }
    return result;
}

/** @brief Keep CS low until the bus has finished the final bit. */
static rc522_status_t RC522_EndAccess(rc522_status_t result)
{
    SPI_Status_t endStatus = SPI_endTransaction();
    digitalWrite(s_csPin, HIGH);
    if (SPI_STATUS_OK != endStatus)
    {
        s_lastSpi = endStatus;
        result = RC522_SPI_ERROR;
    }
    else if (RC522_SPI_ERROR != result)
    {
        s_lastSpi = SPI_STATUS_OK;
    }
    return result;
}

static rc522_status_t RC522_Exchange(uint8_t tx, uint8_t *rx)
{
    rc522_status_t result = RC522_OK;
    uint8_t value = SPI_transfer(tx);
    SPI_Status_t status = SPI_getLastStatus();
    if (SPI_STATUS_OK != status)
    {
        s_lastSpi = status;
        result = RC522_SPI_ERROR;
    }
    else if (NULL != rx)
    {
        *rx = value;
    }
    return result;
}

static uint8_t RC522_ReadAddress(uint8_t reg)
{
    return (uint8_t)(RC522_READ_MASK | ((reg << 1U) & RC522_ADDRESS_MASK));
}

static uint8_t RC522_WriteAddress(uint8_t reg)
{
    return (uint8_t)((reg << 1U) & RC522_ADDRESS_MASK);
}

static rc522_status_t RC522_ReadReg(uint8_t reg, uint8_t *value)
{
    rc522_status_t result = RC522_INVALID_ARG;
    uint8_t readValue = 0U;
    bool acquired = false;
    if (NULL != value)
    {
        result = RC522_BeginAccess();
        acquired = (RC522_OK == result);
        if (RC522_OK == result)
            result = RC522_Exchange(RC522_ReadAddress(reg), NULL);
        if (RC522_OK == result)
            result = RC522_Exchange(RC522_DUMMY, &readValue);
        if (acquired)
            result = RC522_EndAccess(result);
        if (RC522_OK == result)
            *value = readValue;
    }
    return result;
}

static rc522_status_t RC522_WriteReg(uint8_t reg, uint8_t value)
{
    rc522_status_t result = RC522_BeginAccess();
    bool acquired = (RC522_OK == result);
    if (RC522_OK == result)
        result = RC522_Exchange(RC522_WriteAddress(reg), NULL);
    if (RC522_OK == result)
        result = RC522_Exchange(value, NULL);
    if (acquired)
        result = RC522_EndAccess(result);
    return result;
}

static rc522_status_t RC522_WriteBuffer(uint8_t reg, const uint8_t *data, uint8_t length)
{
    rc522_status_t result = RC522_INVALID_ARG;
    uint8_t i = 0U;
    bool acquired = false;
    if ((NULL != data) && (0U != length))
    {
        result = RC522_BeginAccess();
        acquired = (RC522_OK == result);
        if (RC522_OK == result)
            result = RC522_Exchange(RC522_WriteAddress(reg), NULL);
        while ((RC522_OK == result) && (i < length))
        {
            result = RC522_Exchange(data[i], NULL);
            i++;
        }
        if (acquired)
            result = RC522_EndAccess(result);
    }
    return result;
}

/** @brief Preserve original MFRC522 FIFO read framing. */
static rc522_status_t RC522_ReadBuffer(uint8_t reg, uint8_t *data, uint8_t length)
{
    rc522_status_t result = RC522_INVALID_ARG;
    uint8_t i = 0U;
    uint8_t address = RC522_ReadAddress(reg);
    bool acquired = false;
    if ((NULL != data) && (0U != length))
    {
        result = RC522_BeginAccess();
        acquired = (RC522_OK == result);
        if (RC522_OK == result)
            result = RC522_Exchange(address, NULL);
        while ((RC522_OK == result) && (i < length))
        {
            uint8_t tx = ((uint8_t)(i + 1U) < length) ? address : RC522_DUMMY;
            result = RC522_Exchange(tx, &data[i]);
            i++;
        }
        if (acquired)
            result = RC522_EndAccess(result);
    }
    return result;
}

static rc522_status_t RC522_UpdateBits(uint8_t reg, uint8_t mask, bool set)
{
    uint8_t value = 0U;
    rc522_status_t result = RC522_ReadReg(reg, &value);
    if (RC522_OK == result)
    {
        value = set ? (uint8_t)(value | mask) : (uint8_t)(value & (uint8_t)~mask);
        result = RC522_WriteReg(reg, value);
    }
    return result;
}

static rc522_status_t RC522_ApplyDefaults(void)
{
    rc522_status_t result = RC522_WriteReg(RC522_REG_TX_MODE, 0x00U);
    if (RC522_OK == result)
        result = RC522_WriteReg(RC522_REG_RX_MODE, 0x00U);
    if (RC522_OK == result)
        result = RC522_WriteReg(RC522_REG_T_MODE, 0x80U);
    if (RC522_OK == result)
        result = RC522_WriteReg(RC522_REG_T_PRESCALER, 0xA9U);
    if (RC522_OK == result)
        result = RC522_WriteReg(RC522_REG_T_RELOAD_H, 0x03U);
    if (RC522_OK == result)
        result = RC522_WriteReg(RC522_REG_T_RELOAD_L, 0xE8U);
    if (RC522_OK == result)
        result = RC522_WriteReg(RC522_REG_TX_ASK, 0x40U);
    if (RC522_OK == result)
        result = RC522_WriteReg(RC522_REG_MODE, 0x3DU);
    return result;
}

static rc522_status_t RC522_CalculateCRC(const uint8_t *data, uint8_t length, uint8_t *crc)
{
    rc522_status_t result = RC522_INVALID_ARG;
    uint8_t irq = 0U;
    uint32_t remaining = RC522_CRC_POLL_LIMIT;
    if ((NULL != data) && (NULL != crc) && (0U != length) && s_initialized)
    {
        result = RC522_WriteReg(RC522_REG_COMMAND, RC522_CMD_IDLE);
        if (RC522_OK == result)
            result = RC522_WriteReg(RC522_REG_DIV_IRQ, RC522_IRQ_CRC);
        if (RC522_OK == result)
            result = RC522_UpdateBits(RC522_REG_FIFO_LEVEL, RC522_FIFO_FLUSH, true);
        if (RC522_OK == result)
            result = RC522_WriteBuffer(RC522_REG_FIFO_DATA, data, length);
        if (RC522_OK == result)
            result = RC522_WriteReg(RC522_REG_COMMAND, RC522_CMD_CRC);
        while ((RC522_OK == result) && (0UL < remaining))
        {
            result = RC522_ReadReg(RC522_REG_DIV_IRQ, &irq);
            if ((RC522_OK != result) || (0U != (irq & RC522_IRQ_CRC)))
                break;
            remaining--;
        }
        if (RC522_OK == result)
        {
            result = RC522_WriteReg(RC522_REG_COMMAND, RC522_CMD_IDLE);
            if ((RC522_OK == result) && (0U == (irq & RC522_IRQ_CRC)))
                result = RC522_TIMEOUT;
            if (RC522_OK == result)
                result = RC522_ReadReg(RC522_REG_CRC_L, &crc[0]);
            if (RC522_OK == result)
                result = RC522_ReadReg(RC522_REG_CRC_H, &crc[1]);
        }
    }
    else if (!s_initialized)
    {
        result = RC522_NOT_READY;
    }
    return result;
}

/** @brief Execute one PICC frame using MFRC522's internal FIFO/IRQ. */
static rc522_status_t RC522_Transceive(const uint8_t *tx, uint8_t txLength, uint8_t txLastBits,
                                       uint8_t *rx, uint8_t *rxLength, uint8_t *rxValidBits)
{
    rc522_status_t result = RC522_INVALID_ARG;
    uint8_t irq = 0U;
    uint8_t error = 0U;
    uint8_t fifoLength = 0U;
    uint8_t validBits = 0U;
    uint32_t remaining = RC522_RF_POLL_LIMIT;
    if ((NULL != tx) && (0U != txLength) && s_initialized)
    {
        result = RC522_WriteReg(RC522_REG_COMMAND, RC522_CMD_IDLE);
        if (RC522_OK == result)
            result = RC522_WriteReg(RC522_REG_COM_IRQ, 0x7FU);
        if (RC522_OK == result)
            result = RC522_UpdateBits(RC522_REG_FIFO_LEVEL, RC522_FIFO_FLUSH, true);
        if (RC522_OK == result)
            result = RC522_WriteBuffer(RC522_REG_FIFO_DATA, tx, txLength);
        if (RC522_OK == result)
            result = RC522_WriteReg(RC522_REG_BIT_FRAMING, (uint8_t)(txLastBits & 0x07U));
        if (RC522_OK == result)
            result = RC522_WriteReg(RC522_REG_COMMAND, RC522_CMD_TRANSCEIVE);
        if (RC522_OK == result)
            result = RC522_UpdateBits(RC522_REG_BIT_FRAMING, RC522_START_SEND, true);
        while ((RC522_OK == result) && (0UL < remaining))
        {
            result = RC522_ReadReg(RC522_REG_COM_IRQ, &irq);
            if ((RC522_OK != result) || (0U != (irq & RC522_IRQ_WAIT)))
                break;
            remaining--;
        }
        if (RC522_OK == result)
            result = RC522_UpdateBits(RC522_REG_BIT_FRAMING, RC522_START_SEND, false);
        if ((RC522_OK == result) && ((0UL == remaining) || (0U != (irq & RC522_IRQ_TIMER))))
            result = RC522_TIMEOUT;
        if (RC522_OK == result)
            result = RC522_ReadReg(RC522_REG_ERROR, &error);
        if ((RC522_OK == result) && (0U != (error & RC522_ERROR_COLLISION)))
            result = RC522_COLLISION;
        if ((RC522_OK == result) && (0U != (error & RC522_ERROR_CRC)))
            result = RC522_CRC_ERROR;
        if ((RC522_OK == result) && (0U != (error & RC522_ERROR_FATAL)))
            result = RC522_ERROR;
        if ((RC522_OK == result) && (NULL != rx) && (NULL != rxLength))
        {
            result = RC522_ReadReg(RC522_REG_FIFO_LEVEL, &fifoLength);
            if ((RC522_OK == result) && (*rxLength < fifoLength))
                result = RC522_NO_ROOM;
            if ((RC522_OK == result) && (0U < fifoLength))
                result = RC522_ReadBuffer(RC522_REG_FIFO_DATA, rx, fifoLength);
            if (RC522_OK == result)
                *rxLength = fifoLength;
            if (RC522_OK == result)
                result = RC522_ReadReg(RC522_REG_CONTROL, &validBits);
            if ((RC522_OK == result) && (NULL != rxValidBits))
                *rxValidBits = (uint8_t)(validBits & 0x07U);
        }
    }
    else if (!s_initialized)
    {
        result = RC522_NOT_READY;
    }
    return result;
}

static rc522_status_t RC522_RequestA(uint8_t *atqa)
{
    const uint8_t request = RC522_PICC_REQA;
    uint8_t length = 2U;
    uint8_t bits = 0U;
    rc522_status_t result = RC522_INVALID_ARG;
    if (NULL != atqa)
    {
        result = RC522_UpdateBits(RC522_REG_COLL, 0x80U, false);
        if (RC522_OK == result)
            result = RC522_Transceive(&request, 1U, 7U, atqa, &length, &bits);
        if ((RC522_OK == result) && ((2U != length) || (0U != bits)))
            result = RC522_ERROR;
    }
    return result;
}

static rc522_status_t RC522_AntiCollision(uint8_t *uidAndBcc)
{
    const uint8_t frame[2] = {RC522_PICC_SEL_CL1, 0x20U};
    uint8_t length = 5U;
    uint8_t bits = 0U;
    uint8_t bcc = 0U;
    rc522_status_t result = RC522_INVALID_ARG;
    if (NULL != uidAndBcc)
    {
        result = RC522_UpdateBits(RC522_REG_COLL, 0x80U, false);
        if (RC522_OK == result)
            result = RC522_Transceive(frame, 2U, 0U, uidAndBcc, &length, &bits);
        if ((RC522_OK == result) && ((5U != length) || (0U != bits)))
            result = RC522_ERROR;
        if (RC522_OK == result)
        {
            bcc = (uint8_t)(uidAndBcc[0] ^ uidAndBcc[1] ^ uidAndBcc[2] ^ uidAndBcc[3]);
            if (bcc != uidAndBcc[4])
                result = RC522_CRC_ERROR;
        }
    }
    return result;
}

static rc522_status_t RC522_SelectCL1(const uint8_t *uidAndBcc, uint8_t *sak)
{
    uint8_t frame[9] = {RC522_PICC_SEL_CL1, 0x70U, 0U, 0U, 0U, 0U, 0U, 0U, 0U};
    uint8_t response[3] = {0U};
    uint8_t crc[2] = {0U};
    uint8_t length = 3U;
    uint8_t bits = 0U;
    uint8_t i = 0U;
    rc522_status_t result = RC522_INVALID_ARG;
    if ((NULL != uidAndBcc) && (NULL != sak))
    {
        for (i = 0U; i < 5U; i++)
            frame[i + 2U] = uidAndBcc[i];
        result = RC522_CalculateCRC(frame, 7U, &frame[7]);
        if (RC522_OK == result)
            result = RC522_Transceive(frame, 9U, 0U, response, &length, &bits);
        if ((RC522_OK == result) && ((3U != length) || (0U != bits)))
            result = RC522_ERROR;
        if (RC522_OK == result)
            result = RC522_CalculateCRC(response, 1U, crc);
        if ((RC522_OK == result) && ((crc[0] != response[1]) || (crc[1] != response[2])))
            result = RC522_CRC_ERROR;
        if (RC522_OK == result)
        {
            *sak = response[0];
            if (0U != (*sak & 0x04U))
                result = RC522_ERROR; /* UID needs CL2. */
        }
    }
    return result;
}

bool RC522_PCD_Init(uint8_t csPin, uint8_t resetPin)
{
    bool success = false;
    uint8_t version = RC522_VERSION_INVALID;
    rc522_status_t result = RC522_NOT_READY;
    s_initialized = false;
    s_pinsReady = false;
    if (RC522_ValidPins(csPin, resetPin))
    {
        /* Put CS HIGH before any SPI clock or profile change. */
        s_csPin = csPin;
        s_resetPin = resetPin;
        pinMode(s_csPin, OUTPUT);
        pinMode(s_resetPin, OUTPUT);
        digitalWrite(s_csPin, HIGH);
        digitalWrite(s_resetPin, HIGH);
        s_lastSpi = SPI_beginTransaction(&s_spiSettings);
        if (SPI_STATUS_OK == s_lastSpi)
        {
            s_lastSpi = SPI_endTransaction();
            if (SPI_STATUS_OK == s_lastSpi)
            {
                s_pinsReady = true;
                if (RC522_PCD_Reset())
                {
                    result = RC522_ApplyDefaults();
                    if (RC522_OK == result)
                        result = RC522_UpdateBits(RC522_REG_TX_CONTROL, RC522_ANTENNA_MASK, true);
                    if (RC522_OK == result)
                        result = RC522_ReadReg(RC522_REG_VERSION, &version);
                    success = (RC522_OK == result) && RC522_ValidVersion(version);
                }
            }
        }
    }
    else
    {
        s_lastSpi = SPI_STATUS_INVALID_ARG;
    }
    s_initialized = success;
    return success;
}

bool RC522_PCD_Reset(void)
{
    bool success = false;
    uint8_t command = 0U;
    uint32_t remaining = RC522_RESET_POLL_LIMIT;
    rc522_status_t result = RC522_NOT_READY;
    s_initialized = false;
    if (s_pinsReady)
    {
        digitalWrite(s_csPin, HIGH);
        digitalWrite(s_resetPin, LOW);
        delay(2U);
        digitalWrite(s_resetPin, HIGH);
        delay(50U);
        result = RC522_WriteReg(RC522_REG_COMMAND, RC522_CMD_RESET);
        if (RC522_OK == result)
            delay(1U);
        while ((RC522_OK == result) && (0UL < remaining))
        {
            result = RC522_ReadReg(RC522_REG_COMMAND, &command);
            if ((RC522_OK != result) || (0U == (command & 0x10U)))
                break;
            remaining--;
        }
        if ((RC522_OK == result) && (0U == (command & 0x10U)))
        {
            result = RC522_WriteReg(RC522_REG_COMMAND, RC522_CMD_IDLE);
            success = (RC522_OK == result);
        }
    }
    return success;
}

void RC522_PCD_AntennaOn(void)
{
    if (s_initialized)
        (void)RC522_UpdateBits(RC522_REG_TX_CONTROL, RC522_ANTENNA_MASK, true);
}

void RC522_PCD_AntennaOff(void)
{
    if (s_initialized)
        (void)RC522_UpdateBits(RC522_REG_TX_CONTROL, RC522_ANTENNA_MASK, false);
}

uint8_t RC522_PCD_GetVersion(void)
{
    uint8_t version = RC522_VERSION_INVALID;
    if (s_initialized && (RC522_OK != RC522_ReadReg(RC522_REG_VERSION, &version)))
        version = RC522_VERSION_INVALID;
    return version;
}

bool RC522_PCD_IsInitialized(void)
{
    return s_initialized;
}

SPI_Status_t RC522_PCD_GetLastSpiStatus(void)
{
    return s_lastSpi;
}

bool RC522_PICC_IsNewCardPresent(void)
{
    uint8_t atqa[2] = {0U};
    return s_initialized && (RC522_OK == RC522_RequestA(atqa));
}

bool RC522_PICC_ReadCardSerial(rc522_uid_t *uid)
{
    bool success = false;
    uint8_t block[5] = {0U};
    uint8_t sak = 0U;
    uint8_t i = 0U;
    rc522_status_t result = RC522_NOT_READY;
    if (NULL != uid)
    {
        for (i = 0U; i < RC522_UID_MAX_SIZE; i++)
            uid->bytes[i] = 0U;
        uid->size = 0U;
        uid->sak = 0U;
        if (s_initialized)
        {
            result = RC522_AntiCollision(block);
            if (RC522_OK == result)
                result = RC522_SelectCL1(block, &sak);
            if (RC522_OK == result)
            {
                for (i = 0U; i < RC522_UID_SINGLE_SIZE; i++)
                    uid->bytes[i] = block[i];
                uid->size = RC522_UID_SINGLE_SIZE;
                uid->sak = sak;
                success = true;
            }
        }
    }
    return success;
}

void RC522_PICC_HaltA(void)
{
    uint8_t frame[4] = {RC522_PICC_HLTA, 0x00U, 0x00U, 0x00U};
    if (s_initialized && (RC522_OK == RC522_CalculateCRC(frame, 2U, &frame[2])))
    {
        /* A correctly halted card produces no response; timeout is normal. */
        (void)RC522_Transceive(frame, 4U, 0U, NULL, NULL, NULL);
    }
}
