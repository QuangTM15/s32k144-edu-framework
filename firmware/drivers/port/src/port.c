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
/* Private Macros                                                            */
/* ========================================================================= */

/**
 * @brief Create a bit mask for one PORT pin.
 */
#define PORT_PIN_MASK(u8Pin)    (1UL << (u8Pin))

/* ========================================================================= */
/* Public Functions                                                          */
/* ========================================================================= */

void PORT_EnableClock(port_name_t u8PortName)
{
    switch (u8PortName)
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
            /* Invalid PORT name. */
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
        pBase->PCR[u8Pin] &= ~PORT_PCR_MUX_MASK;
        pBase->PCR[u8Pin] |= PORT_PCR_MUX((uint32_t)u8Mux);
    }
    else
    {
        /* Invalid PORT base pointer. */
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
                pBase->PCR[u8Pin] &= ~PORT_PCR_PE_MASK;
                break;

            case PORT_PULL_DOWN:
                pBase->PCR[u8Pin] |= PORT_PCR_PE_MASK;
                pBase->PCR[u8Pin] &= ~PORT_PCR_PS_MASK;
                break;

            case PORT_PULL_UP:
                pBase->PCR[u8Pin] |= PORT_PCR_PE_MASK;
                pBase->PCR[u8Pin] |= PORT_PCR_PS_MASK;
                break;

            default:
                /* Invalid pull configuration. */
                break;
        }
    }
    else
    {
        /* Invalid PORT base pointer. */
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
            pBase->PCR[u8Pin] |= PORT_PCR_PFE_MASK;
        }
        else
        {
            pBase->PCR[u8Pin] &= ~PORT_PCR_PFE_MASK;
        }
    }
    else
    {
        /* Invalid PORT base pointer. */
    }

    return;
}

void PORT_SetPinInterruptConfig(PORT_Type *pBase,
                                uint8_t u8Pin,
                                port_interrupt_config_t u8Config)
{
    if (NULL != pBase)
    {
        /*
         * PCR[IRQC] controls interrupt/DMA generation for the selected pin.
         * Preserve all unrelated PCR fields.
         */
        pBase->PCR[u8Pin] &= ~PORT_PCR_IRQC_MASK;
        pBase->PCR[u8Pin] |= PORT_PCR_IRQC((uint32_t)u8Config);
    }
    else
    {
        /* Invalid PORT base pointer. */
    }

    return;
}

uint32_t PORT_GetInterruptFlags(PORT_Type *pBase)
{
    uint32_t u32Flags = 0U;

    if (NULL != pBase)
    {
        /*
         * ISFR contains one interrupt status flag for each PORT pin.
         */
        u32Flags = pBase->ISFR;
    }
    else
    {
        u32Flags = 0U;
    }

    return u32Flags;
}

uint8_t PORT_GetPinInterruptFlag(PORT_Type *pBase,
                                 uint8_t u8Pin)
{
    uint8_t u8Flag = 0U;

    if (NULL != pBase)
    {
        if (0U != (pBase->ISFR & PORT_PIN_MASK(u8Pin)))
        {
            u8Flag = 1U;
        }
        else
        {
            u8Flag = 0U;
        }
    }
    else
    {
        u8Flag = 0U;
    }

    return u8Flag;
}

void PORT_ClearPinInterruptFlag(PORT_Type *pBase,
                                uint8_t u8Pin)
{
    if (NULL != pBase)
    {
        /*
         * ISFR is write-one-to-clear.
         */
        pBase->ISFR = PORT_PIN_MASK(u8Pin);
    }
    else
    {
        /* Invalid PORT base pointer. */
    }

    return;
}

void PORT_ClearInterruptFlags(PORT_Type *pBase,
                              uint32_t u32Mask)
{
    if (NULL != pBase)
    {
        /*
         * ISFR is write-one-to-clear.
         *
         * Write the captured pending mask directly so that only the
         * corresponding interrupt flags are cleared.
         */
        pBase->ISFR = u32Mask;
    }
    else
    {
        /* Invalid PORT base pointer. */
    }

    return;
}
