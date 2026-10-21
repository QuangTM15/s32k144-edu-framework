/**
 * @file lpspi.c
 * @brief Polling register driver for S32K144 LPSPI0.
 *
 * @details
 * TCR and TDR share the transmit/command FIFO. Never reset that FIFO
 * after queuing a command, and never replace a queued command while busy.
 * CCR is written with MEN=0. TCR updates can be queued with MEN=1 after
 * the module is idle. See S32K144 ERR051472 (spurious REF after MEN toggle)
 * and ERR010655 (MBF may read zero while hardware is busy).
 * GPIO CS management belongs to the upper layer.
 */

#include "lpspi.h"

#define LPSPI_HW                       (LPSPI_INSTANCE)
#define LPSPI_SOURCE_CLOCK_HZ          (40000000UL)
#define LPSPI_MAX_MASTER_SCK_HZ        (10000000UL)
#define LPSPI_PCC_CLOCK_SOURCE         (6U)
#define LPSPI_TIMEOUT_COUNT            (100000UL)
#define LPSPI_PRESCALE_MAX             (7UL)
#define LPSPI_SCKDIV_MAX               (255UL)
#define LPSPI_SCKDIV_BASE              (2UL)
#define LPSPI_DUMMY_BYTE               (0xFFU)
#define LPSPI_DUMMY_HALFWORD           (0xFFFFU)
#define LPSPI_FSR_RXCOUNT_FIELD_MASK   (0xFFUL)
#define LPSPI_FSR_TXCOUNT_FIELD_MASK   (0xFFUL)
#define LPSPI_FSR_RXCOUNT_SHIFT        (16UL)
#define LPSPI_DEFAULT_PCS             (0U)
#define LPSPI_FAULT_FLAGS              (LPSPI_SR_REF_MASK | LPSPI_SR_TEF_MASK)
#define LPSPI_CLEARABLE_FLAGS          (LPSPI_SR_DMF_MASK | LPSPI_SR_REF_MASK | LPSPI_SR_TEF_MASK | LPSPI_SR_TCF_MASK | LPSPI_SR_FCF_MASK | LPSPI_SR_WCF_MASK)

static lpspi_config_t s_Config = {0U};
static bool s_Initialized = false;
static bool s_Enabled = false;
static uint32_t s_ActualBaudrate = 0UL;
static uint32_t s_Command = 0UL;
static volatile uint32_t s_LastFaultSR = 0UL;
static volatile uint32_t s_LastFaultFSR = 0UL;
static volatile uint32_t s_LastFaultTCR = 0UL;
static volatile uint32_t s_LastFaultCR = 0UL;
static volatile uint32_t s_LastFaultCFGR1 = 0UL;

/** @brief Return current receive FIFO word count. */
static uint32_t LPSPI_RxCount(void)
{
    return (LPSPI_HW->FSR >> LPSPI_FSR_RXCOUNT_SHIFT) & LPSPI_FSR_RXCOUNT_FIELD_MASK;
}

/** @brief Return current transmit/command FIFO word count. */
static uint32_t LPSPI_TxCount(void)
{
    return LPSPI_HW->FSR & LPSPI_FSR_TXCOUNT_FIELD_MASK;
}

/** @brief Read MBF twice when initially zero (S32K144 ERR010655). */
static bool LPSPI_IsModuleBusy(void)
{
    bool busy = (0UL != (LPSPI_HW->SR & LPSPI_SR_MBF_MASK));

    if (!busy)
    {
        busy = (0UL != (LPSPI_HW->SR & LPSPI_SR_MBF_MASK));
    }

    return busy;
}

/** @brief Clear spurious REF after an MEN disable/enable transition. */
static void LPSPI_ClearReenableREF(void)
{
    /* S32K144 ERR051472: clear REF after enabling and before TX data. */
    LPSPI_HW->SR = LPSPI_SR_REF_MASK;
}

