/**
 * @file encoder.c
 * @brief Quadrature encoder device implementation for EduFramework.
 *
 * @details
 * This module implements interrupt-based quadrature x4 decoding.
 *
 * Both encoder channels generate interrupts on rising and falling edges.
 * A four-bit transition value is formed from:
 *
 *     previous A/B state + current A/B state
 *
 * A lookup table determines whether the transition represents:
 *
 * - +1 count
 * - -1 count
 * - no valid movement
 *
 * RPM calculation is performed outside interrupt context by Encoder_Update().
 */

#include "encoder.h"

#include "Arduino.h"
#include "arduino_pins.h"
#include "gpio.h"
#include "irq.h"
#include "port.h"

#include <stdbool.h>
#include <stddef.h>

/* ========================================================================= */
/* Private Constants                                                         */
/* ========================================================================= */

/**
 * @brief Encoder RPM calculation interval.
 *
 * @details
 * 100 ms provides reasonably fast speed updates while still accumulating
 * enough encoder counts for useful RPM resolution.
 */
#define ENCODER_RPM_UPDATE_INTERVAL_MS (100U)

/**
 * @brief Number of milliseconds in one minute.
 */
#define ENCODER_MILLISECONDS_PER_MINUTE (60000.0F)

/**
 * @brief Two-bit mask used for an encoder A/B state.
 */
#define ENCODER_STATE_MASK (0x03U)

/**
 * @brief Shift used when combining previous and current quadrature states.
 */
#define ENCODER_PREVIOUS_STATE_SHIFT (2U)

/**
 * @brief Successful initialization result.
 */
#define ENCODER_INIT_SUCCESS (1U)

/**
 * @brief Failed initialization result.
 */
#define ENCODER_INIT_FAILED (0U)

/* ========================================================================= */
/* Private State                                                             */
/* ========================================================================= */

/**
 * @brief Logical pin connected to encoder channel A.
 */
static uint8_t s_u8ChannelAPin = 0U;

/**
 * @brief Logical pin connected to encoder channel B.
 */
static uint8_t s_u8ChannelBPin = 0U;

/**
 * @brief Hardware mapping for encoder channel A.
 */
static ArduinoPinMap_t s_channelAMap;

/**
 * @brief Hardware mapping for encoder channel B.
 */
static ArduinoPinMap_t s_channelBMap;

/**
 * @brief Accumulated signed quadrature count.
 *
 * @details
 * This variable is modified from interrupt context.
 */
static volatile int32_t s_s32Count = 0;

/**
 * @brief Previous two-bit A/B encoder state.
 *
 * Bit 1 = channel A.
 * Bit 0 = channel B.
 */
static volatile uint8_t s_u8PreviousState = 0U;

/**
 * @brief Most recently calculated direction.
 */
static volatile encoder_direction_t s_direction =
    ENCODER_DIRECTION_STOPPED;

/**
 * @brief Number of decoded counts per mechanical revolution.
 */
static uint16_t s_u16CountsPerRevolution = 0U;

/**
 * @brief Encoder count captured during the previous RPM update.
 */
static int32_t s_s32PreviousRpmCount = 0;

/**
 * @brief Timestamp of the previous RPM calculation.
 */
static uint32_t s_u32PreviousRpmTimeMs = 0U;

/**
 * @brief Latest calculated RPM magnitude.
 */
static float s_fRpm = 0.0F;

/**
 * @brief Indicates whether the encoder module has been initialized.
 */
static uint8_t s_u8Initialized = 0U;

/* ========================================================================= */
/* Quadrature Decode Table                                                   */
/* ========================================================================= */

/**
 * @brief Quadrature transition lookup table.
 *
 * @details
 * The index contains:
 *
 *     previous state in bits [3:2]
 *     current state  in bits [1:0]
 *
 * Valid adjacent quadrature transitions produce +1 or -1.
 * Invalid transitions and unchanged states produce zero.
 *
 * Reversing encoder A/B wiring reverses the resulting direction.
 */
