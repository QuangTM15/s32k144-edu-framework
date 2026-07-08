/**
 * @file ultrasonic.c
 * @brief Arduino-style HC-SR04 ultrasonic sensor library implementation.
 *
 * @details
 * This module implements the HC-SR04 ultrasonic sensor device library.
 *
 * Measurement sequence:
 *
 * 1. Drive TRIG LOW briefly.
 * 2. Drive TRIG HIGH for at least 10 us.
 * 3. Drive TRIG LOW again.
 * 4. Wait for ECHO to become HIGH.
 * 5. Measure how long ECHO remains HIGH.
 * 6. Convert echo duration to distance.
 *
 * Distance formula:
 *
 * distance_cm = duration_us * 0.017
 *
 * The factor 0.017 is approximately speed_of_sound_cm_per_us / 2.
 * The division by 2 is required because the sound wave travels from
 * sensor to object and then returns back to the sensor.
 *
 * Calibration notes:
 *
 * If the measured distance is consistently too high or too low, tune:
 *
 * - ULTRASONIC_CM_PER_US
 *   Main distance conversion factor.
 *
 * If readings are noisy, tune:
 *
 * - ULTRASONIC_DEFAULT_FILTER_SAMPLES in ultrasonic.h
 * - ULTRASONIC_FILTER_SAMPLE_DELAY_MS
 * - ULTRASONIC_MAX_REMOVE_EACH_SIDE
 *
 * This module belongs to the Device Layer and must not access S32K144
 * registers directly.
 */

#include "ultrasonic.h"

#include "Arduino.h"

/* ========================================================================= */
/* Private Constants                                                          */
/* ========================================================================= */

/**
 * @brief Pre-trigger LOW time in microseconds.
 *
 * @details
 * This value ensures the TRIG signal starts from a stable LOW level before
 * generating the HIGH trigger pulse.
 *
 * Adjustable:
 * Normally this value does not need tuning.
 */
#define ULTRASONIC_PRE_TRIGGER_LOW_US (2U)

/**
 * @brief HC-SR04 trigger pulse width in microseconds.
 *
 * @details
 * HC-SR04 requires a HIGH pulse of at least 10 us on TRIG.
 *
 * Adjustable:
 * Keep this value at 10 us or slightly higher. Do not reduce it below 10 us.
 */
#define ULTRASONIC_TRIGGER_PULSE_US (10U)

/**
 * @brief Delay between filtered samples in milliseconds.
 *
 * @details
 * Ultrasonic sensors need a short gap between measurements because previous
 * sound reflections may interfere with the next measurement.
 *
 * Adjustable:
 * Increase this value if filtered readings are unstable.
 */
#define ULTRASONIC_FILTER_SAMPLE_DELAY_MS (30U)

/**
 * @brief Maximum number of samples removed from each side after sorting.
 *
 * @details
 * With 20 samples, the filter removes 5 smallest and 5 largest samples,
 * then averages the middle 10 samples.
 *
 * Adjustable:
 * Increase remove count for stronger noise rejection, but too much removal
 * may reduce responsiveness.
 */
#define ULTRASONIC_MAX_REMOVE_EACH_SIDE (5U)

/**
 * @brief Distance conversion factor from echo duration to centimeters.
 *
 * @details
 * Common HC-SR04 Arduino examples use:
 *
 * distance_cm = duration_us * 0.017
 *
 * Adjustable:
 * This is the main calibration constant.
 *
 * If measured distance is always smaller than real distance, increase this
 * value slightly.
 *
 * If measured distance is always larger than real distance, decrease this
 * value slightly.
 */
#define ULTRASONIC_CM_PER_US (0.017f)

/**
 * @brief Distance conversion factor from centimeters to inches.
 */
#define ULTRASONIC_INCH_PER_CM (0.3937008f)

/* ========================================================================= */
/* Private Variables                                                          */
/* ========================================================================= */

/**
 * @brief Default sensor object used by the Arduino-like API.
 */
static Ultrasonic_t g_stDefaultUltrasonic =
    {
        0U,
        0U,
        ULTRASONIC_DEFAULT_TIMEOUT_US};

/**
 * @brief Default filter buffer used by ultrasonicReadFiltered().
 */
static float g_f32DefaultFilterBuffer[ULTRASONIC_DEFAULT_FILTER_SAMPLES];