/** @brief Validate all clock, bit-order and frame-size fields. */
static bool LPSPI_ValidSettings(const lpspi_settings_t *settings)
{
    bool valid = false;

    if (settings != (const lpspi_settings_t *)0)
    {
        valid = ((settings->clockMode <= LPSPI_MODE3) && (settings->bitOrder <= LPSPI_LSB_FIRST) && ((settings->frameSize == LPSPI_FRAME_SIZE_8) || (settings->frameSize == LPSPI_FRAME_SIZE_16)));
    }

    return valid;
}

/** @brief Search all supported dividers without 32-bit multiplication overflow. */
static lpspi_status_t LPSPI_CalculateBaud(uint32_t requested, uint32_t *prescale, uint32_t *sckdiv, uint32_t *actual)
{
    lpspi_status_t status = LPSPI_STATUS_ERROR;
    uint32_t target = requested;
    uint32_t best = 0UL;
    uint32_t bestP = 0UL;
    uint32_t bestD = 0UL;
    uint32_t p = 0UL;
    uint32_t d = 0UL;
    uint32_t candidate = 0UL;
    uint64_t targetDivisor = 0ULL;
    uint64_t required = 0ULL;

    if ((requested == 0UL) || (prescale == (uint32_t *)0) || (sckdiv == (uint32_t *)0) || (actual == (uint32_t *)0))
    {
        status = LPSPI_STATUS_INVALID_ARG;
    }
    else
    {
        if (target > LPSPI_MAX_MASTER_SCK_HZ)
        {
            target = LPSPI_MAX_MASTER_SCK_HZ;
        }
        for (p = 0UL; p <= LPSPI_PRESCALE_MAX; p++)
        {
            targetDivisor = (uint64_t)target * (1ULL << p);
            required = ((uint64_t)LPSPI_SOURCE_CLOCK_HZ + targetDivisor - 1ULL) / targetDivisor;
            if (required < LPSPI_SCKDIV_BASE)
            {
                required = LPSPI_SCKDIV_BASE;
            }
            if (required <= (LPSPI_SCKDIV_MAX + LPSPI_SCKDIV_BASE))
            {
                d = (uint32_t)required;
                candidate = LPSPI_SOURCE_CLOCK_HZ / ((1UL << p) * d);
                if ((candidate > best) && ((uint64_t)LPSPI_SOURCE_CLOCK_HZ <= targetDivisor * (uint64_t)d))
                {
                    best = candidate;
                    bestP = p;
                    bestD = d - LPSPI_SCKDIV_BASE;
                    status = LPSPI_STATUS_OK;
                }
            }
        }
        if (status == LPSPI_STATUS_OK)
        {
            *prescale = bestP;
            *sckdiv = bestD;
            *actual = best;
        }
    }

    return status;
}

/** @brief Pack the next transfer's command without enabling hardware PCS pins. */
static uint32_t LPSPI_MakeCommand(const lpspi_settings_t *settings, uint32_t prescale)
{
    uint32_t command = LPSPI_TCR_PCS(LPSPI_DEFAULT_PCS) | LPSPI_TCR_FRAMESZ((uint32_t)settings->frameSize - 1UL) | LPSPI_TCR_PRESCALE(prescale);

    if ((settings->clockMode == LPSPI_MODE2) || (settings->clockMode == LPSPI_MODE3))
    {
        command |= LPSPI_TCR_CPOL_MASK;
    }
    if ((settings->clockMode == LPSPI_MODE1) || (settings->clockMode == LPSPI_MODE3))
    {
        command |= LPSPI_TCR_CPHA_MASK;
    }
    if (settings->bitOrder == LPSPI_LSB_FIRST)
    {
        command |= LPSPI_TCR_LSBF_MASK;
    }

    return command;
}

/** @brief Build the currently applied settings for legacy setters. */
static lpspi_settings_t LPSPI_CurrentSettings(void)
{
    lpspi_settings_t settings = {0U};

    settings.clockMode = s_Config.clockMode;
    settings.bitOrder = s_Config.bitOrder;
    settings.frameSize = s_Config.frameSize;
    settings.baudrate = s_Config.baudrate;

    return settings;
}

