#include "time.h"

#include "clock.h"
#include "lpit.h"
#include "irq.h"

static volatile uint32_t g_millis = 0U;
static time_callback_t g_timeCallback = (time_callback_t)0;

static void Time_TickCallback(void)
{
    g_millis++;

    if (g_timeCallback != (time_callback_t)0)
    {
        g_timeCallback();
    }
}

void Time_Init(void)
{
    g_millis = 0U;
    g_timeCallback = (time_callback_t)0;

    SOSC_init_8MHz();
    SPLL_init_160MHz();
    NormalRUNmode_80MHz();

    LPIT_Init();
    LPIT_SetTimerPeriod(0U, 40000U);   /* 1 ms tick with 40 MHz LPIT clock */
    LPIT_EnableInterrupt(0U);

    IRQ_LPIT0_Ch0_SetCallback(Time_TickCallback);
    IRQ_LPIT0_Ch0_Init();

    LPIT_StartTimer(0U);
}

uint32_t millis(void)
{
    return g_millis;
}

void delay(uint32_t ms)
{
    uint32_t start = millis();

    while ((millis() - start) < ms)
    {
    }
}

void Time_SetCallback(time_callback_t cb)
{
    g_timeCallback = cb;
}
