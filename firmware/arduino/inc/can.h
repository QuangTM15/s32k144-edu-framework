/**
 * @file can.h
 * @brief Arduino-style CAN API for EduFramework on NXP S32K144.
 *
 * @details
 * This header provides a simple Arduino-like CAN interface built on top
 * of the low-level FlexCAN driver.
 *
 * The API is intentionally simple for educational and automotive use:
 * - Single CAN bus
 * - Classical CAN
 * - Standard 11-bit and extended 29-bit identifiers
 * - Payload length from 0 to 8 bytes
 * - Blocking and non-blocking transmission
 * - Interrupt-driven reception
 * - Receive callback
 * - Normal, Loop-Back, and Listen-Only operating modes
 *
 * The current implementation uses FlexCAN0 with the MaaZEDU-tested pin map:
 * - PTE5 = CAN0_TX
 * - PTE4 = CAN0_RX
 *
 * Message Buffers, interrupts, hardware filtering, and the receive queue are
 * managed internally by the EduFramework CAN layer.
 */

#ifndef CAN_H
#define CAN_H

#include <stdint.h>
#include <stdbool.h>

/* ========================================================================= */
/* Public Constants                                                           */
/* ========================================================================= */

/**
 * @brief Maximum Classical CAN payload length in bytes.
 */
#define CAN_MAX_DATA_LENGTH (8U)

/* ========================================================================= */
/* Public Types                                                               */
/* ========================================================================= */

/**
 * @brief CAN identifier format type.
 */
typedef uint8_t CAN_Format_t;

/**
 * @brief Standard 11-bit CAN identifier.
 */
#define CAN_STANDARD ((CAN_Format_t)0U)

/**
 * @brief Extended 29-bit CAN identifier.
 */
#define CAN_EXTENDED ((CAN_Format_t)1U)

/**
 * @brief CAN operating mode type.
 */
typedef uint8_t CAN_Mode_t;

/**
 * @brief Normal CAN communication mode.
 */
#define CAN_MODE_NORMAL ((CAN_Mode_t)0U)

/**
 * @brief Internal Loop-Back mode.
 *
 * @details
 * Frames transmitted by the controller are internally received by the same
 * controller. This mode is useful for software testing without another CAN
 * node.
 */
#define CAN_MODE_LOOPBACK ((CAN_Mode_t)1U)

/**
 * @brief Listen-Only mode.
 *
 * @details
 * The controller monitors CAN traffic without actively transmitting frames.
 */
#define CAN_MODE_LISTEN_ONLY ((CAN_Mode_t)2U)

/**
 * @brief Classical CAN frame.
 *
 * @details
 * This structure contains only the information normally required by an
 * application. Hardware-specific FlexCAN information is intentionally hidden.
 */
typedef struct
{
    /**
     * @brief 11-bit standard ID or 29-bit extended ID.
     */
    uint32_t id;

    /**
     * @brief Frame payload.
     */
    uint8_t data[CAN_MAX_DATA_LENGTH];

    /**
     * @brief Number of payload bytes from 0 to 8.
     */
    uint8_t length;

    /**
     * @brief Identifier format.
     */
    CAN_Format_t format;

} CAN_Frame_t;

/**
 * @brief CAN receive callback type.
 *
 * @details
 * The callback is executed from interrupt context when a received frame has
 * been stored in the internal receive queue.
 *
 * The callback must remain short and non-blocking. CAN_read() should normally
 * be called from the main application instead of from the callback.
 */
typedef void (*CAN_Callback_t)(void);

/* ========================================================================= */
/* Public API                                                                 */
/* ========================================================================= */

/**
 * @brief Initialize the CAN bus.
 *
 * @details
 * This function initializes FlexCAN0 using the requested bitrate and
 * automatically configures all Message Buffers, interrupts, and internal
 * receive resources required by the Arduino CAN layer.
 *
 * The initial operating mode is CAN_MODE_NORMAL.
 *
 * If CAN has already been initialized, the previous CAN configuration is
 * stopped before the new configuration is applied.
 *
 * @param bitRate CAN bitrate in bits per second.
 *
 * @return Initialization state.
 *
 * @retval true CAN initialized successfully.
 * @retval false CAN initialization failed or bitRate is invalid.
 */