/** @brief Snapshot real FIFO faults before clearing the status register. */
static lpspi_status_t LPSPI_CheckFault(void)
{
    lpspi_status_t status = LPSPI_STATUS_OK;
    uint32_t sr = LPSPI_HW->SR;

    if (0UL != (sr & LPSPI_FAULT_FLAGS))
    {
        s_LastFaultSR = sr;
        s_LastFaultFSR = LPSPI_HW->FSR;
        s_LastFaultTCR = LPSPI_HW->TCR;
        s_LastFaultCR = LPSPI_HW->CR;
        s_LastFaultCFGR1 = LPSPI_HW->CFGR1;
        status = LPSPI_STATUS_FIFO_ERROR;
    }

    return status;
}

/** @brief Check driver state before issuing a command or data word. */
static lpspi_status_t LPSPI_CheckReady(void)
{
    lpspi_status_t status = LPSPI_STATUS_OK;

    if (!s_Initialized || !s_Enabled)
    {
        status = LPSPI_STATUS_NOT_INIT;
    }

    return status;
}

/** @brief Wait until the transmit FIFO can accept a data word. */
static lpspi_status_t LPSPI_WaitTx(void)
{
    lpspi_status_t status = LPSPI_STATUS_TIMEOUT;
    uint32_t timeout = LPSPI_TIMEOUT_COUNT;

    while (timeout > 0UL)
    {
        status = LPSPI_CheckFault();
        if (status != LPSPI_STATUS_OK)
        {
            break;
        }
        if (LPSPI_IsTxReady())
        {
            status = LPSPI_STATUS_OK;
            break;
        }
        status = LPSPI_STATUS_TIMEOUT;
        timeout--;
    }

    return status;
}

/** @brief Wait until at least one received word is present in RX FIFO. */
static lpspi_status_t LPSPI_WaitRx(void)
{
    lpspi_status_t status = LPSPI_STATUS_TIMEOUT;
    uint32_t timeout = LPSPI_TIMEOUT_COUNT;

    while (timeout > 0UL)
    {
        status = LPSPI_CheckFault();
        if (status != LPSPI_STATUS_OK)
        {
            break;
        }
        if ((LPSPI_RxCount() > 0UL) && LPSPI_IsRxReady())
        {
            status = LPSPI_STATUS_OK;
            break;
        }
        status = LPSPI_STATUS_TIMEOUT;
        timeout--;
    }

    return status;
}

/** @brief Wait for master frame completion and FIFO drain after a read. */
static lpspi_status_t LPSPI_WaitMasterComplete(void)
{
    lpspi_status_t status = LPSPI_STATUS_TIMEOUT;
    uint32_t timeout = LPSPI_TIMEOUT_COUNT;
    uint32_t sr = 0UL;

    while (timeout > 0UL)
    {
        status = LPSPI_CheckFault();
        if (status != LPSPI_STATUS_OK)
        {
            break;
        }
        sr = LPSPI_HW->SR;
        if (!LPSPI_IsModuleBusy() && ((sr & LPSPI_SR_TCF_MASK) != 0UL) && (LPSPI_TxCount() == 0UL))
        {
            status = LPSPI_STATUS_OK;
            break;
        }
        status = LPSPI_STATUS_TIMEOUT;
        timeout--;
    }

    return status;
}

