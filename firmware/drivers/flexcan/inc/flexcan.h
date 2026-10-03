#ifndef FLEXCAN_H
#define FLEXCAN_H

/**
 * @file flexcan.h
 * @brief Classical FlexCAN driver interface for NXP S32K144.
 *
 * @details
 * The current EduFramework FlexCAN driver is verified for FlexCAN0 on the
 * MaaZEDU S32K144 board and provides:
 * - Module initialization and shutdown.
 * - Configurable Classical CAN nominal bit timing.
 * - Normal, Loop-Back, and Listen-Only modes.
 * - Standard 11-bit and extended 29-bit identifiers.
 * - Classical CAN data and remote frames with payloads from 0 to 8 bytes.
 * - Message Buffer Tx/Rx, acceptance masking, blocking/non-blocking Tx.
 * - Polling receive and Message Buffer interrupt operation.
 * - A fixed-size software Rx queue for interrupt-driven reception.
 * - Basic synchronization, error-counter, and Bus-Off diagnostics.
 *
 * Rx FIFO, CAN FD data transfer, Pretended Networking, DMA, wake-up handling,
 * and advanced error/status interrupts are outside the current verified scope.
 * The configuration and frame structures retain a small set of CAN-FD-related
 * compatibility fields; enabling CAN FD returns FLEXCAN_STATUS_UNSUPPORTED.
 *
 * This module belongs to the Driver Layer and does not depend on the
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

/**
 * @brief Frame data storage capacity retained for API compatibility.
 *
 * @details
 * The current driver accepts Classical CAN frames only, therefore u8Length
 * must not exceed FLEXCAN_CLASSIC_MAX_DATA_LENGTH and bFD must be false.
 */
#define FLEXCAN_FD_MAX_DATA_LENGTH (64U)

/** @brief Software receive queue capacity. */
#define FLEXCAN_RX_QUEUE_SIZE (16U)

/** @brief Invalid Message Buffer index. */
#define FLEXCAN_INVALID_MB_INDEX (0xFFU)

/** @brief Maximum valid standard CAN identifier. */
#define FLEXCAN_STANDARD_ID_MAX (0x7FFUL)

/** @brief Maximum valid extended CAN identifier. */
#define FLEXCAN_EXTENDED_ID_MAX (0x1FFFFFFFUL)

/** @brief Default timeout used by bounded polling operations. */
#define FLEXCAN_DEFAULT_TIMEOUT (1000000UL)

/* ========================================================================= */
/* Status                                                                    */
/* ========================================================================= */

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
/* Operating Mode                                                            */
/* ========================================================================= */

typedef uint8_t FLEXCAN_Mode_t;

#define FLEXCAN_MODE_NORMAL ((FLEXCAN_Mode_t)0U)
#define FLEXCAN_MODE_LOOPBACK ((FLEXCAN_Mode_t)1U)
#define FLEXCAN_MODE_LISTEN_ONLY ((FLEXCAN_Mode_t)2U)

/* ========================================================================= */
/* Clock Selection                                                           */
/* ========================================================================= */

typedef uint8_t FLEXCAN_ClockSource_t;

#define FLEXCAN_CLOCK_OSCILLATOR ((FLEXCAN_ClockSource_t)0U)
#define FLEXCAN_CLOCK_PERIPHERAL ((FLEXCAN_ClockSource_t)1U)

/* ========================================================================= */
/* Bit Timing                                                                */
/* ========================================================================= */

typedef uint8_t FLEXCAN_TimingMode_t;

#define FLEXCAN_TIMING_AUTOMATIC ((FLEXCAN_TimingMode_t)0U)
#define FLEXCAN_TIMING_MANUAL ((FLEXCAN_TimingMode_t)1U)

typedef struct
{
    uint16_t u16Prescaler;
    uint8_t u8PropSeg;
    uint8_t u8PhaseSeg1;
    uint8_t u8PhaseSeg2;
    uint8_t u8Rjw;
    bool bTripleSampling;
} FLEXCAN_BitTiming_t;

