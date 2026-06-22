/**
 * @file lpspi.c
 * @brief Low Power SPI driver implementation.
 *
 * @details
 * This file implements the polling-based LPSPI0 driver used by
 * EduFramework.
 *
 * The implementation intentionally keeps the driver simple and suitable
 * for educational use:
 * - LPSPI0 only.
 * - Blocking polling transfer.
 * - Timeout protection for polling loops.
 * - 8-bit and 16-bit frame transfer.
 * - Runtime SPI mode, bit order, frame size, and baudrate update.
 *
 * Interrupt and DMA transfer modes are intentionally not implemented in
 * this driver revision.
 */

#include "lpspi.h"

/* ============================================================
 * Local constants
 * ============================================================ */

/**
 * @brief Local alias for the configured LPSPI hardware base.
 */
#define LPSPI_HW_BASE                         (LPSPI_INSTANCE)

/**
 * @brief Boolean-like false value used inside this module.
 */
#define LPSPI_FALSE                           (0U)

/**
 * @brief Boolean-like true value used inside this module.
 */
#define LPSPI_TRUE                            (1U)

/**
 * @brief LPSPI0 PCC clock source selection.
 *
 * @details
 * The current framework clock setup uses SPLL_DIV2 as the LPSPI0
 * functional clock. This preserves the previous implementation intent.
 */
#define LPSPI_PCC_CLOCK_SOURCE_SPLL_DIV2      (6U)

/**
 * @brief LPSPI functional clock frequency in Hz.
 *
 * @details
 * This value must match the PCC clock source selected for LPSPI0.
 * The previous implementation documented SPLL_DIV2_CLK as 40 MHz.
 */
#define LPSPI_SOURCE_CLOCK_HZ                 (40000000UL)

/**
 * @brief Timeout count used for blocking polling loops.
 */
#define LPSPI_TIMEOUT_COUNT                   (100000UL)

/**
 * @brief All status flags clear mask.
 */
#define LPSPI_CLEAR_ALL_STATUS_FLAGS          (0xFFFFFFFFUL)

/**
 * @brief Default PCS line used by the current SPI stack.
 */
#define LPSPI_DEFAULT_PCS                     (0U)

/**
 * @brief Normal single-bit SPI pin configuration.
 *
 * @details
 * PINCFG = 0 means SIN is input and SOUT is output.
 */
#define LPSPI_PINCFG_NORMAL_SPI               (0U)

/**
 * @brief Active-low PCS polarity.
 */
#define LPSPI_PCS_ACTIVE_LOW                  (0U)

/**
 * @brief PCS-to-SCK delay setting.
 */
#define LPSPI_DELAY_PCS_TO_SCK                (0U)

/**
 * @brief SCK-to-PCS delay setting.
 */
#define LPSPI_DELAY_SCK_TO_PCS                (0U)

/**
 * @brief Delay between transfers.
 */
#define LPSPI_DELAY_BETWEEN_TRANSFER          (0U)

/**
 * @brief FIFO watermark used by the blocking driver.
 */
#define LPSPI_FIFO_WATERMARK_ZERO             (0U)

/**
 * @brief CPOL value for inactive-low SPI clock.
 */
#define LPSPI_CPOL_LOW                        (0U)

/**
 * @brief CPOL value for inactive-high SPI clock.
 */
#define LPSPI_CPOL_HIGH                       (1U)

/**
 * @brief CPHA value for sampling on the first clock edge.
 */
#define LPSPI_CPHA_FIRST_EDGE                 (0U)

/**
 * @brief CPHA value for sampling on the second clock edge.
 */
#define LPSPI_CPHA_SECOND_EDGE                (1U)

/**
 * @brief LSBF field value for MSB-first transfer.
 */
#define LPSPI_TCR_MSB_FIRST_VALUE             (0U)

/**
 * @brief LSBF field value for LSB-first transfer.
 */
#define LPSPI_TCR_LSB_FIRST_VALUE             (1U)

/**
 * @brief Minimum frame size supported by this driver.
 */
#define LPSPI_MIN_FRAME_SIZE_BITS             (8U)

/**
 * @brief Maximum frame size supported by this driver.
 */
#define LPSPI_MAX_FRAME_SIZE_BITS             (16U)

/**
 * @brief LPSPI FRAMESZ register value for 8-bit frame.
 */
#define LPSPI_FRAMESZ_8BIT_FIELD              (7U)

/**
 * @brief LPSPI FRAMESZ register value for 16-bit frame.
 */
#define LPSPI_FRAMESZ_16BIT_FIELD             (15U)

/**
 * @brief Maximum LPSPI PRESCALE field value.
 */
#define LPSPI_PRESCALE_FIELD_MAX              (7U)

/**
 * @brief Maximum LPSPI SCKDIV field value.
 */
#define LPSPI_SCKDIV_FIELD_MAX                (255U)

/**
 * @brief Minimum divisor represented by SCKDIV field.
 *
 * @details
 * LPSPI SCK divider is encoded as SCKDIV + 2.
 */
#define LPSPI_SCKDIV_BASE                     (2UL)

/**
 * @brief Dummy byte used to generate SPI clock during read operation.
 */
#define LPSPI_DUMMY_BYTE                      (0xFFU)

/**
 * @brief Dummy halfword used to generate SPI clock during 16-bit read.
 */
#define LPSPI_DUMMY_HALFWORD                  (0xFFFFU)

/**
 * @brief 8-bit receive data mask.
 */
#define LPSPI_RX_DATA_8BIT_MASK               (0xFFU)

/**
 * @brief 16-bit receive data mask.
 */
#define LPSPI_RX_DATA_16BIT_MASK              (0xFFFFU)

