/**
 * @file can.c
 * @brief Arduino-style CAN API implementation for EduFramework.
 *
 * @details
 * This file implements a simple Arduino-style Classical CAN API on top
 * of the EduFramework FlexCAN driver.
 *
 * Design notes:
 * - FlexCAN0 is used internally.
 * - Message Buffer allocation is private to this layer.
 * - Standard and extended data frames are received through separate MBs.
 * - Reception is interrupt-driven.
 * - The low-level FlexCAN receive queue is reused directly.
 * - No dynamic memory allocation is used.
 * - This layer does not access FlexCAN registers directly.
 */

#include "can.h"

#include "flexcan.h"
#include "irq.h"

#include <stddef.h>

/* ========================================================================= */
/* Private Macros                                                             */
/* ========================================================================= */

/**
 * @brief Internal uninitialized state.
 */
#define CAN_STATE_UNINITIALIZED (0U)

/**
 * @brief Internal initialized state.
 */
#define CAN_STATE_INITIALIZED (1U)

/**
 * @brief FlexCAN Message Buffer used for transmission.
 *
 * @details
 * MB20 is intentionally used so Tx completion is handled by the
 * FlexCAN0 MB16-31 interrupt vector.
 */
#define CAN_TX_MB (20U)

/**
 * @brief FlexCAN Message Buffer used for standard frame reception.
 */
#define CAN_RX_STANDARD_MB (4U)

/**
 * @brief FlexCAN Message Buffer used for extended frame reception.
 */
#define CAN_RX_EXTENDED_MB (5U)

/**
 * @brief Receive mask used to accept every identifier.
 */
#define CAN_ACCEPT_ALL_MASK (0UL)

/**
 * @brief Maximum standard CAN identifier.
 */
#define CAN_STANDARD_ID_MAX (0x7FFUL)

/**
 * @brief Maximum extended CAN identifier.
 */
#define CAN_EXTENDED_ID_MAX (0x1FFFFFFFUL)

/**
 * @brief Timeout used by blocking transmission.
 */
#define CAN_TX_TIMEOUT (FLEXCAN_DEFAULT_TIMEOUT)

/* ========================================================================= */
/* Private Variables                                                          */
/* ========================================================================= */

/**
 * @brief CAN initialization state.
 */
static uint8_t s_u8CanInitialized = CAN_STATE_UNINITIALIZED;

/**
 * @brief Current Arduino CAN operating mode.
 */
static CAN_Mode_t s_CanMode = CAN_MODE_NORMAL;

/**
 * @brief Non-blocking transmission activity flag.
 *
 * @details
 * This variable is written from both application and interrupt context.
 */
static volatile bool s_bCanTxBusy = false;

/**
 * @brief Completion state of the most recently started transmission.
 *
 * @details
 * This variable is updated from interrupt context for non-blocking Tx.
 */
static volatile bool s_bCanTxComplete = false;

/**
 * @brief User receive callback.
 */
static CAN_Callback_t s_pfCanReceiveCallback = NULL;

/* ========================================================================= */
/* Private Function Prototypes                                                */
/* ========================================================================= */

static bool CAN_IsInitialized(void);

static bool CAN_IsValidMode(CAN_Mode_t mode);

static bool CAN_IsValidFormat(CAN_Format_t format);

static bool CAN_IsValidFrame(const CAN_Frame_t *frame);

static FLEXCAN_Mode_t CAN_ToFlexcanMode(CAN_Mode_t mode);

static FLEXCAN_FrameFormat_t CAN_ToFlexcanFormat(CAN_Format_t format);

static CAN_Format_t CAN_FromFlexcanFormat(FLEXCAN_FrameFormat_t format);

static void CAN_ToFlexcanFrame(const CAN_Frame_t *source,
                               FLEXCAN_Frame_t *destination);

static void CAN_FromFlexcanFrame(const FLEXCAN_Frame_t *source,
                                 CAN_Frame_t *destination);

