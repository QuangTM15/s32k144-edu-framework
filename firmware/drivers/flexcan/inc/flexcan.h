#ifndef FLEXCAN_H
#define FLEXCAN_H

/**
 * @file flexcan.h
 * @brief FlexCAN driver interface for NXP S32K144.
 *
 * @details
 * This file declares the low-level FlexCAN driver used by EduFramework.
 *
 * The driver is responsible for:
 * - FlexCAN module initialization and shutdown.
 * - CAN clock and bit-timing configuration.
 * - Classical CAN and CAN FD frame handling.
 * - Standard and extended identifiers.
 * - Data and remote frames for Classical CAN.
 * - Message Buffer configuration and access.
 * - Receive filtering with global and individual masks.
 * - Rx FIFO configuration and reception.
 * - Polling and interrupt-driven operation.
 * - Tx completion and abort handling.
 * - Error, warning, Bus-Off, and synchronization status.
 * - Normal, Loop-Back, and Listen-Only operating modes.
 * - Pretended Networking wake-up filtering.
 *
 * This module belongs to the Driver Layer and must not depend on the
 * Arduino-style API layer.
 */

#include "S32K144.h"

#include <stdbool.h>
#include <stdint.h>

/* ========================================================================= */
/* Common Constants                                                          */
/* ========================================================================= */

/** @brief Maximum number of Message Buffers implemented by FlexCAN0. */
#define FLEXCAN_MAX_MB_COUNT (32U)

/** @brief Maximum Classical CAN payload length in bytes. */
#define FLEXCAN_CLASSIC_MAX_DATA_LENGTH (8U)

/** @brief Maximum CAN FD payload length in bytes. */
#define FLEXCAN_FD_MAX_DATA_LENGTH (64U)

/** @brief Software receive queue capacity. */
#define FLEXCAN_RX_QUEUE_SIZE (16U)

/** @brief Invalid Message Buffer index. */
#define FLEXCAN_INVALID_MB_INDEX (0xFFU)

/** @brief Source marker used for frames received through Rx FIFO. */
#define FLEXCAN_RX_FIFO_SOURCE (0xFEU)

/** @brief Maximum valid standard CAN identifier. */
#define FLEXCAN_STANDARD_ID_MAX (0x7FFUL)

/** @brief Maximum valid extended CAN identifier. */
#define FLEXCAN_EXTENDED_ID_MAX (0x1FFFFFFFUL)

/** @brief Default polling timeout used by blocking driver operations. */
#define FLEXCAN_DEFAULT_TIMEOUT (1000000UL)

/* ========================================================================= */
/* Status                                                                     */
/* ========================================================================= */

/** @brief FlexCAN operation status type. */
typedef uint8_t FLEXCAN_Status_t;

#define FLEXCAN_STATUS_OK ((FLEXCAN_Status_t)0U)
#define FLEXCAN_STATUS_ERROR ((FLEXCAN_Status_t)1U)
#define FLEXCAN_STATUS_INVALID_ARGUMENT ((FLEXCAN_Status_t)2U)
#define FLEXCAN_STATUS_TIMEOUT ((FLEXCAN_Status_t)3U)
#define FLEXCAN_STATUS_BUSY ((FLEXCAN_Status_t)4U)
#define FLEXCAN_STATUS_NOT_INITIALIZED ((FLEXCAN_Status_t)5U)
#define FLEXCAN_STATUS_UNSUPPORTED ((FLEXCAN_Status_t)6U)
#define FLEXCAN_STATUS_NO_DATA ((FLEXCAN_Status_t)7U)
#define FLEXCAN_STATUS_QUEUE_FULL ((FLEXCAN_Status_t)8U)

/* ========================================================================= */
/* Operating Modes                                                            */
/* ========================================================================= */

/** @brief FlexCAN operating mode type. */
typedef uint8_t FLEXCAN_Mode_t;

#define FLEXCAN_MODE_NORMAL ((FLEXCAN_Mode_t)0U)
#define FLEXCAN_MODE_LOOPBACK ((FLEXCAN_Mode_t)1U)
#define FLEXCAN_MODE_LISTEN_ONLY ((FLEXCAN_Mode_t)2U)

/* ========================================================================= */
/* Clock Selection                                                            */
/* ========================================================================= */

/** @brief FlexCAN protocol-engine clock source type. */
typedef uint8_t FLEXCAN_ClockSource_t;

/** @brief Use oscillator clock as CAN protocol-engine clock. */
#define FLEXCAN_CLOCK_OSCILLATOR ((FLEXCAN_ClockSource_t)0U)

/** @brief Use peripheral clock as CAN protocol-engine clock. */
#define FLEXCAN_CLOCK_PERIPHERAL ((FLEXCAN_ClockSource_t)1U)

/* ========================================================================= */
/* Bit Timing                                                                 */
/* ========================================================================= */

/** @brief Bit timing selection type. */
typedef uint8_t FLEXCAN_TimingMode_t;