/** @brief Transfer a frame and read exactly one RX FIFO word. */
static lpspi_status_t LPSPI_TransferFrame(uint32_t tx, uint32_t *rx)
{
    lpspi_status_t status = LPSPI_STATUS_INVALID_ARG;

    if (rx != (uint32_t *)0)
    {
        status = LPSPI_CheckReady();
    }
    if (status == LPSPI_STATUS_OK)
    {
        status = LPSPI_WaitTx();
    }
    if ((status == LPSPI_STATUS_OK) && (s_Config.mode == LPSPI_MODE_MASTER) && (LPSPI_RxCount() != 0UL))
    {
        status = LPSPI_STATUS_FIFO_ERROR;
        s_LastFaultSR = LPSPI_HW->SR;
        s_LastFaultFSR = LPSPI_HW->FSR;
        s_LastFaultTCR = LPSPI_HW->TCR;
        s_LastFaultCR = LPSPI_HW->CR;
        s_LastFaultCFGR1 = LPSPI_HW->CFGR1;
    }
    if (status == LPSPI_STATUS_OK)
    {
        LPSPI_HW->SR = LPSPI_SR_TCF_MASK;
        LPSPI_HW->TDR = LPSPI_TDR_DATA(tx);
        status = LPSPI_WaitRx();
    }
    if (status == LPSPI_STATUS_OK)
    {
        *rx = LPSPI_HW->RDR & LPSPI_RDR_DATA_MASK;
        if (s_Config.mode == LPSPI_MODE_MASTER)
        {
            status = LPSPI_WaitMasterComplete();
        }
    }
    if ((status == LPSPI_STATUS_TIMEOUT) || (status == LPSPI_STATUS_FIFO_ERROR))
    {
        (void)LPSPI_Recover();
    }

    return status;
}

/** @brief Initialize bus registers; publish state only on successful validation. */
lpspi_status_t LPSPI_Init(const lpspi_config_t *config)
{
    lpspi_status_t status = LPSPI_STATUS_INVALID_ARG;
    lpspi_settings_t settings = {0U};
    uint32_t prescale = 0UL;
    uint32_t sckdiv = 0UL;
    uint32_t actual = 0UL;
    uint32_t cfgr1 = 0UL;

    if (config != (const lpspi_config_t *)0)
    {
        settings.clockMode = config->clockMode;
        settings.bitOrder = config->bitOrder;
        settings.frameSize = config->frameSize;
        settings.baudrate = config->baudrate;
        if (((config->mode == LPSPI_MODE_MASTER) || (config->mode == LPSPI_MODE_SLAVE)) && LPSPI_ValidSettings(&settings))
        {
            status = LPSPI_STATUS_OK;
            if (config->mode == LPSPI_MODE_MASTER)
            {
                status = LPSPI_CalculateBaud(config->baudrate, &prescale, &sckdiv, &actual);
            }
        }
    }
    if ((status == LPSPI_STATUS_OK) && s_Initialized && s_Enabled)
    {
        status = LPSPI_WaitIdle();
    }
    if (status == LPSPI_STATUS_OK)
    {
        s_Initialized = false;
        s_Enabled = false;
        IP_PCC->PCCn[PCC_LPSPI0_INDEX] = 0UL;
        IP_PCC->PCCn[PCC_LPSPI0_INDEX] = PCC_PCCn_PCS(LPSPI_PCC_CLOCK_SOURCE) | PCC_PCCn_CGC_MASK;
        LPSPI_HW->CR = LPSPI_CR_RST_MASK;
        LPSPI_HW->CR = 0UL;
        LPSPI_HW->IER = 0UL;
        LPSPI_HW->DER = 0UL;
        LPSPI_HW->CFGR0 = 0UL;
        if (config->mode == LPSPI_MODE_MASTER)
        {
            cfgr1 = LPSPI_CFGR1_MASTER(1U);
        }
        LPSPI_HW->CFGR1 = cfgr1 | LPSPI_CFGR1_PINCFG(0U) | LPSPI_CFGR1_PCSPOL(0U);
        LPSPI_HW->FCR = LPSPI_FCR_TXWATER(0U) | LPSPI_FCR_RXWATER(0U);
        LPSPI_HW->CCR = LPSPI_CCR_SCKDIV(sckdiv);
        LPSPI_HW->SR = LPSPI_CLEARABLE_FLAGS;
        s_Config = *config;
        s_ActualBaudrate = actual;
        s_Command = LPSPI_MakeCommand(&settings, prescale);
        s_LastFaultSR = 0UL;
        s_LastFaultFSR = 0UL;
        s_LastFaultTCR = 0UL;
        s_LastFaultCR = 0UL;
        s_LastFaultCFGR1 = 0UL;
        s_Initialized = true;
        LPSPI_Enable();
    }

    return status;
}