static FLEXCAN_Status_t CAN_ConfigReceiveMb(uint8_t mbIndex,
                                            FLEXCAN_FrameFormat_t format);

static void CAN_ResetState(void);

static void CAN_DriverCallback(FLEXCAN_Type *pBase,
                               FLEXCAN_Event_t event,
                               uint8_t mbIndex);

/* ========================================================================= */
/* Private Functions                                                          */
/* ========================================================================= */

/**
 * @brief Check whether the Arduino CAN layer is initialized.
 *
 * @return Initialization state.
 */
static bool CAN_IsInitialized(void)
{
    bool bInitialized = false;

    if (CAN_STATE_INITIALIZED == s_u8CanInitialized)
    {
        bInitialized = true;
    }
    else
    {
        bInitialized = false;
    }

    return bInitialized;
}

/**
 * @brief Validate a CAN operating mode.
 *
 * @param mode CAN operating mode.
 *
 * @return Validity state.
 */
static bool CAN_IsValidMode(CAN_Mode_t mode)
{
    bool bValid = false;

    if ((CAN_MODE_NORMAL == mode) ||
        (CAN_MODE_LOOPBACK == mode) ||
        (CAN_MODE_LISTEN_ONLY == mode))
    {
        bValid = true;
    }
    else
    {
        bValid = false;
    }

    return bValid;
}

/**
 * @brief Validate a CAN identifier format.
 *
 * @param format CAN identifier format.
 *
 * @return Validity state.
 */
static bool CAN_IsValidFormat(CAN_Format_t format)
{
    bool bValid = false;

    if ((CAN_STANDARD == format) ||
        (CAN_EXTENDED == format))
    {
        bValid = true;
    }
    else
    {
        bValid = false;
    }

    return bValid;
}

/**
 * @brief Validate an Arduino CAN frame.
 *
 * @param frame Frame to validate.
 *
 * @return Validity state.
 */
static bool CAN_IsValidFrame(const CAN_Frame_t *frame)
{
    bool bValid = false;

    if (NULL != frame)
    {
        if ((CAN_MAX_DATA_LENGTH >= frame->length) &&
            (true == CAN_IsValidFormat(frame->format)))
        {
            if ((CAN_STANDARD == frame->format) &&
                (CAN_STANDARD_ID_MAX >= frame->id))
            {
                bValid = true;
            }
            else if ((CAN_EXTENDED == frame->format) &&
                     (CAN_EXTENDED_ID_MAX >= frame->id))
            {
                bValid = true;
            }
            else
            {
                bValid = false;
            }
        }
        else
        {
            bValid = false;
        }
    }

    return bValid;
}

/**
 * @brief Convert Arduino CAN mode to FlexCAN driver mode.
 *
 * @param mode Arduino CAN mode.
 *
 * @return FlexCAN driver mode.
 */
static FLEXCAN_Mode_t CAN_ToFlexcanMode(CAN_Mode_t mode)
{
    FLEXCAN_Mode_t FlexcanMode = FLEXCAN_MODE_NORMAL;

    switch (mode)
    {
    case CAN_MODE_LOOPBACK:
        FlexcanMode = FLEXCAN_MODE_LOOPBACK;
        break;

    case CAN_MODE_LISTEN_ONLY:
        FlexcanMode = FLEXCAN_MODE_LISTEN_ONLY;
        break;

    case CAN_MODE_NORMAL:
    default:
        FlexcanMode = FLEXCAN_MODE_NORMAL;
        break;
    }

    return FlexcanMode;
}

/**
 * @brief Convert Arduino CAN format to FlexCAN driver format.
 *
 * @param format Arduino CAN format.
 *
 * @return FlexCAN driver frame format.
 */
static FLEXCAN_FrameFormat_t CAN_ToFlexcanFormat(CAN_Format_t format)
{
    FLEXCAN_FrameFormat_t FlexcanFormat = FLEXCAN_FRAME_STANDARD;

    if (CAN_EXTENDED == format)
    {
        FlexcanFormat = FLEXCAN_FRAME_EXTENDED;
    }
    else
    {
        FlexcanFormat = FLEXCAN_FRAME_STANDARD;
    }

    return FlexcanFormat;
}

