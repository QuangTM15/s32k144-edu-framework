/**
 * @file port.c
 * @brief PORT driver implementation for NXP S32K144.
 *
 * This file implements PORT configuration services by accessing the S32K144
 * PORT and PCC register blocks.
 */

#include "port.h"

#include <stddef.h>

/* ========================================================================= */
/* Public Functions                                                           */
/* ========================================================================= */

void PORT_EnableClock(port_name_t u8PortName)
{
    switch (u8PortName)
    {
        case PORT_NAME_A:
            /* Enable clock gate for PORTA through PCC. */
            IP_PCC->PCCn[PCC_PORTA_INDEX] |= PCC_PCCn_CGC_MASK;
            break;

        case PORT_NAME_B:
            /* Enable clock gate for PORTB through PCC. */
            IP_PCC->PCCn[PCC_PORTB_INDEX] |= PCC_PCCn_CGC_MASK;
            break;

        case PORT_NAME_C:
            /* Enable clock gate for PORTC through PCC. */
            IP_PCC->PCCn[PCC_PORTC_INDEX] |= PCC_PCCn_CGC_MASK;
            break;

        case PORT_NAME_D:
            /* Enable clock gate for PORTD through PCC. */
            IP_PCC->PCCn[PCC_PORTD_INDEX] |= PCC_PCCn_CGC_MASK;
            break;

        case PORT_NAME_E:
            /* Enable clock gate for PORTE through PCC. */
            IP_PCC->PCCn[PCC_PORTE_INDEX] |= PCC_PCCn_CGC_MASK;
            break;

        default:
            /* Invalid PORT name. Keep previous behavior: do nothing. */
            break;
    }

    return;
}

void PORT_SetPinMux(PORT_Type *pBase,
                    uint8_t u8Pin,
                    port_mux_t u8Mux)
{
    if (NULL != pBase)
    {
        /*
         * PCR[MUX] selects the pin function.
         * Clear the current MUX field before programming the new function.
         */
        pBase->PCR[u8Pin] &= ~PORT_PCR_MUX_MASK;
        pBase->PCR[u8Pin] |= PORT_PCR_MUX((uint32_t)u8Mux);
    }
    else
    {
        /* Invalid PORT base pointer. Keep previous behavior: do nothing. */
    }

    return;
}

void PORT_SetPinPull(PORT_Type *pBase,
                     uint8_t u8Pin,
                     port_pull_t u8Pull)
{
    if (NULL != pBase)
    {
        switch (u8Pull)
        {
            case PORT_PULL_DISABLED:
                /*
                 * PE = 0 disables the internal pull resistor.
                 * PS value is ignored while PE is disabled.
                 */
                pBase->PCR[u8Pin] &= ~PORT_PCR_PE_MASK;
                break;

            case PORT_PULL_DOWN:
                /*
                 * PE = 1 enables internal pull resistor.
                 * PS = 0 selects pull-down.
                 */
                pBase->PCR[u8Pin] |= PORT_PCR_PE_MASK;
                pBase->PCR[u8Pin] &= ~PORT_PCR_PS_MASK;
                break;

            case PORT_PULL_UP:
                /*
                 * PE = 1 enables internal pull resistor.
                 * PS = 1 selects pull-up.
                 */
                pBase->PCR[u8Pin] |= PORT_PCR_PE_MASK;
                pBase->PCR[u8Pin] |= PORT_PCR_PS_MASK;
                break;

            default:
                /* Invalid pull configuration. Keep previous behavior: do nothing. */
                break;
        }
    }
    else
    {
        /* Invalid PORT base pointer. Keep previous behavior: do nothing. */
    }

    return;
}

void PORT_SetPassiveFilter(PORT_Type *pBase,
                           uint8_t u8Pin,
                           bool bEnable)
{
    if (NULL != pBase)
    {
        if (true == bEnable)
        {
            /* PFE = 1 enables passive input filter on the selected pin. */
            pBase->PCR[u8Pin] |= PORT_PCR_PFE_MASK;
        }
        else
        {
            /* PFE = 0 disables passive input filter on the selected pin. */
            pBase->PCR[u8Pin] &= ~PORT_PCR_PFE_MASK;
        }
    }
    else
    {
        /* Invalid PORT base pointer. Keep previous behavior: do nothing. */
    }

    return;
}
