/**
 * @file lpuart.c
 * @brief Low Power UART driver implementation.
 *
 * @details
 * This file implements basic LPUART services for the NXP S32K144
 * microcontroller.
 *
 * The driver supports:
 * - LPUART module initialization.
 * - UART pin configuration for supported board instances.
 * - Polling transmit and receive APIs.
 * - RX interrupt handling.
 * - Software RX ring buffer.
 * - Optional RX callback.
 *
 * This module belongs to the Driver Layer and must not depend on the
 * Arduino-style API layer.
 */

#include "lpuart.h"
#include "board_pins.h"

/**
 * @brief Null pointer macro used locally in this driver.
 */
#define LPUART_NULL_PTR ((void *)0)

/**
 * @brief Null callback value used locally in this driver.
 */
#define LPUART_NULL_CALLBACK ((LPUART_Callback_t)0)

/**
 * @brief Number of LPUART instances available on S32K144.
 */
#define LPUART_DRIVER_INSTANCE_COUNT (3U)

/**
 * @brief Invalid LPUART instance index.
 */
#define LPUART_INSTANCE_INVALID (0xFFU)

/**
 * @brief LPUART0 software instance index.
 */
#define LPUART_INSTANCE_0 (0U)

/**
 * @brief LPUART1 software instance index.
 */
#define LPUART_INSTANCE_1 (1U)

/**
 * @brief LPUART2 software instance index.
 */
#define LPUART_INSTANCE_2 (2U)

/**
 * @brief PCC clock source value for SOSC_DIV2_CLK.
 */
#define LPUART_PCC_CLK_SRC_SOSC_DIV2 (1U)

/** @brief PCC clock source selection for SPLL_DIV2. */
#define LPUART_PCC_CLK_SRC_SPLL_DIV2 (6U)

/** @brief Functional clock frequencies of the existing MaaZEDU clock setup. */
#define LPUART_SOSC_DIV2_HZ (8000000U)
#define LPUART_SPLL_DIV2_HZ (40000000U)

/** @brief LPUART oversampling and SBR constraints. */
#define LPUART_OSR_MIN (4U)
#define LPUART_OSR_MAX (32U)
#define LPUART_SBR_MAX (8191U)

/** @brief Maximum accepted baud error: 3 percent. */
#define LPUART_MAX_BAUD_ERROR_PERCENT (3U)

/**
 * @brief Mask used to extract 8-bit received data from DATA register.
 */
#define LPUART_DATA_MASK (0xFFU)

/**
 * @brief PORT mux value used for LPUART pins on the MaaZEDU board.
 */
#define LPUART_PORT_MUX_ALT2 (2U)

/**
 * @brief RX ring buffer object.
 */
typedef struct
{
    uint8_t au8Data[LPUART_RX_BUFFER_SIZE];
    uint8_t u8Head;
    uint8_t u8Tail;
} LPUART_RingBuffer_t;

/**
 * @brief RX software buffers for each LPUART instance.
 */
static volatile LPUART_RingBuffer_t s_axRxBuffer[LPUART_DRIVER_INSTANCE_COUNT] = {0};

/**
 * @brief Optional RX callbacks for each LPUART instance.
 */
static LPUART_Callback_t s_apfRxCallback[LPUART_DRIVER_INSTANCE_COUNT] = {0};

/**
 * @brief Convert LPUART base address to software instance index.
 *
 * @param[in] pBase
 * Pointer to LPUART peripheral instance.
 *
 * @return uint8_t
 * Software instance index, or LPUART_INSTANCE_INVALID if unsupported.
 */
static uint8_t LPUART_GetInstanceIndex(LPUART_Type *pBase)
{
    uint8_t u8Index = LPUART_INSTANCE_INVALID;

    if (IP_LPUART0 == pBase)
    {
        u8Index = LPUART_INSTANCE_0;
    }
    else if (IP_LPUART1 == pBase)
    {
        u8Index = LPUART_INSTANCE_1;
    }
    else if (IP_LPUART2 == pBase)
    {
        u8Index = LPUART_INSTANCE_2;
    }
    else
    {
        /* Unsupported LPUART base address. */
    }

    return u8Index;
}

