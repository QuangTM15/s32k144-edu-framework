/**
 * @file time.c
 * @brief Arduino-style time API implementation.
 *
 * @details
 * This module provides a simple millisecond time base for the
 * Arduino-style API layer.
 *
 * The current implementation uses LPIT channel 0 to generate a
 * periodic 1 ms interrupt. The interrupt updates an internal
 * millisecond counter and optionally executes a user callback.
 */

#include "time.h"

#include "lpit.h"
#include "irq.h"

/**
 * @brief LPIT channel used as the system time base.
 */
#define TIME_LPIT_CHANNEL (0U)

/**
 * @brief LPIT tick value for a 1 ms period.
 *
 * @details
 * With the current project clock configuration, LPIT runs from
 * a 40 MHz functional clock.
 *
 * 40 MHz × 1 ms = 40000 ticks
 */
#define TIME_1MS_TICK_VALUE (40000U)

/**
 * @brief LPIT channel used for short microsecond delays.
 */
#define TIME_LPIT_MICROS_CHANNEL (1U)

/**
 * @brief LPIT tick count for 1 microsecond.
 *
 * @details
 * LPIT functional clock is 40 MHz.
 *
 * 40 MHz × 1 us = 40 ticks
 */
#define TIME_1US_TICK_VALUE (40U)

/**
 * @brief Millisecond system counter.
 *
 * @details
 * This variable is incremented by the LPIT interrupt callback
 * every 1 millisecond.
 */
static volatile uint32_t g_u32Millis = 0U;

/**
 * @brief Optional user callback executed every system tick.
 */
static time_callback_t g_pfTimeCallback = (time_callback_t)0;

/**
 * @brief Internal LPIT tick callback.
 *
 * @details
 * This function is registered with the IRQ module and is called
 * from the LPIT interrupt handler every 1 ms.
 *
 * The callback updates the system tick counter and executes the
 * optional user callback if one has been registered.
 *
 * @return None.
 */
static void Time_TickCallback(void)
{
    g_u32Millis++;

    if ((time_callback_t)0 != g_pfTimeCallback)
    {
        g_pfTimeCallback();
    }
}

/**
 * @brief Initialize the system time base.
 *
 * @details
 * This function initializes LPIT channel 0 as a periodic
 * 1 millisecond timer source.
 *
 * The internal millisecond counter is reset and the optional
 * user callback is cleared.
 *
 * @return None.
 */
void Time_Init(void)
{
    g_u32Millis = 0U;
    g_pfTimeCallback = (time_callback_t)0;

    LPIT_Init();

    LPIT_SetTimerPeriod(
        TIME_LPIT_CHANNEL,
        TIME_1MS_TICK_VALUE);

    LPIT_EnableInterrupt(TIME_LPIT_CHANNEL);

    IRQ_LPIT0_Ch0_SetCallback(Time_TickCallback);
    IRQ_LPIT0_Ch0_Init();

    LPIT_StartTimer(TIME_LPIT_CHANNEL);
}

/**
 * @brief Get elapsed time in milliseconds.
 *
 * @details
 * Returns the number of milliseconds elapsed since
 * Time_Init() was executed.
 *
 * @return uint32_t
 * Elapsed time in milliseconds.
 */
uint32_t millis(void)
{
    return g_u32Millis;
}

/**
 * @brief Blocking delay.
 *
 * @details
 * This function waits until the specified number of milliseconds
 * has elapsed using the system tick counter.
 *
 * This implementation performs a busy wait and should not be
 * used inside interrupt service routines.
 *
 * @param[in] u32Ms
 * Delay duration in milliseconds.
 *
 * @return None.
 */
void delay(uint32_t u32Ms)
{
    uint32_t u32StartTime = millis();

    while ((millis() - u32StartTime) < u32Ms)
    {
        /* Busy wait */
    }
}

/**
 * @brief Register a user tick callback.
 *
 * @details
 * The registered callback is executed every time the
 * 1 ms LPIT tick occurs.
 *
 * Passing a null pointer disables the callback.
 *
 * @param[in] pfCallback
 * Pointer to callback function.
 *
 * @return None.
 */
void Time_SetCallback(time_callback_t pfCallback)
{
    g_pfTimeCallback = pfCallback;
}

/**
 * @brief Blocking delay in microseconds.
 *
 * @details
 * This function uses LPIT channel 1 as a one-shot polling timer.
 *
 * The system millisecond tick uses LPIT channel 0, so this function
 * does not disturb millis() or delay().
 *
 * This implementation is intended for short delays. It performs a busy wait
 * and should not be used inside interrupt service routines.
 *
 * @param[in] u32Us
 * Delay duration in microseconds.
 *
 * @return None.
 */
void delayMicroseconds(uint32_t u32Us)
{
    uint32_t u32Ticks = 0U;

    if (0U != u32Us)
    {
        u32Ticks = u32Us * TIME_1US_TICK_VALUE;

        LPIT_StopTimer(TIME_LPIT_MICROS_CHANNEL);
        LPIT_ClearFlag(TIME_LPIT_MICROS_CHANNEL);
        LPIT_SetTimerPeriod(TIME_LPIT_MICROS_CHANNEL, u32Ticks);
        LPIT_StartTimer(TIME_LPIT_MICROS_CHANNEL);

        while (0U == LPIT_GetFlag(TIME_LPIT_MICROS_CHANNEL))
        {
            /* Busy wait */
        }

        LPIT_StopTimer(TIME_LPIT_MICROS_CHANNEL);
        LPIT_ClearFlag(TIME_LPIT_MICROS_CHANNEL);
    }
}

/**
 * @brief Get elapsed time in microseconds.
 *
 * @details
 * This function combines:
 *
 * - The millisecond counter updated by LPIT channel 0 interrupt.
 * - The current LPIT channel 0 down-counter value.
 *
 * LPIT channel 0 is configured for 1 ms:
 *
 * 40 MHz � 1 ms = 40000 ticks
 *
 * Because LPIT is a down-counter:
 *
 * elapsedTicks = TIME_1MS_TICK_VALUE - currentCounter
 *
 * elapsedUs = elapsedTicks / 40
 *
 * @return uint32_t
 * Elapsed time in microseconds.
 */
uint32_t micros(void)
{
    uint32_t u32MillisBefore = 0U;
    uint32_t u32MillisAfter = 0U;
    uint32_t u32CurrentTicks = 0U;
    uint32_t u32ElapsedTicks = 0U;
    uint32_t u32ElapsedUs = 0U;
    uint32_t u32TimeUs = 0U;

    /*
     * Read millis before and after reading LPIT counter.
     * If an interrupt occurs during the read sequence, repeat the read
     * to avoid mixing an old millisecond value with a new counter value.
     */
    do
    {
        u32MillisBefore = g_u32Millis;
        u32CurrentTicks = LPIT_GetCurrentValue(TIME_LPIT_CHANNEL);
        u32MillisAfter = g_u32Millis;
    }
    while (u32MillisBefore != u32MillisAfter);

    if (TIME_1MS_TICK_VALUE >= u32CurrentTicks)
    {
        u32ElapsedTicks = TIME_1MS_TICK_VALUE - u32CurrentTicks;
        u32ElapsedUs = u32ElapsedTicks / TIME_1US_TICK_VALUE;
    }
    else
    {
        u32ElapsedUs = 0U;
    }

    u32TimeUs = (u32MillisAfter * 1000U) + u32ElapsedUs;

    return u32TimeUs;
}