bool CAN_begin(uint32_t bitRate);

/**
 * @brief Disable the CAN bus.
 *
 * @details
 * This function deinitializes the underlying FlexCAN controller and resets
 * the Arduino CAN layer state.
 */
void CAN_end(void);

/**
 * @brief Change the CAN operating mode.
 *
 * @param mode CAN operating mode.
 *
 * @return Operation state.
 *
 * @retval true Operating mode changed successfully.
 * @retval false CAN is not initialized, transmission is currently active,
 * or mode is invalid.
 */
bool CAN_setMode(CAN_Mode_t mode);

/**
 * @brief Send one CAN frame using blocking transmission.
 *
 * @details
 * This function waits until transmission completes or the low-level driver
 * timeout expires.
 *
 * Transmission is not allowed while CAN_MODE_LISTEN_ONLY is active.
 *
 * @param frame Pointer to the CAN frame to transmit.
 *
 * @return Transmission state.
 *
 * @retval true Frame transmitted successfully.
 * @retval false Transmission failed, CAN is not initialized, another
 * transmission is active, the frame is invalid, or Listen-Only mode is active.
 */
bool CAN_send(const CAN_Frame_t *frame);

/**
 * @brief Start a non-blocking CAN transmission.
 *
 * @details
 * The frame is copied to the FlexCAN hardware before this function returns.
 * The caller may therefore reuse or modify the supplied CAN_Frame_t after a
 * successful call.
 *
 * Use CAN_isTxBusy() to determine whether transmission is still active and
 * CAN_isTxComplete() to check whether the most recently started transmission
 * completed successfully.
 *
 * Transmission is not allowed while CAN_MODE_LISTEN_ONLY is active.
 *
 * @param frame Pointer to the CAN frame to transmit.
 *
 * @return Start state.
 *
 * @retval true Transmission started successfully.
 * @retval false Transmission could not be started.
 */
bool CAN_sendNonBlocking(const CAN_Frame_t *frame);

/**
 * @brief Check whether a non-blocking CAN transmission is active.
 *
 * @return Transmission activity state.
 *
 * @retval true A transmission is currently active.
 * @retval false No transmission is currently active.
 */
bool CAN_isTxBusy(void);

/**
 * @brief Check whether the most recently started transmission completed.
 *
 * @details
 * The completion state is cleared when a new transmission is started.
 *
 * @return Transmission completion state.
 *
 * @retval true The most recent transmission completed successfully.
 * @retval false No transmission has completed since the last transmission
 * was started.
 */
bool CAN_isTxComplete(void);

/**
 * @brief Check whether a received CAN frame is available.
 *
 * @details
 * CAN reception is interrupt-driven. Received frames are stored in the
 * low-level software receive queue before this function is called.
 *
 * @return Receive availability state.
 *
 * @retval true At least one received CAN frame is available.
 * @retval false No frame is available or CAN is not initialized.
 */
bool CAN_available(void);

/**
 * @brief Read one received CAN frame.
 *
 * @details
 * This function removes the oldest frame from the internal receive queue and
 * converts it to the Arduino-style CAN_Frame_t representation.
 *
 * The function is non-blocking.
 *
 * @param frame Pointer to the destination frame.
 *
 * @return Read state.
 *
 * @retval true One CAN frame was read successfully.
 * @retval false No frame is available, CAN is not initialized, or frame is NULL.
 */
bool CAN_read(CAN_Frame_t *frame);

/**
 * @brief Register a receive callback.
 *
 * @details
 * The callback is called from interrupt context after a received frame has
 * been stored in the internal receive queue.
 *
 * Passing NULL removes the currently registered callback.
 *
 * The callback should only perform short operations such as setting a flag or
 * incrementing a counter.
 *
 * @param callback Receive callback, or NULL to disable the user callback.
 */
void CAN_onReceive(CAN_Callback_t callback);

#endif /* CAN_H */