/**
 * @brief Convert FlexCAN driver format to Arduino CAN format.
 *
 * @param format FlexCAN frame format.
 *
 * @return Arduino CAN frame format.
 */
static CAN_Format_t CAN_FromFlexcanFormat(FLEXCAN_FrameFormat_t format)
{
    CAN_Format_t CanFormat = CAN_STANDARD;

    if (FLEXCAN_FRAME_EXTENDED == format)
    {
        CanFormat = CAN_EXTENDED;
    }
    else
    {
        CanFormat = CAN_STANDARD;
    }

    return CanFormat;
}

/**
 * @brief Convert an Arduino CAN frame to a FlexCAN driver frame.
 *
 * @param source Arduino CAN frame.
 * @param destination FlexCAN driver frame.
 */
static void CAN_ToFlexcanFrame(const CAN_Frame_t *source,
                               FLEXCAN_Frame_t *destination)
{
    uint8_t u8Index = 0U;

    if ((NULL != source) &&
        (NULL != destination))
    {
        destination->u32Id = source->id;
        destination->u8Length = source->length;
        destination->u8MbIndex = CAN_TX_MB;
        destination->u8LocalPriority = 0U;

        destination->u8Format =
            CAN_ToFlexcanFormat(source->format);

        destination->u8FrameType =
            FLEXCAN_FRAME_DATA;

        destination->u16Timestamp = 0U;

        destination->bFD = false;
        destination->bBitRateSwitch = false;
        destination->bErrorStateIndicator = false;
        destination->bOverrun = false;

        for (u8Index = 0U;
             u8Index < FLEXCAN_FD_MAX_DATA_LENGTH;
             u8Index++)
        {
            destination->au8Data[u8Index] = 0U;
        }

        for (u8Index = 0U;
             u8Index < source->length;
             u8Index++)
        {
            destination->au8Data[u8Index] =
                source->data[u8Index];
        }
    }

    return;
}

/**
 * @brief Convert a FlexCAN driver frame to an Arduino CAN frame.
 *
 * @param source FlexCAN driver frame.
 * @param destination Arduino CAN frame.
 */
static void CAN_FromFlexcanFrame(const FLEXCAN_Frame_t *source,
                                 CAN_Frame_t *destination)
{
    uint8_t u8Index = 0U;

    if ((NULL != source) &&
        (NULL != destination))
    {
        destination->id =
            source->u32Id;

        destination->length =
            source->u8Length;

        destination->format =
            CAN_FromFlexcanFormat(source->u8Format);

        for (u8Index = 0U;
             u8Index < CAN_MAX_DATA_LENGTH;
             u8Index++)
        {
            destination->data[u8Index] = 0U;
        }

        for (u8Index = 0U;
             (u8Index < source->u8Length) &&
             (u8Index < CAN_MAX_DATA_LENGTH);
             u8Index++)
        {
            destination->data[u8Index] =
                source->au8Data[u8Index];
        }
    }

    return;
}

/**
 * @brief Configure one internal receive Message Buffer.
 *
 * @details
 * An acceptance mask of zero accepts every identifier while the FlexCAN
 * driver still compares IDE and RTR, allowing one MB to receive all standard
 * data frames and another MB to receive all extended data frames.
 *
 * @param mbIndex Message Buffer index.
 * @param format FlexCAN identifier format.
 *
 * @return FlexCAN driver status.
 */
