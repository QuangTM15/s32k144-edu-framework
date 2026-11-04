/**
 * @file onewire.h
 * @brief Standard-speed 1-Wire master for S32K144 GPIO + LPIT0 CH2.
 *
 * @note Physical pin descriptors are used (not Arduino logical pins).
 * @note Requires an external pull-up to MCU I/O voltage (typically 4.7 kohm).
 * @note Supports multidrop electrical buses; ROM selection belongs to device layer.
 * @note No parasite-power strong pull-up, overdrive or ISR/concurrent use.
 * @note Initialize after setup()/Time_Init(); do not reinitialize LPIT while active.
 */
#ifndef ONEWIRE_H
#define ONEWIRE_H

#include <stdint.h>
#include <stdbool.h>
#include "S32K144.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        ONEWIRE_OK = 0,
        ONEWIRE_INVALID_ARGUMENT,
        ONEWIRE_NOT_INITIALIZED,
        ONEWIRE_NO_DEVICE,
        ONEWIRE_BUS_STUCK_LOW,
        ONEWIRE_TIMER_ERROR,
        ONEWIRE_BUSY /* Reserved; concurrent access is not supported in V1. */
    } OneWire_Status_t;

    typedef struct
    {
        PORT_Type *port;
        GPIO_Type *gpio;
        uint8_t pin;
    } OneWire_Pin_t;

    typedef struct
    {
        OneWire_Pin_t io;
        uint8_t initialized;
    } OneWire_t;

    /** Initialize GPIO and dedicated LPIT0 CH2 timing; caller owns zeroed context. */
    OneWire_Status_t OneWire_Init(OneWire_t *bus, const OneWire_Pin_t *pin);
    /** Release GPIO and invalidate context; CH2 remains running. */
    OneWire_Status_t OneWire_Deinit(OneWire_t *bus);
    /** Reset bus and detect at least one slave presence pulse. */
    OneWire_Status_t OneWire_Reset(OneWire_t *bus);
    /** Write one bit; nonzero values mean 1. */
    OneWire_Status_t OneWire_WriteBit(OneWire_t *bus, uint8_t bit);
    /** Read one bit into non-NULL pointer. */
    OneWire_Status_t OneWire_ReadBit(OneWire_t *bus, uint8_t *bit);
    /** Write one byte, LSB first. */
    OneWire_Status_t OneWire_WriteByte(OneWire_t *bus, uint8_t value);
    /** Read one byte, LSB first. */
    OneWire_Status_t OneWire_ReadByte(OneWire_t *bus, uint8_t *value);
    /** Write count bytes; NULL allowed when count == 0. */
    OneWire_Status_t OneWire_Write(OneWire_t *bus, const uint8_t *data, uint32_t count);
    /** Read count bytes; NULL allowed when count == 0. */
    OneWire_Status_t OneWire_Read(OneWire_t *bus, uint8_t *data, uint32_t count);
    /** Dallas/Maxim CRC8, polynomial 0x31 (reflected 0x8C), initial 0. */
    uint8_t OneWire_CRC8(const uint8_t *data, uint32_t count);
    /** Check LPIT CH2 configuration; does not prove oscillator frequency. */
    bool OneWire_IsTimingReady(void);

#ifdef __cplusplus
}
#endif
#endif /* ONEWIRE_H */
