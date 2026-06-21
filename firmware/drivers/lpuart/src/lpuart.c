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

/**
 * @brief LPUART oversampling ratio register value.
 *
 * @details
 * OSR = 15 means 16x oversampling because the hardware uses OSR + 1.
 */
#define LPUART_OSR_VALUE (15U)

/**
 * @brief Number of samples used by the configured oversampling mode.
 */
#define LPUART_OVERSAMPLING_COUNT (LPUART_OSR_VALUE + 1U)

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
static void LPUART_EnableModuleClock(LPUART_Type *pBase)
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
        /*
         * Disable the peripheral clock gate before changing PCS.
         * PCC clock source should be configured while CGC is disabled.
         */
        IP_PCC->PCCn[u32PccIndex] &= ~PCC_PCCn_CGC_MASK;

        /* Select SOSC_DIV2_CLK as LPUART functional clock source. */
        IP_PCC->PCCn[u32PccIndex] &= ~PCC_PCCn_PCS_MASK;
        IP_PCC->PCCn[u32PccIndex] |= PCC_PCCn_PCS(LPUART_PCC_CLK_SRC_SOSC_DIV2);

        /* Enable clock gate for LPUART registers. */
        IP_PCC->PCCn[u32PccIndex] |= PCC_PCCn_CGC_MASK;
    }
}

/**
 * @brief Initialize an LPUART peripheral instance.
 */
LPUART_Status_t LPUART_Init(LPUART_Type *pBase, const LPUART_Config_t *pConfig)
{
    LPUART_Status_t xStatus = LPUART_STATUS_OK;
    uint32_t u32Sbr = 0U;
    uint8_t u8InstanceIndex = LPUART_INSTANCE_INVALID;

    if ((LPUART_NULL_PTR == pBase) || (LPUART_NULL_PTR == pConfig))
    {
        xStatus = LPUART_STATUS_INVALID_ARGUMENT;
    }
    else if (false == LPUART_IsSupportedInstance(pBase))
    {
        xStatus = LPUART_STATUS_INVALID_ARGUMENT;
    }
    else if ((0U == pConfig->u32BaudRate) || (0U == pConfig->u32SrcClockHz))
    {
        xStatus = LPUART_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        LPUART_ConfigPins(pBase);
        LPUART_EnableModuleClock(pBase);

        pBase->CTRL &= ~(LPUART_CTRL_TE_MASK | LPUART_CTRL_RE_MASK);

        u32Sbr = pConfig->u32SrcClockHz /
                 (pConfig->u32BaudRate * LPUART_OVERSAMPLING_COUNT);

        pBase->BAUD &= ~(LPUART_BAUD_OSR_MASK | LPUART_BAUD_SBR_MASK);
        pBase->BAUD |= LPUART_BAUD_OSR(LPUART_OSR_VALUE) | LPUART_BAUD_SBR(u32Sbr);

        pBase->CTRL |= (LPUART_CTRL_TE_MASK | LPUART_CTRL_RE_MASK);

        u8InstanceIndex = LPUART_GetInstanceIndex(pBase);

        s_axRxBuffer[u8InstanceIndex].u8Head = 0U;
        s_axRxBuffer[u8InstanceIndex].u8Tail = 0U;
        s_apfRxCallback[u8InstanceIndex] = LPUART_NULL_CALLBACK;
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