static FLEXCAN_Status_t CAN_ConfigReceiveMb(uint8_t mbIndex,
                                            FLEXCAN_FrameFormat_t format)
{
    FLEXCAN_RxMbConfig_t RxConfig;
    FLEXCAN_Status_t Status = FLEXCAN_STATUS_ERROR;

    RxConfig.u8MbIndex = mbIndex;
    RxConfig.u32Id = 0UL;
    RxConfig.u32Mask = CAN_ACCEPT_ALL_MASK;
    RxConfig.u8Format = format;
    RxConfig.u8FrameType = FLEXCAN_FRAME_DATA;
    RxConfig.bEnableInterrupt = true;

    Status =
        FLEXCAN_ConfigRxMb(
            IP_FLEXCAN0,
            &RxConfig);

    return Status;
}

/**
 * @brief Reset all software state owned by the Arduino CAN layer.
 */
static void CAN_ResetState(void)
{
    s_u8CanInitialized =
        CAN_STATE_UNINITIALIZED;

    s_CanMode =
        CAN_MODE_NORMAL;

    s_bCanTxBusy =
        false;

    s_bCanTxComplete =
        false;

    s_pfCanReceiveCallback =
        NULL;

    return;
}

/**
 * @brief FlexCAN driver event callback.
 *
 * @details
 * This callback runs from interrupt context.
 *
 * Tx completion updates the Arduino non-blocking transmission state.
 * Rx completion optionally notifies the application after the FlexCAN driver
 * has already stored the received frame in its software queue.
 *
 * @param pBase FlexCAN peripheral base address.
 * @param event FlexCAN event.
 * @param mbIndex Message Buffer associated with the event.
 */
static void CAN_DriverCallback(FLEXCAN_Type *pBase,
                               FLEXCAN_Event_t event,
                               uint8_t mbIndex)
{
    CAN_Callback_t ReceiveCallback = NULL;

    if (IP_FLEXCAN0 == pBase)
    {
        if ((FLEXCAN_EVENT_TX_MB == event) &&
            (CAN_TX_MB == mbIndex))
        {
            s_bCanTxBusy = false;
            s_bCanTxComplete = true;
        }
        else if ((FLEXCAN_EVENT_RX_MB == event) &&
                 ((CAN_RX_STANDARD_MB == mbIndex) ||
                  (CAN_RX_EXTENDED_MB == mbIndex)))
        {
            ReceiveCallback =
                s_pfCanReceiveCallback;

            if (NULL != ReceiveCallback)
            {
                ReceiveCallback();
            }
        }
        else
        {
            /*
             * Other FlexCAN events are intentionally hidden by this
             * minimal Arduino CAN layer.
             */
        }
    }

    return;
}

/* ========================================================================= */
/* Public Functions                                                           */
/* ========================================================================= */

/**
 * @copydoc CAN_begin
 */
