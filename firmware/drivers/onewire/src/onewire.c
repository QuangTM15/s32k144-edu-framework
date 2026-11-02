/**
 * @file onewire.c
 * @brief Standard-speed GPIO bit-banged 1-Wire master on S32K144.
 *
 * LPIT0 CH2: dedicated free-running 40 MHz down-counter (25 ns/tick).
 * LPIT0 CH0/CH1 and all existing time/IRQ drivers are left untouched.
 * Timing assumes the existing RUN clock setup (LPIT PCS=6 => 40 MHz).
 * Call only in thread/main context; external pull-up required.
 */
#include "onewire.h"

#define OW_LPIT_CHANNEL (2U)
#define OW_TICKS_PER_US (40UL)
#define OW_LPIT_RELOAD (0xFFFFFFFFUL)
#define OW_LPIT_PCS (6UL)
#define OW_RESET_LOW_US (480UL)
#define OW_PRESENCE_SAMPLE_US (70UL)
#define OW_RESET_RECOVERY_US (410UL)
#define OW_SLOT_US (70UL)
#define OW_WRITE1_LOW_US (6UL)
#define OW_WRITE0_LOW_US (60UL)
#define OW_READ_LOW_US (3UL)
#define OW_READ_SAMPLE_US (14UL)
#define OW_BUS_RISE_US (5UL)

/* PRIMASK restoration preserves the caller's original interrupt state. */
static uint32_t OW_EnterCritical(void)
{
    uint32_t mask = 0UL;
    __asm volatile("MRS %0, PRIMASK" : "=r"(mask) : : "memory");
    __asm volatile("cpsid i" : : : "memory");
    return mask;
}

static void OW_ExitCritical(uint32_t mask)
{
    __asm volatile("MSR PRIMASK, %0" : : "r"(mask) : "memory");
}

static uint32_t OW_Ticks(void)
{
    return IP_LPIT0->TMR[OW_LPIT_CHANNEL].CVAL;
}

/* Elapsed ticks from a down-counter, modulo 2^32. */
static void OW_WaitFrom(uint32_t start, uint32_t us)
{
    const uint32_t ticks = us * OW_TICKS_PER_US;
    while ((uint32_t)(start - OW_Ticks()) < ticks)
    {
        /* Requires a running CH2. Do not reconfigure LPIT during a transaction. */
    }
}

static void OW_WaitUs(uint32_t us)
{
    const uint32_t start = OW_Ticks();
    OW_WaitFrom(start, us);
}

static void OW_DriveLow(const OneWire_t *bus)
{
    const uint32_t mask = 1UL << bus->io.pin;
    bus->io.gpio->PCOR = mask;
    bus->io.gpio->PDDR |= mask;
}

static void OW_Release(const OneWire_t *bus)
{
    bus->io.gpio->PDDR &= ~(1UL << bus->io.pin);
}

static uint8_t OW_Sample(const OneWire_t *bus)
{
    return ((bus->io.gpio->PDIR & (1UL << bus->io.pin)) != 0UL) ? 1U : 0U;
}

static bool OW_ValidPin(const OneWire_Pin_t *pin)
{
    bool valid = false;
    if ((pin != (const OneWire_Pin_t *)0) && (pin->pin < 32U))
    {
        valid = (((pin->port == IP_PORTA) && (pin->gpio == IP_PTA)) ||
                 ((pin->port == IP_PORTB) && (pin->gpio == IP_PTB)) ||
                 ((pin->port == IP_PORTC) && (pin->gpio == IP_PTC)) ||
                 ((pin->port == IP_PORTD) && (pin->gpio == IP_PTD)) ||
                 ((pin->port == IP_PORTE) && (pin->gpio == IP_PTE)));
    }
    return valid;
}

static void OW_EnablePortClock(PORT_Type *port)
{
    uint32_t index = PCC_PORTA_INDEX;
    if (port == IP_PORTB)
    {
        index = PCC_PORTB_INDEX;
    }
    else if (port == IP_PORTC)
    {
        index = PCC_PORTC_INDEX;
    }
    else if (port == IP_PORTD)
    {
        index = PCC_PORTD_INDEX;
    }
    else if (port == IP_PORTE)
    {
        index = PCC_PORTE_INDEX;
    }
    else
    { /* PORTA */
    }
    IP_PCC->PCCn[index] |= PCC_PCCn_CGC_MASK;
}

static bool OW_ModuleReady(void)
{
    bool ready = false;
    const uint32_t pcc = IP_PCC->PCCn[PCC_LPIT_INDEX];
    if (((pcc & PCC_PCCn_CGC_MASK) != 0UL) &&
        (((pcc & PCC_PCCn_PCS_MASK) >> PCC_PCCn_PCS_SHIFT) == OW_LPIT_PCS) &&
        ((IP_LPIT0->MCR & LPIT_MCR_M_CEN_MASK) != 0UL))
    {
        ready = true;
    }
    return ready;
}

