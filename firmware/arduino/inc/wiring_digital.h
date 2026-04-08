#ifndef WIRING_DIGITAL_H
#define WIRING_DIGITAL_H

#include <stdint.h>
#include <stdbool.h>

/* Pin mode */
#define INPUT           0U
#define OUTPUT          1U
#define INPUT_PULLUP    2U
#define INPUT_PULLDOWN  3U

/* Pin state */
#define LOW             0U
#define HIGH            1U

void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t value);
bool digitalRead(uint8_t pin);
void digitalToggle(uint8_t pin);

#endif /* WIRING_DIGITAL_H */
