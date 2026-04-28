#include "Arduino.h"
#include "clock.h"

extern void Time_Init(void);

void setup(void)
{
    SOSC_init_8MHz();
    SPLL_init_160MHz();
    NormalRUNmode_80MHz();

    Time_Init();
}