/** @brief Driver calculates timing from clock frequency and requested bit rate. */
#define FLEXCAN_TIMING_AUTOMATIC ((FLEXCAN_TimingMode_t)0U)

/** @brief Use timing values provided directly by the configuration structure. */
#define FLEXCAN_TIMING_MANUAL ((FLEXCAN_TimingMode_t)1U)

/**
 * @brief CAN bit timing expressed in actual time-quanta values.
 *
 * @details
 * Values in this structure are physical values rather than raw register field
 * encodings. The driver converts them to the corresponding FlexCAN register
 * values.
 */
typedef struct
{
    /** @brief CAN clock prescaler divisor. */
    uint16_t u16Prescaler;

    /** @brief Propagation segment length in time quanta. */
    uint8_t u8PropSeg;

    /** @brief Phase segment 1 length in time quanta. */
    uint8_t u8PhaseSeg1;

    /** @brief Phase segment 2 length in time quanta. */
    uint8_t u8PhaseSeg2;

    /** @brief Resynchronization jump width in time quanta. */
    uint8_t u8Rjw;

    /** @brief Enable triple sampling where supported. */
    bool bTripleSampling;
} FLEXCAN_BitTiming_t;

/* ========================================================================= */
/* Frame Format and Type                                                      */
/* ========================================================================= */

/** @brief CAN identifier format type. */
typedef uint8_t FLEXCAN_FrameFormat_t;

#define FLEXCAN_FRAME_STANDARD ((FLEXCAN_FrameFormat_t)0U)
#define FLEXCAN_FRAME_EXTENDED ((FLEXCAN_FrameFormat_t)1U)

/** @brief CAN frame type. */
typedef uint8_t FLEXCAN_FrameType_t;

#define FLEXCAN_FRAME_DATA ((FLEXCAN_FrameType_t)0U)
#define FLEXCAN_FRAME_REMOTE ((FLEXCAN_FrameType_t)1U)

/* ========================================================================= */
/* CAN FD                                                                     */
/* ========================================================================= */

/** @brief CAN FD Message Buffer payload size type. */
typedef uint8_t FLEXCAN_PayloadSize_t;

#define FLEXCAN_PAYLOAD_8_BYTES ((FLEXCAN_PayloadSize_t)8U)
#define FLEXCAN_PAYLOAD_16_BYTES ((FLEXCAN_PayloadSize_t)16U)
#define FLEXCAN_PAYLOAD_32_BYTES ((FLEXCAN_PayloadSize_t)32U)
#define FLEXCAN_PAYLOAD_64_BYTES ((FLEXCAN_PayloadSize_t)64U)

/* ========================================================================= */
/* Tx Arbitration                                                             */
/* ========================================================================= */

/** @brief Transmit Message Buffer arbitration mode. */
typedef uint8_t FLEXCAN_TxPriorityMode_t;

/** @brief Arbitration primarily follows CAN identifier priority. */
#define FLEXCAN_TX_PRIORITY_ID ((FLEXCAN_TxPriorityMode_t)0U)

/** @brief Use local priority field together with CAN identifier priority. */
#define FLEXCAN_TX_PRIORITY_LOCAL ((FLEXCAN_TxPriorityMode_t)1U)

/** @brief Lowest numbered active Tx Message Buffer has local priority. */
#define FLEXCAN_TX_PRIORITY_LOWEST_MB ((FLEXCAN_TxPriorityMode_t)2U)

/* ========================================================================= */
/* Rx FIFO                                                                    */
/* ========================================================================= */

/** @brief Rx FIFO filter table format. */
typedef uint8_t FLEXCAN_RxFifoFilterFormat_t;

#define FLEXCAN_RX_FIFO_FILTER_FORMAT_A ((FLEXCAN_RxFifoFilterFormat_t)0U)
#define FLEXCAN_RX_FIFO_FILTER_FORMAT_B ((FLEXCAN_RxFifoFilterFormat_t)1U)
#define FLEXCAN_RX_FIFO_FILTER_FORMAT_C ((FLEXCAN_RxFifoFilterFormat_t)2U)
#define FLEXCAN_RX_FIFO_FILTER_FORMAT_REJECT_ALL ((FLEXCAN_RxFifoFilterFormat_t)3U)

/**
 * @brief Logical Rx FIFO filter definition.
 *
 * @details
 * For Format A, one logical filter occupies one table element.
 * For Format B, two logical filters are packed into one table element.
 * For Format C, four partial-ID filters are packed into one table element.
 */
typedef struct
{
    /** @brief Identifier used by the filter. */
    uint32_t u32Id;

    /** @brief Standard or extended identifier format. */
    FLEXCAN_FrameFormat_t u8Format;

    /** @brief Data or remote frame selection. */
    FLEXCAN_FrameType_t u8FrameType;
} FLEXCAN_RxFifoFilter_t;