bool OneWire_IsTimingReady(void)
{
    bool ready = false;
    if (OW_ModuleReady())
    {
        ready = ((IP_LPIT0->TMR[OW_LPIT_CHANNEL].TCTRL == LPIT_TMR_TCTRL_T_EN_MASK) &&
                 (IP_LPIT0->TMR[OW_LPIT_CHANNEL].TVAL == OW_LPIT_RELOAD) &&
                 ((IP_LPIT0->MIER & (1UL << OW_LPIT_CHANNEL)) == 0UL));
    }
    return ready;
}

/* Finite CPU-side check; prevents starting waits on a nonmoving counter.
 * Does NOT provide a runtime timeout if the timer later stops mid-slot. */
static bool OW_CounterMoves(void)
{
    bool moving = false;
    uint32_t i = 0UL;
    const uint32_t before = OW_Ticks();
    for (i = 0UL; (i < 4096UL) && (!moving); i++)
    {
        if (OW_Ticks() != before)
        {
            moving = true;
        }
    }
    return moving;
}

OneWire_Status_t OneWire_Init(OneWire_t *bus, const OneWire_Pin_t *pin)
{
    OneWire_Status_t status = ONEWIRE_INVALID_ARGUMENT;
    if ((bus != (OneWire_t *)0) && OW_ValidPin(pin))
    {
        if (!OW_ModuleReady())
        {
            status = ONEWIRE_TIMER_ERROR;
        }
        else
        {
            /* Never overwrite a running CH2 with an unknown configuration. */
            if ((IP_LPIT0->TMR[OW_LPIT_CHANNEL].TCTRL & LPIT_TMR_TCTRL_T_EN_MASK) == 0UL)
            {
                IP_LPIT0->TMR[OW_LPIT_CHANNEL].TCTRL = 0UL;
                IP_LPIT0->TMR[OW_LPIT_CHANNEL].TVAL = OW_LPIT_RELOAD;
                IP_LPIT0->MIER &= ~(1UL << OW_LPIT_CHANNEL);
                IP_LPIT0->MSR = (1UL << OW_LPIT_CHANNEL);
                IP_LPIT0->TMR[OW_LPIT_CHANNEL].TCTRL = LPIT_TMR_TCTRL_T_EN_MASK;
            }
            if ((!OneWire_IsTimingReady()) || (!OW_CounterMoves()))
            {
                status = ONEWIRE_TIMER_ERROR;
            }
            else
            {
                OW_EnablePortClock(pin->port);
                bus->initialized = 0U;
                bus->io = *pin;
                OW_Release(bus);
                bus->io.port->PCR[bus->io.pin] =
                    (bus->io.port->PCR[bus->io.pin] &
                     ~(PORT_PCR_MUX_MASK | PORT_PCR_PE_MASK | PORT_PCR_PFE_MASK)) |
                    PORT_PCR_MUX(1U);
                bus->io.gpio->PCOR = (1UL << bus->io.pin);
                OW_WaitUs(OW_BUS_RISE_US);
                if (OW_Sample(bus) != 0U)
                {
                    bus->initialized = 1U;
                    status = ONEWIRE_OK;
                }
                else
                {
                    status = ONEWIRE_BUS_STUCK_LOW;
                }
            }
        }
    }
    return status;
}

OneWire_Status_t OneWire_Deinit(OneWire_t *bus)
{
    OneWire_Status_t status = ONEWIRE_INVALID_ARGUMENT;
    if (bus != (OneWire_t *)0)
    {
        if (bus->initialized != 0U)
        {
            OW_Release(bus);
        }
        bus->initialized = 0U;
        status = ONEWIRE_OK;
    }
    return status;
}

static OneWire_Status_t OW_Check(const OneWire_t *bus)
{
    OneWire_Status_t status = ONEWIRE_INVALID_ARGUMENT;
    if (bus != (const OneWire_t *)0)
    {
        status = (bus->initialized == 0U) ? ONEWIRE_NOT_INITIALIZED : (OneWire_IsTimingReady() ? ONEWIRE_OK : ONEWIRE_TIMER_ERROR);
    }
    return status;
}

OneWire_Status_t OneWire_Reset(OneWire_t *bus)
{
    OneWire_Status_t status = OW_Check(bus);
    uint32_t irq = 0UL;
    uint32_t start = 0UL;
    uint8_t presence = 1U;
    if (status == ONEWIRE_OK)
    {
        OW_Release(bus);
        OW_WaitUs(OW_BUS_RISE_US);
        if (OW_Sample(bus) == 0U)
        {
            status = ONEWIRE_BUS_STUCK_LOW;
        }
        else
        {
            /* Reset LOW may be extended by an ISR; release/sample must be protected. */
            OW_DriveLow(bus);
            OW_WaitUs(OW_RESET_LOW_US);
            irq = OW_EnterCritical();
            OW_Release(bus);
            start = OW_Ticks();
            OW_WaitFrom(start, OW_PRESENCE_SAMPLE_US);
            presence = OW_Sample(bus);
            OW_ExitCritical(irq);
            OW_WaitFrom(start, OW_RESET_RECOVERY_US);
            if (OW_Sample(bus) == 0U)
            {
                status = ONEWIRE_BUS_STUCK_LOW;
            }
            else
            {
                status = (presence == 0U) ? ONEWIRE_OK : ONEWIRE_NO_DEVICE;
            }
        }
    }
    return status;
}

