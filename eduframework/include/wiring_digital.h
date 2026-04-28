#ifndef WIRING_DIGITAL_H
#define WIRING_DIGITAL_H

#include <stdint.h>
#include <stdbool.h>

void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t value);
bool digitalRead(uint8_t pin);
void digitalToggle(uint8_t pin);

#endif /* WIRING_DIGITAL_H */