static const int8_t s_as8QuadratureTable[16] =
    {
        0, -1, 1, 0,
        1, 0, 0, -1,
        -1, 0, 0, 1,
        0, 1, -1, 0};

/* ========================================================================= */
/* Private Functions                                                         */
/* ========================================================================= */

/**
 * @brief Read the current two-bit encoder A/B state.
 *
 * @return Encoder state in the range 0 to 3.
 */
static uint8_t Encoder_ReadState(void)
{
    uint8_t u8State = 0U;

    if (true == GPIO_ReadPin(s_channelAMap.gpioBase,
                             s_channelAMap.pinNumber))
    {
        u8State |= 0x02U;
    }
    else
    {
        /* Channel A is low. */
    }

    if (true == GPIO_ReadPin(s_channelBMap.gpioBase,
                             s_channelBMap.pinNumber))
    {
        u8State |= 0x01U;
    }
    else
    {
        /* Channel B is low. */
    }

    return (uint8_t)(u8State & ENCODER_STATE_MASK);
}

/**
 * @brief Decode one quadrature state transition.
 *
 * @details
 * This function is executed from PORT interrupt context and must remain
 * short and non-blocking.
 *
 * @return None.
 */
static void Encoder_DecodeTransition(void)
{
    uint8_t u8CurrentState;
    uint8_t u8Transition;
    int8_t s8Step;

    u8CurrentState = Encoder_ReadState();

    u8Transition =
        (uint8_t)((s_u8PreviousState << ENCODER_PREVIOUS_STATE_SHIFT) |
                  u8CurrentState);

    s8Step = s_as8QuadratureTable[u8Transition];

    if (0 < s8Step)
    {
        s_s32Count++;
    }
    else if (0 > s8Step)
    {
        s_s32Count--;
    }
    else
    {
        /*
         * No valid adjacent quadrature transition.
         *
         * This includes an unchanged state or an invalid two-bit jump.
         */
    }

    s_u8PreviousState = u8CurrentState;

    return;
}

/**
 * @brief Handle PORTD interrupt events used by the encoder.
 *
 * @return None.
 */
static void Encoder_PORTDCallback(void)
{
    uint32_t u32Flags;
    uint32_t u32EncoderFlags = 0U;

    u32Flags = PORT_GetInterruptFlags(IP_PORTD);

    if (IP_PORTD == s_channelAMap.portBase)
    {
        u32EncoderFlags |= (1UL << s_channelAMap.pinNumber);
    }
    else
    {
        /* Channel A is not located on PORTD. */
    }

    if (IP_PORTD == s_channelBMap.portBase)
    {
        u32EncoderFlags |= (1UL << s_channelBMap.pinNumber);
    }
    else
    {
        /* Channel B is not located on PORTD. */
    }

    u32EncoderFlags &= u32Flags;

    if (0U != u32EncoderFlags)
    {
        /*
         * Capture the current A/B state once for this interrupt event.
         */
        Encoder_DecodeTransition();

        /*
         * Clear only encoder-related flags.
         */
        PORT_ClearInterruptFlags(IP_PORTD, u32EncoderFlags);
    }
    else
    {
        /* PORTD interrupt was not generated by this encoder. */
    }

    return;
}

/**
 * @brief Handle PORTE interrupt events used by the encoder.
 *
 * @return None.
 */
static void Encoder_PORTECallback(void)
{
    uint32_t u32Flags;
    uint32_t u32EncoderFlags = 0U;

    u32Flags = PORT_GetInterruptFlags(IP_PORTE);

    if (IP_PORTE == s_channelAMap.portBase)
    {
        u32EncoderFlags |= (1UL << s_channelAMap.pinNumber);
    }
    else
    {
        /* Channel A is not located on PORTE. */
    }

    if (IP_PORTE == s_channelBMap.portBase)
    {
        u32EncoderFlags |= (1UL << s_channelBMap.pinNumber);
    }
    else
    {
        /* Channel B is not located on PORTE. */
    }

    u32EncoderFlags &= u32Flags;

    if (0U != u32EncoderFlags)
    {
        Encoder_DecodeTransition();

        PORT_ClearInterruptFlags(IP_PORTE, u32EncoderFlags);
    }
    else
    {
        /* PORTE interrupt was not generated by this encoder. */
    }

    return;
}

