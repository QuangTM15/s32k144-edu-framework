#include "lpuart.h"
#include "board_pins.h"

/* ============================================================
 * Internal definitions
 * ============================================================ */

#define LPUART_PCC_CLK_SRC_SOSC_DIV2   (1U)
#define LPUART_OSR_VALUE               (15U)   /* 16x oversampling */
#define LPUART_DATA_MASK               (0xFFU)

/* ============================================================
 * Internal types
 * ============================================================ */

typedef struct
{
    uint8_t data[LPUART_RX_BUFFER_SIZE];
    uint8_t head;
    uint8_t tail;
} LPUART_RingBuffer_t;

/* ============================================================
 * Internal state (per instance)
 * ============================================================ */

static volatile LPUART_RingBuffer_t s_rxBuffer[3] = {0};
static LPUART_Callback_t s_rxCallback[3] = {0};

/* ============================================================
 * Helper: Map base to index
 * ============================================================ */

static uint8_t LPUART_GetInstanceIndex(LPUART_Type *base)
{
    if (base == IP_LPUART0)
    {
        return 0U;
    }
    if (base == IP_LPUART1)
    {
        return 1U;
    }
    if (base == IP_LPUART2)
    {
        return 2U;
    }

    return 0U;
}

/* ============================================================
 * Helper: Ring buffer
 * ============================================================ */

static uint8_t LPUART_RxBufferNextIndex(uint8_t index)
{
    index++;

    if (index >= LPUART_RX_BUFFER_SIZE)
    {
        index = 0U;
    }

    return index;
}

static bool LPUART_RxBufferIsEmpty(volatile LPUART_RingBuffer_t *rb)
{
    return (rb->head == rb->tail);
}

static void LPUART_RxBufferPush(volatile LPUART_RingBuffer_t *rb, uint8_t data)
{
    uint8_t nextHead;

    nextHead = LPUART_RxBufferNextIndex(rb->head);

    if (nextHead == rb->tail)
    {
        /* Buffer full: discard new byte */
        return;
    }

    rb->data[rb->head] = data;
    rb->head = nextHead;
}

static bool LPUART_RxBufferPop(volatile LPUART_RingBuffer_t *rb, uint8_t *data)
{
    if ((rb == 0) || (data == 0))
    {
        return false;
    }

    if (LPUART_RxBufferIsEmpty(rb))
    {
        return false;
    }

    *data = rb->data[rb->tail];
    rb->tail = LPUART_RxBufferNextIndex(rb->tail);

    return true;
}

/* ============================================================
 * Enable PORT clock
 * ============================================================ */

static void LPUART_EnablePortClock(PORT_Type *portBase)
{
    if (portBase == IP_PORTA)
    {
        IP_PCC->PCCn[PCC_PORTA_INDEX] |= PCC_PCCn_CGC_MASK;
    }
    else if (portBase == IP_PORTB)
    {
        IP_PCC->PCCn[PCC_PORTB_INDEX] |= PCC_PCCn_CGC_MASK;
    }
    else if (portBase == IP_PORTC)
    {
        IP_PCC->PCCn[PCC_PORTC_INDEX] |= PCC_PCCn_CGC_MASK;
    }
    else if (portBase == IP_PORTD)
    {
        IP_PCC->PCCn[PCC_PORTD_INDEX] |= PCC_PCCn_CGC_MASK;
    }
    else if (portBase == IP_PORTE)
    {
        IP_PCC->PCCn[PCC_PORTE_INDEX] |= PCC_PCCn_CGC_MASK;
    }
    else
    {
        /* Invalid port base: do nothing */
    }
}

/* ============================================================
 * Configure UART pins
 * ============================================================ */

static void LPUART_ConfigPins(LPUART_Type *base)
{
    if (base == IP_LPUART1)
    {
        LPUART_EnablePortClock(BOARD_UART1_RX_PORT);
        LPUART_EnablePortClock(BOARD_UART1_TX_PORT);

        BOARD_UART1_RX_PORT->PCR[BOARD_UART1_RX_PIN] = PORT_PCR_MUX(2U);
        BOARD_UART1_TX_PORT->PCR[BOARD_UART1_TX_PIN] = PORT_PCR_MUX(2U);
    }
    else if (base == IP_LPUART2)
    {
        LPUART_EnablePortClock(BOARD_UART2_RX_PORT);
        LPUART_EnablePortClock(BOARD_UART2_TX_PORT);

        BOARD_UART2_RX_PORT->PCR[BOARD_UART2_RX_PIN] = PORT_PCR_MUX(2U);
        BOARD_UART2_TX_PORT->PCR[BOARD_UART2_TX_PIN] = PORT_PCR_MUX(2U);
    }
    else
    {
        /* Unsupported instance: do nothing */
    }
}

/* ============================================================
 * Enable module clock
 * ============================================================ */

