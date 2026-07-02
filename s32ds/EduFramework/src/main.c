/**
 * @file main.c
 * @brief Real HC-SR04 raw test with high-drive TRIG pin.
 */

#include "Arduino.h"
#include "S32K144.h"

#define TEST_TRIG_PIN          (GPIO2)
#define TEST_ECHO_PIN          (GPIO1)

#define TEST_UART_BAUDRATE     (9600U)
#define TEST_TIMEOUT_US        (60000U)
#define TEST_PERIOD_MS         (500U)

/*
 * GPIO2 on MaaZEDU is PTD14.
 * Enable high drive strength for PTD14 as a test only.
 */
static void Test_EnableTrigHighDrive(void)
{
    IP_PORTD->PCR[14U] |= PORT_PCR_DSE_MASK;
}

static uint32_t HCSR04_ReadRawDurationUs(uint32_t *pu32WaitHighUs);

int main(void)
{
    uint32_t u32WaitHighUs = 0U;
    uint32_t u32DurationUs = 0U;
    float f32DistanceCm = 0.0f;

    setup();

    Serial1_begin(TEST_UART_BAUDRATE);

    pinMode(TEST_TRIG_PIN, OUTPUT);
    pinMode(TEST_ECHO_PIN, INPUT_PULLDOWN);

    Test_EnableTrigHighDrive();

    digitalWrite(TEST_TRIG_PIN, LOW);

    delay(500U);

    Serial1_println("--------------------------------");
    Serial1_println("REAL HC-SR04 HIGH-DRIVE TRIG TEST");
    Serial1_println("TRIG = GPIO2 / PTD14");
    Serial1_println("ECHO = GPIO1 / PTD17");
    Serial1_println("TRIG pulse = 20 us");
    Serial1_println("--------------------------------");

    while (1)
    {
        u32WaitHighUs = 0U;
        u32DurationUs = HCSR04_ReadRawDurationUs(&u32WaitHighUs);

        f32DistanceCm = (float)u32DurationUs * 0.017f;

        Serial1_print("waitHighUs = ");
        Serial1_printlnInt((int32_t)u32WaitHighUs);

        Serial1_print("durationUs = ");
        Serial1_printlnInt((int32_t)u32DurationUs);

        Serial1_print("distanceCm = ");
        Serial1_printlnFloat(f32DistanceCm);

        if (TEST_TIMEOUT_US <= u32WaitHighUs)
        {
            Serial1_println("RESULT = ECHO NEVER HIGH");
        }
        else if (0U == u32DurationUs)
        {
            Serial1_println("RESULT = ECHO HIGH TOO SHORT OR TIMEOUT");
        }
        else
        {
            Serial1_println("RESULT = SENSOR RESPONDED");
        }

        Serial1_println("--------------------------------");

        delay(TEST_PERIOD_MS);
    }

    return 0;
}

static uint32_t HCSR04_ReadRawDurationUs(uint32_t *pu32WaitHighUs)
{
    uint32_t u32WaitStartUs = 0U;
    uint32_t u32EchoStartUs = 0U;
    uint32_t u32EchoEndUs = 0U;
    uint32_t u32WaitHighUs = 0U;
    uint32_t u32DurationUs = 0U;

    /*
     * Make sure TRIG starts LOW.
     */
    digitalWrite(TEST_TRIG_PIN, LOW);
    delayMicroseconds(5U);

    /*
     * HC-SR04 needs at least 10 us.
     * Use 20 us here for debug margin.
     */
    digitalWrite(TEST_TRIG_PIN, HIGH);
    delayMicroseconds(20U);
    digitalWrite(TEST_TRIG_PIN, LOW);

    /*
     * Wait for ECHO rising edge.
     */
    u32WaitStartUs = micros();

    while ((false == digitalRead(TEST_ECHO_PIN)) &&
           ((micros() - u32WaitStartUs) < TEST_TIMEOUT_US))
    {
        /* Busy wait */
    }

    u32WaitHighUs = micros() - u32WaitStartUs;

    if ((uint32_t *)0 != pu32WaitHighUs)
    {
        *pu32WaitHighUs = u32WaitHighUs;
    }

    if (TEST_TIMEOUT_US <= u32WaitHighUs)
    {
        u32DurationUs = 0U;
    }
    else
    {
        u32EchoStartUs = micros();

        while ((true == digitalRead(TEST_ECHO_PIN)) &&
               ((micros() - u32EchoStartUs) < TEST_TIMEOUT_US))
        {
            /* Busy wait */
        }

        u32EchoEndUs = micros();
        u32DurationUs = u32EchoEndUs - u32EchoStartUs;

        if (TEST_TIMEOUT_US <= u32DurationUs)
        {
            u32DurationUs = 0U;
        }
    }

    return u32DurationUs;
}