/* ========================================================================= */
/* Frame Format and Type                                                     */
/* ========================================================================= */

typedef uint8_t FLEXCAN_FrameFormat_t;

#define FLEXCAN_FRAME_STANDARD ((FLEXCAN_FrameFormat_t)0U)
#define FLEXCAN_FRAME_EXTENDED ((FLEXCAN_FrameFormat_t)1U)

typedef uint8_t FLEXCAN_FrameType_t;

#define FLEXCAN_FRAME_DATA ((FLEXCAN_FrameType_t)0U)
#define FLEXCAN_FRAME_REMOTE ((FLEXCAN_FrameType_t)1U)

/* ========================================================================= */
/* Tx Arbitration                                                            */
/* ========================================================================= */

typedef uint8_t FLEXCAN_TxPriorityMode_t;

#define FLEXCAN_TX_PRIORITY_ID ((FLEXCAN_TxPriorityMode_t)0U)
#define FLEXCAN_TX_PRIORITY_LOCAL ((FLEXCAN_TxPriorityMode_t)1U)
#define FLEXCAN_TX_PRIORITY_LOWEST_MB ((FLEXCAN_TxPriorityMode_t)2U)

/* ========================================================================= */
/* CAN FD Compatibility Fields                                               */
/* ========================================================================= */

/**
 * @brief CAN FD payload-size type retained for source compatibility only.
 *
 * @details
 * CAN FD operation is not implemented in this driver release.
 */
typedef uint8_t FLEXCAN_PayloadSize_t;

#define FLEXCAN_PAYLOAD_8_BYTES ((FLEXCAN_PayloadSize_t)8U)
#define FLEXCAN_PAYLOAD_16_BYTES ((FLEXCAN_PayloadSize_t)16U)
#define FLEXCAN_PAYLOAD_32_BYTES ((FLEXCAN_PayloadSize_t)32U)
#define FLEXCAN_PAYLOAD_64_BYTES ((FLEXCAN_PayloadSize_t)64U)

/* ========================================================================= */
/* Frame Object                                                              */
/* ========================================================================= */

typedef struct
{
    uint32_t u32Id;
    uint8_t au8Data[FLEXCAN_FD_MAX_DATA_LENGTH];
    uint8_t u8Length;
    uint8_t u8MbIndex;
    uint8_t u8LocalPriority;
    FLEXCAN_FrameFormat_t u8Format;
    FLEXCAN_FrameType_t u8FrameType;
    uint16_t u16Timestamp;

    /** Must remain false in the current Classical CAN driver. */
    bool bFD;

    /** Reserved for future CAN FD support. */
    bool bBitRateSwitch;

    /** Reserved for future CAN FD support. */
    bool bErrorStateIndicator;

    /** true when a receive Message Buffer reported overrun. */
    bool bOverrun;
} FLEXCAN_Frame_t;

/* ========================================================================= */
/* Message Buffer Configuration                                              */
/* ========================================================================= */

typedef struct
{
    uint8_t u8MbIndex;
    uint32_t u32Id;
    uint32_t u32Mask;
    FLEXCAN_FrameFormat_t u8Format;
    FLEXCAN_FrameType_t u8FrameType;
    bool bEnableInterrupt;
} FLEXCAN_RxMbConfig_t;

/* ========================================================================= */
/* Driver Initialization Configuration                                       */
/* ========================================================================= */

