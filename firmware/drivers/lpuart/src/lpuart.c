#include "lpuart.h"
#include "board_pins.h"

/* ============================================================
 * Internal definitions
 * ============================================================ */

#define LPUART_PCC_CLK_SRC_SOSC_DIV2   (1U)
#define LPUART_OSR_VALUE               (15U)   /* 16x oversampling */
#define LPUART_DATA_MASK               (0xFFU)

/* ============================================================
 * Internal state (per instance)
 * ============================================================ */

static volatile char s_rxChar[3] = {0};
static volatile uint8_t s_rxFlag[3] = {0};

static LPUART_Callback_t s_rxCallback[3] = {0};

/* ============================================================
 * Helper: Map base to index
 * ============================================================ */
static uint8_t LPUART_GetInstanceIndex(LPUART_Type *base)
{
    if (base == IP_LPUART0) return 0U;
    if (base == IP_LPUART1) return 1U;
    if (base == IP_LPUART2) return 2U;
    return 0U;
}

/* ============================================================
 * Enable PORT clock
 * ============================================================ */
static void LPUART_EnablePortClock(PORT_Type *portBase)
{
    if (portBase == IP_PORTA) IP_PCC->PCCn[PCC_PORTA_INDEX] |= PCC_PCCn_CGC_MASK;
    else if (portBase == IP_PORTB) IP_PCC->PCCn[PCC_PORTB_INDEX] |= PCC_PCCn_CGC_MASK;
    else if (portBase == IP_PORTC) IP_PCC->PCCn[PCC_PORTC_INDEX] |= PCC_PCCn_CGC_MASK;
    else if (portBase == IP_PORTD) IP_PCC->PCCn[PCC_PORTD_INDEX] |= PCC_PCCn_CGC_MASK;
    else if (portBase == IP_PORTE) IP_PCC->PCCn[PCC_PORTE_INDEX] |= PCC_PCCn_CGC_MASK;
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
}

/* ============================================================
 * Enable module clock
 * ============================================================ */
static void LPUART_EnableModuleClock(LPUART_Type *base)
{
    uint32_t index = (base == IP_LPUART1) ? PCC_LPUART1_INDEX : PCC_LPUART2_INDEX;

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

    if ((base == 0) || (config == 0)) return LPUART_STATUS_INVALID_ARGUMENT;

    LPUART_ConfigPins(base);
    LPUART_EnableModuleClock(base);

    base->CTRL &= ~(LPUART_CTRL_TE_MASK | LPUART_CTRL_RE_MASK);

    sbr = config->srcClockHz / (config->baudRate * (LPUART_OSR_VALUE + 1U));

    base->BAUD &= ~(LPUART_BAUD_OSR_MASK | LPUART_BAUD_SBR_MASK);
    base->BAUD |= LPUART_BAUD_OSR(LPUART_OSR_VALUE) | LPUART_BAUD_SBR(sbr);

    base->CTRL |= (LPUART_CTRL_TE_MASK | LPUART_CTRL_RE_MASK);

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
    while (!LPUART_IsTxReady(base));
    base->DATA = (uint8_t)ch;
}

char LPUART_ReadChar(LPUART_Type *base)
{
    while (!LPUART_IsRxReady(base));
    return (char)(base->DATA & LPUART_DATA_MASK);
}

void LPUART_WriteString(LPUART_Type *base, const char *str)
{
    while (*str)
    {
        LPUART_WriteChar(base, *str++);
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
    uint8_t idx = LPUART_GetInstanceIndex(base);

    if (base->STAT & LPUART_STAT_RDRF_MASK)
    {
        s_rxChar[idx] = (char)(base->DATA & LPUART_DATA_MASK);
        s_rxFlag[idx] = 1U;

        if (s_rxCallback[idx] != 0)
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
    uint8_t idx = LPUART_GetInstanceIndex(base);
    s_rxCallback[idx] = callback;
}

/* ============================================================
 * Data access
 * ============================================================ */
char LPUART_GetChar(LPUART_Type *base)
{
    uint8_t idx = LPUART_GetInstanceIndex(base);
    return s_rxChar[idx];
}

bool LPUART_IsDataAvailable(LPUART_Type *base)
{
    uint8_t idx = LPUART_GetInstanceIndex(base);

    if (s_rxFlag[idx])
    {
        s_rxFlag[idx] = 0U;
        return true;
    }

    return false;
}
