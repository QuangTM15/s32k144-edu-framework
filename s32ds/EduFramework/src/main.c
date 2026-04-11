#include "wiring_digital.h"
#include "time.h"

#define LED_BLUE    11U

int main(void)
{
    uint32_t last = 0U;

    pinMode(LED_BLUE, OUTPUT);
    Time_Init();

    while (1)
    {
        if ((millis() - last) >= 1000U)
        {
            last = millis();
            digitalToggle(LED_BLUE);
        }
    }
}