/**
 * @brief Configure one logical encoder pin as interrupt input.
 *
 * @param[in] pPinMap
 * Hardware mapping of the encoder pin.
 *
 * @return None.
 */
static void Encoder_ConfigurePin(const ArduinoPinMap_t *pPinMap)
{
    if (NULL != pPinMap)
    {
        /*
         * pinMode() establishes GPIO mux and input direction through the
         * public Arduino digital API.
         */
        GPIO_SetPinDirection(pPinMap->gpioBase,
                             pPinMap->pinNumber,
                             GPIO_DIRECTION_INPUT);

        PORT_SetPinMux(pPinMap->portBase,
                       pPinMap->pinNumber,
                       PORT_MUX_GPIO);

        /*
         * Clear any stale interrupt flag before enabling edge detection.
         */
        PORT_ClearPinInterruptFlag(pPinMap->portBase,
                                   pPinMap->pinNumber);

        /*
         * Quadrature x4 requires both rising and falling edges.
         */
        PORT_SetPinInterruptConfig(pPinMap->portBase,
                                   pPinMap->pinNumber,
                                   PORT_INTERRUPT_EITHER_EDGE);
    }
    else
    {
        /* Invalid pin map pointer. */
    }

    return;
}

/* ========================================================================= */
/* Public Functions                                                          */
/* ========================================================================= */

uint8_t Encoder_Init(uint8_t channelAPin,
                     uint8_t channelBPin,
                     uint16_t countsPerRevolution)
{
    uint8_t u8Result = ENCODER_INIT_FAILED;
    bool bUsesPortD = false;
    bool bUsesPortE = false;

    if ((channelAPin != channelBPin) &&
        (0U != countsPerRevolution) &&
        (ARDUINO_VALID_TRUE == Arduino_HasDigitalCapability(channelAPin)) &&
        (ARDUINO_VALID_TRUE == Arduino_HasDigitalCapability(channelBPin)) &&
        (ARDUINO_VALID_TRUE == Arduino_HasInterruptCapability(channelAPin)) &&
        (ARDUINO_VALID_TRUE == Arduino_HasInterruptCapability(channelBPin)))
    {
        s_u8ChannelAPin = channelAPin;
        s_u8ChannelBPin = channelBPin;

        s_channelAMap = g_arduinoPinMap[s_u8ChannelAPin];
        s_channelBMap = g_arduinoPinMap[s_u8ChannelBPin];

        /*
         * The current EduFramework interrupt bridge supports the logical
         * interrupt GPIO pins located on PORTD and PORTE.
         */
        if (((IP_PORTD == s_channelAMap.portBase) ||
             (IP_PORTE == s_channelAMap.portBase)) &&
            ((IP_PORTD == s_channelBMap.portBase) ||
             (IP_PORTE == s_channelBMap.portBase)))
        {
            s_u16CountsPerRevolution = countsPerRevolution;

            s_s32Count = 0;
            s_s32PreviousRpmCount = 0;
            s_fRpm = 0.0F;
            s_direction = ENCODER_DIRECTION_STOPPED;

            Encoder_ConfigurePin(&s_channelAMap);
            Encoder_ConfigurePin(&s_channelBMap);

            /*
             * Read the real initial A/B state before enabling NVIC.
             * This prevents the first transition from being decoded against
             * an artificial 00 starting state.
             */
            s_u8PreviousState = Encoder_ReadState();

            /*
             * Determine which shared PORT interrupt vectors are required.
             */
            if ((IP_PORTD == s_channelAMap.portBase) ||
                (IP_PORTD == s_channelBMap.portBase))
            {
                bUsesPortD = true;
            }
            else
            {
                bUsesPortD = false;
            }

            if ((IP_PORTE == s_channelAMap.portBase) ||
                (IP_PORTE == s_channelBMap.portBase))
            {
                bUsesPortE = true;
            }
            else
            {
                bUsesPortE = false;
            }

            /*
             * Register callbacks before enabling the corresponding NVIC
             * interrupt lines.
             */
            if (true == bUsesPortD)
            {
                IRQ_PORTD_SetCallback(Encoder_PORTDCallback);
                IRQ_PORTD_Init();
            }
            else
            {
                /* Encoder does not use PORTD. */
            }

            if (true == bUsesPortE)
            {
                IRQ_PORTE_SetCallback(Encoder_PORTECallback);
                IRQ_PORTE_Init();
            }
            else
            {
                /* Encoder does not use PORTE. */
            }

            s_u32PreviousRpmTimeMs = millis();
            s_u8Initialized = ENCODER_INIT_SUCCESS;

            u8Result = ENCODER_INIT_SUCCESS;
        }
        else
        {
            u8Result = ENCODER_INIT_FAILED;
        }
    }
    else
    {
        u8Result = ENCODER_INIT_FAILED;
    }

    return u8Result;
}