/**
 * @brief Check whether an LPUART instance is supported by board mapping.
 *
 * @details
 * The current MaaZEDU board mapping supports LPUART1 and LPUART2.
 * LPUART0 exists in the MCU family but is not configured by this driver.
 *
 * @param[in] pBase
 * Pointer to LPUART peripheral instance.
 *
 * @return bool
 *
 * @retval true
 * Instance is supported.
 *
 * @retval false
 * Instance is not supported.
 */
static bool LPUART_IsSupportedInstance(LPUART_Type *pBase)
{
    bool bIsSupported = false;

    if ((IP_LPUART1 == pBase) || (IP_LPUART2 == pBase))
    {
        bIsSupported = true;
    }

    return bIsSupported;
}

/**
 * @brief Calculate next RX ring buffer index.
 *
 * @param[in] u8Index
 * Current buffer index.
 *
 * @return uint8_t
 * Next buffer index with wrap-around.
 */
static uint8_t LPUART_RxBufferNextIndex(uint8_t u8Index)
{
    uint8_t u8NextIndex = u8Index + 1U;

    if (LPUART_RX_BUFFER_SIZE <= u8NextIndex)
    {
        u8NextIndex = 0U;
    }

    return u8NextIndex;
}

/**
 * @brief Check whether RX ring buffer is empty.
 *
 * @param[in] pxRingBuffer
 * Pointer to RX ring buffer.
 *
 * @return bool
 *
 * @retval true
 * Buffer is empty.
 *
 * @retval false
 * Buffer contains data.
 */
static bool LPUART_RxBufferIsEmpty(volatile LPUART_RingBuffer_t *pxRingBuffer)
{
    bool bIsEmpty = true;

    if (LPUART_NULL_PTR != pxRingBuffer)
    {
        if (pxRingBuffer->u8Head != pxRingBuffer->u8Tail)
        {
            bIsEmpty = false;
        }
    }

    return bIsEmpty;
}

/**
 * @brief Push one byte into RX ring buffer.
 *
 * @details
 * If the buffer is full, the new byte is discarded. This keeps ISR logic
 * short and avoids overwriting unread data.
 *
 * @param[in,out] pxRingBuffer
 * Pointer to RX ring buffer.
 *
 * @param[in] u8Data
 * Byte to push.
 *
 * @return None.
 */
static void LPUART_RxBufferPush(volatile LPUART_RingBuffer_t *pxRingBuffer,
                                uint8_t u8Data)
{
    uint8_t u8NextHead = 0U;

    if (LPUART_NULL_PTR != pxRingBuffer)
    {
        u8NextHead = LPUART_RxBufferNextIndex(pxRingBuffer->u8Head);

        if (u8NextHead != pxRingBuffer->u8Tail)
        {
            pxRingBuffer->au8Data[pxRingBuffer->u8Head] = u8Data;
            pxRingBuffer->u8Head = u8NextHead;
        }
        else
        {
            /*
             * Buffer full.
             * New data is intentionally discarded to preserve unread data.
             */
        }
    }
}

/**
 * @brief Pop one byte from RX ring buffer.
 *
 * @param[in,out] pxRingBuffer
 * Pointer to RX ring buffer.
 *
 * @param[out] pu8Data
 * Pointer to received byte storage.
 *
 * @return bool
 *
 * @retval true
 * One byte was read.
 *
 * @retval false
 * Buffer is empty or argument is invalid.
 */
static bool LPUART_RxBufferPop(volatile LPUART_RingBuffer_t *pxRingBuffer,
                               uint8_t *pu8Data)
{
    bool bResult = false;

    if ((LPUART_NULL_PTR != pxRingBuffer) && (LPUART_NULL_PTR != pu8Data))
    {
        if (false == LPUART_RxBufferIsEmpty(pxRingBuffer))
        {
            *pu8Data = pxRingBuffer->au8Data[pxRingBuffer->u8Tail];
            pxRingBuffer->u8Tail = LPUART_RxBufferNextIndex(pxRingBuffer->u8Tail);
            bResult = true;
        }
    }

    return bResult;
}

/**
 * @brief Enable clock for a PORT peripheral.
 *
 * @param[in] pPortBase
 * Pointer to PORT peripheral instance.
 *
 * @return None.
 */
