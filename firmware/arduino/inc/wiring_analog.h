#ifndef WIRING_ANALOG_H
#define WIRING_ANALOG_H

#include <stdint.h>

/* Initialize analog subsystem with default configuration */
void analogInit(void);

/* Blocking analog read */
int analogRead(uint8_t pin);

/* Start one non-blocking analog conversion */
void analogStart(uint8_t pin);

/* Check whether non-blocking conversion result is ready */
uint8_t analogAvailable(void);

/* Get latest non-blocking conversion result */
int analogGetResult(void);

/* Blocking analog read in millivolts */
int analogReadMilliVolts(uint8_t pin);

void analogWrite(uint8_t pin, uint8_t value);

#endif /* WIRING_ANALOG_H */