/* ========================================================================= */
/* Private Function Prototypes                                                */
/* ========================================================================= */

static uint8_t Ultrasonic_IsValidSensor(Ultrasonic_t *sensor);
static void Ultrasonic_SendTriggerPulse(Ultrasonic_t *sensor);
static uint8_t Ultrasonic_WaitEchoHigh(Ultrasonic_t *sensor,
                                       uint32_t *pu32EchoStartUs);
static uint32_t Ultrasonic_MeasureEchoHigh(Ultrasonic_t *sensor,
                                           uint32_t u32EchoStartUs);
static void Ultrasonic_SortAscending(float *buffer,
                                     uint8_t sampleCount);
static float Ultrasonic_AverageMiddleSamples(float *buffer,
                                             uint8_t sampleCount);

/* ========================================================================= */
/* Private Functions                                                          */
/* ========================================================================= */

/**
 * @brief Check whether sensor pointer is valid.
 *
 * @param[in] sensor
 * Pointer to ultrasonic sensor object.
 *
 * @return uint8_t
 *
 * @retval 0U
 * Sensor pointer is invalid.
 *
 * @retval 1U
 * Sensor pointer is valid.
 */
static uint8_t Ultrasonic_IsValidSensor(Ultrasonic_t *sensor)
{
    uint8_t u8IsValid = 0U;

    if ((Ultrasonic_t *)0 != sensor)
    {
        u8IsValid = 1U;
    }

    return u8IsValid;
}

/**
 * @brief Send HC-SR04 trigger pulse.
 *
 * @details
 * The sensor starts one measurement cycle when TRIG receives a HIGH pulse
 * of at least 10 us.
 *
 * @param[in] sensor
 * Pointer to ultrasonic sensor object.
 *
 * @return None.
 */
static void Ultrasonic_SendTriggerPulse(Ultrasonic_t *sensor)
{
    digitalWrite(sensor->trigPin, LOW);
    delayMicroseconds(ULTRASONIC_PRE_TRIGGER_LOW_US);

    digitalWrite(sensor->trigPin, HIGH);
    delayMicroseconds(ULTRASONIC_TRIGGER_PULSE_US);

    digitalWrite(sensor->trigPin, LOW);
}

/**
 * @brief Wait for ECHO pin to become HIGH.
 *
 * @details
 * This function waits for the rising edge of the ECHO pulse. If ECHO does
 * not become HIGH before timeout, the measurement is invalid.
 *
 * @param[in] sensor
 * Pointer to ultrasonic sensor object.
 *
 * @param[out] pu32EchoStartUs
 * Timestamp in microseconds when ECHO becomes HIGH.
 *
 * @return uint8_t
 *
 * @retval 0U
 * ECHO rising edge was not detected before timeout.
 *
 * @retval 1U
 * ECHO rising edge was detected.
 */
static uint8_t Ultrasonic_WaitEchoHigh(Ultrasonic_t *sensor,
                                       uint32_t *pu32EchoStartUs)
{
    uint32_t u32WaitStartUs = 0U;
    uint8_t u8EchoDetected = 0U;

    if (((Ultrasonic_t *)0 != sensor) &&
        ((uint32_t *)0 != pu32EchoStartUs))
    {
        u32WaitStartUs = micros();

        while ((false == digitalRead(sensor->echoPin)) &&
               ((micros() - u32WaitStartUs) < sensor->timeoutUs))
        {
            /* Busy wait for ECHO rising edge. */
        }

        if ((micros() - u32WaitStartUs) < sensor->timeoutUs)
        {
            *pu32EchoStartUs = micros();
            u8EchoDetected = 1U;
        }
    }

    return u8EchoDetected;
}

/**
 * @brief Measure how long ECHO remains HIGH.
 *
 * @details
 * This function measures the HIGH width of the ECHO pulse. The result is
 * returned in microseconds.
 *
 * @param[in] sensor
 * Pointer to ultrasonic sensor object.
 *
 * @param[in] u32EchoStartUs
 * Timestamp in microseconds when ECHO became HIGH.
 *
 * @return uint32_t
 * ECHO HIGH pulse width in microseconds.
 *
 * @retval 0U
 * ECHO remained HIGH until timeout or sensor pointer is invalid.
 */
