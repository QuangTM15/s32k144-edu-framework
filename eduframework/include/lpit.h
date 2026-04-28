#ifndef LPIT_H
#define LPIT_H

#include <stdint.h>

void LPIT_Init(void);
void LPIT_SetTimerPeriod(uint8_t channel, uint32_t ticks);
void LPIT_StartTimer(uint8_t channel);
void LPIT_StopTimer(uint8_t channel);
uint8_t LPIT_GetFlag(uint8_t channel);
void LPIT_ClearFlag(uint8_t channel);

void LPIT_EnableInterrupt(uint8_t channel);
void LPIT_DisableInterrupt(uint8_t channel);


#endif /* LPIT_H */
