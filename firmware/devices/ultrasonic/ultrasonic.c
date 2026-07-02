/**
 * @file ultrasonic.c
 * @brief Arduino-style HC-SR04 ultrasonic sensor library implementation.
 *
 * @details
 * This file implements the HC-SR04 ultrasonic sensor library for
 * EduFramework.
 *
 * The HC-SR04 measurement sequence is:
 *
 * 1. Drive TRIG LOW briefly.
 * 2. Drive TRIG HIGH for 10 microseconds.
 * 3. Drive TRIG LOW again.
 * 4. Wait for ECHO to become HIGH.
 * 5. Measure how long ECHO remains HIGH.
 * 6. Convert pulse duration to distance.
 *
 * Distance calculation:
 *
 * distance_cm = duration_us * 0.017
 *
 * The 0.017 factor is commonly used in Arduino examples because sound travels
 * from the sensor to the object and then back to the sensor.
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
 * @brief Required trigger pulse width for HC-SR04 in microseconds.
 */
#define ULTRASONIC_TRIGGER_PULSE_US (10U)

/**
 * @brief Small pre-trigger low time in microseconds.
 *
 * @details
 * Pulling TRIG LOW before the trigger pulse helps ensure a clean trigger.
 */
#define ULTRASONIC_PRE_TRIGGER_LOW_US (2U)

/**
 * @brief Delay between filtered measurement samples in milliseconds.
 *
 * @details
 * Ultrasonic waves may reflect and interfere with later measurements.
 * A delay between samples helps reduce measurement interference.
 */
#define ULTRASONIC_FILTER_SAMPLE_DELAY_MS (30U)

/**
 * @brief Maximum number of samples removed from each side of sorted data.
 *
 * @details
 * For 20 samples, the filter removes 5 smallest and 5 largest samples,
 * then averages the 10 middle samples.
 */
#define ULTRASONIC_MAX_REMOVE_EACH_SIDE (5U)

/**
 * @brief Conversion factor from echo duration to centimeters.
 */
#define ULTRASONIC_CM_PER_US (0.017f)

/**
 * @brief Conversion factor from centimeters to inches.
 */
#define ULTRASONIC_INCH_PER_CM (0.3937008f)

/* ========================================================================= */
/* Private Variables                                                          */
/* ========================================================================= */

/**
 * @brief Default ultrasonic sensor object used by the Arduino-like API.
 */
static Ultrasonic_t g_stDefaultUltrasonic =
    {
        0U,
        0U,
        ULTRASONIC_DEFAULT_TIMEOUT_US};

/**
 * @brief Internal sample buffer used by ultrasonicReadFiltered().
 */
static float g_f32DefaultFilterBuffer[ULTRASONIC_DEFAULT_FILTER_SAMPLES];

/* ========================================================================= */
/* Private Function Prototypes                                                */
/* ========================================================================= */

static uint8_t Ultrasonic_IsValidSensor(Ultrasonic_t *sensor);
static void Ultrasonic_SendTriggerPulse(Ultrasonic_t *sensor);
static void Ultrasonic_SortAscending(float *buffer,
                                     uint8_t sampleCount);
static float Ultrasonic_AverageMiddleSamples(float *buffer,
                                             uint8_t sampleCount);

/* ========================================================================= */
/* Private Functions                                                          */
/* ========================================================================= */

/**
 * @brief Check whether a sensor object pointer is valid.
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
 * @brief Generate HC-SR04 trigger pulse.
 *
 * @details
 * The HC-SR04 starts a measurement when TRIG receives a HIGH pulse
 * of at least 10 microseconds.
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
 * @brief Sort float samples in ascending order.
 *
 * @details
 * This function uses a simple nested-loop sort instead of a more complex
 * algorithm. The goal is readability for students.
 *
 * @param[in,out] buffer
 * Sample buffer to sort.
 *
 * @param[in] sampleCount
 * Number of samples inside the buffer.
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
 * The filter removes noisy low-end and high-end samples after sorting.
 *
 * With 20 samples:
 *
 * - Remove 5 smallest samples.
 * - Remove 5 largest samples.
 * - Average the 10 middle samples.
 *
 * For smaller sample counts, the remove count is sampleCount / 4.
 *
 * Invalid distance values are skipped during averaging.
 *
 * @param[in] buffer
 * Sorted sample buffer.
 *
 * @param[in] sampleCount
 * Number of samples inside the buffer.
 *
 * @return float
 * Average middle distance in centimeters, or ULTRASONIC_INVALID_DISTANCE_CM.
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

/**
 * @brief Initialize the default ultrasonic sensor.
 *
 * @param[in] trigPin
 * Logical pin connected to TRIG.
 *
 * @param[in] echoPin
 * Logical pin connected to ECHO.
 *
 * @return None.
 */
void ultrasonicBegin(uint8_t trigPin,
                     uint8_t echoPin)
{
    Ultrasonic_Begin(&g_stDefaultUltrasonic,
                     trigPin,
                     echoPin);
}

/**
 * @brief Set timeout for the default ultrasonic sensor.
 *
 * @param[in] timeoutUs
 * Timeout value in microseconds.
 *
 * @return None.
 */
void ultrasonicSetTimeout(uint32_t timeoutUs)
{
    Ultrasonic_SetTimeout(&g_stDefaultUltrasonic,
                          timeoutUs);
}

/**
 * @brief Read echo duration from the default ultrasonic sensor.
 *
 * @return uint32_t
 * Echo duration in microseconds.
 */
uint32_t ultrasonicReadDuration(void)
{
    return Ultrasonic_ReadDurationUs(&g_stDefaultUltrasonic);
}