static uint32_t Ultrasonic_MeasureEchoHigh(Ultrasonic_t *sensor,
                                           uint32_t u32EchoStartUs)
{
    uint32_t u32EchoEndUs = 0U;
    uint32_t u32DurationUs = 0U;

    if (1U == Ultrasonic_IsValidSensor(sensor))
    {
        while ((true == digitalRead(sensor->echoPin)) &&
               ((micros() - u32EchoStartUs) < sensor->timeoutUs))
        {
            /* Busy wait for ECHO falling edge. */
        }

        u32EchoEndUs = micros();
        u32DurationUs = u32EchoEndUs - u32EchoStartUs;

        if (sensor->timeoutUs <= u32DurationUs)
        {
            u32DurationUs = 0U;
        }
    }

    return u32DurationUs;
}

/**
 * @brief Sort distance sample buffer in ascending order.
 *
 * @details
 * A simple nested-loop sort is used because sample count is small and the
 * implementation is easy for students to understand.
 *
 * @param[in,out] buffer
 * Sample buffer.
 *
 * @param[in] sampleCount
 * Number of samples in buffer.
 *
 * @return None.
 */
static void Ultrasonic_SortAscending(float *buffer,
                                     uint8_t sampleCount)
{
    uint8_t u8Index = 0U;
    uint8_t u8CompareIndex = 0U;
    float f32Swap = 0.0f;

    if (((float *)0 != buffer) && (1U < sampleCount))
    {
        for (u8Index = 0U; u8Index < (sampleCount - 1U); u8Index++)
        {
            for (u8CompareIndex = (u8Index + 1U);
                 u8CompareIndex < sampleCount;
                 u8CompareIndex++)
            {
                if (buffer[u8Index] > buffer[u8CompareIndex])
                {
                    f32Swap = buffer[u8Index];
                    buffer[u8Index] = buffer[u8CompareIndex];
                    buffer[u8CompareIndex] = f32Swap;
                }
            }
        }
    }
}

/**
 * @brief Average middle samples after sorting.
 *
 * @details
 * This function removes low-end and high-end samples after sorting, then
 * averages the middle samples.
 *
 * Invalid samples are skipped.
 *
 * @param[in] buffer
 * Sorted sample buffer.
 *
 * @param[in] sampleCount
 * Number of samples in buffer.
 *
 * @return float
 * Filtered distance in centimeters.
 *
 * @retval ULTRASONIC_INVALID_DISTANCE_CM
 * No valid sample is available.
 */
static float Ultrasonic_AverageMiddleSamples(float *buffer,
                                             uint8_t sampleCount)
{
    uint8_t u8RemoveEachSide = 0U;
    uint8_t u8StartIndex = 0U;
    uint8_t u8EndIndex = 0U;
    uint8_t u8Index = 0U;
    uint8_t u8ValidCount = 0U;
    float f32Sum = 0.0f;
    float f32Average = ULTRASONIC_INVALID_DISTANCE_CM;

    if (((float *)0 != buffer) && (0U != sampleCount))
    {
        u8RemoveEachSide = sampleCount / 4U;

        if (ULTRASONIC_MAX_REMOVE_EACH_SIDE < u8RemoveEachSide)
        {
            u8RemoveEachSide = ULTRASONIC_MAX_REMOVE_EACH_SIDE;
        }

        u8StartIndex = u8RemoveEachSide;
        u8EndIndex = sampleCount - u8RemoveEachSide;

        for (u8Index = u8StartIndex; u8Index < u8EndIndex; u8Index++)
        {
            if (ULTRASONIC_INVALID_DISTANCE_CM != buffer[u8Index])
            {
                f32Sum += buffer[u8Index];
                u8ValidCount++;
            }
        }

        if (0U != u8ValidCount)
        {
            f32Average = f32Sum / (float)u8ValidCount;
        }
    }

    return f32Average;
}

/* ========================================================================= */
/* Arduino-like Simple API                                                    */
/* ========================================================================= */

void ultrasonicBegin(uint8_t trigPin,
                     uint8_t echoPin)
{
    Ultrasonic_Begin(&g_stDefaultUltrasonic,
                     trigPin,
                     echoPin);
}

void ultrasonicSetTimeout(uint32_t timeoutUs)
{
    Ultrasonic_SetTimeout(&g_stDefaultUltrasonic,
                          timeoutUs);
}

