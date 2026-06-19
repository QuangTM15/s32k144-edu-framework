#ifndef ARDUINO_TIME_H
#define ARDUINO_TIME_H

/**
 * @file time.h
 * @brief Arduino-style time API interface.
 *
 * @details
 * This file declares the time-related APIs used by the Arduino-style
 * layer of EduFramework.
 *
 * The time module provides:
 * - Millisecond system tick initialization.
 * - Millisecond counter access through millis().
 * - Blocking delay based on the millisecond tick.
 * - Optional user callback executed on every time tick.
 *
 * The current implementation uses LPIT channel 0 as the hardware time base.
 */

#include <stdint.h>

/**
 * @brief Time tick callback function type.
 *
 * @details
 * A callback registered with Time_SetCallback() is executed from the
 * LPIT interrupt context on every system tick.
 *
 * The callback must be short and must not call blocking functions such as
 * delay().
 */
typedef void (*time_callback_t)(void);

/**
 * @brief Initialize the Arduino-style time base.
 *
 * @details
 * This function initializes the internal millisecond counter and configures
 * the hardware timer used as the system time base.
 *
 * The current implementation uses LPIT channel 0 to generate a 1 ms tick.
 *
 * @return None.
 */
void Time_Init(void);

/**
 * @brief Get elapsed time since time base initialization.
 *
 * @details
 * This function returns the number of milliseconds elapsed since Time_Init()
 * was called.
 *
 * @return uint32_t
 * Elapsed time in milliseconds.
 */
uint32_t millis(void);

/**
 * @brief Block program execution for a specified time.
 *
 * @details
 * This function waits until the requested number of milliseconds has elapsed.
 * It uses millis() internally, so the LPIT-based time base must be running.
 *
 * This is a blocking delay and should not be used inside interrupt service
 * routines.
 *
 * @param[in] u32Ms
 * Delay duration in milliseconds.
 *
 * @return None.
 */
void delay(uint32_t u32Ms);

/**
 * @brief Register a user callback for the system tick.
 *
 * @details
 * The registered callback is called from the LPIT interrupt context on each
 * time tick. Passing a null callback disables the user callback.
 *
 * The callback should only perform short, non-blocking operations.
 *
 * @param[in] pfCallback
 * Pointer to callback function. Use null to disable callback.
 *
 * @return None.
 */
void Time_SetCallback(time_callback_t pfCallback);

#endif /* ARDUINO_TIME_H */