static void LPUART_EnableModuleClock(LPUART_Type *base)
{
    uint32_t index;

    if (base == IP_LPUART1)
    {
        index = PCC_LPUART1_INDEX;
    }
    else if (base == IP_LPUART2)
    {
        index = PCC_LPUART2_INDEX;
    }
    else
    {
        return;
    }

    IP_PCC->PCCn[index] &= ~PCC_PCCn_CGC_MASK;
    IP_PCC->PCCn[index] &= ~PCC_PCCn_PCS_MASK;
    IP_PCC->PCCn[index] |= PCC_PCCn_PCS(LPUART_PCC_CLK_SRC_SOSC_DIV2);
    IP_PCC->PCCn[index] |= PCC_PCCn_CGC_MASK;
}

/* ============================================================
 * Initialization
 * ============================================================ */

LPUART_Status_t LPUART_Init(LPUART_Type *base, const LPUART_Config_t *config)
{
    uint32_t sbr;
    uint8_t idx;

    if ((base == 0) || (config == 0))
    {
        return LPUART_STATUS_INVALID_ARGUMENT;
    }

    if ((config->baudRate == 0U) || (config->srcClockHz == 0U))
    {
        return LPUART_STATUS_INVALID_ARGUMENT;
    }

    LPUART_ConfigPins(base);
    LPUART_EnableModuleClock(base);

    base->CTRL &= ~(LPUART_CTRL_TE_MASK | LPUART_CTRL_RE_MASK);

    sbr = config->srcClockHz / (config->baudRate * (LPUART_OSR_VALUE + 1U));

    base->BAUD &= ~(LPUART_BAUD_OSR_MASK | LPUART_BAUD_SBR_MASK);
    base->BAUD |= LPUART_BAUD_OSR(LPUART_OSR_VALUE) | LPUART_BAUD_SBR(sbr);

    base->CTRL |= (LPUART_CTRL_TE_MASK | LPUART_CTRL_RE_MASK);

    idx = LPUART_GetInstanceIndex(base);
    s_rxBuffer[idx].head = 0U;
    s_rxBuffer[idx].tail = 0U;
    s_rxCallback[idx] = (LPUART_Callback_t)0;

    return LPUART_STATUS_OK;
}

/* ============================================================
 * Polling APIs
 * ============================================================ */

bool LPUART_IsTxReady(LPUART_Type *base)
{
    return ((base->STAT & LPUART_STAT_TDRE_MASK) != 0U);
}

bool LPUART_IsRxReady(LPUART_Type *base)
{
    return ((base->STAT & LPUART_STAT_RDRF_MASK) != 0U);
}

void LPUART_WriteChar(LPUART_Type *base, char ch)
{
    while (!LPUART_IsTxReady(base))
    {
    }

    base->DATA = (uint8_t)ch;
}

char LPUART_ReadChar(LPUART_Type *base)
{
    while (!LPUART_IsRxReady(base))
    {
    }

    return (char)(base->DATA & LPUART_DATA_MASK);
}

void LPUART_WriteString(LPUART_Type *base, const char *str)
{
    if (str == 0)
    {
        return;
    }

    while (*str != '\0')
    {
        LPUART_WriteChar(base, *str);
        str++;
    }
}

/* ============================================================
 * Interrupt control
 * ============================================================ */

void LPUART_EnableRxInterrupt(LPUART_Type *base)
{
    base->CTRL |= LPUART_CTRL_RIE_MASK;
}

void LPUART_DisableRxInterrupt(LPUART_Type *base)
{
    base->CTRL &= ~LPUART_CTRL_RIE_MASK;
}

/* ============================================================
 * Interrupt handler (called from IRQ layer)
 * ============================================================ */

void LPUART_IRQHandler(LPUART_Type *base)
{
    uint8_t idx;
    uint8_t rxData;

    idx = LPUART_GetInstanceIndex(base);

    if ((base->STAT & LPUART_STAT_RDRF_MASK) != 0U)
    {
        rxData = (uint8_t)(base->DATA & LPUART_DATA_MASK);
        LPUART_RxBufferPush(&s_rxBuffer[idx], rxData);

        if (s_rxCallback[idx] != (LPUART_Callback_t)0)
        {
            s_rxCallback[idx]();
        }
    }
}

/* ============================================================
 * Callback
 * ============================================================ */

void LPUART_SetRxCallback(LPUART_Type *base, LPUART_Callback_t callback)
{
    uint8_t idx;

    idx = LPUART_GetInstanceIndex(base);
    s_rxCallback[idx] = callback;
}

/* ============================================================
 * RX buffer access
 * ============================================================ */

char LPUART_GetChar(LPUART_Type *base)
{
    uint8_t idx;
    uint8_t data = 0U;

    idx = LPUART_GetInstanceIndex(base);

    if (LPUART_RxBufferPop(&s_rxBuffer[idx], &data))
    {
        return (char)data;
    }

    return '\0';
}

bool LPUART_IsDataAvailable(LPUART_Type *base)
{
    uint8_t idx;

    idx = LPUART_GetInstanceIndex(base);
    return !LPUART_RxBufferIsEmpty(&s_rxBuffer[idx]);
}