/* ============================================================
 * Internal state
 * ============================================================ */

static lpspi_config_t s_LpspiConfig = {0U};
static uint8_t s_u8LpspiInitialized = LPSPI_FALSE;
static uint8_t s_u8LpspiEnabled = LPSPI_FALSE;

/* ============================================================
 * Internal helpers
 * ============================================================ */

/**
 * @brief Check whether the LPSPI role value is valid.
 *
 * @param[in] Mode
 * LPSPI role value.
 *
 * @return uint8_t
 *
 * @retval LPSPI_TRUE Role is valid.
 * @retval LPSPI_FALSE Role is invalid.
 */
static uint8_t LPSPI_IsValidRole(lpspi_mode_t Mode)
{
    uint8_t u8IsValid = LPSPI_FALSE;

    if ((LPSPI_MODE_MASTER == Mode) || (LPSPI_MODE_SLAVE == Mode))
    {
        u8IsValid = LPSPI_TRUE;
    }

    return u8IsValid;
}

/**
 * @brief Check whether the SPI clock mode value is valid.
 *
 * @param[in] ClockMode
 * SPI clock mode.
 *
 * @return uint8_t
 *
 * @retval LPSPI_TRUE Clock mode is valid.
 * @retval LPSPI_FALSE Clock mode is invalid.
 */
static uint8_t LPSPI_IsValidClockMode(lpspi_clock_mode_t ClockMode)
{
    uint8_t u8IsValid = LPSPI_FALSE;

    if ((LPSPI_MODE0 == ClockMode) ||
        (LPSPI_MODE1 == ClockMode) ||
        (LPSPI_MODE2 == ClockMode) ||
        (LPSPI_MODE3 == ClockMode))
    {
        u8IsValid = LPSPI_TRUE;
    }

    return u8IsValid;
}

/**
 * @brief Check whether the bit order value is valid.
 *
 * @param[in] BitOrder
 * Bit order value.
 *
 * @return uint8_t
 *
 * @retval LPSPI_TRUE Bit order is valid.
 * @retval LPSPI_FALSE Bit order is invalid.
 */
static uint8_t LPSPI_IsValidBitOrder(lpspi_bit_order_t BitOrder)
{
    uint8_t u8IsValid = LPSPI_FALSE;

    if ((LPSPI_MSB_FIRST == BitOrder) || (LPSPI_LSB_FIRST == BitOrder))
    {
        u8IsValid = LPSPI_TRUE;
    }

    return u8IsValid;
}

/**
 * @brief Check whether the frame size value is supported.
 *
 * @param[in] FrameSize
 * Frame size value.
 *
 * @return uint8_t
 *
 * @retval LPSPI_TRUE Frame size is supported.
 * @retval LPSPI_FALSE Frame size is unsupported.
 */
static uint8_t LPSPI_IsValidFrameSize(lpspi_frameSize_t FrameSize)
{
    uint8_t u8IsValid = LPSPI_FALSE;

    if ((LPSPI_FRAME_SIZE_8 == FrameSize) ||
        (LPSPI_FRAME_SIZE_16 == FrameSize))
    {
        u8IsValid = LPSPI_TRUE;
    }

    return u8IsValid;
}

/**
 * @brief Validate initialization configuration.
 *
 * @param[in] pConfig
 * Pointer to configuration.
 *
 * @return lpspi_status_t
 *
 * @retval LPSPI_STATUS_OK Configuration is valid.
 * @retval LPSPI_STATUS_INVALID_ARG Configuration is invalid.
 */
static lpspi_status_t LPSPI_ValidateConfig(const lpspi_config_t *pConfig)
{
    lpspi_status_t RetVal = LPSPI_STATUS_OK;

    if ((const lpspi_config_t *)0 == pConfig)
    {
        RetVal = LPSPI_STATUS_INVALID_ARG;
    }
    else if (LPSPI_FALSE == LPSPI_IsValidRole(pConfig->mode))
    {
        RetVal = LPSPI_STATUS_INVALID_ARG;
    }
    else if (LPSPI_FALSE == LPSPI_IsValidClockMode(pConfig->clockMode))
    {
        RetVal = LPSPI_STATUS_INVALID_ARG;
    }
    else if (LPSPI_FALSE == LPSPI_IsValidBitOrder(pConfig->bitOrder))
    {
        RetVal = LPSPI_STATUS_INVALID_ARG;
    }
    else if (LPSPI_FALSE == LPSPI_IsValidFrameSize(pConfig->frameSize))
    {
        RetVal = LPSPI_STATUS_INVALID_ARG;
    }
    else if ((LPSPI_MODE_MASTER == pConfig->mode) && (0UL == pConfig->baudrate))
    {
        RetVal = LPSPI_STATUS_INVALID_ARG;
    }
    else
    {
        RetVal = LPSPI_STATUS_OK;
    }

    return RetVal;
}

/**
 * @brief Convert frame size to LPSPI FRAMESZ field value.
 *
 * @param[in] FrameSize
 * Frame size value.
 *
 * @param[out] pFrameSizeField
 * Pointer used to receive FRAMESZ field value.
 *
 * @return lpspi_status_t
 *
 * @retval LPSPI_STATUS_OK Frame size converted successfully.
 * @retval LPSPI_STATUS_INVALID_ARG Frame size is invalid.
 */
