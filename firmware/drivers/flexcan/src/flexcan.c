/**
 * @file flexcan.c
 * @brief FlexCAN driver implementation for NXP S32K144.
 *
 * @details
 * This implementation is intentionally built in verified blocks. The current
 * block implements module infrastructure, pin/clock setup, Disable/Freeze
 * transitions, Classical CAN nominal bit timing, operating mode control, and
 * basic diagnostic access. Message Buffer transfer, FIFO, interrupt, CAN FD,
 * and Pretended Networking services are implemented in subsequent blocks.
 */

#include "flexcan.h"
#include "port.h"

#include <stddef.h>

/* ========================================================================= */
/* Private Constants                                                         */
/* ========================================================================= */

#define FLEXCAN_DRIVER_INSTANCE_COUNT (3U)
#define FLEXCAN_INSTANCE_INVALID (0xFFU)

#define FLEXCAN_CAN0_RX_PIN (4U)
#define FLEXCAN_CAN0_TX_PIN (5U)
#define FLEXCAN_CAN0_PIN_MUX (PORT_MUX_ALT5)

#define FLEXCAN_TIMING_MIN_TQ (8U)
#define FLEXCAN_TIMING_MAX_TQ (25U)
#define FLEXCAN_TIMING_TARGET_SAMPLE_POINT (75U)
#define FLEXCAN_TIMING_MIN_PROP_SEG (1U)
#define FLEXCAN_TIMING_MAX_PROP_SEG (8U)
#define FLEXCAN_TIMING_MIN_PHASE_SEG1 (1U)
#define FLEXCAN_TIMING_MAX_PHASE_SEG1 (8U)
#define FLEXCAN_TIMING_MIN_PHASE_SEG2 (2U)
#define FLEXCAN_TIMING_MAX_PHASE_SEG2 (8U)
#define FLEXCAN_TIMING_MIN_RJW (1U)
#define FLEXCAN_TIMING_MAX_RJW (4U)
#define FLEXCAN_TIMING_MIN_PRESCALER (1U)
#define FLEXCAN_TIMING_MAX_PRESCALER (256U)

#define FLEXCAN_FLTCONF_BUS_OFF_VALUE (2UL << FLEXCAN_ESR1_FLTCONF_SHIFT)

/* ========================================================================= */
/* Private Types                                                             */
/* ========================================================================= */

typedef struct
{
    bool bInitialized;
    bool bEnableSelfReception;
    FLEXCAN_Mode_t u8Mode;
} FLEXCAN_DriverState_t;

/* ========================================================================= */
/* Private Data                                                              */
/* ========================================================================= */

static FLEXCAN_DriverState_t s_axFlexcanState[FLEXCAN_DRIVER_INSTANCE_COUNT] = {0};

/* ========================================================================= */
/* Private Functions                                                         */
/* ========================================================================= */

static uint8_t FLEXCAN_GetInstanceIndex(FLEXCAN_Type *pBase)
{
    uint8_t u8Index = FLEXCAN_INSTANCE_INVALID;

    if (IP_FLEXCAN0 == pBase)
    {
        u8Index = 0U;
    }
    else if (IP_FLEXCAN1 == pBase)
    {
        u8Index = 1U;
    }
    else if (IP_FLEXCAN2 == pBase)
    {
        u8Index = 2U;
    }
    else
    {
        /* Unsupported FlexCAN base address. */
    }

    return u8Index;
}

static bool FLEXCAN_IsSupportedBoardInstance(FLEXCAN_Type *pBase)
{
    bool bSupported = false;

    if (IP_FLEXCAN0 == pBase)
    {
        bSupported = true;
    }

    return bSupported;
}

static FLEXCAN_Status_t FLEXCAN_EnableModuleClock(FLEXCAN_Type *pBase)
{
    FLEXCAN_Status_t status = FLEXCAN_STATUS_INVALID_ARGUMENT;

    if (IP_FLEXCAN0 == pBase)
    {
        IP_PCC->PCCn[PCC_FlexCAN0_INDEX] |= PCC_PCCn_CGC_MASK;
        status = FLEXCAN_STATUS_OK;
    }

    return status;
}

static FLEXCAN_Status_t FLEXCAN_DisableModuleClock(FLEXCAN_Type *pBase)
{
    FLEXCAN_Status_t status = FLEXCAN_STATUS_INVALID_ARGUMENT;

    if (IP_FLEXCAN0 == pBase)
    {
        IP_PCC->PCCn[PCC_FlexCAN0_INDEX] &= ~PCC_PCCn_CGC_MASK;
        status = FLEXCAN_STATUS_OK;
    }

    return status;
}

static void FLEXCAN_ConfigPins(FLEXCAN_Type *pBase)
{
    if (IP_FLEXCAN0 == pBase)
    {
        PORT_EnableClock(PORT_NAME_E);
        PORT_SetPinMux(IP_PORTE, FLEXCAN_CAN0_RX_PIN, FLEXCAN_CAN0_PIN_MUX);
        PORT_SetPinMux(IP_PORTE, FLEXCAN_CAN0_TX_PIN, FLEXCAN_CAN0_PIN_MUX);
    }
    else
    {
        /* Current MaaZEDU board mapping exposes FlexCAN0 only. */
    }

    return;
}

static bool FLEXCAN_IsValidMode(FLEXCAN_Mode_t u8Mode)
{
    bool bValid = false;

    if ((FLEXCAN_MODE_NORMAL == u8Mode) ||
        (FLEXCAN_MODE_LOOPBACK == u8Mode) ||
        (FLEXCAN_MODE_LISTEN_ONLY == u8Mode))
    {
        bValid = true;
    }

    return bValid;
}

static bool FLEXCAN_IsValidClockSource(FLEXCAN_ClockSource_t u8ClockSource)
{
    bool bValid = false;

    if ((FLEXCAN_CLOCK_OSCILLATOR == u8ClockSource) ||
        (FLEXCAN_CLOCK_PERIPHERAL == u8ClockSource))
    {
        bValid = true;
    }

    return bValid;
}

static bool FLEXCAN_IsValidTxPriorityMode(FLEXCAN_TxPriorityMode_t u8Mode)
{
    bool bValid = false;

    if ((FLEXCAN_TX_PRIORITY_ID == u8Mode) ||
        (FLEXCAN_TX_PRIORITY_LOCAL == u8Mode) ||
        (FLEXCAN_TX_PRIORITY_LOWEST_MB == u8Mode))
    {
        bValid = true;
    }

    return bValid;
}

static bool FLEXCAN_IsValidNominalTiming(const FLEXCAN_BitTiming_t *pTiming)
{
    bool bValid = false;
    uint16_t u16TotalTq = 0U;

    if (NULL != pTiming)
    {
        u16TotalTq = 1U +
                     (uint16_t)pTiming->u8PropSeg +
                     (uint16_t)pTiming->u8PhaseSeg1 +
                     (uint16_t)pTiming->u8PhaseSeg2;

        if ((FLEXCAN_TIMING_MIN_PRESCALER <= pTiming->u16Prescaler) &&
            (FLEXCAN_TIMING_MAX_PRESCALER >= pTiming->u16Prescaler) &&
            (FLEXCAN_TIMING_MIN_PROP_SEG <= pTiming->u8PropSeg) &&
            (FLEXCAN_TIMING_MAX_PROP_SEG >= pTiming->u8PropSeg) &&
            (FLEXCAN_TIMING_MIN_PHASE_SEG1 <= pTiming->u8PhaseSeg1) &&
            (FLEXCAN_TIMING_MAX_PHASE_SEG1 >= pTiming->u8PhaseSeg1) &&
            (FLEXCAN_TIMING_MIN_PHASE_SEG2 <= pTiming->u8PhaseSeg2) &&
            (FLEXCAN_TIMING_MAX_PHASE_SEG2 >= pTiming->u8PhaseSeg2) &&
            (FLEXCAN_TIMING_MIN_RJW <= pTiming->u8Rjw) &&
            (FLEXCAN_TIMING_MAX_RJW >= pTiming->u8Rjw) &&
            (pTiming->u8Rjw <= pTiming->u8PhaseSeg2) &&
            (FLEXCAN_TIMING_MIN_TQ <= u16TotalTq) &&
            (FLEXCAN_TIMING_MAX_TQ >= u16TotalTq))
        {
            bValid = true;
        }
    }

    return bValid;
}

static FLEXCAN_Status_t FLEXCAN_EnableModule(FLEXCAN_Type *pBase,
                                             uint32_t u32Timeout)
{
    FLEXCAN_Status_t xStatus = FLEXCAN_STATUS_OK;

    pBase->MCR &= ~FLEXCAN_MCR_MDIS_MASK;

    while ((0U != (pBase->MCR & FLEXCAN_MCR_LPMACK_MASK)) &&
           (0U < u32Timeout))
    {
        u32Timeout--;
    }

    if (0U != (pBase->MCR & FLEXCAN_MCR_LPMACK_MASK))
    {
        xStatus = FLEXCAN_STATUS_TIMEOUT;
    }
    else if (0U == (pBase->MCR & FLEXCAN_MCR_HALT_MASK))
    {
        while ((0U != (pBase->MCR & FLEXCAN_MCR_NOTRDY_MASK)) &&
               (0U < u32Timeout))
        {
            u32Timeout--;
        }

        if (0U != (pBase->MCR & FLEXCAN_MCR_NOTRDY_MASK))
        {
            xStatus = FLEXCAN_STATUS_TIMEOUT;
        }
    }
    else
    {
        /* Module is enabled and intentionally held in Freeze mode. */
    }

    return xStatus;
}

static FLEXCAN_Status_t FLEXCAN_DisableModule(FLEXCAN_Type *pBase,
                                              uint32_t u32Timeout)
{
    FLEXCAN_Status_t xStatus = FLEXCAN_STATUS_OK;

    pBase->MCR |= FLEXCAN_MCR_MDIS_MASK;

    while ((0U == (pBase->MCR & FLEXCAN_MCR_LPMACK_MASK)) &&
           (0U < u32Timeout))
    {
        u32Timeout--;
    }

    if (0U == (pBase->MCR & FLEXCAN_MCR_LPMACK_MASK))
    {
        xStatus = FLEXCAN_STATUS_TIMEOUT;
    }

    return xStatus;
}