/**
 * @brief Read distance in centimeters from the default ultrasonic sensor.
 *
 * @return float
 * Distance in centimeters, or ULTRASONIC_INVALID_DISTANCE_CM.
 */
float ultrasonicRead(void)
{
    return Ultrasonic_ReadCm(&g_stDefaultUltrasonic);
}

/**
 * @brief Read distance in inches from the default ultrasonic sensor.
 *
 * @return float
 * Distance in inches, or ULTRASONIC_INVALID_DISTANCE_CM.
 */
float ultrasonicReadInch(void)
{
    return Ultrasonic_ReadInch(&g_stDefaultUltrasonic);
}

/**
 * @brief Read filtered distance from the default ultrasonic sensor.
 *
 * @return float
 * Filtered distance in centimeters, or ULTRASONIC_INVALID_DISTANCE_CM.
 */
float ultrasonicReadFiltered(void)
{
    return Ultrasonic_ReadCmFiltered(
        &g_stDefaultUltrasonic,
        g_f32DefaultFilterBuffer,
        ULTRASONIC_DEFAULT_FILTER_SAMPLES);
}

/* ========================================================================= */
/* Multi-instance API                                                         */
/* ========================================================================= */

/**
 * @brief Initialize an ultrasonic sensor instance.
 *
 * @details
 * This function stores logical pin information, configures the TRIG pin
 * as output, configures the ECHO pin as input, and drives TRIG LOW.
 *
 * @param[in,out] sensor
 * Pointer to ultrasonic sensor object.
 *
 * @param[in] trigPin
 * Logical pin connected to TRIG.
 *
 * @param[in] echoPin
 * Logical pin connected to ECHO.
 *
 * @return None.
 */
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

/**
 * @brief Set timeout for an ultrasonic sensor instance.
 *
 * @param[in,out] sensor
 * Pointer to ultrasonic sensor object.
 *
 * @param[in] timeoutUs
 * Timeout value in microseconds.
 *
 * @return None.
 */
void Ultrasonic_SetTimeout(Ultrasonic_t *sensor,
                           uint32_t timeoutUs)
{
    if ((1U == Ultrasonic_IsValidSensor(sensor)) &&
        (0U != timeoutUs))
    {
        sensor->timeoutUs = timeoutUs;
    }
}

/**
 * @brief Read echo pulse duration in microseconds.
 *
 * @details
 * This function sends a trigger pulse, waits for ECHO to become HIGH,
 * then measures the HIGH pulse width using micros().
 *
 * This version does not call delayMicroseconds(1U) inside the measurement
 * loops. Therefore it avoids repeatedly restarting LPIT channel 1 while
 * measuring the ECHO pulse.
 *
 * @param[in] sensor
 * Pointer to ultrasonic sensor object.
 *
 * @return uint32_t
 * Echo pulse duration in microseconds.
 *
 * @retval 0U
 * Timeout or invalid sensor.
 */
uint32_t Ultrasonic_ReadDurationUs(Ultrasonic_t *sensor)
{
    uint32_t u32StartTimeUs = 0U;
    uint32_t u32EchoStartUs = 0U;
    uint32_t u32EchoEndUs = 0U;
    uint32_t u32DurationUs = 0U;
    uint8_t u8IsTimeout = 0U;

    if (1U == Ultrasonic_IsValidSensor(sensor))
    {
        /*
         * Send the HC-SR04 trigger pulse.
         */
        Ultrasonic_SendTriggerPulse(sensor);

        /*
         * Wait for ECHO to become HIGH.
         */
        u32StartTimeUs = micros();

        while ((false == digitalRead(sensor->echoPin)) &&
               ((micros() - u32StartTimeUs) < sensor->timeoutUs))
        {
            /* Busy wait for echo rising edge. */
        }

        if ((micros() - u32StartTimeUs) >= sensor->timeoutUs)
        {
            u8IsTimeout = 1U;
        }

        /*
         * Measure how long ECHO stays HIGH.
         */
        if (0U == u8IsTimeout)
        {
            u32EchoStartUs = micros();

            while ((true == digitalRead(sensor->echoPin)) &&
                   ((micros() - u32EchoStartUs) < sensor->timeoutUs))
            {
                /* Busy wait for echo falling edge. */
            }

            u32EchoEndUs = micros();

            u32DurationUs = u32EchoEndUs - u32EchoStartUs;

            if (u32DurationUs >= sensor->timeoutUs)
            {
                u32DurationUs = 0U;
            }
        }
    }

    return u32DurationUs;
}

/**
 * @brief Read distance in centimeters.
 *
 * @param[in] sensor
 * Pointer to ultrasonic sensor object.
 *
 * @return float
 * Distance in centimeters, or ULTRASONIC_INVALID_DISTANCE_CM.
 */
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

/**
 * @brief Read distance in inches.
 *
 * @param[in] sensor
 * Pointer to ultrasonic sensor object.
 *
 * @return float
 * Distance in inches, or ULTRASONIC_INVALID_DISTANCE_CM.
 */
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

/**
 * @brief Read filtered distance in centimeters.
 *
 * @details
 * This function:
 *
 * 1. Takes sampleCount measurements.
 * 2. Stores all samples in the user-provided buffer.
 * 3. Sorts the buffer in ascending order.
 * 4. Removes noisy low-end and high-end samples.
 * 5. Returns the average of the middle samples.
 *
 * @param[in] sensor
 * Pointer to ultrasonic sensor object.
 *
 * @param[in,out] buffer
 * Buffer used to store distance samples.
 *
 * @param[in] sampleCount
 * Number of samples to collect.
 *
 * @return float
 * Filtered distance in centimeters, or ULTRASONIC_INVALID_DISTANCE_CM.
 */
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