/**
 * @brief Rx FIFO configuration.
 */
typedef struct
{
    /** @brief FIFO ID filter table format. */
    FLEXCAN_RxFifoFilterFormat_t u8FilterFormat;

    /** @brief Pointer to logical filter definitions. */
    const FLEXCAN_RxFifoFilter_t *pxFilters;

    /** @brief Number of logical filter definitions in pxFilters. */
    uint16_t u16FilterCount;

    /**
     * @brief Global FIFO acceptance mask.
     *
     * @details
     * A set mask bit means the corresponding identifier bit is checked.
     * A clear mask bit means that identifier bit is treated as don't-care.
     */
    uint32_t u32GlobalMask;

    /**
     * @brief Optional pointer to individual FIFO filter masks.
     *
     * @details
     * Passing a null pointer selects global masking for all FIFO filters.
     */
    const uint32_t *pu32IndividualMasks;

    /** @brief Enable interrupt when a frame is available in FIFO. */
    bool bEnableFrameAvailableInterrupt;

    /** @brief Enable FIFO warning interrupt. */
    bool bEnableWarningInterrupt;

    /** @brief Enable FIFO overflow interrupt. */
    bool bEnableOverflowInterrupt;
} FLEXCAN_RxFifoConfig_t;

/* ========================================================================= */
/* Pretended Networking                                                       */
/* ========================================================================= */

/** @brief Pretended Networking comparison type. */
typedef uint8_t FLEXCAN_PnComparison_t;

#define FLEXCAN_PN_COMPARE_EXACT ((FLEXCAN_PnComparison_t)0U)
#define FLEXCAN_PN_COMPARE_GREATER_OR_EQUAL ((FLEXCAN_PnComparison_t)1U)
#define FLEXCAN_PN_COMPARE_LESS_OR_EQUAL ((FLEXCAN_PnComparison_t)2U)
#define FLEXCAN_PN_COMPARE_RANGE ((FLEXCAN_PnComparison_t)3U)

/** @brief Pretended Networking filter-combination type. */
typedef uint8_t FLEXCAN_PnCombination_t;

#define FLEXCAN_PN_FILTER_ID_ONLY ((FLEXCAN_PnCombination_t)0U)
#define FLEXCAN_PN_FILTER_ID_AND_PAYLOAD ((FLEXCAN_PnCombination_t)1U)
#define FLEXCAN_PN_FILTER_ID_N_TIMES ((FLEXCAN_PnCombination_t)2U)
#define FLEXCAN_PN_FILTER_ID_PAYLOAD_N_TIMES ((FLEXCAN_PnCombination_t)3U)

/**
 * @brief Pretended Networking wake-up filter configuration.
 *
 * @details
 * Pretended Networking applies only to Classical CAN reception. CAN FD frames
 * are not used as Pretended Networking wake-up frames.
 */
typedef struct
{
    /** @brief ID comparison mode. */
    FLEXCAN_PnComparison_t u8IdComparison;

    /** @brief Payload comparison mode. */
    FLEXCAN_PnComparison_t u8PayloadComparison;

    /** @brief ID/payload filter-combination mode. */
    FLEXCAN_PnCombination_t u8Combination;

    /** @brief Target frame identifier format. */
    FLEXCAN_FrameFormat_t u8Format;

    /** @brief Target frame type. */
    FLEXCAN_FrameType_t u8FrameType;

    /** @brief Include IDE in ID filtering. */
    bool bMaskFormat;

    /** @brief Include RTR in ID filtering. */
    bool bMaskFrameType;

    /** @brief Exact target, minimum value, or lower range limit for ID filtering. */
    uint32_t u32Id1;

    /** @brief Upper range limit for ID filtering. */
    uint32_t u32Id2;

    /** @brief ID mask used by exact-ID comparison. */
    uint32_t u32IdMask;

    /** @brief Lower DLC accepted by payload filtering. */
    uint8_t u8DlcLow;

    /** @brief Upper DLC accepted by payload filtering. */
    uint8_t u8DlcHigh;

    /** @brief Exact target, minimum value, or lower range payload value. */
    uint8_t au8Payload1[FLEXCAN_CLASSIC_MAX_DATA_LENGTH];

    /** @brief Upper range payload value. */
    uint8_t au8Payload2[FLEXCAN_CLASSIC_MAX_DATA_LENGTH];

    /** @brief Payload mask used by exact-payload comparison. */
    uint8_t au8PayloadMask[FLEXCAN_CLASSIC_MAX_DATA_LENGTH];

    /** @brief Required number of matching messages, from 1 to 255. */
    uint8_t u8MatchCount;

    /** @brief Pretended Networking timeout comparison value. */
    uint16_t u16MatchTimeout;

    /** @brief Enable wake-up generation after a successful filter match. */
    bool bEnableMatchWakeup;

    /** @brief Enable wake-up generation after timeout. */
    bool bEnableTimeoutWakeup;
} FLEXCAN_PnConfig_t;