static FLEXCAN_Status_t FLEXCAN_EnterFreezeInternal(FLEXCAN_Type *pBase,
                                                    uint32_t u32Timeout)
{
    FLEXCAN_Status_t xStatus = FLEXCAN_STATUS_OK;

    if (0U != (pBase->MCR & FLEXCAN_MCR_MDIS_MASK))
    {
        xStatus = FLEXCAN_STATUS_ERROR;
    }
    else
    {
        pBase->MCR |= (FLEXCAN_MCR_FRZ_MASK | FLEXCAN_MCR_HALT_MASK);

        while ((0U == (pBase->MCR & FLEXCAN_MCR_FRZACK_MASK)) &&
               (0U < u32Timeout))
        {
            u32Timeout--;
        }

        if (0U == (pBase->MCR & FLEXCAN_MCR_FRZACK_MASK))
        {
            xStatus = FLEXCAN_STATUS_TIMEOUT;
        }
    }

    return xStatus;
}

static FLEXCAN_Status_t FLEXCAN_ExitFreezeInternal(FLEXCAN_Type *pBase,
                                                   uint32_t u32Timeout)
{
    FLEXCAN_Status_t xStatus = FLEXCAN_STATUS_OK;

    pBase->MCR &= ~FLEXCAN_MCR_HALT_MASK;

    while (((0U != (pBase->MCR & FLEXCAN_MCR_FRZACK_MASK)) ||
            (0U != (pBase->MCR & FLEXCAN_MCR_NOTRDY_MASK))) &&
           (0U < u32Timeout))
    {
        u32Timeout--;
    }

    if ((0U != (pBase->MCR & FLEXCAN_MCR_FRZACK_MASK)) ||
        (0U != (pBase->MCR & FLEXCAN_MCR_NOTRDY_MASK)))
    {
        xStatus = FLEXCAN_STATUS_TIMEOUT;
    }

    return xStatus;
}

static FLEXCAN_Status_t FLEXCAN_ApplyNominalTiming(FLEXCAN_Type *pBase,
                                                   const FLEXCAN_BitTiming_t *pTiming)
{
    FLEXCAN_Status_t xStatus = FLEXCAN_STATUS_OK;
    uint32_t u32Ctrl1 = 0U;

    if (false == FLEXCAN_IsValidNominalTiming(pTiming))
    {
        xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        u32Ctrl1 = pBase->CTRL1;

        u32Ctrl1 &= ~(FLEXCAN_CTRL1_PRESDIV_MASK |
                      FLEXCAN_CTRL1_RJW_MASK |
                      FLEXCAN_CTRL1_PSEG1_MASK |
                      FLEXCAN_CTRL1_PSEG2_MASK |
                      FLEXCAN_CTRL1_PROPSEG_MASK |
                      FLEXCAN_CTRL1_SMP_MASK);

        u32Ctrl1 |= FLEXCAN_CTRL1_PRESDIV((uint32_t)pTiming->u16Prescaler - 1U);
        u32Ctrl1 |= FLEXCAN_CTRL1_RJW((uint32_t)pTiming->u8Rjw - 1U);
        u32Ctrl1 |= FLEXCAN_CTRL1_PSEG1((uint32_t)pTiming->u8PhaseSeg1 - 1U);
        u32Ctrl1 |= FLEXCAN_CTRL1_PSEG2((uint32_t)pTiming->u8PhaseSeg2 - 1U);
        u32Ctrl1 |= FLEXCAN_CTRL1_PROPSEG((uint32_t)pTiming->u8PropSeg - 1U);

        if (true == pTiming->bTripleSampling)
        {
            u32Ctrl1 |= FLEXCAN_CTRL1_SMP_MASK;
        }

        pBase->CTRL1 = u32Ctrl1;
    }

    return xStatus;
}

static FLEXCAN_Status_t FLEXCAN_ApplyMode(FLEXCAN_Type *pBase,
                                          FLEXCAN_Mode_t u8Mode,
                                          bool bEnableSelfReception)
{
    FLEXCAN_Status_t xStatus = FLEXCAN_STATUS_OK;

    if (false == FLEXCAN_IsValidMode(u8Mode))
    {
        xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        pBase->CTRL1 &= ~(FLEXCAN_CTRL1_LPB_MASK | FLEXCAN_CTRL1_LOM_MASK);

        if (FLEXCAN_MODE_LOOPBACK == u8Mode)
        {
            pBase->CTRL1 |= FLEXCAN_CTRL1_LPB_MASK;
            pBase->MCR &= ~FLEXCAN_MCR_SRXDIS_MASK;
        }
        else if (FLEXCAN_MODE_LISTEN_ONLY == u8Mode)
        {
            pBase->CTRL1 |= FLEXCAN_CTRL1_LOM_MASK;

            if (true == bEnableSelfReception)
            {
                pBase->MCR &= ~FLEXCAN_MCR_SRXDIS_MASK;
            }
            else
            {
                pBase->MCR |= FLEXCAN_MCR_SRXDIS_MASK;
            }
        }
        else
        {
            if (true == bEnableSelfReception)
            {
                pBase->MCR &= ~FLEXCAN_MCR_SRXDIS_MASK;
            }
            else
            {
                pBase->MCR |= FLEXCAN_MCR_SRXDIS_MASK;
            }
        }
    }

    return xStatus;
}

static void FLEXCAN_ClearMessageBufferMemory(FLEXCAN_Type *pBase)
{
    uint32_t u32Index = 0U;

    for (u32Index = 0U; u32Index < FLEXCAN_RAMn_COUNT; u32Index++)
    {
        pBase->RAMn[u32Index] = 0U;
    }

    for (u32Index = 0U; u32Index < FLEXCAN_RXIMR_COUNT; u32Index++)
    {
        pBase->RXIMR[u32Index] = 0xFFFFFFFFUL;
    }

    pBase->RXMGMASK = 0xFFFFFFFFUL;
    pBase->RX14MASK = 0xFFFFFFFFUL;
    pBase->RX15MASK = 0xFFFFFFFFUL;
    pBase->RXFGMASK = 0xFFFFFFFFUL;

    pBase->IMASK1 = 0U;
    pBase->IFLAG1 = 0xFFFFFFFFUL;

    return;
}

/* ========================================================================= */
/* Configuration and Module Control                                          */
/* ========================================================================= */

void FLEXCAN_GetDefaultConfig(FLEXCAN_Config_t *pConfig)
{
    if (NULL != pConfig)
    {
        pConfig->u8ClockSource = FLEXCAN_CLOCK_OSCILLATOR;
        pConfig->u32ClockFrequencyHz = 8000000UL;
        pConfig->u32NominalBitRate = 500000UL;
        pConfig->u8NominalTimingMode = FLEXCAN_TIMING_AUTOMATIC;

        pConfig->xNominalTiming.u16Prescaler = 1U;
        pConfig->xNominalTiming.u8PropSeg = 7U;
        pConfig->xNominalTiming.u8PhaseSeg1 = 4U;
        pConfig->xNominalTiming.u8PhaseSeg2 = 4U;
        pConfig->xNominalTiming.u8Rjw = 4U;
        pConfig->xNominalTiming.bTripleSampling = true;

        pConfig->u8Mode = FLEXCAN_MODE_NORMAL;
        pConfig->u8TxPriorityMode = FLEXCAN_TX_PRIORITY_ID;
        pConfig->u8MaxMessageBuffer = FLEXCAN_MAX_MB_COUNT - 1U;
        pConfig->bEnableSelfReception = false;
        pConfig->bEnableTxAbort = true;
        pConfig->bEnableIndividualMasking = true;
        pConfig->bEnableAutomaticBusOffRecovery = true;

        pConfig->bEnableFD = false;
        pConfig->u8FdPayloadSize = FLEXCAN_PAYLOAD_8_BYTES;
        pConfig->u32DataBitRate = 2000000UL;
        pConfig->u8DataTimingMode = FLEXCAN_TIMING_AUTOMATIC;

        pConfig->xDataTiming.u16Prescaler = 1U;
        pConfig->xDataTiming.u8PropSeg = 1U;
        pConfig->xDataTiming.u8PhaseSeg1 = 2U;
        pConfig->xDataTiming.u8PhaseSeg2 = 2U;
        pConfig->xDataTiming.u8Rjw = 2U;
        pConfig->xDataTiming.bTripleSampling = false;

        pConfig->bEnableFdBitRateSwitch = false;
        pConfig->bEnableTransceiverDelayCompensation = false;
        pConfig->u8TransceiverDelayOffset = 0U;
    }

    return;
}