bool CAN_begin(uint32_t bitRate)
{
    FLEXCAN_Config_t Config;
    FLEXCAN_Status_t Status = FLEXCAN_STATUS_ERROR;
    bool bDriverInitialized = false;
    bool bInitialized = false;

    if (0UL != bitRate)
    {
        if (true == CAN_IsInitialized())
        {
            CAN_end();
        }

        FLEXCAN_GetDefaultConfig(
            &Config);

        Config.u32NominalBitRate =
            bitRate;

        Config.u8Mode =
            FLEXCAN_MODE_NORMAL;

        Config.bEnableSelfReception =
            false;

        Config.bEnableFD =
            false;

        Status =
            FLEXCAN_Init(
                IP_FLEXCAN0,
                &Config);

        if (FLEXCAN_STATUS_OK == Status)
        {
            bDriverInitialized = true;

            /*
             * Register the private Arduino-layer callback before
             * Message Buffer interrupts are enabled in NVIC.
             */
            FLEXCAN_SetCallback(
                IP_FLEXCAN0,
                CAN_DriverCallback);

            /*
             * TX MB20 is used internally for both blocking and
             * non-blocking transmission.
             *
             * Tx completion interrupt is enabled for non-blocking
             * transmission.
             */
            Status =
                FLEXCAN_ConfigTxMb(
                    IP_FLEXCAN0,
                    CAN_TX_MB,
                    true);
        }

        if (FLEXCAN_STATUS_OK == Status)
        {
            /*
             * RX MB4 accepts all standard Classical CAN data frames.
             */
            Status =
                CAN_ConfigReceiveMb(
                    CAN_RX_STANDARD_MB,
                    FLEXCAN_FRAME_STANDARD);
        }

        if (FLEXCAN_STATUS_OK == Status)
        {
            /*
             * RX MB5 accepts all extended Classical CAN data frames.
             */
            Status =
                CAN_ConfigReceiveMb(
                    CAN_RX_EXTENDED_MB,
                    FLEXCAN_FRAME_EXTENDED);
        }

        if (FLEXCAN_STATUS_OK == Status)
        {
            /*
             * Start with an empty receive queue.
             */
            FLEXCAN_FlushRxQueue(
                IP_FLEXCAN0);

            /*
             * Enable the FlexCAN0 Message Buffer interrupt vectors.
             *
             * RX MB4/MB5 use the MB0-15 vector.
             * TX MB20 uses the MB16-31 vector.
             */
            IRQ_FLEXCAN0_MB_Init();

            s_CanMode =
                CAN_MODE_NORMAL;

            s_bCanTxBusy =
                false;

            s_bCanTxComplete =
                false;

            s_pfCanReceiveCallback =
                NULL;

            s_u8CanInitialized =
                CAN_STATE_INITIALIZED;

            bInitialized = true;
        }
        else
        {
            /*
             * Clean up only when the low-level FlexCAN driver was
             * successfully initialized.
             */
            if (true == bDriverInitialized)
            {
                FLEXCAN_SetCallback(
                    IP_FLEXCAN0,
                    NULL);

                (void)FLEXCAN_Deinit(
                    IP_FLEXCAN0);
            }

            CAN_ResetState();

            bInitialized = false;
        }
    }

    return bInitialized;
}

/**
 * @copydoc CAN_end
 */
void CAN_end(void)
{
    if (true == CAN_IsInitialized())
    {
        FLEXCAN_SetCallback(
            IP_FLEXCAN0,
            NULL);

        (void)FLEXCAN_Deinit(
            IP_FLEXCAN0);
    }

    CAN_ResetState();

    return;
}

/**
 * @copydoc CAN_setMode
 */
bool CAN_setMode(CAN_Mode_t mode)
{
    FLEXCAN_Status_t Status = FLEXCAN_STATUS_ERROR;
    bool bSuccess = false;

    if ((true == CAN_IsInitialized()) &&
        (false == s_bCanTxBusy) &&
        (true == CAN_IsValidMode(mode)))
    {
        Status =
            FLEXCAN_SetMode(
                IP_FLEXCAN0,
                CAN_ToFlexcanMode(mode));

        if (FLEXCAN_STATUS_OK == Status)
        {
            s_CanMode = mode;
            bSuccess = true;
        }
    }

    return bSuccess;
}

/**
 * @copydoc CAN_send
 */
bool CAN_send(const CAN_Frame_t *frame)
{
    FLEXCAN_Frame_t FlexcanFrame;
    FLEXCAN_Status_t Status = FLEXCAN_STATUS_ERROR;
    FLEXCAN_Status_t RestoreStatus = FLEXCAN_STATUS_ERROR;
    bool bSuccess = false;

    if ((true == CAN_IsInitialized()) &&
        (false == s_bCanTxBusy) &&
        (CAN_MODE_LISTEN_ONLY != s_CanMode) &&
        (true == CAN_IsValidFrame(frame)))
    {
        CAN_ToFlexcanFrame(
            frame,
            &FlexcanFrame);

        /*
         * The current FlexCAN driver blocking API polls the Tx IFLAG while
         * the interrupt handler also acknowledges that same flag.
         *
         * Temporarily mask this Tx MB interrupt so blocking transmission has
         * sole ownership of the completion flag.
         */
        Status =
            FLEXCAN_DisableMbInterrupt(
                IP_FLEXCAN0,
                CAN_TX_MB);

        if (FLEXCAN_STATUS_OK == Status)
        {
            s_bCanTxBusy = true;
            s_bCanTxComplete = false;

            Status =
                FLEXCAN_TransmitBlocking(
                    IP_FLEXCAN0,
                    CAN_TX_MB,
                    &FlexcanFrame,
                    CAN_TX_TIMEOUT);

            s_bCanTxBusy = false;

            if (FLEXCAN_STATUS_OK == Status)
            {
                s_bCanTxComplete = true;
            }
            else
            {
                s_bCanTxComplete = false;
            }

            RestoreStatus =
                FLEXCAN_EnableMbInterrupt(
                    IP_FLEXCAN0,
                    CAN_TX_MB);

            if ((FLEXCAN_STATUS_OK == Status) &&
                (FLEXCAN_STATUS_OK == RestoreStatus))
            {
                bSuccess = true;
            }
        }
    }

    return bSuccess;
}