/* ========================================================================= */
/* Frame Object                                                               */
/* ========================================================================= */

/**
 * @brief CAN frame object used by transmit and receive APIs.
 */
typedef struct
{
    /** @brief 11-bit standard ID or 29-bit extended ID. */
    uint32_t u32Id;

    /** @brief Frame payload. */
    uint8_t au8Data[FLEXCAN_FD_MAX_DATA_LENGTH];

    /**
     * @brief Number of payload bytes.
     *
     * @details
     * Classical CAN supports 0 to 8 bytes. CAN FD supports the valid CAN FD
     * payload lengths up to 64 bytes.
     */
    uint8_t u8Length;

    /** @brief Message Buffer that produced the frame, or FLEXCAN_RX_FIFO_SOURCE. */
    uint8_t u8MbIndex;

    /** @brief Local transmit priority value, from 0 to 7 when enabled. */
    uint8_t u8LocalPriority;

    /** @brief Standard or extended identifier format. */
    FLEXCAN_FrameFormat_t u8Format;

    /** @brief Data or remote frame type. */
    FLEXCAN_FrameType_t u8FrameType;

    /** @brief Received or transmitted hardware timestamp. */
    uint16_t u16Timestamp;

    /** @brief true selects CAN FD format; false selects Classical CAN. */
    bool bFD;

    /** @brief CAN FD Bit Rate Switch flag. */
    bool bBitRateSwitch;

    /** @brief CAN FD Error State Indicator flag. */
    bool bErrorStateIndicator;

    /** @brief true indicates that an Rx Message Buffer reported overrun. */
    bool bOverrun;
} FLEXCAN_Frame_t;

/* ========================================================================= */
/* Message Buffer Configuration                                               */
/* ========================================================================= */

/**
 * @brief Receive Message Buffer configuration.
 */
typedef struct
{
    /** @brief Receive Message Buffer number. */
    uint8_t u8MbIndex;

    /** @brief Identifier used as receive-match target. */
    uint32_t u32Id;

    /** @brief Identifier acceptance mask. */
    uint32_t u32Mask;

    /** @brief Standard or extended identifier format. */
    FLEXCAN_FrameFormat_t u8Format;

    /** @brief Data or remote frame selection. */
    FLEXCAN_FrameType_t u8FrameType;

    /** @brief Enable interrupt for this receive Message Buffer. */
    bool bEnableInterrupt;
} FLEXCAN_RxMbConfig_t;

/* ========================================================================= */
/* Driver Initialization Configuration                                        */
/* ========================================================================= */

/**
 * @brief FlexCAN initialization configuration.
 */
typedef struct
{
    /** @brief Protocol-engine clock source. */
    FLEXCAN_ClockSource_t u8ClockSource;

    /**
     * @brief Frequency of the selected CAN protocol-engine clock in Hz.
     */
    uint32_t u32ClockFrequencyHz;

    /** @brief Requested nominal CAN bit rate in bits per second. */
    uint32_t u32NominalBitRate;

    /** @brief Automatic or manual nominal bit timing. */
    FLEXCAN_TimingMode_t u8NominalTimingMode;

    /** @brief Manual nominal timing values when manual timing is selected. */
    FLEXCAN_BitTiming_t xNominalTiming;

    /** @brief Initial operating mode. */
    FLEXCAN_Mode_t u8Mode;

    /** @brief Tx arbitration priority strategy. */
    FLEXCAN_TxPriorityMode_t u8TxPriorityMode;

    /**
     * @brief Index of the last Message Buffer participating in matching and
     * arbitration.
     */
    uint8_t u8MaxMessageBuffer;

    /** @brief Enable reception of frames transmitted by the same FlexCAN node. */
    bool bEnableSelfReception;

    /** @brief Enable safe transmission abort mechanism. */
    bool bEnableTxAbort;

    /** @brief Enable individual receive masks through RXIMR. */
    bool bEnableIndividualMasking;

    /** @brief Enable automatic Bus-Off recovery. */
    bool bEnableAutomaticBusOffRecovery;

    /** @brief Enable CAN FD operation. */
    bool bEnableFD;

    /** @brief Message Buffer payload size when CAN FD is enabled. */
    FLEXCAN_PayloadSize_t u8FdPayloadSize;

    /** @brief Requested CAN FD data-phase bit rate in bits per second. */
    uint32_t u32DataBitRate;

    /** @brief Automatic or manual CAN FD data-phase timing. */
    FLEXCAN_TimingMode_t u8DataTimingMode;

    /** @brief Manual CAN FD data-phase timing values. */
    FLEXCAN_BitTiming_t xDataTiming;

    /** @brief Enable use of CAN FD Bit Rate Switch. */
    bool bEnableFdBitRateSwitch;

    /** @brief Enable CAN FD Transceiver Delay Compensation. */
    bool bEnableTransceiverDelayCompensation;

    /** @brief CAN FD Transceiver Delay Compensation offset. */
    uint8_t u8TransceiverDelayOffset;
} FLEXCAN_Config_t;