static lpspi_status_t LPSPI_GetFrameSizeField(lpspi_frameSize_t FrameSize,
                                              uint32_t *pFrameSizeField)
{
    lpspi_status_t RetVal = LPSPI_STATUS_OK;

    if ((uint32_t *)0 == pFrameSizeField)
    {
        RetVal = LPSPI_STATUS_INVALID_ARG;
    }
    else
    {
        switch (FrameSize)
        {
        case LPSPI_FRAME_SIZE_8:
            *pFrameSizeField = LPSPI_FRAMESZ_8BIT_FIELD;
            RetVal = LPSPI_STATUS_OK;
            break;

        case LPSPI_FRAME_SIZE_16:
            *pFrameSizeField = LPSPI_FRAMESZ_16BIT_FIELD;
            RetVal = LPSPI_STATUS_OK;
            break;

        default:
            RetVal = LPSPI_STATUS_INVALID_ARG;
            break;
        }
    }

    return RetVal;
}

/**
 * @brief Build CPOL and CPHA bits for the selected SPI mode.
 *
 * @param[in] ClockMode
 * SPI clock mode.
 *
 * @param[out] pTcrBits
 * Pointer used to receive TCR CPOL/CPHA bits.
 *
 * @return lpspi_status_t
 *
 * @retval LPSPI_STATUS_OK Clock mode converted successfully.
 * @retval LPSPI_STATUS_INVALID_ARG Clock mode is invalid.
 */
static lpspi_status_t LPSPI_GetClockModeBits(lpspi_clock_mode_t ClockMode,
                                             uint32_t *pTcrBits)
{
    lpspi_status_t RetVal = LPSPI_STATUS_OK;

    if ((uint32_t *)0 == pTcrBits)
    {
        RetVal = LPSPI_STATUS_INVALID_ARG;
    }
    else
    {
        switch (ClockMode)
        {
        case LPSPI_MODE0:
            *pTcrBits = LPSPI_TCR_CPOL(LPSPI_CPOL_LOW) |
                        LPSPI_TCR_CPHA(LPSPI_CPHA_FIRST_EDGE);
            RetVal = LPSPI_STATUS_OK;
            break;

        case LPSPI_MODE1:
            *pTcrBits = LPSPI_TCR_CPOL(LPSPI_CPOL_LOW) |
                        LPSPI_TCR_CPHA(LPSPI_CPHA_SECOND_EDGE);
            RetVal = LPSPI_STATUS_OK;
            break;

        case LPSPI_MODE2:
            *pTcrBits = LPSPI_TCR_CPOL(LPSPI_CPOL_HIGH) |
                        LPSPI_TCR_CPHA(LPSPI_CPHA_FIRST_EDGE);
            RetVal = LPSPI_STATUS_OK;
            break;

        case LPSPI_MODE3:
            *pTcrBits = LPSPI_TCR_CPOL(LPSPI_CPOL_HIGH) |
                        LPSPI_TCR_CPHA(LPSPI_CPHA_SECOND_EDGE);
            RetVal = LPSPI_STATUS_OK;
            break;

        default:
            RetVal = LPSPI_STATUS_INVALID_ARG;
            break;
        }
    }

    return RetVal;
}

/**
 * @brief Build initial TCR value from configuration.
 *
 * @param[in] pConfig
 * Pointer to LPSPI configuration.
 *
 * @param[out] pTcrValue
 * Pointer used to receive TCR value.
 *
 * @return lpspi_status_t
 *
 * @retval LPSPI_STATUS_OK TCR value built successfully.
 * @retval LPSPI_STATUS_INVALID_ARG Configuration is invalid.
 */
static lpspi_status_t LPSPI_BuildTCR(const lpspi_config_t *pConfig,
                                     uint32_t *pTcrValue)
{
    lpspi_status_t RetVal = LPSPI_STATUS_OK;
    uint32_t u32Tcr = 0UL;
    uint32_t u32ClockModeBits = 0UL;
    uint32_t u32FrameSizeField = 0UL;

    if (((const lpspi_config_t *)0 == pConfig) || ((uint32_t *)0 == pTcrValue))
    {
        RetVal = LPSPI_STATUS_INVALID_ARG;
    }
    else
    {
        RetVal = LPSPI_GetClockModeBits(pConfig->clockMode, &u32ClockModeBits);
    }

    if (LPSPI_STATUS_OK == RetVal)
    {
        RetVal = LPSPI_GetFrameSizeField(pConfig->frameSize, &u32FrameSizeField);
    }

    if (LPSPI_STATUS_OK == RetVal)
    {
        u32Tcr = u32ClockModeBits;
        u32Tcr |= LPSPI_TCR_PCS(LPSPI_DEFAULT_PCS);
        u32Tcr |= LPSPI_TCR_FRAMESZ(u32FrameSizeField);

        if (LPSPI_LSB_FIRST == pConfig->bitOrder)
        {
            u32Tcr |= LPSPI_TCR_LSBF(LPSPI_TCR_LSB_FIRST_VALUE);
        }
        else
        {
            u32Tcr |= LPSPI_TCR_LSBF(LPSPI_TCR_MSB_FIRST_VALUE);
        }

        *pTcrValue = u32Tcr;
    }

    return RetVal;
}

/**
 * @brief Wait until module busy flag is cleared.
 *
 * @return lpspi_status_t
 *
 * @retval LPSPI_STATUS_OK Module is idle.
 * @retval LPSPI_STATUS_TIMEOUT Timeout occurred.
 */
static lpspi_status_t LPSPI_WaitModuleIdle(void)
{
    lpspi_status_t RetVal = LPSPI_STATUS_TIMEOUT;
    uint32_t u32Timeout = LPSPI_TIMEOUT_COUNT;

    while ((0U != (LPSPI_HW_BASE->SR & LPSPI_SR_MBF_MASK)) &&
           (0UL != u32Timeout))
    {
        u32Timeout--;
    }

    if (0UL != u32Timeout)
    {
        RetVal = LPSPI_STATUS_OK;
    }

    return RetVal;
}