int32_t Encoder_GetCount(void)
{
    int32_t s32Count;

    /*
     * Cortex-M4 performs aligned 32-bit reads atomically.
     */
    s32Count = s_s32Count;

    return s32Count;
}

void Encoder_Reset(void)
{
    s_s32Count = 0;
    s_s32PreviousRpmCount = 0;

    s_fRpm = 0.0F;
    s_direction = ENCODER_DIRECTION_STOPPED;

    s_u32PreviousRpmTimeMs = millis();

    return;
}

encoder_direction_t Encoder_GetDirection(void)
{
    encoder_direction_t direction;

    direction = s_direction;

    return direction;
}

void Encoder_Update(void)
{
    uint32_t u32CurrentTimeMs;
    uint32_t u32ElapsedTimeMs;
    int32_t s32CurrentCount;
    int32_t s32DeltaCount;
    float fAbsoluteDeltaCount;

    if (ENCODER_INIT_SUCCESS == s_u8Initialized)
    {
        u32CurrentTimeMs = millis();

        u32ElapsedTimeMs =
            (uint32_t)(u32CurrentTimeMs - s_u32PreviousRpmTimeMs);

        if (ENCODER_RPM_UPDATE_INTERVAL_MS <= u32ElapsedTimeMs)
        {
            s32CurrentCount = s_s32Count;

            s32DeltaCount =
                s32CurrentCount - s_s32PreviousRpmCount;

            if (0 < s32DeltaCount)
            {
                s_direction = ENCODER_DIRECTION_FORWARD;
                fAbsoluteDeltaCount = (float)s32DeltaCount;
            }
            else if (0 > s32DeltaCount)
            {
                s_direction = ENCODER_DIRECTION_REVERSE;
                fAbsoluteDeltaCount = (float)(-s32DeltaCount);
            }
            else
            {
                s_direction = ENCODER_DIRECTION_STOPPED;
                fAbsoluteDeltaCount = 0.0F;
            }

            /*
             * RPM =
             *
             * counts
             * ------------------- x milliseconds per minute
             * CPR x elapsed_ms
             */
            s_fRpm =
                (fAbsoluteDeltaCount *
                 ENCODER_MILLISECONDS_PER_MINUTE) /
                ((float)s_u16CountsPerRevolution *
                 (float)u32ElapsedTimeMs);

            s_s32PreviousRpmCount = s32CurrentCount;
            s_u32PreviousRpmTimeMs = u32CurrentTimeMs;
        }
        else
        {
            /*
             * Measurement interval has not elapsed yet.
             * Keep the previous RPM value.
             */
        }
    }
    else
    {
        /*
         * Encoder has not been initialized.
         */
    }

    return;
}

float Encoder_GetRpm(void)
{
    return s_fRpm;
}