/* ========================================================================= */
/* Interrupt Sources and Events                                               */
/* ========================================================================= */

/** @brief FlexCAN non-Message-Buffer interrupt source mask type. */
typedef uint32_t FLEXCAN_InterruptSource_t;

#define FLEXCAN_INTERRUPT_ERROR ((FLEXCAN_InterruptSource_t)(1UL << 0U))
#define FLEXCAN_INTERRUPT_BUS_OFF ((FLEXCAN_InterruptSource_t)(1UL << 1U))
#define FLEXCAN_INTERRUPT_BUS_OFF_DONE ((FLEXCAN_InterruptSource_t)(1UL << 2U))
#define FLEXCAN_INTERRUPT_TX_WARNING ((FLEXCAN_InterruptSource_t)(1UL << 3U))
#define FLEXCAN_INTERRUPT_RX_WARNING ((FLEXCAN_InterruptSource_t)(1UL << 4U))
#define FLEXCAN_INTERRUPT_WAKEUP ((FLEXCAN_InterruptSource_t)(1UL << 5U))
#define FLEXCAN_INTERRUPT_PN_MATCH ((FLEXCAN_InterruptSource_t)(1UL << 6U))
#define FLEXCAN_INTERRUPT_PN_TIMEOUT ((FLEXCAN_InterruptSource_t)(1UL << 7U))

/** @brief FlexCAN callback event type. */
typedef uint32_t FLEXCAN_Event_t;

#define FLEXCAN_EVENT_RX_MB ((FLEXCAN_Event_t)(1UL << 0U))
#define FLEXCAN_EVENT_TX_MB ((FLEXCAN_Event_t)(1UL << 1U))
#define FLEXCAN_EVENT_RX_FIFO ((FLEXCAN_Event_t)(1UL << 2U))
#define FLEXCAN_EVENT_RX_FIFO_WARNING ((FLEXCAN_Event_t)(1UL << 3U))
#define FLEXCAN_EVENT_RX_FIFO_OVERFLOW ((FLEXCAN_Event_t)(1UL << 4U))
#define FLEXCAN_EVENT_ERROR ((FLEXCAN_Event_t)(1UL << 5U))
#define FLEXCAN_EVENT_BUS_OFF ((FLEXCAN_Event_t)(1UL << 6U))
#define FLEXCAN_EVENT_BUS_OFF_RECOVERED ((FLEXCAN_Event_t)(1UL << 7U))
#define FLEXCAN_EVENT_TX_WARNING ((FLEXCAN_Event_t)(1UL << 8U))
#define FLEXCAN_EVENT_RX_WARNING ((FLEXCAN_Event_t)(1UL << 9U))
#define FLEXCAN_EVENT_WAKEUP ((FLEXCAN_Event_t)(1UL << 10U))
#define FLEXCAN_EVENT_PN_MATCH ((FLEXCAN_Event_t)(1UL << 11U))
#define FLEXCAN_EVENT_PN_TIMEOUT ((FLEXCAN_Event_t)(1UL << 12U))
#define FLEXCAN_EVENT_RX_QUEUE_OVERFLOW ((FLEXCAN_Event_t)(1UL << 13U))

/**
 * @brief FlexCAN driver callback type.
 *
 * @details
 * The callback executes from interrupt context and must remain short and
 * non-blocking. u8MbIndex is FLEXCAN_INVALID_MB_INDEX for events that are not
 * associated with a specific Message Buffer.
 */
typedef void (*FLEXCAN_Callback_t)(FLEXCAN_Type *pBase,
                                   FLEXCAN_Event_t u32Event,
                                   uint8_t u8MbIndex);

/* ========================================================================= */
/* Error and Diagnostic Status                                                */
/* ========================================================================= */

/**
 * @brief FlexCAN error and diagnostic snapshot.
 */
typedef struct
{
    /** @brief Raw Error and Status 1 register snapshot. */
    uint32_t u32Esr1;

    /** @brief Transmit error counter. */
    uint8_t u8TxErrorCount;

    /** @brief Receive error counter. */
    uint8_t u8RxErrorCount;

    /** @brief CAN FD fast-phase transmit error counter. */
    uint8_t u8TxFastErrorCount;

    /** @brief CAN FD fast-phase receive error counter. */
    uint8_t u8RxFastErrorCount;

    bool bSynchronized;
    bool bBusOff;
    bool bTxWarning;
    bool bRxWarning;

    bool bStuffError;
    bool bFormError;
    bool bCrcError;
    bool bAckError;
    bool bBit0Error;
    bool bBit1Error;

    bool bFastStuffError;
    bool bFastFormError;
    bool bFastCrcError;
    bool bFastBit0Error;
    bool bFastBit1Error;
} FLEXCAN_ErrorStatus_t;