typedef struct
{
    FLEXCAN_ClockSource_t u8ClockSource;
    uint32_t u32ClockFrequencyHz;
    uint32_t u32NominalBitRate;
    FLEXCAN_TimingMode_t u8NominalTimingMode;
    FLEXCAN_BitTiming_t xNominalTiming;

    FLEXCAN_Mode_t u8Mode;
    FLEXCAN_TxPriorityMode_t u8TxPriorityMode;
    uint8_t u8MaxMessageBuffer;
    bool bEnableSelfReception;
    bool bEnableTxAbort;
    bool bEnableIndividualMasking;
    bool bEnableAutomaticBusOffRecovery;

    /*
     * Compatibility fields for the planned CAN FD extension.
     * bEnableFD=true is rejected with FLEXCAN_STATUS_UNSUPPORTED.
     */
    bool bEnableFD;
    FLEXCAN_PayloadSize_t u8FdPayloadSize;
    uint32_t u32DataBitRate;
    FLEXCAN_TimingMode_t u8DataTimingMode;
    FLEXCAN_BitTiming_t xDataTiming;
    bool bEnableFdBitRateSwitch;
    bool bEnableTransceiverDelayCompensation;
    uint8_t u8TransceiverDelayOffset;
} FLEXCAN_Config_t;

/* ========================================================================= */
/* Interrupt Events                                                          */
/* ========================================================================= */

typedef uint32_t FLEXCAN_Event_t;

#define FLEXCAN_EVENT_RX_MB ((FLEXCAN_Event_t)(1UL << 0U))
#define FLEXCAN_EVENT_TX_MB ((FLEXCAN_Event_t)(1UL << 1U))
#define FLEXCAN_EVENT_RX_QUEUE_OVERFLOW ((FLEXCAN_Event_t)(1UL << 2U))

typedef void (*FLEXCAN_Callback_t)(FLEXCAN_Type *pBase,
                                   FLEXCAN_Event_t u32Event,
                                   uint8_t u8MbIndex);

/* ========================================================================= */
/* Error and Diagnostic Status                                               */
/* ========================================================================= */