FLEXCAN_Status_t FLEXCAN_Init(FLEXCAN_Type *pBase,
                              const FLEXCAN_Config_t *pConfig)
{
    FLEXCAN_Status_t xStatus = FLEXCAN_STATUS_OK;
    FLEXCAN_BitTiming_t xTiming = {0};
    uint8_t u8InstanceIndex = FLEXCAN_INSTANCE_INVALID;
    uint32_t u32Mcr = 0U;
    uint32_t u32Ctrl1 = 0U;

    if ((NULL == pBase) || (NULL == pConfig))
    {
        xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
    }
    else if (false == FLEXCAN_IsSupportedBoardInstance(pBase))
    {
        xStatus = FLEXCAN_STATUS_UNSUPPORTED;
    }
    else if ((false == FLEXCAN_IsValidClockSource(pConfig->u8ClockSource)) ||
             (false == FLEXCAN_IsValidMode(pConfig->u8Mode)) ||
             (false == FLEXCAN_IsValidTxPriorityMode(pConfig->u8TxPriorityMode)) ||
             (0U == pConfig->u32ClockFrequencyHz) ||
             (0U == pConfig->u32NominalBitRate) ||
             (FLEXCAN_MAX_MB_COUNT <= pConfig->u8MaxMessageBuffer))
    {
        xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
    }
    else if (true == pConfig->bEnableFD)
    {
        /* CAN FD is implemented in a later verified block. */
        xStatus = FLEXCAN_STATUS_UNSUPPORTED;
    }
    else
    {
        u8InstanceIndex = FLEXCAN_GetInstanceIndex(pBase);

        FLEXCAN_EnableModuleClock(pBase);
        FLEXCAN_ConfigPins(pBase);

        /* Clock source selection is only writable while FlexCAN is disabled. */
        pBase->MCR |= FLEXCAN_MCR_MDIS_MASK;

        if (FLEXCAN_CLOCK_PERIPHERAL == pConfig->u8ClockSource)
        {
            pBase->CTRL1 |= FLEXCAN_CTRL1_CLKSRC_MASK;
        }
        else
        {
            pBase->CTRL1 &= ~FLEXCAN_CTRL1_CLKSRC_MASK;
        }

        /* Ensure that enabling the module brings it into Freeze mode. */
        pBase->MCR |= (FLEXCAN_MCR_FRZ_MASK | FLEXCAN_MCR_HALT_MASK);

        xStatus = FLEXCAN_EnableModule(pBase, FLEXCAN_DEFAULT_TIMEOUT);

        if (FLEXCAN_STATUS_OK == xStatus)
        {
            xStatus = FLEXCAN_EnterFreezeInternal(pBase, FLEXCAN_DEFAULT_TIMEOUT);
        }

        if (FLEXCAN_STATUS_OK == xStatus)
        {
            u32Mcr = pBase->MCR;
            u32Mcr &= ~(FLEXCAN_MCR_RFEN_MASK |
                        FLEXCAN_MCR_DMA_MASK |
                        FLEXCAN_MCR_PNET_EN_MASK |
                        FLEXCAN_MCR_LPRIOEN_MASK |
                        FLEXCAN_MCR_AEN_MASK |
                        FLEXCAN_MCR_FDEN_MASK |
                        FLEXCAN_MCR_IRMQ_MASK |
                        FLEXCAN_MCR_SRXDIS_MASK |
                        FLEXCAN_MCR_MAXMB_MASK);

            u32Mcr |= FLEXCAN_MCR_FRZ_MASK |
                      FLEXCAN_MCR_HALT_MASK |
                      FLEXCAN_MCR_WRNEN_MASK |
                      FLEXCAN_MCR_MAXMB(pConfig->u8MaxMessageBuffer);

            if (true == pConfig->bEnableIndividualMasking)
            {
                u32Mcr |= FLEXCAN_MCR_IRMQ_MASK;
            }

            if (true == pConfig->bEnableTxAbort)
            {
                u32Mcr |= FLEXCAN_MCR_AEN_MASK;
            }

            if (false == pConfig->bEnableSelfReception)
            {
                u32Mcr |= FLEXCAN_MCR_SRXDIS_MASK;
            }

            if (FLEXCAN_TX_PRIORITY_LOCAL == pConfig->u8TxPriorityMode)
            {
                u32Mcr |= FLEXCAN_MCR_LPRIOEN_MASK;
            }

            pBase->MCR = u32Mcr;

            u32Ctrl1 = pBase->CTRL1;
            u32Ctrl1 &= ~(FLEXCAN_CTRL1_LBUF_MASK |
                          FLEXCAN_CTRL1_BOFFREC_MASK |
                          FLEXCAN_CTRL1_LPB_MASK |
                          FLEXCAN_CTRL1_LOM_MASK |
                          FLEXCAN_CTRL1_ERRMSK_MASK |
                          FLEXCAN_CTRL1_BOFFMSK_MASK |
                          FLEXCAN_CTRL1_TWRNMSK_MASK |
                          FLEXCAN_CTRL1_RWRNMSK_MASK);

            if (FLEXCAN_TX_PRIORITY_LOWEST_MB == pConfig->u8TxPriorityMode)
            {
                u32Ctrl1 |= FLEXCAN_CTRL1_LBUF_MASK;
            }

            if (false == pConfig->bEnableAutomaticBusOffRecovery)
            {
                u32Ctrl1 |= FLEXCAN_CTRL1_BOFFREC_MASK;
            }

            pBase->CTRL1 = u32Ctrl1;

            FLEXCAN_ClearMessageBufferMemory(pBase);

            if (FLEXCAN_TIMING_AUTOMATIC == pConfig->u8NominalTimingMode)
            {
                xStatus = FLEXCAN_CalculateNominalBitTiming(
                    pConfig->u32ClockFrequencyHz,
                    pConfig->u32NominalBitRate,
                    &xTiming);
            }
            else if (FLEXCAN_TIMING_MANUAL == pConfig->u8NominalTimingMode)
            {
                xTiming = pConfig->xNominalTiming;

                if (false == FLEXCAN_IsValidNominalTiming(&xTiming))
                {
                    xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
                }
            }
            else
            {
                xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
            }
        }

        if (FLEXCAN_STATUS_OK == xStatus)
        {
            xStatus = FLEXCAN_ApplyNominalTiming(pBase, &xTiming);
        }

        if (FLEXCAN_STATUS_OK == xStatus)
        {
            xStatus = FLEXCAN_ApplyMode(pBase,
                                        pConfig->u8Mode,
                                        pConfig->bEnableSelfReception);
        }

        if (FLEXCAN_STATUS_OK == xStatus)
        {
            s_axFlexcanState[u8InstanceIndex].bEnableSelfReception =
                pConfig->bEnableSelfReception;
            s_axFlexcanState[u8InstanceIndex].u8Mode = pConfig->u8Mode;

            xStatus = FLEXCAN_ExitFreezeInternal(pBase, FLEXCAN_DEFAULT_TIMEOUT);
        }

        if (FLEXCAN_STATUS_OK == xStatus)
        {
            s_axFlexcanState[u8InstanceIndex].bInitialized = true;
        }
        else
        {
            s_axFlexcanState[u8InstanceIndex].bInitialized = false;
        }
    }

    return xStatus;
}

FLEXCAN_Status_t FLEXCAN_Deinit(FLEXCAN_Type *pBase)
{
    FLEXCAN_Status_t xStatus = FLEXCAN_STATUS_OK;
    uint8_t u8InstanceIndex = FLEXCAN_INSTANCE_INVALID;

    if ((NULL == pBase) || (false == FLEXCAN_IsSupportedBoardInstance(pBase)))
    {
        xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        u8InstanceIndex = FLEXCAN_GetInstanceIndex(pBase);

        if (false == s_axFlexcanState[u8InstanceIndex].bInitialized)
        {
            xStatus = FLEXCAN_STATUS_NOT_INITIALIZED;
        }
        else
        {
            if (0U != (pBase->MCR & FLEXCAN_MCR_MDIS_MASK))
            {
                xStatus = FLEXCAN_EnableModule(pBase, FLEXCAN_DEFAULT_TIMEOUT);
            }

            if (FLEXCAN_STATUS_OK == xStatus)
            {
                xStatus = FLEXCAN_EnterFreezeInternal(pBase, FLEXCAN_DEFAULT_TIMEOUT);
            }

            if (FLEXCAN_STATUS_OK == xStatus)
            {
                pBase->IMASK1 = 0U;
                pBase->IFLAG1 = 0xFFFFFFFFUL;
                xStatus = FLEXCAN_DisableModule(pBase, FLEXCAN_DEFAULT_TIMEOUT);
            }

            if (FLEXCAN_STATUS_OK == xStatus)
            {
                FLEXCAN_DisableModuleClock(pBase);
                s_axFlexcanState[u8InstanceIndex].bInitialized = false;
                s_axFlexcanState[u8InstanceIndex].bEnableSelfReception = false;
                s_axFlexcanState[u8InstanceIndex].u8Mode = FLEXCAN_MODE_NORMAL;
            }
        }
    }

    return xStatus;
}

FLEXCAN_Status_t FLEXCAN_Enable(FLEXCAN_Type *pBase)
{
    FLEXCAN_Status_t xStatus = FLEXCAN_STATUS_OK;
    uint8_t u8InstanceIndex = FLEXCAN_INSTANCE_INVALID;

    if ((NULL == pBase) || (false == FLEXCAN_IsSupportedBoardInstance(pBase)))
    {
        xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        u8InstanceIndex = FLEXCAN_GetInstanceIndex(pBase);

        if (false == s_axFlexcanState[u8InstanceIndex].bInitialized)
        {
            xStatus = FLEXCAN_STATUS_NOT_INITIALIZED;
        }
        else
        {
            xStatus = FLEXCAN_EnableModule(pBase, FLEXCAN_DEFAULT_TIMEOUT);
        }
    }

    return xStatus;
}

FLEXCAN_Status_t FLEXCAN_Disable(FLEXCAN_Type *pBase)
{
    FLEXCAN_Status_t xStatus = FLEXCAN_STATUS_OK;
    uint8_t u8InstanceIndex = FLEXCAN_INSTANCE_INVALID;

    if ((NULL == pBase) || (false == FLEXCAN_IsSupportedBoardInstance(pBase)))
    {
        xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        u8InstanceIndex = FLEXCAN_GetInstanceIndex(pBase);

        if (false == s_axFlexcanState[u8InstanceIndex].bInitialized)
        {
            xStatus = FLEXCAN_STATUS_NOT_INITIALIZED;
        }
        else
        {
            xStatus = FLEXCAN_DisableModule(pBase, FLEXCAN_DEFAULT_TIMEOUT);
        }
    }

    return xStatus;
}

FLEXCAN_Status_t FLEXCAN_EnterFreezeMode(FLEXCAN_Type *pBase,
                                         uint32_t u32Timeout)
{
    FLEXCAN_Status_t xStatus = FLEXCAN_STATUS_OK;
    uint8_t u8InstanceIndex = FLEXCAN_INSTANCE_INVALID;

    if ((NULL == pBase) || (0U == u32Timeout) ||
        (false == FLEXCAN_IsSupportedBoardInstance(pBase)))
    {
        xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        u8InstanceIndex = FLEXCAN_GetInstanceIndex(pBase);

        if (false == s_axFlexcanState[u8InstanceIndex].bInitialized)
        {
            xStatus = FLEXCAN_STATUS_NOT_INITIALIZED;
        }
        else
        {
            xStatus = FLEXCAN_EnterFreezeInternal(pBase, u32Timeout);
        }
    }

    return xStatus;
}

FLEXCAN_Status_t FLEXCAN_ExitFreezeMode(FLEXCAN_Type *pBase,
                                        uint32_t u32Timeout)
{
    FLEXCAN_Status_t xStatus = FLEXCAN_STATUS_OK;
    uint8_t u8InstanceIndex = FLEXCAN_INSTANCE_INVALID;

    if ((NULL == pBase) || (0U == u32Timeout) ||
        (false == FLEXCAN_IsSupportedBoardInstance(pBase)))
    {
        xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        u8InstanceIndex = FLEXCAN_GetInstanceIndex(pBase);

        if (false == s_axFlexcanState[u8InstanceIndex].bInitialized)
        {
            xStatus = FLEXCAN_STATUS_NOT_INITIALIZED;
        }
        else
        {
            xStatus = FLEXCAN_ExitFreezeInternal(pBase, u32Timeout);
        }
    }

    return xStatus;
}

