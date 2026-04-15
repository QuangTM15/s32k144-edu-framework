#include "S32K144.h"
#include "clock.h"
#include "ftm.h"
#include "port.h"

static void WDOG_Disable(void)
{
    IP_WDOG->CNT = 0xD928C520;
    IP_WDOG->TOVAL = 0x0000FFFF;
    IP_WDOG->CS = 0x00002100;
}

static void DelayLoop(volatile uint32_t count)
{
    while (count--)
    {
        __asm("nop");
    }
}

static void FTM_TestPinsInit(void)
{
    PORT_EnableClock(PORT_NAME_D);

    /* LED_RED   -> PTD15 -> FTM0_CH0 -> ALT2 */
    PORT_SetPinMux(IP_PORTD, 15U, PORT_MUX_ALT2);

    /* LED_BLUE  -> PTD16 -> FTM0_CH1 -> ALT2 */
    PORT_SetPinMux(IP_PORTD, 16U, PORT_MUX_ALT2);

    /* LED_GREEN -> PTD0  -> FTM0_CH2 -> ALT2 */
    PORT_SetPinMux(IP_PORTD, 0U, PORT_MUX_ALT2);
}

int main(void)
{
    FTM_PwmConfig_t pwmConfig;
    uint8_t duty;

    WDOG_Disable();

    SOSC_init_8MHz();
    SPLL_init_160MHz();
    NormalRUNmode_80MHz();

    FTM_TestPinsInit();

    pwmConfig.srcClockHz = 8000000UL;
    pwmConfig.pwmFreqHz = 1000UL;
    pwmConfig.clockSource = FTM_CLOCK_SOURCE_EXTERNAL;
    pwmConfig.prescaler = FTM_PRESCALER_DIV_1;

    if (FTM_InitPwm(IP_FTM_0, &pwmConfig) != FTM_STATUS_OK)
    {
        while (1) {}
    }

    /* Vì LED onboard của bạn đang active-low theo thực tế test board,
       nên bắt đầu thử LOW_TRUE để dễ quan sát. */
    if (FTM_SetChannelModePwm(IP_FTM_0, FTM_CHANNEL_0, FTM_PWM_EDGE_ALIGNED_LOW_TRUE) != FTM_STATUS_OK)
    {
        while (1) {}
    }

    if (FTM_SetChannelModePwm(IP_FTM_0, FTM_CHANNEL_1, FTM_PWM_EDGE_ALIGNED_LOW_TRUE) != FTM_STATUS_OK)
    {
        while (1) {}
    }

    if (FTM_SetChannelModePwm(IP_FTM_0, FTM_CHANNEL_2, FTM_PWM_EDGE_ALIGNED_LOW_TRUE) != FTM_STATUS_OK)
    {
        while (1) {}
    }

    /* Ban đầu tắt hết */
    FTM_SetPwmDutyPercent(IP_FTM_0, FTM_CHANNEL_0, 0U);
    FTM_SetPwmDutyPercent(IP_FTM_0, FTM_CHANNEL_1, 0U);
    FTM_SetPwmDutyPercent(IP_FTM_0, FTM_CHANNEL_2, 0U);

    if (FTM_StartCounter(IP_FTM_0) != FTM_STATUS_OK)
    {
        while (1) {}
    }

    while (1)
    {
        /* Test từng LED riêng */
        FTM_SetPwmDutyPercent(IP_FTM_0, FTM_CHANNEL_0, 100U);
        FTM_SetPwmDutyPercent(IP_FTM_0, FTM_CHANNEL_1, 0U);
        FTM_SetPwmDutyPercent(IP_FTM_0, FTM_CHANNEL_2, 0U);
        DelayLoop(2000000UL);

        FTM_SetPwmDutyPercent(IP_FTM_0, FTM_CHANNEL_0, 0U);
        FTM_SetPwmDutyPercent(IP_FTM_0, FTM_CHANNEL_1, 100U);
        FTM_SetPwmDutyPercent(IP_FTM_0, FTM_CHANNEL_2, 0U);
        DelayLoop(2000000UL);

        FTM_SetPwmDutyPercent(IP_FTM_0, FTM_CHANNEL_0, 0U);
        FTM_SetPwmDutyPercent(IP_FTM_0, FTM_CHANNEL_1, 0U);
        FTM_SetPwmDutyPercent(IP_FTM_0, FTM_CHANNEL_2, 100U);
        DelayLoop(2000000UL);

        /* Fade cả 3 cùng lúc */
        for (duty = 0U; duty <= 100U; duty++)
        {
            FTM_SetPwmDutyPercent(IP_FTM_0, FTM_CHANNEL_0, duty);
            FTM_SetPwmDutyPercent(IP_FTM_0, FTM_CHANNEL_1, duty);
            FTM_SetPwmDutyPercent(IP_FTM_0, FTM_CHANNEL_2, duty);
            DelayLoop(50000UL);
        }

        for (duty = 100U; duty > 0U; duty--)
        {
            FTM_SetPwmDutyPercent(IP_FTM_0, FTM_CHANNEL_0, (uint8_t)(duty - 1U));
            FTM_SetPwmDutyPercent(IP_FTM_0, FTM_CHANNEL_1, (uint8_t)(duty - 1U));
            FTM_SetPwmDutyPercent(IP_FTM_0, FTM_CHANNEL_2, (uint8_t)(duty - 1U));
            DelayLoop(50000UL);
        }
    }
}