/** @brief Enable without resetting a previously queued transmit command. */
void LPSPI_Enable(void)
{
    if (s_Initialized && !s_Enabled)
    {
        LPSPI_HW->CR |= LPSPI_CR_MEN_MASK;
        s_Enabled = true;
        LPSPI_ClearReenableREF();
        LPSPI_HW->TCR = s_Command;
    }
}

/** @brief Stop LPSPI0, including when an earlier transfer has failed. */
void LPSPI_Disable(void)
{
    if (s_Initialized)
    {
        if (s_Enabled)
        {
            (void)LPSPI_WaitIdle();
        }
        LPSPI_HW->CR &= ~LPSPI_CR_MEN_MASK;
    }
    s_Enabled = false;
}

/** @brief Wait until the shift engine and command FIFO are both idle. */
lpspi_status_t LPSPI_WaitIdle(void)
{
    lpspi_status_t status = LPSPI_STATUS_NOT_INIT;
    uint32_t timeout = LPSPI_TIMEOUT_COUNT;

    if (s_Initialized)
    {
        status = LPSPI_STATUS_OK;
        if (s_Enabled)
        {
            status = LPSPI_STATUS_TIMEOUT;
            while (timeout > 0UL)
            {
                status = LPSPI_CheckFault();
                if (status != LPSPI_STATUS_OK)
                {
                    break;
                }
                if (!LPSPI_IsModuleBusy() && (LPSPI_TxCount() == 0UL))
                {
                    status = LPSPI_STATUS_OK;
                    break;
                }
                status = LPSPI_STATUS_TIMEOUT;
                timeout--;
            }
        }
    }

    return status;
}

/** @brief Flush a failed transaction and requeue the last valid command. */
lpspi_status_t LPSPI_Recover(void)
{
    lpspi_status_t status = LPSPI_STATUS_NOT_INIT;
    bool restoreEnabled = s_Enabled;

    if (s_Initialized)
    {
        LPSPI_HW->CR &= ~LPSPI_CR_MEN_MASK;
        LPSPI_HW->CR = LPSPI_CR_RTF_MASK | LPSPI_CR_RRF_MASK;
        LPSPI_HW->SR = LPSPI_CLEARABLE_FLAGS;
        if (restoreEnabled)
        {
            LPSPI_HW->CR |= LPSPI_CR_MEN_MASK;
            LPSPI_ClearReenableREF();
            LPSPI_HW->TCR = s_Command;
        }
        status = LPSPI_STATUS_OK;
    }

    return status;
}