static void LPUART_EnablePortClock(PORT_Type *pPortBase)
{
    if (IP_PORTA == pPortBase)
    {
        IP_PCC->PCCn[PCC_PORTA_INDEX] |= PCC_PCCn_CGC_MASK;
    }
    else if (IP_PORTB == pPortBase)
    {
        IP_PCC->PCCn[PCC_PORTB_INDEX] |= PCC_PCCn_CGC_MASK;
    }
    else if (IP_PORTC == pPortBase)
    {
        IP_PCC->PCCn[PCC_PORTC_INDEX] |= PCC_PCCn_CGC_MASK;
    }
    else if (IP_PORTD == pPortBase)
    {
        IP_PCC->PCCn[PCC_PORTD_INDEX] |= PCC_PCCn_CGC_MASK;
    }
    else if (IP_PORTE == pPortBase)
    {
        IP_PCC->PCCn[PCC_PORTE_INDEX] |= PCC_PCCn_CGC_MASK;
    }
    else
    {
        /* Invalid PORT base address. */
    }
}

/**
 * @brief Configure board pins for the selected LPUART instance.
 *
 * @param[in] pBase
 * Pointer to LPUART peripheral instance.
 *
 * @return None.
 */
static void LPUART_ConfigPins(LPUART_Type *pBase)
{
    if (IP_LPUART1 == pBase)
    {
        LPUART_EnablePortClock(BOARD_UART1_RX_PORT);
        LPUART_EnablePortClock(BOARD_UART1_TX_PORT);

        BOARD_UART1_RX_PORT->PCR[BOARD_UART1_RX_PIN] = PORT_PCR_MUX(LPUART_PORT_MUX_ALT2);
        BOARD_UART1_TX_PORT->PCR[BOARD_UART1_TX_PIN] = PORT_PCR_MUX(LPUART_PORT_MUX_ALT2);
    }
    else if (IP_LPUART2 == pBase)
    {
        LPUART_EnablePortClock(BOARD_UART2_RX_PORT);
        LPUART_EnablePortClock(BOARD_UART2_TX_PORT);

        BOARD_UART2_RX_PORT->PCR[BOARD_UART2_RX_PIN] = PORT_PCR_MUX(LPUART_PORT_MUX_ALT2);
        BOARD_UART2_TX_PORT->PCR[BOARD_UART2_TX_PIN] = PORT_PCR_MUX(LPUART_PORT_MUX_ALT2);
    }
    else
    {
        /* Unsupported LPUART instance for current board mapping. */
    }
}

/**
 * @brief Enable module clock for the selected LPUART instance.
 *
 * @param[in] pBase
 * Pointer to LPUART peripheral instance.
 *
 * @return None.
 */
static void LPUART_EnableModuleClock(LPUART_Type *pBase, uint32_t u32Pcs)
{
    uint32_t u32PccIndex = 0U;
    bool bIsValid = true;

    if (IP_LPUART1 == pBase)
    {
        u32PccIndex = PCC_LPUART1_INDEX;
    }
    else if (IP_LPUART2 == pBase)
    {
        u32PccIndex = PCC_LPUART2_INDEX;
    }
    else
    {
        bIsValid = false;
    }

    if (true == bIsValid)
    {
        IP_PCC->PCCn[u32PccIndex] &= ~PCC_PCCn_CGC_MASK;
        IP_PCC->PCCn[u32PccIndex] &= ~PCC_PCCn_PCS_MASK;
        IP_PCC->PCCn[u32PccIndex] |= PCC_PCCn_PCS(u32Pcs);
        IP_PCC->PCCn[u32PccIndex] |= PCC_PCCn_CGC_MASK;
    }
}

/**
 * @brief Select an already configured board clock without changing SCG.
 * @details The frequencies are valid for clock.c supplied with MaaZEDU.
 *          Reject unavailable sources rather than silently using a wrong baud.
 */