FLEXCAN_Status_t FLEXCAN_SetMode(FLEXCAN_Type *pBase,
                                 FLEXCAN_Mode_t u8Mode)
{
    FLEXCAN_Status_t xStatus = FLEXCAN_STATUS_OK;
    uint8_t u8InstanceIndex = FLEXCAN_INSTANCE_INVALID;

    if ((NULL == pBase) || (false == FLEXCAN_IsSupportedBoardInstance(pBase)) ||
        (false == FLEXCAN_IsValidMode(u8Mode)))
    {
        xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        u8InstanceIndex = FLEXCAN_GetInstanceIndex(pBase);

        if (false == s_axFlexcanState[u8InstanceIndex].bInitialized)
        {
            xStatus = FLEXCAN_STATUS_NOT_INITIALIZED;
        }
        else
        {
            xStatus = FLEXCAN_EnterFreezeInternal(pBase, FLEXCAN_DEFAULT_TIMEOUT);

            if (FLEXCAN_STATUS_OK == xStatus)
            {
                xStatus = FLEXCAN_ApplyMode(
                    pBase,
                    u8Mode,
                    s_axFlexcanState[u8InstanceIndex].bEnableSelfReception);
            }

            if (FLEXCAN_STATUS_OK == xStatus)
            {
                xStatus = FLEXCAN_ExitFreezeInternal(pBase, FLEXCAN_DEFAULT_TIMEOUT);
            }

            if (FLEXCAN_STATUS_OK == xStatus)
            {
                s_axFlexcanState[u8InstanceIndex].u8Mode = u8Mode;
            }
        }
    }

    return xStatus;
}

/* ========================================================================= */
/* Bit Timing                                                                */
/* ========================================================================= */