uint32_t ultrasonicReadDuration(void)
{
    return Ultrasonic_ReadDurationUs(&g_stDefaultUltrasonic);
}

float ultrasonicRead(void)
{
    return Ultrasonic_ReadCm(&g_stDefaultUltrasonic);
}

float ultrasonicReadInch(void)
{
    return Ultrasonic_ReadInch(&g_stDefaultUltrasonic);
}

float ultrasonicReadFiltered(void)
{
    return Ultrasonic_ReadCmFiltered(&g_stDefaultUltrasonic,
                                     g_f32DefaultFilterBuffer,
                                     ULTRASONIC_DEFAULT_FILTER_SAMPLES);
}

/* ========================================================================= */
/* Multi-instance API                                                         */
/* ========================================================================= */

void Ultrasonic_Begin(Ultrasonic_t *sensor,
                      uint8_t trigPin,
                      uint8_t echoPin)
{
    if (1U == Ultrasonic_IsValidSensor(sensor))
    {
        sensor->trigPin = trigPin;
        sensor->echoPin = echoPin;
        sensor->timeoutUs = ULTRASONIC_DEFAULT_TIMEOUT_US;

        pinMode(sensor->trigPin, OUTPUT);
        pinMode(sensor->echoPin, INPUT);

        digitalWrite(sensor->trigPin, LOW);
    }
}

void Ultrasonic_SetTimeout(Ultrasonic_t *sensor,
                           uint32_t timeoutUs)
{
    if ((1U == Ultrasonic_IsValidSensor(sensor)) &&
        (0U != timeoutUs))
    {
        sensor->timeoutUs = timeoutUs;
    }
}

uint32_t Ultrasonic_ReadDurationUs(Ultrasonic_t *sensor)
{
    uint32_t u32EchoStartUs = 0U;
    uint32_t u32DurationUs = 0U;

    if (1U == Ultrasonic_IsValidSensor(sensor))
    {
        Ultrasonic_SendTriggerPulse(sensor);

        if (1U == Ultrasonic_WaitEchoHigh(sensor,
                                          &u32EchoStartUs))
        {
            u32DurationUs = Ultrasonic_MeasureEchoHigh(sensor,
                                                       u32EchoStartUs);
        }
    }

    return u32DurationUs;
}

float Ultrasonic_ReadCm(Ultrasonic_t *sensor)
{
    uint32_t u32DurationUs = 0U;
    float f32DistanceCm = ULTRASONIC_INVALID_DISTANCE_CM;

    u32DurationUs = Ultrasonic_ReadDurationUs(sensor);

    if (0U != u32DurationUs)
    {
        f32DistanceCm = (float)u32DurationUs * ULTRASONIC_CM_PER_US;
    }

    return f32DistanceCm;
}

float Ultrasonic_ReadInch(Ultrasonic_t *sensor)
{
    float f32DistanceCm = ULTRASONIC_INVALID_DISTANCE_CM;
    float f32DistanceInch = ULTRASONIC_INVALID_DISTANCE_CM;

    f32DistanceCm = Ultrasonic_ReadCm(sensor);

    if (ULTRASONIC_INVALID_DISTANCE_CM != f32DistanceCm)
    {
        f32DistanceInch = f32DistanceCm * ULTRASONIC_INCH_PER_CM;
    }

    return f32DistanceInch;
}

float Ultrasonic_ReadCmFiltered(Ultrasonic_t *sensor,
                                float *buffer,
                                uint8_t sampleCount)
{
    uint8_t u8Sample = 0U;
    float f32FilteredDistance = ULTRASONIC_INVALID_DISTANCE_CM;

    if ((1U == Ultrasonic_IsValidSensor(sensor)) &&
        ((float *)0 != buffer) &&
        (0U != sampleCount))
    {
        for (u8Sample = 0U; u8Sample < sampleCount; u8Sample++)
        {
            buffer[u8Sample] = Ultrasonic_ReadCm(sensor);
            delay(ULTRASONIC_FILTER_SAMPLE_DELAY_MS);
        }

        Ultrasonic_SortAscending(buffer,
                                 sampleCount);

        f32FilteredDistance = Ultrasonic_AverageMiddleSamples(buffer,
                                                              sampleCount);
    }

    return f32FilteredDistance;
}