/** @brief Update all settings atomically between logical SPI transactions. */
lpspi_status_t LPSPI_ApplySettings(const lpspi_settings_t *settings)
{
    lpspi_status_t status = LPSPI_STATUS_NOT_INIT;
    uint32_t prescale = 0UL;
    uint32_t sckdiv = 0UL;
    uint32_t actual = 0UL;
    bool changed = false;
    bool wasEnabled = s_Enabled;

    if (s_Initialized)
    {
        status = LPSPI_STATUS_INVALID_ARG;
        if (LPSPI_ValidSettings(settings))
        {
            status = LPSPI_STATUS_OK;
            if (s_Config.mode == LPSPI_MODE_MASTER)
            {
                status = LPSPI_CalculateBaud(settings->baudrate, &prescale, &sckdiv, &actual);
            }
        }
    }
    if (status == LPSPI_STATUS_OK)
    {
        changed = ((s_Config.clockMode != settings->clockMode) || (s_Config.bitOrder != settings->bitOrder) || (s_Config.frameSize != settings->frameSize) || ((s_Config.mode == LPSPI_MODE_MASTER) && (s_Config.baudrate != settings->baudrate)));
        if (changed && wasEnabled)
        {
            status = LPSPI_WaitIdle();
        }
    }
    if ((status == LPSPI_STATUS_OK) && changed && (s_Config.mode == LPSPI_MODE_MASTER) && (LPSPI_RxCount() != 0UL))
    {
        status = LPSPI_STATUS_FIFO_ERROR;
        s_LastFaultSR = LPSPI_HW->SR;
        s_LastFaultFSR = LPSPI_HW->FSR;
        s_LastFaultTCR = LPSPI_HW->TCR;
        s_LastFaultCR = LPSPI_HW->CR;
        s_LastFaultCFGR1 = LPSPI_HW->CFGR1;
    }
    if ((status == LPSPI_STATUS_OK) && changed)
    {
        /* CPOL, CPHA, LSBF and FRAMESZ belong to TCR, not CCR. */
        /* Avoid toggling MEN unless the clock-divider register must change. */
        if ((s_Config.mode == LPSPI_MODE_MASTER) && (LPSPI_HW->CCR != LPSPI_CCR_SCKDIV(sckdiv)))
        {
            if (wasEnabled)
            {
                LPSPI_HW->CR &= ~LPSPI_CR_MEN_MASK;
            }
            LPSPI_HW->CCR = LPSPI_CCR_SCKDIV(sckdiv);
            if (wasEnabled)
            {
                LPSPI_HW->CR |= LPSPI_CR_MEN_MASK;
                LPSPI_ClearReenableREF();
            }
        }
        s_Config.clockMode = settings->clockMode;
        s_Config.bitOrder = settings->bitOrder;
        s_Config.frameSize = settings->frameSize;
        s_Config.baudrate = settings->baudrate;
        s_ActualBaudrate = actual;
        s_Command = LPSPI_MakeCommand(settings, prescale);
        if (wasEnabled)
        {
            /* TCR shares the command FIFO with TDR; do not flush it here. */
            LPSPI_HW->TCR = s_Command;
        }
    }

    return status;
}

/** @brief Return calculated master SCK; zero for slave mode. */
uint32_t LPSPI_GetActualBaudRate(void)
{
    return s_ActualBaudrate;
}

/** @brief Change CPOL and CPHA with validation. */
lpspi_status_t LPSPI_SetMode(lpspi_clock_mode_t mode)
{
    lpspi_settings_t settings = LPSPI_CurrentSettings();

    settings.clockMode = mode;
    return LPSPI_ApplySettings(&settings);
}

/** @brief Change transmit and receive bit order. */
lpspi_status_t LPSPI_SetBitOrder(lpspi_bit_order_t bitOrder)
{
    lpspi_settings_t settings = LPSPI_CurrentSettings();

    settings.bitOrder = bitOrder;
    return LPSPI_ApplySettings(&settings);
}

/** @brief Change requested baudrate; only meaningful in master mode. */
lpspi_status_t LPSPI_SetBaudRate(uint32_t baudrate)
{
    lpspi_status_t status = LPSPI_STATUS_NOT_INIT;
    lpspi_settings_t settings = LPSPI_CurrentSettings();

    if (s_Initialized)
    {
        status = LPSPI_STATUS_INVALID_ARG;
        if (s_Config.mode == LPSPI_MODE_MASTER)
        {
            settings.baudrate = baudrate;
            status = LPSPI_ApplySettings(&settings);
        }
    }

    return status;
}

/** @brief Change frame size and return a status code. */
lpspi_status_t LPSPI_SetFrameSizeChecked(lpspi_frameSize_t frameSize)
{
    lpspi_settings_t settings = LPSPI_CurrentSettings();

    settings.frameSize = frameSize;
    return LPSPI_ApplySettings(&settings);
}

/** @brief Backward-compatible void frame-size setter. */
void LPSPI_SetFrameSize(lpspi_frameSize_t frameSize)
{
    (void)LPSPI_SetFrameSizeChecked(frameSize);
}