/**
 * @brief Pretended Networking status snapshot.
 */
typedef struct
{
    /** @brief Wake-up match flag. */
    bool bMatchDetected;

    /** @brief Wake-up timeout flag. */
    bool bTimeoutDetected;

    /** @brief Number of matching messages detected by PN logic. */
    uint8_t u8MatchCount;
} FLEXCAN_PnStatus_t;

/* ========================================================================= */
/* Configuration and Module Control                                           */
/* ========================================================================= */

/**
 * @brief Fill a FlexCAN configuration structure with recommended defaults.
 *
 * @param[out] pConfig Pointer to configuration structure.
 */
void FLEXCAN_GetDefaultConfig(FLEXCAN_Config_t *pConfig);

/**
 * @brief Initialize a FlexCAN peripheral instance.
 *
 * @param[in] pBase FlexCAN peripheral base address.
 * @param[in] pConfig Initialization configuration.
 *
 * @return FLEXCAN_Status_t
 */
FLEXCAN_Status_t FLEXCAN_Init(FLEXCAN_Type *pBase,
                              const FLEXCAN_Config_t *pConfig);

/**
 * @brief Deinitialize a FlexCAN peripheral instance.
 */
FLEXCAN_Status_t FLEXCAN_Deinit(FLEXCAN_Type *pBase);

/**
 * @brief Enable the selected FlexCAN module.
 */
FLEXCAN_Status_t FLEXCAN_Enable(FLEXCAN_Type *pBase);

/**
 * @brief Disable the selected FlexCAN module.
 */
FLEXCAN_Status_t FLEXCAN_Disable(FLEXCAN_Type *pBase);

/**
 * @brief Enter FlexCAN Freeze mode and wait for acknowledgement.
 */
FLEXCAN_Status_t FLEXCAN_EnterFreezeMode(FLEXCAN_Type *pBase,
                                         uint32_t u32Timeout);

/**
 * @brief Exit FlexCAN Freeze mode and wait until the module is ready.
 */
FLEXCAN_Status_t FLEXCAN_ExitFreezeMode(FLEXCAN_Type *pBase,
                                        uint32_t u32Timeout);

/**
 * @brief Change FlexCAN operating mode.
 */
FLEXCAN_Status_t FLEXCAN_SetMode(FLEXCAN_Type *pBase,
                                 FLEXCAN_Mode_t u8Mode);

/* ========================================================================= */
/* Bit Timing                                                                 */
/* ========================================================================= */

/**
 * @brief Calculate a valid nominal CAN bit timing configuration.
 */
FLEXCAN_Status_t FLEXCAN_CalculateNominalBitTiming(uint32_t u32ClockFrequencyHz,
                                                   uint32_t u32BitRate,
                                                   FLEXCAN_BitTiming_t *pTiming);

/**
 * @brief Calculate a valid CAN FD data-phase timing configuration.
 */
FLEXCAN_Status_t FLEXCAN_CalculateDataBitTiming(uint32_t u32ClockFrequencyHz,
                                                uint32_t u32BitRate,
                                                FLEXCAN_BitTiming_t *pTiming);

/**
 * @brief Apply nominal CAN bit timing.
 */
FLEXCAN_Status_t FLEXCAN_SetNominalBitTiming(FLEXCAN_Type *pBase,
                                             const FLEXCAN_BitTiming_t *pTiming);

/**
 * @brief Apply CAN FD data-phase bit timing.
 */
FLEXCAN_Status_t FLEXCAN_SetDataBitTiming(FLEXCAN_Type *pBase,
                                          const FLEXCAN_BitTiming_t *pTiming);

/* ========================================================================= */
/* Message Buffer Configuration                                               */
/* ========================================================================= */

/**
 * @brief Configure one Message Buffer for transmission.
 *
 * @param[in] pBase FlexCAN peripheral base address.
 * @param[in] u8MbIndex Message Buffer index.
 * @param[in] bEnableInterrupt Enable Tx completion interrupt for this MB.
 */
FLEXCAN_Status_t FLEXCAN_ConfigTxMb(FLEXCAN_Type *pBase,
                                    uint8_t u8MbIndex,
                                    bool bEnableInterrupt);

/**
 * @brief Configure one Message Buffer for reception.
 */
FLEXCAN_Status_t FLEXCAN_ConfigRxMb(FLEXCAN_Type *pBase,
                                    const FLEXCAN_RxMbConfig_t *pConfig);

/**
 * @brief Disable one Message Buffer.
 */
FLEXCAN_Status_t FLEXCAN_DisableMb(FLEXCAN_Type *pBase,
                                   uint8_t u8MbIndex);

/**
 * @brief Configure an individual receive mask for one Message Buffer.
 */
FLEXCAN_Status_t FLEXCAN_SetRxMbMask(FLEXCAN_Type *pBase,
                                     uint8_t u8MbIndex,
                                     uint32_t u32Mask,
                                     FLEXCAN_FrameFormat_t u8Format);