/**
 * @brief Wait until TX FIFO can accept data.
 *
 * @return lpspi_status_t
 *
 * @retval LPSPI_STATUS_OK TX FIFO is ready.
 * @retval LPSPI_STATUS_TIMEOUT Timeout occurred.
 */
static lpspi_status_t LPSPI_WaitTxReady(void)
{
    lpspi_status_t RetVal = LPSPI_STATUS_TIMEOUT;
    uint32_t u32Timeout = LPSPI_TIMEOUT_COUNT;

    while ((false == LPSPI_IsTxReady()) && (0UL != u32Timeout))
    {
        u32Timeout--;
    }

    if (0UL != u32Timeout)
    {
        RetVal = LPSPI_STATUS_OK;
    }

    return RetVal;
}

/**
 * @brief Wait until RX FIFO has unread data.
 *
 * @return lpspi_status_t
 *
 * @retval LPSPI_STATUS_OK RX FIFO has data.
 * @retval LPSPI_STATUS_TIMEOUT Timeout occurred.
 */
static lpspi_status_t LPSPI_WaitRxReady(void)
{
    lpspi_status_t RetVal = LPSPI_STATUS_TIMEOUT;
    uint32_t u32Timeout = LPSPI_TIMEOUT_COUNT;

    while ((false == LPSPI_IsRxReady()) && (0UL != u32Timeout))
    {
        u32Timeout--;
    }

    if (0UL != u32Timeout)
    {
        RetVal = LPSPI_STATUS_OK;
    }

    return RetVal;
}

/**
 * @brief Check whether LPSPI is initialized and enabled for transfer.
 *
 * @return lpspi_status_t
 *
 * @retval LPSPI_STATUS_OK Driver is ready for transfer.
 * @retval LPSPI_STATUS_NOT_INIT Driver is not initialized or not enabled.
 */
static lpspi_status_t LPSPI_CheckTransferReady(void)
{
    lpspi_status_t RetVal = LPSPI_STATUS_OK;

    if ((LPSPI_FALSE == s_u8LpspiInitialized) ||
        (LPSPI_FALSE == s_u8LpspiEnabled))
    {
        RetVal = LPSPI_STATUS_NOT_INIT;
    }
    else
    {
        RetVal = LPSPI_STATUS_OK;
    }

    return RetVal;
}

/**
 * @brief Apply frame size to TCR.
 *
 * @param[in] FrameSize
 * Frame size value.
 *
 * @return lpspi_status_t
 *
 * @retval LPSPI_STATUS_OK Frame size applied successfully.
 * @retval LPSPI_STATUS_INVALID_ARG Frame size is invalid.
 * @retval LPSPI_STATUS_NOT_INIT Driver is not initialized.
 * @retval LPSPI_STATUS_TIMEOUT Module did not become idle.
 */
static lpspi_status_t LPSPI_SetFrameSizeInternal(lpspi_frameSize_t FrameSize)
{
    lpspi_status_t RetVal = LPSPI_STATUS_OK;
    uint32_t u32Tcr = 0UL;
    uint32_t u32FrameSizeField = 0UL;

    if (LPSPI_FALSE == s_u8LpspiInitialized)
    {
        RetVal = LPSPI_STATUS_NOT_INIT;
    }
    else
    {
        RetVal = LPSPI_GetFrameSizeField(FrameSize, &u32FrameSizeField);
    }

    if (LPSPI_STATUS_OK == RetVal)
    {
        RetVal = LPSPI_WaitModuleIdle();
    }

    if (LPSPI_STATUS_OK == RetVal)
    {
        u32Tcr = LPSPI_HW_BASE->TCR;
        u32Tcr &= ~LPSPI_TCR_FRAMESZ_MASK;
        u32Tcr |= LPSPI_TCR_FRAMESZ(u32FrameSizeField);

        LPSPI_HW_BASE->TCR = u32Tcr;
        s_LpspiConfig.frameSize = FrameSize;
    }

    return RetVal;
}

/**
 * @brief Calculate PRESCALE and SCKDIV fields from target baudrate.
 *
 * @details
 * LPSPI master SCK frequency is calculated as:
 *
 * SCK = sourceClock / ((2 ^ PRESCALE) * (SCKDIV + 2))
 *
 * This helper selects the fastest SCK that does not exceed the requested
 * baudrate. If the requested baudrate is higher than the hardware maximum,
 * the hardware maximum is selected.
 *
 * @param[in] u32Baudrate
 * Target baudrate in Hz.
 *
 * @param[out] pPrescale
 * Pointer used to receive PRESCALE field value.
 *
 * @param[out] pSckdiv
 * Pointer used to receive SCKDIV field value.
 *
 * @return lpspi_status_t
 *
 * @retval LPSPI_STATUS_OK Dividers calculated successfully.
 * @retval LPSPI_STATUS_INVALID_ARG Invalid argument.
 * @retval LPSPI_STATUS_ERROR Target baudrate cannot be generated.
 */