/** @brief Send/receive one byte using an 8-bit frame. */
lpspi_status_t LPSPI_Transfer8(uint8_t txData, uint8_t *rxData)
{
    lpspi_status_t status = LPSPI_STATUS_INVALID_ARG;
    uint32_t rxWord = 0UL;

    if (rxData != (uint8_t *)0)
    {
        status = LPSPI_SetFrameSizeChecked(LPSPI_FRAME_SIZE_8);
    }
    if (status == LPSPI_STATUS_OK)
    {
        status = LPSPI_TransferFrame((uint32_t)txData, &rxWord);
    }
    if (status == LPSPI_STATUS_OK)
    {
        *rxData = (uint8_t)(rxWord & 0xFFUL);
    }

    return status;
}

/** @brief Send/receive one halfword using a 16-bit frame. */
lpspi_status_t LPSPI_Transfer16(uint16_t txData, uint16_t *rxData)
{
    lpspi_status_t status = LPSPI_STATUS_INVALID_ARG;
    uint32_t rxWord = 0UL;

    if (rxData != (uint16_t *)0)
    {
        status = LPSPI_SetFrameSizeChecked(LPSPI_FRAME_SIZE_16);
    }
    if (status == LPSPI_STATUS_OK)
    {
        *rxData = 0U;
        status = LPSPI_TransferFrame((uint32_t)txData, &rxWord);
    }
    if (status == LPSPI_STATUS_OK)
    {
        *rxData = (uint16_t)(rxWord & 0xFFFFUL);
    }

    return status;
}

/** @brief Exchange a byte buffer; null TX sends 0xFF, null RX discards. */
lpspi_status_t LPSPI_TransferBuffer(const uint8_t *txBuf, uint8_t *rxBuf, uint32_t length)
{
    lpspi_status_t status = LPSPI_STATUS_INVALID_ARG;
    uint32_t index = 0UL;
    uint32_t received = 0UL;
    uint8_t sent = LPSPI_DUMMY_BYTE;

    if ((txBuf != (const uint8_t *)0) || (rxBuf != (uint8_t *)0))
    {
        status = LPSPI_STATUS_OK;
        if (length != 0UL)
        {
            status = LPSPI_SetFrameSizeChecked(LPSPI_FRAME_SIZE_8);
        }
    }
    while ((status == LPSPI_STATUS_OK) && (index < length))
    {
        sent = (txBuf != (const uint8_t *)0) ? txBuf[index] : LPSPI_DUMMY_BYTE;
        status = LPSPI_TransferFrame((uint32_t)sent, &received);
        if ((status == LPSPI_STATUS_OK) && (rxBuf != (uint8_t *)0))
        {
            rxBuf[index] = (uint8_t)(received & 0xFFUL);
        }
        index++;
    }

    return status;
}

/** @brief Discard the returned byte of a master write. */
lpspi_status_t LPSPI_Write8(uint8_t data)
{
    lpspi_status_t status = LPSPI_STATUS_OK;
    uint8_t ignored = 0U;

    if (s_Config.mode == LPSPI_MODE_MASTER)
    {
        status = LPSPI_Transfer8(data, &ignored);
    }
    else
    {
        status = LPSPI_SetFrameSizeChecked(LPSPI_FRAME_SIZE_8);
        if (status == LPSPI_STATUS_OK)
        {
            status = LPSPI_CheckReady();
        }
        if (status == LPSPI_STATUS_OK)
        {
            status = LPSPI_WaitTx();
        }
        if (status == LPSPI_STATUS_OK)
        {
            LPSPI_HW->TDR = LPSPI_TDR_DATA((uint32_t)data);
        }
    }

    return status;
}

/** @brief Discard the returned halfword of a master write. */
lpspi_status_t LPSPI_Write16(uint16_t data)
{
    lpspi_status_t status = LPSPI_STATUS_OK;
    uint16_t ignored = 0U;

    if (s_Config.mode == LPSPI_MODE_MASTER)
    {
        status = LPSPI_Transfer16(data, &ignored);
    }
    else
    {
        status = LPSPI_SetFrameSizeChecked(LPSPI_FRAME_SIZE_16);
        if (status == LPSPI_STATUS_OK)
        {
            status = LPSPI_CheckReady();
        }
        if (status == LPSPI_STATUS_OK)
        {
            status = LPSPI_WaitTx();
        }
        if (status == LPSPI_STATUS_OK)
        {
            LPSPI_HW->TDR = LPSPI_TDR_DATA((uint32_t)data);
        }
    }

    return status;
}

