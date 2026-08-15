#ifndef ESC_H
#define ESC_H

/**
 * @file esc.h
 * @brief Arduino-style ESC (Electronic Speed Controller) driver for EduFramework.
 *
 * @details
 * This library generates a standard 50Hz RC PWM signal required to drive
 * Brushless DC (BLDC) motor ESCs. It accesses the low-level FTM driver
 * to produce precise 1000us to 2000us pulses.
 */

#include "Arduino.h" //
#include "ftm.h"     //
#include <stdint.h>
#include <stdbool.h>

/* ========================================================================= */
/* Configuration Types & Constants                                           */
/* ========================================================================= */

#define ESC_DEFAULT_MIN_PULSE_US   (1000U)
#define ESC_DEFAULT_MAX_PULSE_US   (2000U)

/* ========================================================================= */
/* Object Context Structure                                                  */
/* ========================================================================= */

/**
 * @brief ESC device context structure.
 *
 * @details
 * Stores the hardware mapping (FTM instance and channel) and pulse width
 * configuration for an individual ESC.
 */
typedef struct
{
    uint8_t logicalPin;         /**< Arduino logical pin number. */
    FTM_Instance_t instance;    /**< Hardware FTM instance. */
    FTM_Channel_t channel;      /**< Hardware FTM channel. */

    uint16_t minPulseUs;        /**< Minimum throttle pulse in microseconds. */
    uint16_t maxPulseUs;        /**< Maximum throttle pulse in microseconds. */
    uint32_t timerClockHz;      /**< Underlying FTM clock frequency for count calculations. */
} ESC_t;

/* ========================================================================= */
/* Arduino-style API Prototypes                                              */
/* ========================================================================= */

/**
 * @brief Initialize the ESC driver on a specific pin.
 *
 * @details
 * The selected pin must support PWM output (e.g., GPIO2 - GPIO8)[cite: 4].
 * This function configures the FTM hardware for a 50Hz output.
 *
 * @param[in,out] esc Pointer to the ESC context.
 * @param[in] pin Logical Arduino pin number.
 *
 * @return bool true if successful, false if the pin does not support PWM.
 */
bool ESC_Init(ESC_t *esc, uint8_t pin);

/**
 * @brief Set custom minimum and maximum pulse widths.
 *
 * @param[in,out] esc Pointer to the ESC context.
 * @param[in] minUs Minimum pulse in microseconds (throttle 0%).
 * @param[in] maxUs Maximum pulse in microseconds (throttle 100%).
 */
void ESC_SetPulseRange(ESC_t *esc, uint16_t minUs, uint16_t maxUs);

/**
 * @brief Arm the ESC to unlock motor spinning.
 *
 * @details
 * Most ESCs require a 0% throttle signal (1000us) for 2 to 3 seconds
 * immediately after power-up. This function blocks for 3 seconds while
 * sending the minimum pulse.
 *
 * @param[in,out] esc Pointer to the ESC context.
 */
void ESC_Arm(ESC_t *esc);

/**
 * @brief Set the throttle percentage.
 *
 * @param[in,out] esc Pointer to the ESC context.
 * @param[in] percent Throttle value from 0U to 100U.
 */
void ESC_SetThrottle(ESC_t *esc, uint8_t percent);

/**
 * @brief Send a raw pulse width in microseconds to the ESC.
 *
 * @param[in,out] esc Pointer to the ESC context.
 * @param[in] us Pulse width in microseconds.
 */
void ESC_SetMicroseconds(ESC_t *esc, uint16_t us);

#endif /* ESC_H */
