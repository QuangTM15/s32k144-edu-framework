#include "lpuart.h"
#include "board_pins.h"

#define LPUART_PCC_CLK_SRC_SOSC_DIV2   (1U)
#define LPUART_OSR_VALUE               (15U)   /* Oversampling ratio = OSR + 1 = 16 */
#define LPUART_DATA_MASK               (0xFFU)

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
        /* Do nothing */
    }
}

static void LPUART_ConfigPins(LPUART_Type *base)
{
    if (base == IP_LPUART1)
    {
        LPUART_EnablePortClock(BOARD_UART1_RX_PORT);
        LPUART_EnablePortClock(BOARD_UART1_TX_PORT);

        /* PTC6 -> LPUART1_RX, PTC7 -> LPUART1_TX */
        BOARD_UART1_RX_PORT->PCR[BOARD_UART1_RX_PIN] = PORT_PCR_MUX(2U);
        BOARD_UART1_TX_PORT->PCR[BOARD_UART1_TX_PIN] = PORT_PCR_MUX(2U);
    }
    else if (base == IP_LPUART2)
    {
        LPUART_EnablePortClock(BOARD_UART2_RX_PORT);
        LPUART_EnablePortClock(BOARD_UART2_TX_PORT);

        /* PTD6 -> LPUART2_RX, PTD7 -> LPUART2_TX */
        BOARD_UART2_RX_PORT->PCR[BOARD_UART2_RX_PIN] = PORT_PCR_MUX(2U);
        BOARD_UART2_TX_PORT->PCR[BOARD_UART2_TX_PIN] = PORT_PCR_MUX(2U);
    }
    else
    {
        /* Do nothing */
    }
}

static void LPUART_EnableModuleClock(LPUART_Type *base)
{
    if (base == IP_LPUART1)
    {
        /* Disable CGC before changing PCS */
    	/* 1. Disable clock */
    	IP_PCC->PCCn[PCC_LPUART1_INDEX] &= ~PCC_PCCn_CGC_MASK;
    	/* 2. Clear PCS field */
    	IP_PCC->PCCn[PCC_LPUART1_INDEX] &= ~PCC_PCCn_PCS_MASK;
    	/* 3. Set PCS */
    	IP_PCC->PCCn[PCC_LPUART1_INDEX] |= PCC_PCCn_PCS(LPUART_PCC_CLK_SRC_SOSC_DIV2);
    	/* 4. Enable clock */
    	IP_PCC->PCCn[PCC_LPUART1_INDEX] |= PCC_PCCn_CGC_MASK;
    }
    else if (base == IP_LPUART2)
    {
    	IP_PCC->PCCn[PCC_LPUART2_INDEX] &= ~PCC_PCCn_CGC_MASK;
    	IP_PCC->PCCn[PCC_LPUART2_INDEX] &= ~PCC_PCCn_PCS_MASK;
    	IP_PCC->PCCn[PCC_LPUART2_INDEX] |= PCC_PCCn_PCS(LPUART_PCC_CLK_SRC_SOSC_DIV2);
    	IP_PCC->PCCn[PCC_LPUART2_INDEX] |= PCC_PCCn_CGC_MASK;
    }
    else
    {
        /* Do nothing */
    }
}

static LPUART_Status_t LPUART_ValidateConfig(LPUART_Type *base, const LPUART_Config_t *config)
{
    if ((base == (LPUART_Type *)0) || (config == (const LPUART_Config_t *)0))
    {
        return LPUART_STATUS_INVALID_ARGUMENT;
    }

    if (config->baudRate == 0U)
    {
        return LPUART_STATUS_INVALID_ARGUMENT;
    }

    if (config->srcClockHz == 0U)
    {
        return LPUART_STATUS_INVALID_ARGUMENT;
    }

    if ((base != IP_LPUART1) && (base != IP_LPUART2))
    {
        return LPUART_STATUS_INVALID_ARGUMENT;
    }

    return LPUART_STATUS_OK;
}