/**
 * @copydoc CAN_sendNonBlocking
 */
bool CAN_sendNonBlocking(const CAN_Frame_t *frame)
{
    FLEXCAN_Frame_t FlexcanFrame;
    FLEXCAN_Status_t Status = FLEXCAN_STATUS_ERROR;
    bool bSuccess = false;

    if ((true == CAN_IsInitialized()) &&
        (false == s_bCanTxBusy) &&
        (CAN_MODE_LISTEN_ONLY != s_CanMode) &&
        (true == CAN_IsValidFrame(frame)))
    {
        CAN_ToFlexcanFrame(
            frame,
            &FlexcanFrame);

        /*
         * Publish busy state before starting hardware transmission.
         * This prevents a fast Loop-Back completion IRQ from racing with
         * state initialization.
         */
        s_bCanTxBusy = true;
        s_bCanTxComplete = false;

        Status =
            FLEXCAN_Transmit(
                IP_FLEXCAN0,
                CAN_TX_MB,
                &FlexcanFrame);

        if (FLEXCAN_STATUS_OK == Status)
        {
            bSuccess = true;
        }
        else
        {
            s_bCanTxBusy = false;
            s_bCanTxComplete = false;
        }
    }

    return bSuccess;
}

/**
 * @copydoc CAN_isTxBusy
 */
bool CAN_isTxBusy(void)
{
    bool bBusy = false;

    if (true == CAN_IsInitialized())
    {
        bBusy = s_bCanTxBusy;
    }

    return bBusy;
}

/**
 * @copydoc CAN_isTxComplete
 */
bool CAN_isTxComplete(void)
{
    bool bComplete = false;

    if (true == CAN_IsInitialized())
    {
        bComplete = s_bCanTxComplete;
    }

    return bComplete;
}

/**
 * @copydoc CAN_available
 */
bool CAN_available(void)
{
    bool bAvailable = false;

    if (true == CAN_IsInitialized())
    {
        bAvailable =
            FLEXCAN_IsDataAvailable(
                IP_FLEXCAN0);
    }

    return bAvailable;
}

/**
 * @copydoc CAN_read
 */
bool CAN_read(CAN_Frame_t *frame)
{
    FLEXCAN_Frame_t FlexcanFrame;
    FLEXCAN_Status_t Status = FLEXCAN_STATUS_ERROR;
    bool bSuccess = false;

    if ((true == CAN_IsInitialized()) &&
        (NULL != frame))
    {
        Status =
            FLEXCAN_Read(
                IP_FLEXCAN0,
                &FlexcanFrame);

        if (FLEXCAN_STATUS_OK == Status)
        {
            CAN_FromFlexcanFrame(
                &FlexcanFrame,
                frame);

            bSuccess = true;
        }
    }

    return bSuccess;
}

/**
 * @copydoc CAN_onReceive
 */
void CAN_onReceive(CAN_Callback_t callback)
{
    s_pfCanReceiveCallback =
        callback;

    return;
}