/**
 * @file wiring_tone.c
 * @brief Arduino-style tone API implementation.
 *
 * @details
 * This file implements continuous Arduino-style tone generation.
 *
 * Tone generation is delegated to the shared wiring_pwm module so that
 * Arduino-style APIs do not access the low-level FTM driver independently.
 *
 * A tone is generated using a 50 percent PWM duty cycle.
 */

#include "wiring_tone.h"
#include "wiring_pwm.h"

/* ============================================================
 * Local constants
 * ============================================================ */

/**
 * @brief Duty cycle used for tone square-wave generation.
 */
#define TONE_DUTY_PERCENT (50U)

/* ============================================================
 * Public API
 * ============================================================ */

/**
 * @copydoc tone
 */
void tone(uint8_t u8Pin, uint32_t u32Frequency)
{
    if (0UL != u32Frequency)
    {
        (void)WiringPwm_Start(
            u8Pin,
            u32Frequency,
            TONE_DUTY_PERCENT);
    }
    else
    {
        (void)WiringPwm_Stop(u8Pin);
    }
}

/**
 * @copydoc noTone
 */
void noTone(uint8_t u8Pin)
{
    (void)WiringPwm_Stop(u8Pin);
}