typedef struct
{
    uint32_t u32Esr1;
    uint8_t u8TxErrorCount;
    uint8_t u8RxErrorCount;

    /* Fast-phase counters are retained as raw diagnostic fields. */
    uint8_t u8TxFastErrorCount;
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

/* ========================================================================= */
/* Configuration and Module Control                                          */
/* ========================================================================= */

void FLEXCAN_GetDefaultConfig(FLEXCAN_Config_t *pConfig);

FLEXCAN_Status_t FLEXCAN_Init(FLEXCAN_Type *pBase,
                              const FLEXCAN_Config_t *pConfig);

FLEXCAN_Status_t FLEXCAN_Deinit(FLEXCAN_Type *pBase);

FLEXCAN_Status_t FLEXCAN_Enable(FLEXCAN_Type *pBase);

FLEXCAN_Status_t FLEXCAN_Disable(FLEXCAN_Type *pBase);

FLEXCAN_Status_t FLEXCAN_EnterFreezeMode(FLEXCAN_Type *pBase,
                                         uint32_t u32Timeout);

FLEXCAN_Status_t FLEXCAN_ExitFreezeMode(FLEXCAN_Type *pBase,
                                        uint32_t u32Timeout);

FLEXCAN_Status_t FLEXCAN_SetMode(FLEXCAN_Type *pBase,
                                 FLEXCAN_Mode_t u8Mode);

/* ========================================================================= */
/* Bit Timing                                                                */
/* ========================================================================= */

FLEXCAN_Status_t FLEXCAN_CalculateNominalBitTiming(uint32_t u32ClockFrequencyHz,
                                                   uint32_t u32BitRate,
                                                   FLEXCAN_BitTiming_t *pTiming);

FLEXCAN_Status_t FLEXCAN_SetNominalBitTiming(FLEXCAN_Type *pBase,
                                             const FLEXCAN_BitTiming_t *pTiming);

/* ========================================================================= */
/* Message Buffer Configuration                                              */
/* ========================================================================= */

FLEXCAN_Status_t FLEXCAN_ConfigTxMb(FLEXCAN_Type *pBase,
                                    uint8_t u8MbIndex,
                                    bool bEnableInterrupt);

FLEXCAN_Status_t FLEXCAN_ConfigRxMb(FLEXCAN_Type *pBase,
                                    const FLEXCAN_RxMbConfig_t *pConfig);

FLEXCAN_Status_t FLEXCAN_DisableMb(FLEXCAN_Type *pBase,
                                   uint8_t u8MbIndex);

FLEXCAN_Status_t FLEXCAN_SetRxMbMask(FLEXCAN_Type *pBase,
                                     uint8_t u8MbIndex,
                                     uint32_t u32Mask,
                                     FLEXCAN_FrameFormat_t u8Format);

FLEXCAN_Status_t FLEXCAN_SetGlobalRxMask(FLEXCAN_Type *pBase,
                                         uint32_t u32Mask,
                                         FLEXCAN_FrameFormat_t u8Format);

/* ========================================================================= */
/* Transmission                                                              */
/* ========================================================================= */

FLEXCAN_Status_t FLEXCAN_Transmit(FLEXCAN_Type *pBase,
                                  uint8_t u8MbIndex,
                                  const FLEXCAN_Frame_t *pFrame);

FLEXCAN_Status_t FLEXCAN_TransmitBlocking(FLEXCAN_Type *pBase,
                                          uint8_t u8MbIndex,
                                          const FLEXCAN_Frame_t *pFrame,
                                          uint32_t u32Timeout);

bool FLEXCAN_IsTxComplete(FLEXCAN_Type *pBase,
                          uint8_t u8MbIndex);

FLEXCAN_Status_t FLEXCAN_AbortTransmit(FLEXCAN_Type *pBase,
                                       uint8_t u8MbIndex,
                                       uint32_t u32Timeout);

/* ========================================================================= */
/* Polling Reception                                                         */
/* ========================================================================= */

bool FLEXCAN_IsRxReady(FLEXCAN_Type *pBase,
                       uint8_t u8MbIndex);

FLEXCAN_Status_t FLEXCAN_Receive(FLEXCAN_Type *pBase,
                                 uint8_t u8MbIndex,
                                 FLEXCAN_Frame_t *pFrame);

/* ========================================================================= */
/* Interrupt-Driven Receive Queue                                            */
/* ========================================================================= */

bool FLEXCAN_IsDataAvailable(FLEXCAN_Type *pBase);

FLEXCAN_Status_t FLEXCAN_Read(FLEXCAN_Type *pBase,
                              FLEXCAN_Frame_t *pFrame);

uint8_t FLEXCAN_GetRxQueueCount(FLEXCAN_Type *pBase);

void FLEXCAN_FlushRxQueue(FLEXCAN_Type *pBase);

/* ========================================================================= */
/* Message Buffer Interrupt Control                                          */
/* ========================================================================= */

FLEXCAN_Status_t FLEXCAN_EnableMbInterrupt(FLEXCAN_Type *pBase,
                                           uint8_t u8MbIndex);

FLEXCAN_Status_t FLEXCAN_DisableMbInterrupt(FLEXCAN_Type *pBase,
                                            uint8_t u8MbIndex);

uint32_t FLEXCAN_GetMbInterruptFlags(FLEXCAN_Type *pBase);

void FLEXCAN_ClearMbInterruptFlags(FLEXCAN_Type *pBase,
                                   uint32_t u32Mask);

void FLEXCAN_SetCallback(FLEXCAN_Type *pBase,
                         FLEXCAN_Callback_t pfCallback);

/* ========================================================================= */
/* Diagnostics                                                               */
/* ========================================================================= */

FLEXCAN_Status_t FLEXCAN_GetErrorStatus(FLEXCAN_Type *pBase,
                                        FLEXCAN_ErrorStatus_t *pStatus);

bool FLEXCAN_IsSynchronized(FLEXCAN_Type *pBase);

bool FLEXCAN_IsBusOff(FLEXCAN_Type *pBase);

uint16_t FLEXCAN_GetTimer(FLEXCAN_Type *pBase);

/* ========================================================================= */
/* Driver IRQ Bridge                                                         */
/* ========================================================================= */

void FLEXCAN_MBIRQHandler(FLEXCAN_Type *pBase,
                          uint8_t u8FirstMb,
                          uint8_t u8LastMb);

#endif /* FLEXCAN_H */