/**
 * @brief Configure the legacy global receive mask.
 */
FLEXCAN_Status_t FLEXCAN_SetGlobalRxMask(FLEXCAN_Type *pBase,
                                         uint32_t u32Mask,
                                         FLEXCAN_FrameFormat_t u8Format);

/* ========================================================================= */
/* Transmission                                                               */
/* ========================================================================= */

/**
 * @brief Start non-blocking transmission using a configured Tx Message Buffer.
 */
FLEXCAN_Status_t FLEXCAN_Transmit(FLEXCAN_Type *pBase,
                                  uint8_t u8MbIndex,
                                  const FLEXCAN_Frame_t *pFrame);

/**
 * @brief Transmit a frame and wait for completion or timeout.
 */
FLEXCAN_Status_t FLEXCAN_TransmitBlocking(FLEXCAN_Type *pBase,
                                          uint8_t u8MbIndex,
                                          const FLEXCAN_Frame_t *pFrame,
                                          uint32_t u32Timeout);

/**
 * @brief Check whether a Tx Message Buffer completed transmission.
 */
bool FLEXCAN_IsTxComplete(FLEXCAN_Type *pBase,
                          uint8_t u8MbIndex);

/**
 * @brief Abort an active transmission safely when abort support is enabled.
 */
FLEXCAN_Status_t FLEXCAN_AbortTransmit(FLEXCAN_Type *pBase,
                                       uint8_t u8MbIndex,
                                       uint32_t u32Timeout);

/* ========================================================================= */
/* Polling Reception                                                          */
/* ========================================================================= */

/**
 * @brief Check whether a configured Rx Message Buffer has a new frame flag.
 */
bool FLEXCAN_IsRxReady(FLEXCAN_Type *pBase,
                       uint8_t u8MbIndex);

/**
 * @brief Read one frame directly from a receive Message Buffer.
 *
 * @details
 * This function follows the required FlexCAN receive service sequence,
 * including Message Buffer locking, IFLAG acknowledgement, and TIMER read to
 * unlock the Message Buffer.
 */
FLEXCAN_Status_t FLEXCAN_Receive(FLEXCAN_Type *pBase,
                                 uint8_t u8MbIndex,
                                 FLEXCAN_Frame_t *pFrame);

/* ========================================================================= */
/* Interrupt-Driven Receive Queue                                             */
/* ========================================================================= */

/**
 * @brief Check whether the software receive queue contains at least one frame.
 */
bool FLEXCAN_IsDataAvailable(FLEXCAN_Type *pBase);

/**
 * @brief Get one frame from the software receive queue.
 */
FLEXCAN_Status_t FLEXCAN_Read(FLEXCAN_Type *pBase,
                              FLEXCAN_Frame_t *pFrame);

/**
 * @brief Return the number of frames currently stored in the software queue.
 */
uint8_t FLEXCAN_GetRxQueueCount(FLEXCAN_Type *pBase);

/**
 * @brief Remove all frames from the software receive queue.
 */
void FLEXCAN_FlushRxQueue(FLEXCAN_Type *pBase);

/* ========================================================================= */
/* Rx FIFO                                                                    */
/* ========================================================================= */

/**
 * @brief Configure and enable the hardware Rx FIFO.
 *
 * @details
 * Rx FIFO is available for Classical CAN operation and is not used while CAN
 * FD operation is enabled.
 */
FLEXCAN_Status_t FLEXCAN_ConfigRxFifo(FLEXCAN_Type *pBase,
                                      const FLEXCAN_RxFifoConfig_t *pConfig);

/**
 * @brief Disable the hardware Rx FIFO.
 */
FLEXCAN_Status_t FLEXCAN_DisableRxFifo(FLEXCAN_Type *pBase);

/**
 * @brief Check whether at least one frame is available in hardware Rx FIFO.
 */
bool FLEXCAN_IsRxFifoReady(FLEXCAN_Type *pBase);

/**
 * @brief Read one frame directly from the hardware Rx FIFO.
 */
FLEXCAN_Status_t FLEXCAN_ReceiveRxFifo(FLEXCAN_Type *pBase,
                                       FLEXCAN_Frame_t *pFrame);

/* ========================================================================= */
/* Message Buffer Interrupt Control                                           */
/* ========================================================================= */

/**
 * @brief Enable interrupt generation for one Message Buffer.
 */
FLEXCAN_Status_t FLEXCAN_EnableMbInterrupt(FLEXCAN_Type *pBase,
                                           uint8_t u8MbIndex);

/**
 * @brief Disable interrupt generation for one Message Buffer.
 */
FLEXCAN_Status_t FLEXCAN_DisableMbInterrupt(FLEXCAN_Type *pBase,
                                            uint8_t u8MbIndex);

/**
 * @brief Read all Message Buffer interrupt flags.
 */