static lpspi_status_t LPSPI_CalculateBaudRate(uint32_t u32Baudrate,
                                              uint32_t *pPrescale,
                                              uint32_t *pSckdiv)
{
    lpspi_status_t RetVal = LPSPI_STATUS_ERROR;
    uint8_t u8PrescaleIndex = 0U;
    uint32_t u32PrescaleDivisor = 1UL;
    uint32_t u32Divider = 0UL;
    uint32_t u32Sckdiv = 0UL;
    uint32_t u32ActualBaudrate = 0UL;
    uint32_t u32BestBaudrate = 0UL;
    uint32_t u32BestPrescale = 0UL;
    uint32_t u32BestSckdiv = 0UL;

    if ((0UL == u32Baudrate) ||
        ((uint32_t *)0 == pPrescale) ||
        ((uint32_t *)0 == pSckdiv))
    {
        RetVal = LPSPI_STATUS_INVALID_ARG;
    }
    else
    {
        for (u8PrescaleIndex = 0U;
             u8PrescaleIndex <= LPSPI_PRESCALE_FIELD_MAX;
             u8PrescaleIndex++)
        {
            u32PrescaleDivisor = (uint32_t)(1UL << u8PrescaleIndex);

            /*
             * Divider is rounded up so the generated SCK does not exceed
             * the requested target baudrate.
             */
            u32Divider = (LPSPI_SOURCE_CLOCK_HZ +
                          ((uint32_t)u32Baudrate * u32PrescaleDivisor) -
                          1UL) /
                         ((uint32_t)u32Baudrate * u32PrescaleDivisor);

            if (LPSPI_SCKDIV_BASE > u32Divider)
            {
                u32Divider = LPSPI_SCKDIV_BASE;
            }

            u32Sckdiv = u32Divider - LPSPI_SCKDIV_BASE;

            if (LPSPI_SCKDIV_FIELD_MAX >= u32Sckdiv)
            {
                u32ActualBaudrate = LPSPI_SOURCE_CLOCK_HZ /
                                    (u32PrescaleDivisor *
                                     (u32Sckdiv + LPSPI_SCKDIV_BASE));

                if ((0UL != u32ActualBaudrate) &&
                    (u32Baudrate >= u32ActualBaudrate) &&
                    (u32ActualBaudrate > u32BestBaudrate))
                {
                    u32BestBaudrate = u32ActualBaudrate;
                    u32BestPrescale = (uint32_t)u8PrescaleIndex;
                    u32BestSckdiv = u32Sckdiv;
                    RetVal = LPSPI_STATUS_OK;
                }
            }
        }

        if (LPSPI_STATUS_OK == RetVal)
        {
            *pPrescale = u32BestPrescale;
            *pSckdiv = u32BestSckdiv;
        }
    }

    return RetVal;
}

/**
 * @brief Apply baudrate configuration to CCR and TCR.
 *
 * @param[in] u32Baudrate
 * Target baudrate in Hz.
 *
 * @return lpspi_status_t
 *
 * @retval LPSPI_STATUS_OK Baudrate applied successfully.
 * @retval LPSPI_STATUS_ERROR Baudrate cannot be generated.
 * @retval LPSPI_STATUS_INVALID_ARG Baudrate is invalid.
 */
static lpspi_status_t LPSPI_ApplyBaudRate(uint32_t u32Baudrate)
{
    lpspi_status_t RetVal = LPSPI_STATUS_OK;
    uint32_t u32Prescale = 0UL;
    uint32_t u32Sckdiv = 0UL;
    uint32_t u32Tcr = 0UL;

    RetVal = LPSPI_CalculateBaudRate(u32Baudrate, &u32Prescale, &u32Sckdiv);

    if (LPSPI_STATUS_OK == RetVal)
    {
        /*
         * CCR:
         * Delay fields are currently kept at zero for a simple educational
         * blocking SPI driver. SCKDIV is calculated from baudrate.
         */
        LPSPI_HW_BASE->CCR = LPSPI_CCR_SCKPCS(LPSPI_DELAY_SCK_TO_PCS) |
                             LPSPI_CCR_PCSSCK(LPSPI_DELAY_PCS_TO_SCK) |
                             LPSPI_CCR_DBT(LPSPI_DELAY_BETWEEN_TRANSFER) |
                             LPSPI_CCR_SCKDIV(u32Sckdiv);

        /*
         * PRESCALE lives in TCR, so update only that field and preserve
         * CPOL, CPHA, PCS, LSBF, and FRAMESZ.
         */
        u32Tcr = LPSPI_HW_BASE->TCR;
        u32Tcr &= ~LPSPI_TCR_PRESCALE_MASK;
        u32Tcr |= LPSPI_TCR_PRESCALE(u32Prescale);
        LPSPI_HW_BASE->TCR = u32Tcr;

        s_LpspiConfig.baudrate = u32Baudrate;
    }

    return RetVal;
}

/**
 * @brief Write one frame and read the received frame.
 *
 * @param[in] u32TxData
 * Data to transmit.
 *
 * @param[out] pRxData
 * Pointer used to receive raw data.
 *
 * @return lpspi_status_t
 *
 * @retval LPSPI_STATUS_OK Transfer completed successfully.
 * @retval LPSPI_STATUS_INVALID_ARG Receive pointer is invalid.
 * @retval LPSPI_STATUS_NOT_INIT Driver is not ready.
 * @retval LPSPI_STATUS_TIMEOUT Polling timeout occurred.
 */
static lpspi_status_t LPSPI_TransferFrame(uint32_t u32TxData, uint32_t *pRxData)
{
    lpspi_status_t RetVal = LPSPI_STATUS_OK;

    if ((uint32_t *)0 == pRxData)
    {
        RetVal = LPSPI_STATUS_INVALID_ARG;
    }
    else
    {
        RetVal = LPSPI_CheckTransferReady();
    }

    if (LPSPI_STATUS_OK == RetVal)
    {
        RetVal = LPSPI_WaitTxReady();
    }

    if (LPSPI_STATUS_OK == RetVal)
    {
        LPSPI_HW_BASE->TDR = LPSPI_TDR_DATA(u32TxData);
        RetVal = LPSPI_WaitRxReady();
    }

    if (LPSPI_STATUS_OK == RetVal)
    {
        *pRxData = LPSPI_HW_BASE->RDR & LPSPI_RDR_DATA_MASK;
    }

    return RetVal;
}

/* ============================================================
 * Public API implementation
 * ============================================================ */

