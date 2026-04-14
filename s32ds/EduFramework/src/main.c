#include "S32K144.h"
#include <stdint.h>

#include "clock.h"
#include "lpuart.h"
#include "adc.h"
#include "irq.h"

static void DelayLoop(volatile uint32_t count)
{
    while (count > 0U)
    {
        count--;
    }
}

static void UIntToString(uint16_t value, char *buffer)
{
    char temp[6];
    uint8_t i = 0U;
    uint8_t j = 0U;

    if (buffer == (char *)0)
    {
        return;
    }

    if (value == 0U)
    {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    while (value > 0U)
    {
        temp[i] = (char)('0' + (value % 10U));
        value /= 10U;
        i++;
    }

    while (i > 0U)
    {
        i--;
        buffer[j] = temp[i];
        j++;
    }

    buffer[j] = '\0';
}

int main(void)
{
    LPUART_Config_t uart1Config;
    ADC_Config_t adcConfig;
    uint16_t adcValue = 0U;
    ADC_Status_t adcStatus;
    char textBuffer[8];

    SOSC_init_8MHz();
    SPLL_init_160MHz();
    NormalRUNmode_80MHz();

    uart1Config.baudRate = 9600U;
    uart1Config.srcClockHz = 8000000U;

    if (LPUART_Init(IP_LPUART1, &uart1Config) != LPUART_STATUS_OK)
    {
        while (1)
        {
        }
    }

    LPUART_WriteString(IP_LPUART1, "\r\n=== ADC interrupt test start ===\r\n");

    adcConfig.srcClockHz = 8000000U;
    adcConfig.resolution = ADC_RESOLUTION_12BIT;
    adcConfig.reference = ADC_REF_DEFAULT;
    adcConfig.sampleTime = 12U;
    adcConfig.average = ADC_AVERAGE_DISABLED;
    adcConfig.enableInterrupt = 1U;

    adcStatus = ADC_Init(IP_ADC_0, &adcConfig);
    if (adcStatus != ADC_STATUS_OK)
    {
        LPUART_WriteString(IP_LPUART1, "ADC_Init failed\r\n");
        while (1)
        {
        }
    }

    adcStatus = ADC_Calibrate(IP_ADC_0);
    if (adcStatus != ADC_STATUS_OK)
    {
        LPUART_WriteString(IP_LPUART1, "ADC_Calibrate failed\r\n");
        while (1)
        {
        }
    }

    IRQ_ADC0_Init();

    LPUART_WriteString(IP_LPUART1, "ADC ready\r\n");

    while (1)
    {
        adcStatus = ADC_StartConversion_IT(IP_ADC_0, ADC_CHANNEL_SE12);
        if (adcStatus == ADC_STATUS_OK)
        {
            while (ADC_IsDone(IP_ADC_0) == 0U)
            {
            }

            adcStatus = ADC_GetResult(IP_ADC_0, &adcValue);
            if (adcStatus == ADC_STATUS_OK)
            {
                UIntToString(adcValue, textBuffer);
                LPUART_WriteString(IP_LPUART1, "ADC0_SE12 = ");
                LPUART_WriteString(IP_LPUART1, textBuffer);
                LPUART_WriteString(IP_LPUART1, "\r\n");
            }
            else
            {
                LPUART_WriteString(IP_LPUART1, "ADC_GetResult failed\r\n");
            }
        }
        else
        {
            LPUART_WriteString(IP_LPUART1, "ADC_StartConversion_IT failed\r\n");
        }

        DelayLoop(800000U);
    }
}