uint32_t FLEXCAN_GetMbInterruptFlags(FLEXCAN_Type *pBase);

/**
 * @brief Clear selected Message Buffer interrupt flags.
 *
 * @details
 * IFLAG1 is write-one-to-clear. Only bits set in u32Mask are cleared.
 */
void FLEXCAN_ClearMbInterruptFlags(FLEXCAN_Type *pBase,
                                   uint32_t u32Mask);

/* ========================================================================= */
/* Non-Message-Buffer Interrupt Control                                       */
/* ========================================================================= */

/**
 * @brief Enable selected error, status, warning, or wake-up interrupt sources.
 */
FLEXCAN_Status_t FLEXCAN_EnableInterruptSources(FLEXCAN_Type *pBase,
                                                FLEXCAN_InterruptSource_t u32Sources);

/**
 * @brief Disable selected error, status, warning, or wake-up interrupt sources.
 */
FLEXCAN_Status_t FLEXCAN_DisableInterruptSources(FLEXCAN_Type *pBase,
                                                 FLEXCAN_InterruptSource_t u32Sources);

/**
 * @brief Register the driver event callback for one FlexCAN instance.
 */
void FLEXCAN_SetCallback(FLEXCAN_Type *pBase,
                         FLEXCAN_Callback_t pfCallback);

/* ========================================================================= */
/* Error and Diagnostic APIs                                                  */
/* ========================================================================= */

/**
 * @brief Read current FlexCAN error counters and error/status flags.
 */
FLEXCAN_Status_t FLEXCAN_GetErrorStatus(FLEXCAN_Type *pBase,
                                        FLEXCAN_ErrorStatus_t *pStatus);

/**
 * @brief Clear selected writable error/status flags in ESR1.
 */
void FLEXCAN_ClearErrorFlags(FLEXCAN_Type *pBase,
                             uint32_t u32Mask);

/**
 * @brief Check whether FlexCAN is synchronized to the CAN bus.
 */
bool FLEXCAN_IsSynchronized(FLEXCAN_Type *pBase);

/**
 * @brief Check whether FlexCAN is currently in Bus-Off state.
 */
bool FLEXCAN_IsBusOff(FLEXCAN_Type *pBase);

/**
 * @brief Read the 16-bit FlexCAN free-running timer.
 */
uint16_t FLEXCAN_GetTimer(FLEXCAN_Type *pBase);

/* ========================================================================= */
/* Pretended Networking                                                       */
/* ========================================================================= */

/**
 * @brief Configure Pretended Networking wake-up filtering.
 */
FLEXCAN_Status_t FLEXCAN_ConfigPretendedNetworking(FLEXCAN_Type *pBase,
                                                   const FLEXCAN_PnConfig_t *pConfig);

/**
 * @brief Enable Pretended Networking operation.
 */
FLEXCAN_Status_t FLEXCAN_EnablePretendedNetworking(FLEXCAN_Type *pBase);

/**
 * @brief Disable Pretended Networking operation.
 */
FLEXCAN_Status_t FLEXCAN_DisablePretendedNetworking(FLEXCAN_Type *pBase);

/**
 * @brief Read Pretended Networking match/timeout status.
 */
FLEXCAN_Status_t FLEXCAN_GetPretendedNetworkingStatus(FLEXCAN_Type *pBase,
                                                      FLEXCAN_PnStatus_t *pStatus);

/**
 * @brief Read one Pretended Networking wake-up Message Buffer.
 *
 * @param[in] u8WakeBufferIndex Wake-up Message Buffer index from 0 to 3.
 */
FLEXCAN_Status_t FLEXCAN_GetPretendedNetworkingFrame(FLEXCAN_Type *pBase,
                                                     uint8_t u8WakeBufferIndex,
                                                     FLEXCAN_Frame_t *pFrame);

/* ========================================================================= */
/* Driver IRQ Handlers                                                        */
/* ========================================================================= */

/**
 * @brief Service an OR'ed Message Buffer interrupt range.
 *
 * @details
 * The IRQ layer calls this handler for the hardware vector associated with the
 * supplied Message Buffer range. On S32K144 FlexCAN0, MB0-15 and MB16-31 use
 * separate OR'ed NVIC vectors.
 */
void FLEXCAN_MBIRQHandler(FLEXCAN_Type *pBase, uint8_t u8FirstMb, uint8_t u8LastMb);

/**
 * @brief Service FlexCAN protocol error interrupts.
 */
void FLEXCAN_ErrorIRQHandler(FLEXCAN_Type *pBase);

/**
 * @brief Service Bus-Off, warning, and related OR'ed status interrupts.
 */
void FLEXCAN_StatusIRQHandler(FLEXCAN_Type *pBase);

/**
 * @brief Service FlexCAN wake-up and Pretended Networking wake-up interrupts.
 */
void FLEXCAN_WakeUpIRQHandler(FLEXCAN_Type *pBase);

#endif /* FLEXCAN_H */