/**
 * @copydoc LPSPI_Init
 */
lpspi_status_t LPSPI_Init(const lpspi_config_t *pConfig)
{
    lpspi_status_t RetVal = LPSPI_STATUS_OK;
    uint32_t u32Cfgr1 = 0UL;
    uint32_t u32Tcr = 0UL;

    RetVal = LPSPI_ValidateConfig(pConfig);

    if (LPSPI_STATUS_OK == RetVal)
    {
        s_LpspiConfig = *pConfig;
        s_u8LpspiInitialized = LPSPI_FALSE;
        s_u8LpspiEnabled = LPSPI_FALSE;

        /*
         * Select and enable LPSPI0 functional clock.
         * PCC must be configured before accessing the peripheral safely.
         */
        IP_PCC->PCCn[PCC_LPSPI0_INDEX] = 0U;
        IP_PCC->PCCn[PCC_LPSPI0_INDEX] =
            PCC_PCCn_PCS(LPSPI_PCC_CLOCK_SOURCE_SPLL_DIV2) |
            PCC_PCCn_CGC_MASK;

        LPSPI_HW_BASE->CR &= ~LPSPI_CR_MEN_MASK;

        /*
         * Reset FIFOs and clear status flags so the first transfer starts
         * from a known state.
         */
        LPSPI_HW_BASE->CR = LPSPI_CR_RTF_MASK | LPSPI_CR_RRF_MASK;
        LPSPI_HW_BASE->SR = LPSPI_CLEAR_ALL_STATUS_FLAGS;

        u32Cfgr1 = 0UL;

        if (LPSPI_MODE_MASTER == pConfig->mode)
        {
            u32Cfgr1 |= LPSPI_CFGR1_MASTER(1U);
        }
        else
        {
            u32Cfgr1 |= LPSPI_CFGR1_MASTER(0U);
        }

        /*
         * Normal 4-wire SPI:
         * - SIN input.
         * - SOUT output.
         * - PCS active low.
         */
        u32Cfgr1 |= LPSPI_CFGR1_PINCFG(LPSPI_PINCFG_NORMAL_SPI);
        u32Cfgr1 |= LPSPI_CFGR1_PCSPOL(LPSPI_PCS_ACTIVE_LOW);

        LPSPI_HW_BASE->CFGR1 = u32Cfgr1;

        RetVal = LPSPI_BuildTCR(pConfig, &u32Tcr);
    }

    if (LPSPI_STATUS_OK == RetVal)
    {
        LPSPI_HW_BASE->TCR = u32Tcr;

        if (LPSPI_MODE_MASTER == pConfig->mode)
        {
            RetVal = LPSPI_ApplyBaudRate(pConfig->baudrate);
        }
    }

    if (LPSPI_STATUS_OK == RetVal)
    {
        LPSPI_HW_BASE->FCR = LPSPI_FCR_TXWATER(LPSPI_FIFO_WATERMARK_ZERO) |
                             LPSPI_FCR_RXWATER(LPSPI_FIFO_WATERMARK_ZERO);

        s_u8LpspiInitialized = LPSPI_TRUE;
        LPSPI_Enable();
    }

    return RetVal;
}

/**
 * @copydoc LPSPI_Enable
 */
void LPSPI_Enable(void)
{
    if (LPSPI_FALSE != s_u8LpspiInitialized)
    {
        LPSPI_HW_BASE->CR |= LPSPI_CR_MEN_MASK;
        s_u8LpspiEnabled = LPSPI_TRUE;
    }
}

/**
 * @copydoc LPSPI_Disable
 */
void LPSPI_Disable(void)
{
    LPSPI_HW_BASE->CR &= ~LPSPI_CR_MEN_MASK;
    s_u8LpspiEnabled = LPSPI_FALSE;
}

/**
 * @copydoc LPSPI_SetMode
 */
lpspi_status_t LPSPI_SetMode(lpspi_clock_mode_t Mode)
{
    lpspi_status_t RetVal = LPSPI_STATUS_OK;
    uint32_t u32Tcr = 0UL;
    uint32_t u32ClockModeBits = 0UL;

    if (LPSPI_FALSE == s_u8LpspiInitialized)
    {
        RetVal = LPSPI_STATUS_NOT_INIT;
    }
    else if (LPSPI_FALSE == LPSPI_IsValidClockMode(Mode))
    {
        RetVal = LPSPI_STATUS_INVALID_ARG;
    }
    else
    {
        RetVal = LPSPI_WaitModuleIdle();
    }

    if (LPSPI_STATUS_OK == RetVal)
    {
        RetVal = LPSPI_GetClockModeBits(Mode, &u32ClockModeBits);
    }

    if (LPSPI_STATUS_OK == RetVal)
    {
        u32Tcr = LPSPI_HW_BASE->TCR;
        u32Tcr &= ~(LPSPI_TCR_CPOL_MASK | LPSPI_TCR_CPHA_MASK);
        u32Tcr |= u32ClockModeBits;

        LPSPI_HW_BASE->TCR = u32Tcr;
        s_LpspiConfig.clockMode = Mode;
    }

    return RetVal;
}

/**
 * @copydoc LPSPI_SetBitOrder
 */