LPUART_Status_t LPUART_Init(LPUART_Type *base, const LPUART_Config_t *config)
{
    uint32_t sbr;
    LPUART_Status_t status;

    status = LPUART_ValidateConfig(base, config);
    if (status != LPUART_STATUS_OK)
    {
        return status;
    }

    LPUART_ConfigPins(base);
    LPUART_EnableModuleClock(base);

    /* Disable TX/RX before configuration */
    base->CTRL &= ~(LPUART_CTRL_TE_MASK | LPUART_CTRL_RE_MASK);

    /* Calculate SBR for baud rate
       Baud = srcClockHz / ((OSR + 1) * SBR)
       Here: OSR = 15 -> oversampling = 16
    */
    sbr = config->srcClockHz / (config->baudRate * (LPUART_OSR_VALUE + 1U));

    if (sbr == 0U)
    {
        return LPUART_STATUS_ERROR;
    }

    /* Clear BAUD relevant fields first */
    base->BAUD &= ~(LPUART_BAUD_OSR_MASK | LPUART_BAUD_SBR_MASK | LPUART_BAUD_SBNS_MASK);

    /* 1 stop bit, no parity, 8-bit data, OSR = 15 */
    base->BAUD |= LPUART_BAUD_OSR(LPUART_OSR_VALUE) | LPUART_BAUD_SBR(sbr);

    /* Enable TX and RX */
    base->CTRL |= (LPUART_CTRL_TE_MASK | LPUART_CTRL_RE_MASK);

    return LPUART_STATUS_OK;
}

bool LPUART_IsTxReady(LPUART_Type *base)
{
    if (base == (LPUART_Type *)0)
    {
        return false;
    }

    return ((base->STAT & LPUART_STAT_TDRE_MASK) != 0U);
}

bool LPUART_IsRxReady(LPUART_Type *base)
{
    if (base == (LPUART_Type *)0)
    {
        return false;
    }

    return ((base->STAT & LPUART_STAT_RDRF_MASK) != 0U);
}

void LPUART_WriteChar(LPUART_Type *base, char ch)
{
    if (base == (LPUART_Type *)0)
    {
        return;
    }

    while (!LPUART_IsTxReady(base))
    {
    }

    base->DATA = (uint8_t)ch;
}

char LPUART_ReadChar(LPUART_Type *base)
{
    if (base == (LPUART_Type *)0)
    {
        return '\0';
    }

    while (!LPUART_IsRxReady(base))
    {
    }

    return (char)(base->DATA & LPUART_DATA_MASK);
}

void LPUART_WriteString(LPUART_Type *base, const char *str)
{
    if ((base == (LPUART_Type *)0) || (str == (const char *)0))
    {
        return;
    }

    while (*str != '\0')
    {
        LPUART_WriteChar(base, *str);
        str++;
    }
}

void LPUART_EnableRxInterrupt(LPUART_Type *base)
{
    if (base != (LPUART_Type *)0)
    {
        base->CTRL |= LPUART_CTRL_RIE_MASK;
    }
}

void LPUART_DisableRxInterrupt(LPUART_Type *base)
{
    if (base != (LPUART_Type *)0)
    {
        base->CTRL &= ~LPUART_CTRL_RIE_MASK;
    }
}

void LPUART_EnableTxInterrupt(LPUART_Type *base)
{
    if (base != (LPUART_Type *)0)
    {
        base->CTRL |= LPUART_CTRL_TIE_MASK;
    }
}

void LPUART_DisableTxInterrupt(LPUART_Type *base)
{
    if (base != (LPUART_Type *)0)
    {
        base->CTRL &= ~LPUART_CTRL_TIE_MASK;
    }
}

void LPUART_SetRxCallback(LPUART_Type *base, LPUART_Callback_t callback)
{
    (void)base;
    (void)callback;
}

void LPUART_SetTxCallback(LPUART_Type *base, LPUART_Callback_t callback)
{
    (void)base;
    (void)callback;
}