/** @brief Read a byte by writing a dummy byte in master mode. */
lpspi_status_t LPSPI_Read8(uint8_t *data)
{
    lpspi_status_t status = LPSPI_STATUS_INVALID_ARG;
    uint32_t received = 0UL;

    if (data != (uint8_t *)0)
    {
        if (s_Config.mode == LPSPI_MODE_MASTER)
        {
            status = LPSPI_Transfer8(LPSPI_DUMMY_BYTE, data);
        }
        else
        {
            status = LPSPI_SetFrameSizeChecked(LPSPI_FRAME_SIZE_8);
            if (status == LPSPI_STATUS_OK)
            {
                status = LPSPI_CheckReady();
            }
            if (status == LPSPI_STATUS_OK)
            {
                status = LPSPI_WaitRx();
            }
            if (status == LPSPI_STATUS_OK)
            {
                received = LPSPI_HW->RDR & LPSPI_RDR_DATA_MASK;
                *data = (uint8_t)(received & 0xFFUL);
            }
        }
    }

    return status;
}

/** @brief Read a halfword by writing a dummy halfword in master mode. */
lpspi_status_t LPSPI_Read16(uint16_t *data)
{
    lpspi_status_t status = LPSPI_STATUS_INVALID_ARG;
    uint32_t received = 0UL;

    if (data != (uint16_t *)0)
    {
        if (s_Config.mode == LPSPI_MODE_MASTER)
        {
            status = LPSPI_Transfer16(LPSPI_DUMMY_HALFWORD, data);
        }
        else
        {
            status = LPSPI_SetFrameSizeChecked(LPSPI_FRAME_SIZE_16);
            if (status == LPSPI_STATUS_OK)
            {
                status = LPSPI_CheckReady();
            }
            if (status == LPSPI_STATUS_OK)
            {
                status = LPSPI_WaitRx();
            }
            if (status == LPSPI_STATUS_OK)
            {
                received = LPSPI_HW->RDR & LPSPI_RDR_DATA_MASK;
                *data = (uint16_t)(received & 0xFFFFUL);
            }
        }
    }

    return status;
}

/** @brief Return current transmit FIFO ready flag. */
bool LPSPI_IsTxReady(void)
{
    bool ready = false;

    if (s_Initialized && ((LPSPI_HW->SR & LPSPI_SR_TDF_MASK) != 0UL))
    {
        ready = true;
    }

    return ready;
}

/** @brief Return current receive FIFO ready flag. */
bool LPSPI_IsRxReady(void)
{
    bool ready = false;

    if (s_Initialized && ((LPSPI_HW->SR & LPSPI_SR_RDF_MASK) != 0UL))
    {
        ready = true;
    }

    return ready;
}

/** @brief Return current hardware transfer complete flag. */
bool LPSPI_IsTransferComplete(void)
{
    bool complete = false;

    if (s_Initialized && ((LPSPI_HW->SR & LPSPI_SR_TCF_MASK) != 0UL))
    {
        complete = true;
    }

    return complete;
}

/** @brief Return the most recent captured fault SR value. */
uint32_t LPSPI_GetLastFaultSR(void)
{
    return s_LastFaultSR;
}

/** @brief Return the most recent captured fault FSR value. */
uint32_t LPSPI_GetLastFaultFSR(void)
{
    return s_LastFaultFSR;
}

/** @brief Return the most recent captured fault TCR value. */
uint32_t LPSPI_GetLastFaultTCR(void)
{
    return s_LastFaultTCR;
}

/** @brief Return the control register captured with the last fault. */
uint32_t LPSPI_GetLastFaultCR(void)
{
    return s_LastFaultCR;
}

/** @brief Return the configuration register captured with the last fault. */
uint32_t LPSPI_GetLastFaultCFGR1(void)
{
    return s_LastFaultCFGR1;
}