lpspi_status_t LPSPI_SetBitOrder(lpspi_bit_order_t BitOrder)
{
    lpspi_status_t RetVal = LPSPI_STATUS_OK;
    uint32_t u32Tcr = 0UL;

    if (LPSPI_FALSE == s_u8LpspiInitialized)
    {
        RetVal = LPSPI_STATUS_NOT_INIT;
    }
    else if (LPSPI_FALSE == LPSPI_IsValidBitOrder(BitOrder))
    {
        RetVal = LPSPI_STATUS_INVALID_ARG;
    }
    else
    {
        RetVal = LPSPI_WaitModuleIdle();
    }

    if (LPSPI_STATUS_OK == RetVal)
    {
        u32Tcr = LPSPI_HW_BASE->TCR;
        u32Tcr &= ~LPSPI_TCR_LSBF_MASK;

        if (LPSPI_LSB_FIRST == BitOrder)
        {
            u32Tcr |= LPSPI_TCR_LSBF(LPSPI_TCR_LSB_FIRST_VALUE);
        }
        else
        {
            u32Tcr |= LPSPI_TCR_LSBF(LPSPI_TCR_MSB_FIRST_VALUE);
        }

        LPSPI_HW_BASE->TCR = u32Tcr;
        s_LpspiConfig.bitOrder = BitOrder;
    }

    return RetVal;
}

/**
 * @copydoc LPSPI_SetBaudRate
 */
lpspi_status_t LPSPI_SetBaudRate(uint32_t baudrate)
{
    lpspi_status_t RetVal = LPSPI_STATUS_OK;

    if (LPSPI_FALSE == s_u8LpspiInitialized)
    {
        RetVal = LPSPI_STATUS_NOT_INIT;
    }
    else if (0UL == baudrate)
    {
        RetVal = LPSPI_STATUS_INVALID_ARG;
    }
    else if (LPSPI_MODE_MASTER != s_LpspiConfig.mode)
    {
        /*
         * Slave mode does not generate SCK, so baudrate configuration is
         * not meaningful for the peripheral.
         */
        RetVal = LPSPI_STATUS_INVALID_ARG;
    }
    else
    {
        RetVal = LPSPI_WaitModuleIdle();
    }

    if (LPSPI_STATUS_OK == RetVal)
    {
        RetVal = LPSPI_ApplyBaudRate(baudrate);
    }

    return RetVal;
}

/**
 * @copydoc LPSPI_SetFrameSize
 */
void LPSPI_SetFrameSize(lpspi_frameSize_t frameSize)
{
    (void)LPSPI_SetFrameSizeInternal(frameSize);
}

/**
 * @copydoc LPSPI_Transfer8
 */
lpspi_status_t LPSPI_Transfer8(uint8_t txData, uint8_t *rxData)
{
    lpspi_status_t RetVal = LPSPI_STATUS_OK;
    uint32_t u32RxData = 0UL;

    if ((uint8_t *)0 == rxData)
    {
        RetVal = LPSPI_STATUS_INVALID_ARG;
    }
    else
    {
        RetVal = LPSPI_SetFrameSizeInternal(LPSPI_FRAME_SIZE_8);
    }

    if (LPSPI_STATUS_OK == RetVal)
    {
        RetVal = LPSPI_TransferFrame((uint32_t)txData, &u32RxData);
    }

    if (LPSPI_STATUS_OK == RetVal)
    {
        *rxData = (uint8_t)(u32RxData & LPSPI_RX_DATA_8BIT_MASK);
    }

    return RetVal;
}

/**
 * @copydoc LPSPI_Transfer16
 */
lpspi_status_t LPSPI_Transfer16(uint16_t txData, uint16_t *rxData)
{
    lpspi_status_t RetVal = LPSPI_STATUS_OK;
    uint32_t u32RxData = 0UL;

    if ((uint16_t *)0 == rxData)
    {
        RetVal = LPSPI_STATUS_INVALID_ARG;
    }
    else
    {
        RetVal = LPSPI_SetFrameSizeInternal(LPSPI_FRAME_SIZE_16);
    }

    if (LPSPI_STATUS_OK == RetVal)
    {
        RetVal = LPSPI_TransferFrame((uint32_t)txData, &u32RxData);
    }

    if (LPSPI_STATUS_OK == RetVal)
    {
        *rxData = (uint16_t)(u32RxData & LPSPI_RX_DATA_16BIT_MASK);
    }

    return RetVal;
}

/**
 * @copydoc LPSPI_TransferBuffer
 */
lpspi_status_t LPSPI_TransferBuffer(const uint8_t *txBuf,
                                    uint8_t *rxBuf,
                                    uint32_t length)
{
    lpspi_status_t RetVal = LPSPI_STATUS_OK;
    uint32_t u32Index = 0UL;
    uint32_t u32RxData = 0UL;
    uint8_t u8TxData = LPSPI_DUMMY_BYTE;

    if (((const uint8_t *)0 == txBuf) && ((uint8_t *)0 == rxBuf))
    {
        RetVal = LPSPI_STATUS_INVALID_ARG;
    }
    else if (0UL == length)
    {
        RetVal = LPSPI_STATUS_OK;
    }
    else
    {
        RetVal = LPSPI_SetFrameSizeInternal(LPSPI_FRAME_SIZE_8);
    }

    if ((LPSPI_STATUS_OK == RetVal) && (0UL != length))
    {
        for (u32Index = 0UL; u32Index < length; u32Index++)
        {
            if ((const uint8_t *)0 != txBuf)
            {
                u8TxData = txBuf[u32Index];
            }
            else
            {
                u8TxData = LPSPI_DUMMY_BYTE;
            }

            RetVal = LPSPI_TransferFrame((uint32_t)u8TxData, &u32RxData);

            if (LPSPI_STATUS_OK != RetVal)
            {
                break;
            }

            if ((uint8_t *)0 != rxBuf)
            {
                rxBuf[u32Index] = (uint8_t)(u32RxData & LPSPI_RX_DATA_8BIT_MASK);
            }
        }
    }

    return RetVal;
}

/**
 * @copydoc LPSPI_Write8
 */