static LPUART_Status_t LPUART_SelectClock(uint32_t u32ClockHz, uint32_t *pu32Pcs)
{
    LPUART_Status_t xStatus = LPUART_STATUS_INVALID_ARGUMENT;

    if (LPUART_NULL_PTR != pu32Pcs)
    {
        if ((LPUART_SOSC_DIV2_HZ == u32ClockHz) &&
            (0U != (IP_SCG->SOSCCSR & SCG_SOSCCSR_SOSCVLD_MASK)) &&
            (0x00000100U == (IP_SCG->SOSCDIV & 0x00000700U)))
        {
            *pu32Pcs = LPUART_PCC_CLK_SRC_SOSC_DIV2;
            xStatus = LPUART_STATUS_OK;
        }
        else if ((LPUART_SPLL_DIV2_HZ == u32ClockHz) &&
                 (0U != (IP_SCG->SPLLCSR & SCG_SPLLCSR_SPLLVLD_MASK)) &&
                 (0x00000300U == (IP_SCG->SPLLDIV & 0x00000700U)))
        {
            *pu32Pcs = LPUART_PCC_CLK_SRC_SPLL_DIV2;
            xStatus = LPUART_STATUS_OK;
        }
        else
        {
            /* Source unavailable or does not match the board clock setup. */
        }
    }

    return xStatus;
}

/**
 * @brief Find the lowest-error valid OSR/SBR pair using 64-bit arithmetic.
 * @details Compare exact rational errors, not rounded baud-rate integers.
 *          Prefer higher oversampling when errors are equal.
 */
static LPUART_Status_t LPUART_CalculateBaud(uint32_t u32ClockHz,
                                            uint32_t u32BaudRate,
                                            uint32_t *pu32Osr,
                                            uint32_t *pu32Sbr)
{
    LPUART_Status_t xStatus = LPUART_STATUS_INVALID_ARGUMENT;
    uint32_t u32Osr = 0U;
    uint32_t u32CandidateSbr = 0U;
    uint32_t u32BestOsr = 0U;
    uint32_t u32BestSbr = 0U;
    uint64_t u64Denominator = 0U;
    uint64_t u64ActualDenominator = 0U;
    uint64_t u64Difference = 0U;
    uint64_t u64BestDifference = 0U;
    uint64_t u64BestDenominator = 1U;
    uint64_t u64TargetProduct = 0U;

    if ((0U != u32ClockHz) && (0U != u32BaudRate) &&
        (LPUART_NULL_PTR != pu32Osr) && (LPUART_NULL_PTR != pu32Sbr))
    {
        for (u32Osr = LPUART_OSR_MIN; u32Osr <= LPUART_OSR_MAX; u32Osr++)
        {
            u64Denominator = (uint64_t)u32BaudRate * (uint64_t)u32Osr;
            /* Reject out-of-range divisors before narrowing to uint32_t. */
            if (((uint64_t)u32ClockHz + (u64Denominator / 2U)) /
                u64Denominator <= (uint64_t)LPUART_SBR_MAX)
            {
                u32CandidateSbr = (uint32_t)(((uint64_t)u32ClockHz +
                                             (u64Denominator / 2U)) / u64Denominator);
            }
            else
            {
                u32CandidateSbr = 0U;
            }

            if ((1U <= u32CandidateSbr) && (LPUART_SBR_MAX >= u32CandidateSbr))
            {
                u64ActualDenominator = (uint64_t)u32Osr * (uint64_t)u32CandidateSbr;
                u64TargetProduct = (uint64_t)u32BaudRate * u64ActualDenominator;
                if ((uint64_t)u32ClockHz >= u64TargetProduct)
                {
                    u64Difference = (uint64_t)u32ClockHz - u64TargetProduct;
                }
                else
                {
                    u64Difference = u64TargetProduct - (uint64_t)u32ClockHz;
                }

                if ((0U == u32BestOsr) ||
                    ((u64Difference * u64BestDenominator) <
                     (u64BestDifference * u64ActualDenominator)) ||
                    (((u64Difference * u64BestDenominator) ==
                      (u64BestDifference * u64ActualDenominator)) &&
                     (u32Osr > u32BestOsr)))
                {
                    u32BestOsr = u32Osr;
                    u32BestSbr = u32CandidateSbr;
                    u64BestDifference = u64Difference;
                    u64BestDenominator = u64ActualDenominator;
                }
            }
        }

        if ((0U != u32BestOsr) &&
            ((u64BestDifference * 100U) <=
             ((uint64_t)LPUART_MAX_BAUD_ERROR_PERCENT *
              (uint64_t)u32BaudRate * u64BestDenominator)))
        {
            *pu32Osr = u32BestOsr;
            *pu32Sbr = u32BestSbr;
            xStatus = LPUART_STATUS_OK;
        }
    }

    return xStatus;
}