FLEXCAN_Status_t FLEXCAN_CalculateNominalBitTiming(uint32_t u32ClockFrequencyHz,
                                                   uint32_t u32BitRate,
                                                   FLEXCAN_BitTiming_t *pTiming)
{
    FLEXCAN_Status_t xStatus = FLEXCAN_STATUS_ERROR;
    uint16_t u16Prescaler = 0U;
    uint8_t u8PropSeg = 0U;
    uint8_t u8PhaseSeg1 = 0U;
    uint8_t u8PhaseSeg2 = 0U;
    uint16_t u16TotalTq = 0U;
    uint32_t u32CalculatedRate = 0U;
    uint32_t u32SamplePoint = 0U;
    uint32_t u32SampleError = 0U;
    uint32_t u32BestSampleError = 0xFFFFFFFFUL;
    uint32_t u32SymmetryError = 0U;
    uint32_t u32BestSymmetryError = 0xFFFFFFFFUL;
    uint16_t u16BestTotalTq = 0U;

    if ((0U == u32ClockFrequencyHz) || (0U == u32BitRate) || (NULL == pTiming))
    {
        xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        for (u16Prescaler = FLEXCAN_TIMING_MIN_PRESCALER;
             u16Prescaler <= FLEXCAN_TIMING_MAX_PRESCALER;
             u16Prescaler++)
        {
            for (u8PropSeg = FLEXCAN_TIMING_MIN_PROP_SEG;
                 u8PropSeg <= FLEXCAN_TIMING_MAX_PROP_SEG;
                 u8PropSeg++)
            {
                for (u8PhaseSeg1 = FLEXCAN_TIMING_MIN_PHASE_SEG1;
                     u8PhaseSeg1 <= FLEXCAN_TIMING_MAX_PHASE_SEG1;
                     u8PhaseSeg1++)
                {
                    for (u8PhaseSeg2 = FLEXCAN_TIMING_MIN_PHASE_SEG2;
                         u8PhaseSeg2 <= FLEXCAN_TIMING_MAX_PHASE_SEG2;
                         u8PhaseSeg2++)
                    {
                        u16TotalTq = 1U +
                                     (uint16_t)u8PropSeg +
                                     (uint16_t)u8PhaseSeg1 +
                                     (uint16_t)u8PhaseSeg2;

                        if ((FLEXCAN_TIMING_MIN_TQ <= u16TotalTq) &&
                            (FLEXCAN_TIMING_MAX_TQ >= u16TotalTq))
                        {
                            u32CalculatedRate = u32ClockFrequencyHz /
                                                ((uint32_t)u16Prescaler *
                                                 (uint32_t)u16TotalTq);

                            if ((u32BitRate == u32CalculatedRate) &&
                                (u32ClockFrequencyHz ==
                                 (u32CalculatedRate *
                                  (uint32_t)u16Prescaler *
                                  (uint32_t)u16TotalTq)))
                            {
                                u32SamplePoint =
                                    ((1UL +
                                      (uint32_t)u8PropSeg +
                                      (uint32_t)u8PhaseSeg1) *
                                     100UL) /
                                    (uint32_t)u16TotalTq;

                                if (FLEXCAN_TIMING_TARGET_SAMPLE_POINT >= u32SamplePoint)
                                {
                                    u32SampleError =
                                        FLEXCAN_TIMING_TARGET_SAMPLE_POINT - u32SamplePoint;
                                }
                                else
                                {
                                    u32SampleError =
                                        u32SamplePoint - FLEXCAN_TIMING_TARGET_SAMPLE_POINT;
                                }

                                if (u8PhaseSeg1 >= u8PhaseSeg2)
                                {
                                    u32SymmetryError =
                                        (uint32_t)u8PhaseSeg1 - (uint32_t)u8PhaseSeg2;
                                }
                                else
                                {
                                    u32SymmetryError =
                                        (uint32_t)u8PhaseSeg2 - (uint32_t)u8PhaseSeg1;
                                }

                                if ((u32BestSampleError > u32SampleError) ||
                                    ((u32BestSampleError == u32SampleError) &&
                                     (u32BestSymmetryError > u32SymmetryError)) ||
                                    ((u32BestSampleError == u32SampleError) &&
                                     (u32BestSymmetryError == u32SymmetryError) &&
                                     (u16BestTotalTq < u16TotalTq)))
                                {
                                    pTiming->u16Prescaler = u16Prescaler;
                                    pTiming->u8PropSeg = u8PropSeg;
                                    pTiming->u8PhaseSeg1 = u8PhaseSeg1;
                                    pTiming->u8PhaseSeg2 = u8PhaseSeg2;
                                    pTiming->u8Rjw = u8PhaseSeg2;

                                    if (FLEXCAN_TIMING_MAX_RJW < pTiming->u8Rjw)
                                    {
                                        pTiming->u8Rjw = FLEXCAN_TIMING_MAX_RJW;
                                    }

                                    pTiming->bTripleSampling = false;

                                    u32BestSampleError = u32SampleError;
                                    u32BestSymmetryError = u32SymmetryError;
                                    u16BestTotalTq = u16TotalTq;
                                    xStatus = FLEXCAN_STATUS_OK;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return xStatus;
}

FLEXCAN_Status_t FLEXCAN_SetNominalBitTiming(FLEXCAN_Type *pBase,
                                             const FLEXCAN_BitTiming_t *pTiming)
{
    FLEXCAN_Status_t xStatus = FLEXCAN_STATUS_OK;
    uint8_t u8InstanceIndex = FLEXCAN_INSTANCE_INVALID;

    if ((NULL == pBase) || (NULL == pTiming) ||
        (false == FLEXCAN_IsSupportedBoardInstance(pBase)))
    {
        xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        u8InstanceIndex = FLEXCAN_GetInstanceIndex(pBase);

        if (false == s_axFlexcanState[u8InstanceIndex].bInitialized)
        {
            xStatus = FLEXCAN_STATUS_NOT_INITIALIZED;
        }
        else if (false == FLEXCAN_IsValidNominalTiming(pTiming))
        {
            xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
        }
        else
        {
            xStatus = FLEXCAN_EnterFreezeInternal(pBase, FLEXCAN_DEFAULT_TIMEOUT);

            if (FLEXCAN_STATUS_OK == xStatus)
            {
                xStatus = FLEXCAN_ApplyNominalTiming(pBase, pTiming);
            }

            if (FLEXCAN_STATUS_OK == xStatus)
            {
                xStatus = FLEXCAN_ExitFreezeInternal(pBase, FLEXCAN_DEFAULT_TIMEOUT);
            }
        }
    }

    return xStatus;
}

/* ========================================================================= */
/* Error and Diagnostic APIs                                                 */
/* ========================================================================= */

FLEXCAN_Status_t FLEXCAN_GetErrorStatus(FLEXCAN_Type *pBase,
                                        FLEXCAN_ErrorStatus_t *pStatus)
{
    FLEXCAN_Status_t xStatus = FLEXCAN_STATUS_OK;
    uint8_t u8InstanceIndex = FLEXCAN_INSTANCE_INVALID;
    uint32_t u32Esr1 = 0U;
    uint32_t u32Ecr = 0U;

    if ((NULL == pBase) || (NULL == pStatus) ||
        (false == FLEXCAN_IsSupportedBoardInstance(pBase)))
    {
        xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        u8InstanceIndex = FLEXCAN_GetInstanceIndex(pBase);

        if (false == s_axFlexcanState[u8InstanceIndex].bInitialized)
        {
            xStatus = FLEXCAN_STATUS_NOT_INITIALIZED;
        }
        else
        {
            u32Esr1 = pBase->ESR1;
            u32Ecr = pBase->ECR;

            pStatus->u32Esr1 = u32Esr1;
            pStatus->u8TxErrorCount =
                (uint8_t)((u32Ecr & FLEXCAN_ECR_TXERRCNT_MASK) >>
                          FLEXCAN_ECR_TXERRCNT_SHIFT);
            pStatus->u8RxErrorCount =
                (uint8_t)((u32Ecr & FLEXCAN_ECR_RXERRCNT_MASK) >>
                          FLEXCAN_ECR_RXERRCNT_SHIFT);
            pStatus->u8TxFastErrorCount =
                (uint8_t)((u32Ecr & FLEXCAN_ECR_TXERRCNT_FAST_MASK) >>
                          FLEXCAN_ECR_TXERRCNT_FAST_SHIFT);
            pStatus->u8RxFastErrorCount =
                (uint8_t)((u32Ecr & FLEXCAN_ECR_RXERRCNT_FAST_MASK) >>
                          FLEXCAN_ECR_RXERRCNT_FAST_SHIFT);

            pStatus->bSynchronized =
                (0U != (u32Esr1 & FLEXCAN_ESR1_SYNCH_MASK));
            pStatus->bBusOff =
                ((u32Esr1 & FLEXCAN_ESR1_FLTCONF_MASK) >=
                 FLEXCAN_FLTCONF_BUS_OFF_VALUE);
            pStatus->bTxWarning =
                (0U != (u32Esr1 & FLEXCAN_ESR1_TXWRN_MASK));
            pStatus->bRxWarning =
                (0U != (u32Esr1 & FLEXCAN_ESR1_RXWRN_MASK));

            pStatus->bStuffError =
                (0U != (u32Esr1 & FLEXCAN_ESR1_STFERR_MASK));
            pStatus->bFormError =
                (0U != (u32Esr1 & FLEXCAN_ESR1_FRMERR_MASK));
            pStatus->bCrcError =
                (0U != (u32Esr1 & FLEXCAN_ESR1_CRCERR_MASK));
            pStatus->bAckError =
                (0U != (u32Esr1 & FLEXCAN_ESR1_ACKERR_MASK));
            pStatus->bBit0Error =
                (0U != (u32Esr1 & FLEXCAN_ESR1_BIT0ERR_MASK));
            pStatus->bBit1Error =
                (0U != (u32Esr1 & FLEXCAN_ESR1_BIT1ERR_MASK));

            pStatus->bFastStuffError =
                (0U != (u32Esr1 & FLEXCAN_ESR1_STFERR_FAST_MASK));
            pStatus->bFastFormError =
                (0U != (u32Esr1 & FLEXCAN_ESR1_FRMERR_FAST_MASK));
            pStatus->bFastCrcError =
                (0U != (u32Esr1 & FLEXCAN_ESR1_CRCERR_FAST_MASK));
            pStatus->bFastBit0Error =
                (0U != (u32Esr1 & FLEXCAN_ESR1_BIT0ERR_FAST_MASK));
            pStatus->bFastBit1Error =
                (0U != (u32Esr1 & FLEXCAN_ESR1_BIT1ERR_FAST_MASK));
        }
    }

    return xStatus;
}

bool FLEXCAN_IsSynchronized(FLEXCAN_Type *pBase)
{
    bool bSynchronized = false;
    uint8_t u8InstanceIndex = FLEXCAN_INSTANCE_INVALID;

    if ((NULL != pBase) && (true == FLEXCAN_IsSupportedBoardInstance(pBase)))
    {
        u8InstanceIndex = FLEXCAN_GetInstanceIndex(pBase);

        if ((true == s_axFlexcanState[u8InstanceIndex].bInitialized) &&
            (0U != (pBase->ESR1 & FLEXCAN_ESR1_SYNCH_MASK)))
        {
            bSynchronized = true;
        }
    }

    return bSynchronized;
}

bool FLEXCAN_IsBusOff(FLEXCAN_Type *pBase)
{
    bool bBusOff = false;
    uint8_t u8InstanceIndex = FLEXCAN_INSTANCE_INVALID;
    uint32_t u32Esr1 = 0U;

    if ((NULL != pBase) && (true == FLEXCAN_IsSupportedBoardInstance(pBase)))
    {
        u8InstanceIndex = FLEXCAN_GetInstanceIndex(pBase);

        if (true == s_axFlexcanState[u8InstanceIndex].bInitialized)
        {
            u32Esr1 = pBase->ESR1;

            if ((u32Esr1 & FLEXCAN_ESR1_FLTCONF_MASK) >=
                FLEXCAN_FLTCONF_BUS_OFF_VALUE)
            {
                bBusOff = true;
            }
        }
    }

    return bBusOff;
}

uint16_t FLEXCAN_GetTimer(FLEXCAN_Type *pBase)
{
    uint16_t u16Timer = 0U;
    uint8_t u8InstanceIndex = FLEXCAN_INSTANCE_INVALID;

    if ((NULL != pBase) && (true == FLEXCAN_IsSupportedBoardInstance(pBase)))
    {
        u8InstanceIndex = FLEXCAN_GetInstanceIndex(pBase);

        if (true == s_axFlexcanState[u8InstanceIndex].bInitialized)
        {
            u16Timer = (uint16_t)(pBase->TIMER & FLEXCAN_TIMER_TIMER_MASK);
        }
    }

    return u16Timer;
}

/* ========================================================================= */
/* Classical CAN Message Buffer Support                                      */
/* ========================================================================= */

/*
 * Classical CAN Message Buffer layout (8-byte payload):
 *   word 0: Control / Status
 *   word 1: Identifier
 *   word 2: Data bytes 0..3
 *   word 3: Data bytes 4..7
 */
#define FLEXCAN_CLASSIC_MB_WORD_COUNT (4U)
#define FLEXCAN_MB_CS_WORD_OFFSET (0U)
#define FLEXCAN_MB_ID_WORD_OFFSET (1U)
#define FLEXCAN_MB_DATA0_WORD_OFFSET (2U)
#define FLEXCAN_MB_DATA1_WORD_OFFSET (3U)

#define FLEXCAN_MB_CS_TIMESTAMP_MASK (0x0000FFFFUL)
#define FLEXCAN_MB_CS_DLC_MASK (0x000F0000UL)
#define FLEXCAN_MB_CS_DLC_SHIFT (16U)
#define FLEXCAN_MB_CS_RTR_MASK (0x00100000UL)
#define FLEXCAN_MB_CS_IDE_MASK (0x00200000UL)
#define FLEXCAN_MB_CS_SRR_MASK (0x00400000UL)
#define FLEXCAN_MB_CS_CODE_MASK (0x0F000000UL)
#define FLEXCAN_MB_CS_CODE_SHIFT (24U)

#define FLEXCAN_MB_CODE_RX_INACTIVE (0x0U)
#define FLEXCAN_MB_CODE_RX_FULL (0x2U)
#define FLEXCAN_MB_CODE_RX_EMPTY (0x4U)
#define FLEXCAN_MB_CODE_RX_OVERRUN (0x6U)
#define FLEXCAN_MB_CODE_TX_INACTIVE (0x8U)
#define FLEXCAN_MB_CODE_TX_ABORT (0x9U)
#define FLEXCAN_MB_CODE_TX_DATA (0xCU)

#define FLEXCAN_MB_STANDARD_ID_SHIFT (18U)
#define FLEXCAN_MB_STANDARD_ID_MASK (0x1FFC0000UL)
#define FLEXCAN_MB_EXTENDED_ID_MASK (0x1FFFFFFFUL)
#define FLEXCAN_MB_LOCAL_PRIORITY_MASK (0xE0000000UL)
#define FLEXCAN_MB_LOCAL_PRIORITY_SHIFT (29U)

/*
 * Rx acceptance-mask alignment is not identical to the MB C/S layout.
 * With CTRL2[EACEN] enabled, mask bit 31 controls RTR comparison and mask bit
 * 30 controls IDE comparison. Identifier bits then follow the MB ID layout.
 */
#define FLEXCAN_RX_MASK_RTR_MASK (0x80000000UL)
#define FLEXCAN_RX_MASK_IDE_MASK (0x40000000UL)

static bool FLEXCAN_IsValidFrameFormat(FLEXCAN_FrameFormat_t u8Format)
{
    bool bValid = false;

    if ((FLEXCAN_FRAME_STANDARD == u8Format) ||
        (FLEXCAN_FRAME_EXTENDED == u8Format))
    {
        bValid = true;
    }

    return bValid;
}

static bool FLEXCAN_IsValidFrameType(FLEXCAN_FrameType_t u8FrameType)
{
    bool bValid = false;

    if ((FLEXCAN_FRAME_DATA == u8FrameType) ||
        (FLEXCAN_FRAME_REMOTE == u8FrameType))
    {
        bValid = true;
    }

    return bValid;
}

static bool FLEXCAN_IsValidClassicalFrame(const FLEXCAN_Frame_t *pFrame)
{
    bool bValid = false;

    if ((NULL != pFrame) &&
        (false == pFrame->bFD) &&
        (FLEXCAN_CLASSIC_MAX_DATA_LENGTH >= pFrame->u8Length) &&
        (7U >= pFrame->u8LocalPriority) &&
        (true == FLEXCAN_IsValidFrameFormat(pFrame->u8Format)) &&
        (true == FLEXCAN_IsValidFrameType(pFrame->u8FrameType)))
    {
        if ((FLEXCAN_FRAME_STANDARD == pFrame->u8Format) &&
            (FLEXCAN_STANDARD_ID_MAX >= pFrame->u32Id))
        {
            bValid = true;
        }
        else if ((FLEXCAN_FRAME_EXTENDED == pFrame->u8Format) &&
                 (FLEXCAN_EXTENDED_ID_MAX >= pFrame->u32Id))
        {
            bValid = true;
        }
        else
        {
            /* Invalid identifier for the selected frame format. */
        }
    }

    return bValid;
}

static bool FLEXCAN_IsMbIndexValid(FLEXCAN_Type *pBase, uint8_t u8MbIndex)
{
    bool bValid = false;
    uint8_t u8LastMb = 0U;

    if ((NULL != pBase) && (FLEXCAN_MAX_MB_COUNT > u8MbIndex))
    {
        u8LastMb = (uint8_t)((pBase->MCR & FLEXCAN_MCR_MAXMB_MASK) >>
                             FLEXCAN_MCR_MAXMB_SHIFT);

        if (u8LastMb >= u8MbIndex)
        {
            bValid = true;
        }
    }

    return bValid;
}

static uint32_t FLEXCAN_GetMbWordIndex(uint8_t u8MbIndex)
{
    return ((uint32_t)u8MbIndex * FLEXCAN_CLASSIC_MB_WORD_COUNT);
}

static uint32_t FLEXCAN_GetMbFlagMask(uint8_t u8MbIndex)
{
    return (1UL << u8MbIndex);
}

static uint8_t FLEXCAN_GetMbCode(uint32_t u32Cs)
{
    return (uint8_t)((u32Cs & FLEXCAN_MB_CS_CODE_MASK) >>
                     FLEXCAN_MB_CS_CODE_SHIFT);
}

static uint32_t FLEXCAN_EncodeId(uint32_t u32Id,
                                 FLEXCAN_FrameFormat_t u8Format,
                                 uint8_t u8LocalPriority)
{
    uint32_t u32IdWord = 0U;

    if (FLEXCAN_FRAME_EXTENDED == u8Format)
    {
        u32IdWord = u32Id & FLEXCAN_MB_EXTENDED_ID_MASK;
    }
    else
    {
        u32IdWord = (u32Id << FLEXCAN_MB_STANDARD_ID_SHIFT) &
                    FLEXCAN_MB_STANDARD_ID_MASK;
    }

    u32IdWord |= (((uint32_t)u8LocalPriority << FLEXCAN_MB_LOCAL_PRIORITY_SHIFT) &
                  FLEXCAN_MB_LOCAL_PRIORITY_MASK);

    return u32IdWord;
}

static uint32_t FLEXCAN_EncodeRxMask(uint32_t u32Mask,
                                     FLEXCAN_FrameFormat_t u8Format,
                                     bool bCompareFrameType)
{
    uint32_t u32EncodedMask = FLEXCAN_RX_MASK_IDE_MASK;

    if (FLEXCAN_FRAME_EXTENDED == u8Format)
    {
        u32EncodedMask |= u32Mask & FLEXCAN_MB_EXTENDED_ID_MASK;
    }
    else
    {
        u32EncodedMask |= (u32Mask << FLEXCAN_MB_STANDARD_ID_SHIFT) &
                          FLEXCAN_MB_STANDARD_ID_MASK;
    }

    if (true == bCompareFrameType)
    {
        u32EncodedMask |= FLEXCAN_RX_MASK_RTR_MASK;
    }

    return u32EncodedMask;
}

static uint32_t FLEXCAN_PackDataWord(const uint8_t *pData)
{
    uint32_t u32Word = 0U;

    u32Word = ((uint32_t)pData[0U] << 24U) |
              ((uint32_t)pData[1U] << 16U) |
              ((uint32_t)pData[2U] << 8U) |
              ((uint32_t)pData[3U]);

    return u32Word;
}

static void FLEXCAN_UnpackDataWord(uint32_t u32Word, uint8_t *pData)
{
    pData[0U] = (uint8_t)(u32Word >> 24U);
    pData[1U] = (uint8_t)(u32Word >> 16U);
    pData[2U] = (uint8_t)(u32Word >> 8U);
    pData[3U] = (uint8_t)u32Word;

    return;
}

static void FLEXCAN_WriteRxMaskInternal(FLEXCAN_Type *pBase,
                                        uint8_t u8MbIndex,
                                        uint32_t u32EncodedMask)
{
    if (0U != (pBase->MCR & FLEXCAN_MCR_IRMQ_MASK))
    {
        pBase->RXIMR[u8MbIndex] = u32EncodedMask;
    }
    else if (14U == u8MbIndex)
    {
        pBase->RX14MASK = u32EncodedMask;
    }
    else if (15U == u8MbIndex)
    {
        pBase->RX15MASK = u32EncodedMask;
    }
    else
    {
        pBase->RXMGMASK = u32EncodedMask;
    }

    return;
}

FLEXCAN_Status_t FLEXCAN_ConfigTxMb(FLEXCAN_Type *pBase,
                                    uint8_t u8MbIndex,
                                    bool bEnableInterrupt)
{
    FLEXCAN_Status_t xStatus = FLEXCAN_STATUS_OK;
    uint8_t u8InstanceIndex = FLEXCAN_INSTANCE_INVALID;
    uint32_t u32WordIndex = 0U;
    uint32_t u32FlagMask = 0U;

    if ((NULL == pBase) ||
        (false == FLEXCAN_IsSupportedBoardInstance(pBase)))
    {
        xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        u8InstanceIndex = FLEXCAN_GetInstanceIndex(pBase);

        if (false == s_axFlexcanState[u8InstanceIndex].bInitialized)
        {
            xStatus = FLEXCAN_STATUS_NOT_INITIALIZED;
        }
        else if (false == FLEXCAN_IsMbIndexValid(pBase, u8MbIndex))
        {
            xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
        }
        else
        {
            xStatus = FLEXCAN_EnterFreezeInternal(pBase,
                                                  FLEXCAN_DEFAULT_TIMEOUT);

            if (FLEXCAN_STATUS_OK == xStatus)
            {
                u32WordIndex = FLEXCAN_GetMbWordIndex(u8MbIndex);
                u32FlagMask = FLEXCAN_GetMbFlagMask(u8MbIndex);

                pBase->RAMn[u32WordIndex + FLEXCAN_MB_CS_WORD_OFFSET] =
                    ((uint32_t)FLEXCAN_MB_CODE_TX_INACTIVE << FLEXCAN_MB_CS_CODE_SHIFT);
                pBase->RAMn[u32WordIndex + FLEXCAN_MB_ID_WORD_OFFSET] = 0U;
                pBase->RAMn[u32WordIndex + FLEXCAN_MB_DATA0_WORD_OFFSET] = 0U;
                pBase->RAMn[u32WordIndex + FLEXCAN_MB_DATA1_WORD_OFFSET] = 0U;

                /* IFLAG is write-one-to-clear. Clear only this MB flag. */
                pBase->IFLAG1 = u32FlagMask;

                if (true == bEnableInterrupt)
                {
                    pBase->IMASK1 |= u32FlagMask;
                }
                else
                {
                    pBase->IMASK1 &= ~u32FlagMask;
                }

                xStatus = FLEXCAN_ExitFreezeInternal(pBase,
                                                     FLEXCAN_DEFAULT_TIMEOUT);
            }
        }
    }

    return xStatus;
}

FLEXCAN_Status_t FLEXCAN_ConfigRxMb(FLEXCAN_Type *pBase,
                                    const FLEXCAN_RxMbConfig_t *pConfig)
{
    FLEXCAN_Status_t xStatus = FLEXCAN_STATUS_OK;
    uint8_t u8InstanceIndex = FLEXCAN_INSTANCE_INVALID;
    uint32_t u32WordIndex = 0U;
    uint32_t u32FlagMask = 0U;
    uint32_t u32Cs = 0U;
    uint32_t u32EncodedMask = 0U;

    if ((NULL == pBase) || (NULL == pConfig) ||
        (false == FLEXCAN_IsSupportedBoardInstance(pBase)))
    {
        xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        u8InstanceIndex = FLEXCAN_GetInstanceIndex(pBase);

        if (false == s_axFlexcanState[u8InstanceIndex].bInitialized)
        {
            xStatus = FLEXCAN_STATUS_NOT_INITIALIZED;
        }
        else if ((false == FLEXCAN_IsMbIndexValid(pBase, pConfig->u8MbIndex)) ||
                 (false == FLEXCAN_IsValidFrameFormat(pConfig->u8Format)) ||
                 (false == FLEXCAN_IsValidFrameType(pConfig->u8FrameType)) ||
                 ((FLEXCAN_FRAME_STANDARD == pConfig->u8Format) &&
                  ((FLEXCAN_STANDARD_ID_MAX < pConfig->u32Id) ||
                   (FLEXCAN_STANDARD_ID_MAX < pConfig->u32Mask))) ||
                 ((FLEXCAN_FRAME_EXTENDED == pConfig->u8Format) &&
                  ((FLEXCAN_EXTENDED_ID_MAX < pConfig->u32Id) ||
                   (FLEXCAN_EXTENDED_ID_MAX < pConfig->u32Mask))))
        {
            xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
        }
        else
        {
            xStatus = FLEXCAN_EnterFreezeInternal(pBase,
                                                  FLEXCAN_DEFAULT_TIMEOUT);

            if (FLEXCAN_STATUS_OK == xStatus)
            {
                u32WordIndex = FLEXCAN_GetMbWordIndex(pConfig->u8MbIndex);
                u32FlagMask = FLEXCAN_GetMbFlagMask(pConfig->u8MbIndex);

                /*
                 * Store remote requests in software-visible MBs and compare
                 * both IDE and RTR during mailbox matching.
                 */
                pBase->CTRL2 |= (FLEXCAN_CTRL2_EACEN_MASK |
                                 FLEXCAN_CTRL2_RRS_MASK);

                /* Inactivate before changing the receive filter. */
                pBase->RAMn[u32WordIndex + FLEXCAN_MB_CS_WORD_OFFSET] =
                    ((uint32_t)FLEXCAN_MB_CODE_RX_INACTIVE << FLEXCAN_MB_CS_CODE_SHIFT);

                pBase->RAMn[u32WordIndex + FLEXCAN_MB_ID_WORD_OFFSET] =
                    FLEXCAN_EncodeId(pConfig->u32Id,
                                     pConfig->u8Format,
                                     0U);

                u32EncodedMask = FLEXCAN_EncodeRxMask(pConfig->u32Mask,
                                                      pConfig->u8Format,
                                                      true);
                FLEXCAN_WriteRxMaskInternal(pBase,
                                            pConfig->u8MbIndex,
                                            u32EncodedMask);

                u32Cs = ((uint32_t)FLEXCAN_MB_CODE_RX_EMPTY << FLEXCAN_MB_CS_CODE_SHIFT);

                if (FLEXCAN_FRAME_EXTENDED == pConfig->u8Format)
                {
                    u32Cs |= FLEXCAN_MB_CS_IDE_MASK;
                }

                if (FLEXCAN_FRAME_REMOTE == pConfig->u8FrameType)
                {
                    u32Cs |= FLEXCAN_MB_CS_RTR_MASK;
                }

                /* EMPTY is written last to activate the Rx MB. */
                pBase->RAMn[u32WordIndex + FLEXCAN_MB_CS_WORD_OFFSET] = u32Cs;

                pBase->IFLAG1 = u32FlagMask;

                if (true == pConfig->bEnableInterrupt)
                {
                    pBase->IMASK1 |= u32FlagMask;
                }
                else
                {
                    pBase->IMASK1 &= ~u32FlagMask;
                }

                xStatus = FLEXCAN_ExitFreezeInternal(pBase,
                                                     FLEXCAN_DEFAULT_TIMEOUT);
            }
        }
    }

    return xStatus;
}

FLEXCAN_Status_t FLEXCAN_DisableMb(FLEXCAN_Type *pBase,
                                   uint8_t u8MbIndex)
{
    FLEXCAN_Status_t xStatus = FLEXCAN_STATUS_OK;
    uint8_t u8InstanceIndex = FLEXCAN_INSTANCE_INVALID;
    uint32_t u32WordIndex = 0U;
    uint32_t u32FlagMask = 0U;

    if ((NULL == pBase) ||
        (false == FLEXCAN_IsSupportedBoardInstance(pBase)))
    {
        xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        u8InstanceIndex = FLEXCAN_GetInstanceIndex(pBase);

        if (false == s_axFlexcanState[u8InstanceIndex].bInitialized)
        {
            xStatus = FLEXCAN_STATUS_NOT_INITIALIZED;
        }
        else if (false == FLEXCAN_IsMbIndexValid(pBase, u8MbIndex))
        {
            xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
        }
        else
        {
            xStatus = FLEXCAN_EnterFreezeInternal(pBase,
                                                  FLEXCAN_DEFAULT_TIMEOUT);

            if (FLEXCAN_STATUS_OK == xStatus)
            {
                u32WordIndex = FLEXCAN_GetMbWordIndex(u8MbIndex);
                u32FlagMask = FLEXCAN_GetMbFlagMask(u8MbIndex);

                /* CODE=0000 makes the MB inactive for matching/arbitration. */
                pBase->RAMn[u32WordIndex + FLEXCAN_MB_CS_WORD_OFFSET] = 0U;
                pBase->RAMn[u32WordIndex + FLEXCAN_MB_ID_WORD_OFFSET] = 0U;
                pBase->RAMn[u32WordIndex + FLEXCAN_MB_DATA0_WORD_OFFSET] = 0U;
                pBase->RAMn[u32WordIndex + FLEXCAN_MB_DATA1_WORD_OFFSET] = 0U;

                pBase->IMASK1 &= ~u32FlagMask;
                pBase->IFLAG1 = u32FlagMask;

                xStatus = FLEXCAN_ExitFreezeInternal(pBase,
                                                     FLEXCAN_DEFAULT_TIMEOUT);
            }
        }
    }

    return xStatus;
}

FLEXCAN_Status_t FLEXCAN_SetRxMbMask(FLEXCAN_Type *pBase,
                                     uint8_t u8MbIndex,
                                     uint32_t u32Mask,
                                     FLEXCAN_FrameFormat_t u8Format)
{
    FLEXCAN_Status_t xStatus = FLEXCAN_STATUS_OK;
    uint8_t u8InstanceIndex = FLEXCAN_INSTANCE_INVALID;
    uint32_t u32EncodedMask = 0U;
    uint32_t u32RtrMask = 0U;

    if ((NULL == pBase) ||
        (false == FLEXCAN_IsSupportedBoardInstance(pBase)))
    {
        xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        u8InstanceIndex = FLEXCAN_GetInstanceIndex(pBase);

        if (false == s_axFlexcanState[u8InstanceIndex].bInitialized)
        {
            xStatus = FLEXCAN_STATUS_NOT_INITIALIZED;
        }
        else if ((false == FLEXCAN_IsMbIndexValid(pBase, u8MbIndex)) ||
                 (false == FLEXCAN_IsValidFrameFormat(u8Format)) ||
                 ((FLEXCAN_FRAME_STANDARD == u8Format) &&
                  (FLEXCAN_STANDARD_ID_MAX < u32Mask)) ||
                 ((FLEXCAN_FRAME_EXTENDED == u8Format) &&
                  (FLEXCAN_EXTENDED_ID_MAX < u32Mask)))
        {
            xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
        }
        else
        {
            xStatus = FLEXCAN_EnterFreezeInternal(pBase,
                                                  FLEXCAN_DEFAULT_TIMEOUT);

            if (FLEXCAN_STATUS_OK == xStatus)
            {
                /* Preserve RTR comparison chosen by FLEXCAN_ConfigRxMb(). */
                if (0U != (pBase->MCR & FLEXCAN_MCR_IRMQ_MASK))
                {
                    u32RtrMask = pBase->RXIMR[u8MbIndex] &
                                 FLEXCAN_RX_MASK_RTR_MASK;
                }

                u32EncodedMask = FLEXCAN_EncodeRxMask(u32Mask,
                                                      u8Format,
                                                      false) |
                                 u32RtrMask;

                FLEXCAN_WriteRxMaskInternal(pBase,
                                            u8MbIndex,
                                            u32EncodedMask);

                xStatus = FLEXCAN_ExitFreezeInternal(pBase,
                                                     FLEXCAN_DEFAULT_TIMEOUT);
            }
        }
    }

    return xStatus;
}

FLEXCAN_Status_t FLEXCAN_SetGlobalRxMask(FLEXCAN_Type *pBase,
                                         uint32_t u32Mask,
                                         FLEXCAN_FrameFormat_t u8Format)
{
    FLEXCAN_Status_t xStatus = FLEXCAN_STATUS_OK;
    uint8_t u8InstanceIndex = FLEXCAN_INSTANCE_INVALID;

    if ((NULL == pBase) ||
        (false == FLEXCAN_IsSupportedBoardInstance(pBase)))
    {
        xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        u8InstanceIndex = FLEXCAN_GetInstanceIndex(pBase);

        if (false == s_axFlexcanState[u8InstanceIndex].bInitialized)
        {
            xStatus = FLEXCAN_STATUS_NOT_INITIALIZED;
        }
        else if ((false == FLEXCAN_IsValidFrameFormat(u8Format)) ||
                 ((FLEXCAN_FRAME_STANDARD == u8Format) &&
                  (FLEXCAN_STANDARD_ID_MAX < u32Mask)) ||
                 ((FLEXCAN_FRAME_EXTENDED == u8Format) &&
                  (FLEXCAN_EXTENDED_ID_MAX < u32Mask)))
        {
            xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
        }
        else
        {
            xStatus = FLEXCAN_EnterFreezeInternal(pBase,
                                                  FLEXCAN_DEFAULT_TIMEOUT);

            if (FLEXCAN_STATUS_OK == xStatus)
            {
                pBase->RXMGMASK = FLEXCAN_EncodeRxMask(u32Mask,
                                                       u8Format,
                                                       false);

                xStatus = FLEXCAN_ExitFreezeInternal(pBase,
                                                     FLEXCAN_DEFAULT_TIMEOUT);
            }
        }
    }

    return xStatus;
}

/* ========================================================================= */
/* Classical CAN Transmission                                                */
/* ========================================================================= */

FLEXCAN_Status_t FLEXCAN_Transmit(FLEXCAN_Type *pBase,
                                  uint8_t u8MbIndex,
                                  const FLEXCAN_Frame_t *pFrame)
{
    FLEXCAN_Status_t xStatus = FLEXCAN_STATUS_OK;
    uint8_t u8InstanceIndex = FLEXCAN_INSTANCE_INVALID;
    uint8_t u8Code = 0U;
    uint32_t u32WordIndex = 0U;
    uint32_t u32FlagMask = 0U;
    uint32_t u32Cs = 0U;
    uint32_t u32Data0 = 0U;
    uint32_t u32Data1 = 0U;
    uint8_t au8Data[FLEXCAN_CLASSIC_MAX_DATA_LENGTH] = {0};
    uint8_t u8Index = 0U;

    if ((NULL == pBase) || (NULL == pFrame) ||
        (false == FLEXCAN_IsSupportedBoardInstance(pBase)))
    {
        xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        u8InstanceIndex = FLEXCAN_GetInstanceIndex(pBase);

        if (false == s_axFlexcanState[u8InstanceIndex].bInitialized)
        {
            xStatus = FLEXCAN_STATUS_NOT_INITIALIZED;
        }
        else if ((false == FLEXCAN_IsMbIndexValid(pBase, u8MbIndex)) ||
                 (false == FLEXCAN_IsValidClassicalFrame(pFrame)))
        {
            xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
        }
        else
        {
            u32WordIndex = FLEXCAN_GetMbWordIndex(u8MbIndex);
            u32FlagMask = FLEXCAN_GetMbFlagMask(u8MbIndex);
            u8Code = FLEXCAN_GetMbCode(
                pBase->RAMn[u32WordIndex + FLEXCAN_MB_CS_WORD_OFFSET]);

            if (FLEXCAN_MB_CODE_TX_DATA == u8Code)
            {
                xStatus = FLEXCAN_STATUS_BUSY;
            }
            else if ((FLEXCAN_MB_CODE_TX_INACTIVE != u8Code) &&
                     (FLEXCAN_MB_CODE_TX_ABORT != u8Code))
            {
                /* The API requires a Message Buffer configured for Tx. */
                xStatus = FLEXCAN_STATUS_ERROR;
            }
            else
            {
                /* Clear only the previous completion flag for this MB. */
                pBase->IFLAG1 = u32FlagMask;

                pBase->RAMn[u32WordIndex + FLEXCAN_MB_ID_WORD_OFFSET] =
                    FLEXCAN_EncodeId(pFrame->u32Id,
                                     pFrame->u8Format,
                                     pFrame->u8LocalPriority);

                if (FLEXCAN_FRAME_DATA == pFrame->u8FrameType)
                {
                    for (u8Index = 0U; u8Index < pFrame->u8Length; u8Index++)
                    {
                        au8Data[u8Index] = pFrame->au8Data[u8Index];
                    }
                }

                u32Data0 = FLEXCAN_PackDataWord(&au8Data[0U]);
                u32Data1 = FLEXCAN_PackDataWord(&au8Data[4U]);

                pBase->RAMn[u32WordIndex + FLEXCAN_MB_DATA0_WORD_OFFSET] =
                    u32Data0;
                pBase->RAMn[u32WordIndex + FLEXCAN_MB_DATA1_WORD_OFFSET] =
                    u32Data1;

                u32Cs = ((uint32_t)FLEXCAN_MB_CODE_TX_DATA << FLEXCAN_MB_CS_CODE_SHIFT) |
                        (((uint32_t)pFrame->u8Length << FLEXCAN_MB_CS_DLC_SHIFT) &
                         FLEXCAN_MB_CS_DLC_MASK);

                if (FLEXCAN_FRAME_EXTENDED == pFrame->u8Format)
                {
                    u32Cs |= (FLEXCAN_MB_CS_IDE_MASK |
                              FLEXCAN_MB_CS_SRR_MASK);
                }

                if (FLEXCAN_FRAME_REMOTE == pFrame->u8FrameType)
                {
                    u32Cs |= FLEXCAN_MB_CS_RTR_MASK;
                }

                /* CODE is written last; this activates transmission. */
                pBase->RAMn[u32WordIndex + FLEXCAN_MB_CS_WORD_OFFSET] = u32Cs;
            }
        }
    }

    return xStatus;
}

FLEXCAN_Status_t FLEXCAN_TransmitBlocking(FLEXCAN_Type *pBase,
                                          uint8_t u8MbIndex,
                                          const FLEXCAN_Frame_t *pFrame,
                                          uint32_t u32Timeout)
{
    FLEXCAN_Status_t xStatus = FLEXCAN_STATUS_OK;
    uint32_t u32FlagMask = 0U;

    if (0U == u32Timeout)
    {
        xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        xStatus = FLEXCAN_Transmit(pBase, u8MbIndex, pFrame);

        if (FLEXCAN_STATUS_OK == xStatus)
        {
            u32FlagMask = FLEXCAN_GetMbFlagMask(u8MbIndex);

            while ((0U == (pBase->IFLAG1 & u32FlagMask)) &&
                   (0U < u32Timeout))
            {
                u32Timeout--;
            }

            if (0U == (pBase->IFLAG1 & u32FlagMask))
            {
                xStatus = FLEXCAN_STATUS_TIMEOUT;
            }
            else
            {
                /*
                 * AEN blocks a completed Tx MB until its IFLAG is cleared.
                 * Clear only this MB flag after the blocking transfer.
                 */
                pBase->IFLAG1 = u32FlagMask;
            }
        }
    }

    return xStatus;
}

bool FLEXCAN_IsTxComplete(FLEXCAN_Type *pBase,
                          uint8_t u8MbIndex)
{
    bool bComplete = false;
    uint8_t u8InstanceIndex = FLEXCAN_INSTANCE_INVALID;
    uint32_t u32FlagMask = 0U;

    if ((NULL != pBase) &&
        (true == FLEXCAN_IsSupportedBoardInstance(pBase)))
    {
        u8InstanceIndex = FLEXCAN_GetInstanceIndex(pBase);

        if ((true == s_axFlexcanState[u8InstanceIndex].bInitialized) &&
            (true == FLEXCAN_IsMbIndexValid(pBase, u8MbIndex)))
        {
            u32FlagMask = FLEXCAN_GetMbFlagMask(u8MbIndex);

            if (0U != (pBase->IFLAG1 & u32FlagMask))
            {
                bComplete = true;
            }
        }
    }

    return bComplete;
}

FLEXCAN_Status_t FLEXCAN_AbortTransmit(FLEXCAN_Type *pBase,
                                       uint8_t u8MbIndex,
                                       uint32_t u32Timeout)
{
    FLEXCAN_Status_t xStatus = FLEXCAN_STATUS_OK;
    uint8_t u8InstanceIndex = FLEXCAN_INSTANCE_INVALID;
    uint8_t u8Code = 0U;
    uint32_t u32WordIndex = 0U;
    uint32_t u32FlagMask = 0U;
    uint32_t u32Cs = 0U;

    if ((NULL == pBase) || (0U == u32Timeout) ||
        (false == FLEXCAN_IsSupportedBoardInstance(pBase)))
    {
        xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        u8InstanceIndex = FLEXCAN_GetInstanceIndex(pBase);

        if (false == s_axFlexcanState[u8InstanceIndex].bInitialized)
        {
            xStatus = FLEXCAN_STATUS_NOT_INITIALIZED;
        }
        else if (false == FLEXCAN_IsMbIndexValid(pBase, u8MbIndex))
        {
            xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
        }
        else if (0U == (pBase->MCR & FLEXCAN_MCR_AEN_MASK))
        {
            xStatus = FLEXCAN_STATUS_UNSUPPORTED;
        }
        else
        {
            u32WordIndex = FLEXCAN_GetMbWordIndex(u8MbIndex);
            u32FlagMask = FLEXCAN_GetMbFlagMask(u8MbIndex);
            u32Cs = pBase->RAMn[u32WordIndex + FLEXCAN_MB_CS_WORD_OFFSET];
            u8Code = FLEXCAN_GetMbCode(u32Cs);

            if (FLEXCAN_MB_CODE_TX_DATA != u8Code)
            {
                /* Nothing is currently pending in this Tx MB. */
                xStatus = FLEXCAN_STATUS_OK;
            }
            else
            {
                pBase->IFLAG1 = u32FlagMask;

                u32Cs &= ~FLEXCAN_MB_CS_CODE_MASK;
                u32Cs |= ((uint32_t)FLEXCAN_MB_CODE_TX_ABORT << FLEXCAN_MB_CS_CODE_SHIFT);

                pBase->RAMn[u32WordIndex + FLEXCAN_MB_CS_WORD_OFFSET] = u32Cs;

                while ((0U == (pBase->IFLAG1 & u32FlagMask)) &&
                       (0U < u32Timeout))
                {
                    u32Timeout--;
                }

                if (0U == (pBase->IFLAG1 & u32FlagMask))
                {
                    xStatus = FLEXCAN_STATUS_TIMEOUT;
                }
                else
                {
                    pBase->IFLAG1 = u32FlagMask;

                    u8Code = FLEXCAN_GetMbCode(
                        pBase->RAMn[u32WordIndex + FLEXCAN_MB_CS_WORD_OFFSET]);

                    if ((FLEXCAN_MB_CODE_TX_ABORT != u8Code) &&
                        (FLEXCAN_MB_CODE_TX_INACTIVE != u8Code))
                    {
                        /* The frame may have completed before abort won. */
                        xStatus = FLEXCAN_STATUS_ERROR;
                    }
                }
            }
        }
    }

    return xStatus;
}

/* ========================================================================= */
/* Classical CAN Polling Reception                                           */
/* ========================================================================= */

bool FLEXCAN_IsRxReady(FLEXCAN_Type *pBase,
                       uint8_t u8MbIndex)
{
    bool bReady = false;
    uint8_t u8InstanceIndex = FLEXCAN_INSTANCE_INVALID;
    uint32_t u32FlagMask = 0U;

    if ((NULL != pBase) &&
        (true == FLEXCAN_IsSupportedBoardInstance(pBase)))
    {
        u8InstanceIndex = FLEXCAN_GetInstanceIndex(pBase);

        if ((true == s_axFlexcanState[u8InstanceIndex].bInitialized) &&
            (true == FLEXCAN_IsMbIndexValid(pBase, u8MbIndex)))
        {
            u32FlagMask = FLEXCAN_GetMbFlagMask(u8MbIndex);

            if (0U != (pBase->IFLAG1 & u32FlagMask))
            {
                bReady = true;
            }
        }
    }

    return bReady;
}

FLEXCAN_Status_t FLEXCAN_Receive(FLEXCAN_Type *pBase,
                                 uint8_t u8MbIndex,
                                 FLEXCAN_Frame_t *pFrame)
{
    FLEXCAN_Status_t xStatus = FLEXCAN_STATUS_OK;
    uint8_t u8InstanceIndex = FLEXCAN_INSTANCE_INVALID;
    uint8_t u8Code = 0U;
    uint8_t u8Dlc = 0U;
    uint8_t u8Index = 0U;
    uint32_t u32WordIndex = 0U;
    uint32_t u32FlagMask = 0U;
    uint32_t u32Cs = 0U;
    uint32_t u32IdWord = 0U;
    uint32_t u32Data0 = 0U;
    uint32_t u32Data1 = 0U;
    uint32_t u32Timeout = FLEXCAN_DEFAULT_TIMEOUT;
    uint16_t u16UnlockRead = 0U;

    if ((NULL == pBase) || (NULL == pFrame) ||
        (false == FLEXCAN_IsSupportedBoardInstance(pBase)))
    {
        xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        u8InstanceIndex = FLEXCAN_GetInstanceIndex(pBase);

        if (false == s_axFlexcanState[u8InstanceIndex].bInitialized)
        {
            xStatus = FLEXCAN_STATUS_NOT_INITIALIZED;
        }
        else if (false == FLEXCAN_IsMbIndexValid(pBase, u8MbIndex))
        {
            xStatus = FLEXCAN_STATUS_INVALID_ARGUMENT;
        }
        else
        {
            u32FlagMask = FLEXCAN_GetMbFlagMask(u8MbIndex);

            /* Poll reception status through IFLAG, never through CODE. */
            if (0U == (pBase->IFLAG1 & u32FlagMask))
            {
                xStatus = FLEXCAN_STATUS_NO_DATA;
            }
            else
            {
                u32WordIndex = FLEXCAN_GetMbWordIndex(u8MbIndex);

                /*
                 * Reading C/S locks FULL/OVERRUN Rx MBs. Retry while BUSY
                 * (CODE[0] = 1) indicates that FlexCAN is moving a frame in.
                 */
                do
                {
                    u32Cs = pBase->RAMn[u32WordIndex + FLEXCAN_MB_CS_WORD_OFFSET];
                    u8Code = FLEXCAN_GetMbCode(u32Cs);
                    u32Timeout--;
                } while ((0U != (u8Code & 0x1U)) && (0U < u32Timeout));

                if (0U != (u8Code & 0x1U))
                {
                    xStatus = FLEXCAN_STATUS_TIMEOUT;
                }
                else if ((FLEXCAN_MB_CODE_RX_FULL != u8Code) &&
                         (FLEXCAN_MB_CODE_RX_OVERRUN != u8Code))
                {
                    /* IFLAG was set, but the MB is not a received Rx frame. */
                    xStatus = FLEXCAN_STATUS_ERROR;
                }
                else
                {
                    u32IdWord = pBase->RAMn[u32WordIndex + FLEXCAN_MB_ID_WORD_OFFSET];
                    u32Data0 = pBase->RAMn[u32WordIndex + FLEXCAN_MB_DATA0_WORD_OFFSET];
                    u32Data1 = pBase->RAMn[u32WordIndex + FLEXCAN_MB_DATA1_WORD_OFFSET];

                    pFrame->u8MbIndex = u8MbIndex;
                    pFrame->u16Timestamp =
                        (uint16_t)(u32Cs & FLEXCAN_MB_CS_TIMESTAMP_MASK);
                    pFrame->bFD = false;
                    pFrame->bBitRateSwitch = false;
                    pFrame->bErrorStateIndicator = false;
                    pFrame->bOverrun =
                        (FLEXCAN_MB_CODE_RX_OVERRUN == u8Code);
                    pFrame->u8LocalPriority = 0U;

                    if (0U != (u32Cs & FLEXCAN_MB_CS_IDE_MASK))
                    {
                        pFrame->u8Format = FLEXCAN_FRAME_EXTENDED;
                        pFrame->u32Id = u32IdWord &
                                        FLEXCAN_MB_EXTENDED_ID_MASK;
                    }
                    else
                    {
                        pFrame->u8Format = FLEXCAN_FRAME_STANDARD;
                        pFrame->u32Id =
                            (u32IdWord & FLEXCAN_MB_STANDARD_ID_MASK) >>
                            FLEXCAN_MB_STANDARD_ID_SHIFT;
                    }

                    if (0U != (u32Cs & FLEXCAN_MB_CS_RTR_MASK))
                    {
                        pFrame->u8FrameType = FLEXCAN_FRAME_REMOTE;
                    }
                    else
                    {
                        pFrame->u8FrameType = FLEXCAN_FRAME_DATA;
                    }

                    u8Dlc = (uint8_t)((u32Cs & FLEXCAN_MB_CS_DLC_MASK) >>
                                      FLEXCAN_MB_CS_DLC_SHIFT);

                    if (FLEXCAN_CLASSIC_MAX_DATA_LENGTH < u8Dlc)
                    {
                        pFrame->u8Length = FLEXCAN_CLASSIC_MAX_DATA_LENGTH;
                    }
                    else
                    {
                        pFrame->u8Length = u8Dlc;
                    }

                    FLEXCAN_UnpackDataWord(u32Data0, &pFrame->au8Data[0U]);
                    FLEXCAN_UnpackDataWord(u32Data1, &pFrame->au8Data[4U]);

                    for (u8Index = pFrame->u8Length;
                         u8Index < FLEXCAN_FD_MAX_DATA_LENGTH;
                         u8Index++)
                    {
                        pFrame->au8Data[u8Index] = 0U;
                    }

                    /* Acknowledge only this MB flag (W1C). */
                    pBase->IFLAG1 = u32FlagMask;

                    /* TIMER read unlocks the Rx Message Buffer. */
                    u16UnlockRead =
                        (uint16_t)(pBase->TIMER & FLEXCAN_TIMER_TIMER_MASK);
                    (void)u16UnlockRead;
                }
            }
        }
    }

    return xStatus;
}