lpspi_status_t LPSPI_Write8(uint8_t data)
{
    lpspi_status_t RetVal = LPSPI_STATUS_OK;
    uint8_t u8DummyRx = 0U;

    if (LPSPI_MODE_MASTER == s_LpspiConfig.mode)
    {
        /*
         * Master write still receives one frame. Discard it so RX FIFO
         * does not accumulate stale data.
         */
        RetVal = LPSPI_Transfer8(data, &u8DummyRx);
    }
    else
    {
        RetVal = LPSPI_SetFrameSizeInternal(LPSPI_FRAME_SIZE_8);

        if (LPSPI_STATUS_OK == RetVal)
        {
            RetVal = LPSPI_CheckTransferReady();
        }

        if (LPSPI_STATUS_OK == RetVal)
        {
            RetVal = LPSPI_WaitTxReady();
        }

        if (LPSPI_STATUS_OK == RetVal)
        {
            LPSPI_HW_BASE->TDR = LPSPI_TDR_DATA((uint32_t)data);
        }
    }

    return RetVal;
}

/**
 * @copydoc LPSPI_Write16
 */
lpspi_status_t LPSPI_Write16(uint16_t data)
{
    lpspi_status_t RetVal = LPSPI_STATUS_OK;
    uint16_t u16DummyRx = 0U;

    if (LPSPI_MODE_MASTER == s_LpspiConfig.mode)
    {
        /*
         * Master write still receives one frame. Discard it so RX FIFO
         * does not accumulate stale data.
         */
        RetVal = LPSPI_Transfer16(data, &u16DummyRx);
    }
    else
    {
        RetVal = LPSPI_SetFrameSizeInternal(LPSPI_FRAME_SIZE_16);

        if (LPSPI_STATUS_OK == RetVal)
        {
            RetVal = LPSPI_CheckTransferReady();
        }

        if (LPSPI_STATUS_OK == RetVal)
        {
            RetVal = LPSPI_WaitTxReady();
        }

        if (LPSPI_STATUS_OK == RetVal)
        {
            LPSPI_HW_BASE->TDR = LPSPI_TDR_DATA((uint32_t)data);
        }
    }

    return RetVal;
}

/**
 * @copydoc LPSPI_Read8
 */
lpspi_status_t LPSPI_Read8(uint8_t *data)
{
    lpspi_status_t RetVal = LPSPI_STATUS_OK;
    uint32_t u32RxData = 0UL;

    if ((uint8_t *)0 == data)
    {
        RetVal = LPSPI_STATUS_INVALID_ARG;
    }
    else if (LPSPI_MODE_MASTER == s_LpspiConfig.mode)
    {
        /*
         * SPI master must transmit dummy data to generate SCK for reading.
         */
        RetVal = LPSPI_Transfer8(LPSPI_DUMMY_BYTE, data);
    }
    else
    {
        RetVal = LPSPI_SetFrameSizeInternal(LPSPI_FRAME_SIZE_8);

        if (LPSPI_STATUS_OK == RetVal)
        {
            RetVal = LPSPI_CheckTransferReady();
        }

        if (LPSPI_STATUS_OK == RetVal)
        {
            RetVal = LPSPI_WaitRxReady();
        }

        if (LPSPI_STATUS_OK == RetVal)
        {
            u32RxData = LPSPI_HW_BASE->RDR & LPSPI_RDR_DATA_MASK;
            *data = (uint8_t)(u32RxData & LPSPI_RX_DATA_8BIT_MASK);
        }
    }

    return RetVal;
}

/**
 * @copydoc LPSPI_Read16
 */
lpspi_status_t LPSPI_Read16(uint16_t *data)
{
    lpspi_status_t RetVal = LPSPI_STATUS_OK;
    uint32_t u32RxData = 0UL;

    if ((uint16_t *)0 == data)
    {
        RetVal = LPSPI_STATUS_INVALID_ARG;
    }
    else if (LPSPI_MODE_MASTER == s_LpspiConfig.mode)
    {
        /*
         * SPI master must transmit dummy data to generate SCK for reading.
         */
        RetVal = LPSPI_Transfer16(LPSPI_DUMMY_HALFWORD, data);
    }
    else
    {
        RetVal = LPSPI_SetFrameSizeInternal(LPSPI_FRAME_SIZE_16);

        if (LPSPI_STATUS_OK == RetVal)
        {
            RetVal = LPSPI_CheckTransferReady();
        }

        if (LPSPI_STATUS_OK == RetVal)
        {
            RetVal = LPSPI_WaitRxReady();
        }

        if (LPSPI_STATUS_OK == RetVal)
        {
            u32RxData = LPSPI_HW_BASE->RDR & LPSPI_RDR_DATA_MASK;
            *data = (uint16_t)(u32RxData & LPSPI_RX_DATA_16BIT_MASK);
        }
    }

    return RetVal;
}

/**
 * @copydoc LPSPI_IsTxReady
 */
bool LPSPI_IsTxReady(void)
{
    bool IsReady = false;

    if (0U != (LPSPI_HW_BASE->SR & LPSPI_SR_TDF_MASK))
    {
        IsReady = true;
    }

    return IsReady;
}

/**
 * @copydoc LPSPI_IsRxReady
 */
bool LPSPI_IsRxReady(void)
{
    bool IsReady = false;

    if (0U != (LPSPI_HW_BASE->SR & LPSPI_SR_RDF_MASK))
    {
        IsReady = true;
    }

    return IsReady;
}

/**
 * @copydoc LPSPI_IsTransferComplete
 */
bool LPSPI_IsTransferComplete(void)
{
    bool IsComplete = false;

    if (0U != (LPSPI_HW_BASE->SR & LPSPI_SR_TCF_MASK))
    {
        IsComplete = true;
    }

    return IsComplete;
}