/**
 * @brief Initialize an LPUART peripheral instance.
 */
LPUART_Status_t LPUART_Init(LPUART_Type *pBase, const LPUART_Config_t *pConfig)
{
    LPUART_Status_t xStatus = LPUART_STATUS_INVALID_ARGUMENT;
    uint32_t u32Osr = 0U;
    uint32_t u32Sbr = 0U;
    uint32_t u32Pcs = 0U;
    uint32_t u32BaudRegister = 0U;
    uint8_t u8InstanceIndex = LPUART_INSTANCE_INVALID;

    if ((LPUART_NULL_PTR != pBase) && (LPUART_NULL_PTR != pConfig) &&
        (true == LPUART_IsSupportedInstance(pBase)) &&
        (0U != pConfig->u32BaudRate))
    {
        xStatus = LPUART_SelectClock(pConfig->u32SrcClockHz, &u32Pcs);

        if (LPUART_STATUS_OK == xStatus)
        {
            xStatus = LPUART_CalculateBaud(pConfig->u32SrcClockHz,
                                          pConfig->u32BaudRate,
                                          &u32Osr, &u32Sbr);
        }

        if (LPUART_STATUS_OK == xStatus)
        {
            LPUART_ConfigPins(pBase);
            LPUART_EnableModuleClock(pBase, u32Pcs);
            pBase->CTRL &= ~(LPUART_CTRL_TE_MASK | LPUART_CTRL_RE_MASK |
                             LPUART_CTRL_RIE_MASK);

            u32BaudRegister = pBase->BAUD;
            u32BaudRegister &= ~(LPUART_BAUD_OSR_MASK |
                                 LPUART_BAUD_SBR_MASK |
                                 LPUART_BAUD_BOTHEDGE_MASK);
            u32BaudRegister |= LPUART_BAUD_OSR(u32Osr - 1U) |
                               LPUART_BAUD_SBR(u32Sbr);
            if (u32Osr <= 7U)
            {
                u32BaudRegister |= LPUART_BAUD_BOTHEDGE_MASK;
            }
            pBase->BAUD = u32BaudRegister;

            u8InstanceIndex = LPUART_GetInstanceIndex(pBase);
            s_axRxBuffer[u8InstanceIndex].u8Head = 0U;
            s_axRxBuffer[u8InstanceIndex].u8Tail = 0U;
            s_apfRxCallback[u8InstanceIndex] = LPUART_NULL_CALLBACK;

            pBase->CTRL |= (LPUART_CTRL_TE_MASK | LPUART_CTRL_RE_MASK);
        }
    }

    return xStatus;
}

/**
 * @brief Check whether the transmit data register is ready.
 */
bool LPUART_IsTxReady(LPUART_Type *pBase)
{
    bool bIsReady = false;

    if (LPUART_NULL_PTR != pBase)
    {
        if (0U != (pBase->STAT & LPUART_STAT_TDRE_MASK))
        {
            bIsReady = true;
        }
    }

    return bIsReady;
}

/**
 * @brief Check whether received data is available in the hardware register.
 */
bool LPUART_IsRxReady(LPUART_Type *pBase)
{
    bool bIsReady = false;

    if (LPUART_NULL_PTR != pBase)
    {
        if (0U != (pBase->STAT & LPUART_STAT_RDRF_MASK))
        {
            bIsReady = true;
        }
    }

    return bIsReady;
}

/**
 * @brief Write one character using polling mode.
 */
void LPUART_WriteChar(LPUART_Type *pBase, char cCharacter)
{
    if (LPUART_NULL_PTR != pBase)
    {
        while (false == LPUART_IsTxReady(pBase))
        {
            /* Wait until transmit data register is ready. */
        }

        pBase->DATA = (uint8_t)cCharacter;
    }
}

/**
 * @brief Read one character using polling mode.
 */
char LPUART_ReadChar(LPUART_Type *pBase)
{
    char cCharacter = '\0';

    if (LPUART_NULL_PTR != pBase)
    {
        while (false == LPUART_IsRxReady(pBase))
        {
            /* Wait until receive data flag is set. */
        }

        cCharacter = (char)(pBase->DATA & LPUART_DATA_MASK);
    }

    return cCharacter;
}