OneWire_Status_t OneWire_WriteBit(OneWire_t *bus, uint8_t bit)
{
    OneWire_Status_t status = OW_Check(bus);
    uint32_t irq = 0UL;
    uint32_t start = 0UL;
    if (status == ONEWIRE_OK)
    {
        irq = OW_EnterCritical();
        OW_DriveLow(bus);
        start = OW_Ticks();
        OW_WaitFrom(start, (bit != 0U) ? OW_WRITE1_LOW_US : OW_WRITE0_LOW_US);
        OW_Release(bus);
        OW_ExitCritical(irq);
        OW_WaitFrom(start, OW_SLOT_US);
    }
    return status;
}

OneWire_Status_t OneWire_ReadBit(OneWire_t *bus, uint8_t *bit)
{
    OneWire_Status_t status = OW_Check(bus);
    uint32_t irq = 0UL;
    uint32_t start = 0UL;
    uint8_t sampled = 0U;
    if ((status == ONEWIRE_OK) && (bit == (uint8_t *)0))
    {
        status = ONEWIRE_INVALID_ARGUMENT;
    }
    if (status == ONEWIRE_OK)
    {
        irq = OW_EnterCritical();
        OW_DriveLow(bus);
        start = OW_Ticks();
        OW_WaitFrom(start, OW_READ_LOW_US);
        OW_Release(bus);
        OW_WaitFrom(start, OW_READ_SAMPLE_US);
        sampled = OW_Sample(bus);
        OW_ExitCritical(irq);
        OW_WaitFrom(start, OW_SLOT_US);
        *bit = sampled;
    }
    return status;
}

OneWire_Status_t OneWire_WriteByte(OneWire_t *bus, uint8_t value)
{
    OneWire_Status_t status = OW_Check(bus);
    uint8_t i = 0U;
    for (i = 0U; (i < 8U) && (status == ONEWIRE_OK); i++)
    {
        status = OneWire_WriteBit(bus, (uint8_t)(value & 1U));
        value >>= 1U;
    }
    return status;
}

OneWire_Status_t OneWire_ReadByte(OneWire_t *bus, uint8_t *value)
{
    OneWire_Status_t status = OW_Check(bus);
    uint8_t i = 0U;
    uint8_t bit = 0U;
    uint8_t result = 0U;
    if ((status == ONEWIRE_OK) && (value == (uint8_t *)0))
    {
        status = ONEWIRE_INVALID_ARGUMENT;
    }
    for (i = 0U; (i < 8U) && (status == ONEWIRE_OK); i++)
    {
        status = OneWire_ReadBit(bus, &bit);
        if (status == ONEWIRE_OK)
        {
            result |= (uint8_t)(bit << i);
        }
    }
    if (status == ONEWIRE_OK)
    {
        *value = result;
    }
    return status;
}

OneWire_Status_t OneWire_Write(OneWire_t *bus, const uint8_t *data, uint32_t count)
{
    OneWire_Status_t status = OW_Check(bus);
    uint32_t i = 0UL;
    if ((count != 0UL) && (data == (const uint8_t *)0))
    {
        status = ONEWIRE_INVALID_ARGUMENT;
    }
    for (i = 0UL; (i < count) && (status == ONEWIRE_OK); i++)
    {
        status = OneWire_WriteByte(bus, data[i]);
    }
    return status;
}

OneWire_Status_t OneWire_Read(OneWire_t *bus, uint8_t *data, uint32_t count)
{
    OneWire_Status_t status = OW_Check(bus);
    uint32_t i = 0UL;
    if ((count != 0UL) && (data == (uint8_t *)0))
    {
        status = ONEWIRE_INVALID_ARGUMENT;
    }
    for (i = 0UL; (i < count) && (status == ONEWIRE_OK); i++)
    {
        status = OneWire_ReadByte(bus, &data[i]);
    }
    return status;
}

uint8_t OneWire_CRC8(const uint8_t *data, uint32_t count)
{
    uint8_t crc = 0U;
    uint8_t bit = 0U;
    uint32_t i = 0UL;
    if ((data != (const uint8_t *)0) || (count == 0UL))
    {
        for (i = 0UL; i < count; i++)
        {
            uint8_t value = data[i];
            for (bit = 0U; bit < 8U; bit++)
            {
                const uint8_t mix = (uint8_t)((crc ^ value) & 1U);
                crc >>= 1U;
                if (mix != 0U)
                {
                    crc ^= 0x8CU;
                }
                value >>= 1U;
            }
        }
    }
    return crc;
}
