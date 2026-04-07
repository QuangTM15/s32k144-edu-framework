#include "port.h"

void PORT_EnableClock(port_name_t portName)
{
    switch (portName)
    {
        case PORT_NAME_A:
            IP_PCC->PCCn[PCC_PORTA_INDEX] |= PCC_PCCn_CGC_MASK;
            break;

        case PORT_NAME_B:
            IP_PCC->PCCn[PCC_PORTB_INDEX] |= PCC_PCCn_CGC_MASK;
            break;

        case PORT_NAME_C:
            IP_PCC->PCCn[PCC_PORTC_INDEX] |= PCC_PCCn_CGC_MASK;
            break;

        case PORT_NAME_D:
            IP_PCC->PCCn[PCC_PORTD_INDEX] |= PCC_PCCn_CGC_MASK;
            break;

        case PORT_NAME_E:
            IP_PCC->PCCn[PCC_PORTE_INDEX] |= PCC_PCCn_CGC_MASK;
            break;

        default:
            /* Do nothing */
            break;
    }
}

void PORT_SetPinMux(PORT_Type *base, uint8_t pin, port_mux_t mux)
{
    base->PCR[pin] &= ~PORT_PCR_MUX_MASK;
    base->PCR[pin] |= PORT_PCR_MUX((uint32_t)mux);
}

void PORT_SetPinPull(PORT_Type *base, uint8_t pin, port_pull_t pull)
{
    switch (pull)
    {
        case PORT_PULL_DISABLED:
            base->PCR[pin] &= ~PORT_PCR_PE_MASK;
            break;

        case PORT_PULL_DOWN:
            base->PCR[pin] |= PORT_PCR_PE_MASK;
            base->PCR[pin] &= ~PORT_PCR_PS_MASK;
            break;

        case PORT_PULL_UP:
            base->PCR[pin] |= PORT_PCR_PE_MASK;
            base->PCR[pin] |= PORT_PCR_PS_MASK;
            break;

        default:
            /* Do nothing */
            break;
    }
}

void PORT_SetPassiveFilter(PORT_Type *base, uint8_t pin, bool enable)
{
    if (enable == true)
    {
        base->PCR[pin] |= PORT_PCR_PFE_MASK;
    }
    else
    {
        base->PCR[pin] &= ~PORT_PCR_PFE_MASK;
    }
}