/**
 * @brief Write a null-terminated string using polling mode.
 */
void LPUART_WriteString(LPUART_Type *pBase, const char *pcString)
{
    const char *pcCurrent = pcString;

    if ((LPUART_NULL_PTR != pBase) && (LPUART_NULL_PTR != pcCurrent))
    {
        while ('\0' != *pcCurrent)
        {
            LPUART_WriteChar(pBase, *pcCurrent);
            pcCurrent++;
        }
    }
}

/**
 * @brief Enable LPUART receive interrupt.
 */
void LPUART_EnableRxInterrupt(LPUART_Type *pBase)
{
    if (LPUART_NULL_PTR != pBase)
    {
        pBase->CTRL |= LPUART_CTRL_RIE_MASK;
    }
}

/**
 * @brief Disable LPUART receive interrupt.
 */
void LPUART_DisableRxInterrupt(LPUART_Type *pBase)
{
    if (LPUART_NULL_PTR != pBase)
    {
        pBase->CTRL &= ~LPUART_CTRL_RIE_MASK;
    }
}

/**
 * @brief LPUART interrupt handler called by the IRQ layer.
 */
void LPUART_IRQHandler(LPUART_Type *pBase)
{
    uint8_t u8InstanceIndex = LPUART_INSTANCE_INVALID;
    uint8_t u8RxData = 0U;

    if (LPUART_NULL_PTR != pBase)
    {
        u8InstanceIndex = LPUART_GetInstanceIndex(pBase);

        if (LPUART_INSTANCE_INVALID != u8InstanceIndex)
        {
            if (0U != (pBase->STAT & LPUART_STAT_RDRF_MASK))
            {
                u8RxData = (uint8_t)(pBase->DATA & LPUART_DATA_MASK);

                LPUART_RxBufferPush(&s_axRxBuffer[u8InstanceIndex], u8RxData);

                if (LPUART_NULL_CALLBACK != s_apfRxCallback[u8InstanceIndex])
                {
                    s_apfRxCallback[u8InstanceIndex]();
                }
            }
        }
    }
}

/**
 * @brief Register RX callback for an LPUART instance.
 */
void LPUART_SetRxCallback(LPUART_Type *pBase, LPUART_Callback_t pfCallback)
{
    uint8_t u8InstanceIndex = LPUART_INSTANCE_INVALID;

    if (LPUART_NULL_PTR != pBase)
    {
        u8InstanceIndex = LPUART_GetInstanceIndex(pBase);

        if (LPUART_INSTANCE_INVALID != u8InstanceIndex)
        {
            s_apfRxCallback[u8InstanceIndex] = pfCallback;
        }
    }
}

/**
 * @brief Get one character from the internal RX ring buffer.
 */
char LPUART_GetChar(LPUART_Type *pBase)
{
    uint8_t u8InstanceIndex = LPUART_INSTANCE_INVALID;
    uint8_t u8Data = 0U;
    char cCharacter = '\0';

    if (LPUART_NULL_PTR != pBase)
    {
        u8InstanceIndex = LPUART_GetInstanceIndex(pBase);

        if (LPUART_INSTANCE_INVALID != u8InstanceIndex)
        {
            if (true == LPUART_RxBufferPop(&s_axRxBuffer[u8InstanceIndex], &u8Data))
            {
                cCharacter = (char)u8Data;
            }
        }
    }

    return cCharacter;
}

/**
 * @brief Check whether software RX buffer contains data.
 */
bool LPUART_IsDataAvailable(LPUART_Type *pBase)
{
    uint8_t u8InstanceIndex = LPUART_INSTANCE_INVALID;
    bool bIsAvailable = false;

    if (LPUART_NULL_PTR != pBase)
    {
        u8InstanceIndex = LPUART_GetInstanceIndex(pBase);

        if (LPUART_INSTANCE_INVALID != u8InstanceIndex)
        {
            if (false == LPUART_RxBufferIsEmpty(&s_axRxBuffer[u8InstanceIndex]))
            {
                bIsAvailable = true;
            }
        }
    }

    return bIsAvailable;
}
