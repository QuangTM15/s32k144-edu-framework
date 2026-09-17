#ifndef WIRING_TONE_H
#define WIRING_TONE_H

/**
 * @file wiring_tone.h
 * @brief Arduino-style tone API public interface.
 *
 * @details
 * This file declares Arduino-style tone generation APIs for EduFramework.
 *
 * The tone API generates a square-wave signal on a PWM-capable logical
 * pin using the shared Arduino-layer PWM service.
 *
 * The current implementation supports:
 * - Starting a continuous tone at a specified frequency.
 * - Changing the frequency by calling tone() again.
 * - Stopping the tone using noTone().
 *
 * Timed tone duration is not supported by the current version.
 */

#include <stdint.h>

/**
 * @brief Generate a continuous tone on a PWM-capable pin.
 *
 * @details
 * This function generates a square-wave signal with approximately
 * 50 percent duty cycle at the requested frequency.
 *
 * Calling tone() again on the same pin changes the output frequency.
 * The tone continues until noTone() is called or another operation
 * reconfigures the underlying PWM resource.
 *
 * @param[in] pin
 * Arduino-style logical pin identifier.
 *
 * @param[in] frequency
 * Tone frequency in Hz.
 *
 * @return None.
 */
void tone(uint8_t pin, uint32_t frequency);

/**
 * @brief Stop tone generation on a pin.
 *
 * @details
 * This function disables the tone waveform on the selected pin.
 *
 * The underlying FTM counter is not stopped because other PWM channels
 * mapped to the same FTM instance may still require it.
 *
 * Calling noTone() for a pin without an active PWM configuration has
 * no effect.
 *
 * @param[in] pin
 * Arduino-style logical pin identifier.
 *
 * @return None.
 */
void noTone(uint8_t pin);

#endif /* WIRING_TONE_H */