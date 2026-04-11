#ifndef ARDUINO_TIME_H
#define ARDUINO_TIME_H

#include <stdint.h>

typedef void (*time_callback_t)(void);

/* Public API */
uint32_t millis(void);
void delay(uint32_t ms);
void Time_SetCallback(time_callback_t cb);

#endif /* ARDUINO_TIME_H */