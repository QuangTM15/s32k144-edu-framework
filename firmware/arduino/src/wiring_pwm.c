/**
 * @file wiring_pwm.c
 * @brief Internal Arduino-layer PWM service implementation.
 *
 * @details
 * This file implements shared PWM resource handling for Arduino-style APIs.
 *
 * PWM output is implemented using:
 * - Arduino pin mapping metadata.
 * - PORT driver for pin mux configuration.
 * - FTM driver for hardware PWM generation.
 *
 * The module maintains initialization state per FTM instance and channel
 * so that PWM hardware is configured only when required.
 *
 * The current default PWM configuration preserves the behavior previously
 * implemented directly inside wiring_analog.c:
 * - 8 MHz FTM source clock.
 * - 1 kHz PWM frequency.
 * - External FTM clock source selection.
 * - Prescaler divide-by-1.
 * - Edge-aligned low-true PWM.
 *
 * This module belongs to the Arduino-style API layer and must access FTM
 * hardware only through the low-level FTM driver.
 */

#include "wiring_pwm.h"
#include "arduino_pins.h"
#include "ftm.h"
#include "port.h"

/* ============================================================
 * Local constants
 * ============================================================ */

/**
 * @brief Generic boolean-like false value used inside this module.
 */
#define WIRING_PWM_FALSE (0U)

/**
 * @brief Generic boolean-like true value used inside this module.
 */
#define WIRING_PWM_TRUE (1U)

/**
 * @brief Default FTM source clock frequency used by Arduino PWM.
 */
#define WIRING_PWM_SRC_CLOCK_HZ (8000000UL)

/**
 * @brief Default PWM frequency used by analogWrite().
 */
#define WIRING_PWM_DEFAULT_FREQ_HZ (1000UL)

/**
 * @brief Maximum PWM duty percentage accepted by this module.
 */
#define WIRING_PWM_MAX_PERCENT (100U)

/**
 * @brief Number of FTM instances supported by the current PWM mapping.
 */
#define WIRING_PWM_INSTANCE_COUNT (3U)

/**
 * @brief Number of channels available per supported FTM instance.
 */
#define WIRING_PWM_CHANNEL_COUNT (8U)

/* ============================================================
 * Internal state
 * ============================================================ */

/**
 * @brief Initialization state for each supported FTM instance.
 */
static uint8_t s_au8PwmInitialized[WIRING_PWM_INSTANCE_COUNT] =
    {
        WIRING_PWM_FALSE};

/**
 * @brief Configuration state for each supported FTM PWM channel.
 */
static uint8_t
    s_au8PwmChannelConfigured[WIRING_PWM_INSTANCE_COUNT][WIRING_PWM_CHANNEL_COUNT] =
        {
            {WIRING_PWM_FALSE}};

/* ============================================================
 * Internal helpers
 * ============================================================ */

/**
 * @brief Check whether an FTM instance and channel can be tracked.
 *
 * @param[in] Instance
 * FTM instance identifier.
 *
 * @param[in] Channel
 * FTM channel identifier.
 *
 * @return uint8_t
 *
 * @retval WIRING_PWM_TRUE
 * Instance and channel are inside the supported state-table range.
 *
 * @retval WIRING_PWM_FALSE
 * Instance or channel is outside the supported range.
 */
static uint8_t Pwm_IsValidResource(FTM_Instance_t Instance,
                                   FTM_Channel_t Channel)
{
    uint8_t u8IsValid = WIRING_PWM_FALSE;

    if (((uint8_t)WIRING_PWM_INSTANCE_COUNT > (uint8_t)Instance) &&
        ((uint8_t)WIRING_PWM_CHANNEL_COUNT > (uint8_t)Channel))
    {
        u8IsValid = WIRING_PWM_TRUE;
    }

    return u8IsValid;
}

/**
 * @brief Enable PORT clock for a PORT register base pointer.
 *
 * @param[in] pBase
 * Pointer to PORT register block.
 *
 * @return uint8_t
 *
 * @retval WIRING_PWM_TRUE
 * PORT pointer is recognized and its clock has been enabled.
 *
 * @retval WIRING_PWM_FALSE
 * PORT pointer is not supported.
 */
static uint8_t Pwm_EnablePortClock(PORT_Type *pBase)
{
    uint8_t u8Result = WIRING_PWM_TRUE;

    if (IP_PORTA == pBase)
    {
        PORT_EnableClock(PORT_NAME_A);
    }
    else if (IP_PORTB == pBase)
    {
        PORT_EnableClock(PORT_NAME_B);
    }
    else if (IP_PORTC == pBase)
    {
        PORT_EnableClock(PORT_NAME_C);
    }
    else if (IP_PORTD == pBase)
    {
        PORT_EnableClock(PORT_NAME_D);
    }
    else if (IP_PORTE == pBase)
    {
        PORT_EnableClock(PORT_NAME_E);
    }
    else
    {
        u8Result = WIRING_PWM_FALSE;
    }

    return u8Result;
}

/**
 * @brief Ensure an FTM instance has been initialized for default PWM.
 *
 * @details
 * Each FTM instance is initialized only on its first use. Later channels
 * mapped to the same instance reuse the existing FTM base configuration.
 *
 * @param[in] Instance
 * FTM instance identifier.
 *
 * @return uint8_t
 *
 * @retval WIRING_PWM_TRUE
 * Instance is initialized or was already initialized.
 *
 * @retval WIRING_PWM_FALSE
 * Instance is invalid or initialization failed.
 */
static uint8_t Pwm_EnsureInstanceInitialized(FTM_Instance_t Instance)
{
    FTM_PwmConfig_t PwmConfig = {0U};
    uint8_t u8Result = WIRING_PWM_FALSE;

    if ((uint8_t)WIRING_PWM_INSTANCE_COUNT <= (uint8_t)Instance)
    {
        u8Result = WIRING_PWM_FALSE;
    }
    else if (WIRING_PWM_FALSE !=
             s_au8PwmInitialized[(uint8_t)Instance])
    {
        u8Result = WIRING_PWM_TRUE;
    }
    else
    {
        PwmConfig.srcClockHz = WIRING_PWM_SRC_CLOCK_HZ;
        PwmConfig.pwmFreqHz = WIRING_PWM_DEFAULT_FREQ_HZ;
        PwmConfig.clockSource = FTM_CLOCK_SOURCE_EXTERNAL;
        PwmConfig.prescaler = FTM_PRESCALER_DIV_1;

        if (FTM_STATUS_OK == FTM_InitPwm(Instance, &PwmConfig))
        {
            s_au8PwmInitialized[(uint8_t)Instance] = WIRING_PWM_TRUE;
            u8Result = WIRING_PWM_TRUE;
        }
        else
        {
            u8Result = WIRING_PWM_FALSE;
        }
    }

    return u8Result;
}

/**
 * @brief Ensure a logical PWM pin is configured for FTM output.
 *
 * @details
 * This helper performs all one-time configuration required before PWM duty
 * can be updated:
 * - Resolve Arduino PWM mapping.
 * - Validate the mapped FTM instance and channel.
 * - Enable the corresponding PORT clock.
 * - Configure the pin mux for FTM output.
 * - Initialize the mapped FTM instance if required.
 * - Configure the mapped channel for edge-aligned low-true PWM.
 * - Start the FTM counter.
 *
 * @param[in] u8Pin
 * Arduino-style logical pin identifier.
 *
 * @param[out] pPwmMap
 * Pointer used to receive the resolved PWM mapping.
 *
 * @return uint8_t
 *
 * @retval WIRING_PWM_TRUE
 * Pin is configured and ready for duty-cycle updates.
 *
 * @retval WIRING_PWM_FALSE
 * Pin mapping or hardware configuration failed.
 */
static uint8_t Pwm_EnsurePinConfigured(uint8_t u8Pin,
                                       ArduinoPwmMap_t *pPwmMap)
{
    const ArduinoPinMap_t *pPinMap = (const ArduinoPinMap_t *)0;
    uint8_t u8Result = WIRING_PWM_FALSE;

    if ((ArduinoPwmMap_t *)0 == pPwmMap)
    {
        u8Result = WIRING_PWM_FALSE;
    }
    else if (ARDUINO_VALID_TRUE != Arduino_GetPwmMap(u8Pin, pPwmMap))
    {
        u8Result = WIRING_PWM_FALSE;
    }
    else if (WIRING_PWM_FALSE ==
             Pwm_IsValidResource(pPwmMap->instance, pPwmMap->channel))
    {
        u8Result = WIRING_PWM_FALSE;
    }
    else
    {
        pPinMap = &g_arduinoPinMap[u8Pin];

        if (WIRING_PWM_FALSE == Pwm_EnablePortClock(pPinMap->portBase))
        {
            u8Result = WIRING_PWM_FALSE;
        }
        else
        {
            PORT_SetPinMux(pPinMap->portBase,
                           pPinMap->pinNumber,
                           pPwmMap->mux);

            if (WIRING_PWM_FALSE ==
                Pwm_EnsureInstanceInitialized(pPwmMap->instance))
            {
                u8Result = WIRING_PWM_FALSE;
            }
            else
            {
                if (WIRING_PWM_FALSE ==
                    s_au8PwmChannelConfigured
                        [(uint8_t)pPwmMap->instance]
                        [(uint8_t)pPwmMap->channel])
                {
                    if (FTM_STATUS_OK ==
                        FTM_SetChannelModePwm(
                            pPwmMap->instance,
                            pPwmMap->channel,
                            FTM_PWM_EDGE_ALIGNED_LOW_TRUE))
                    {
                        s_au8PwmChannelConfigured
                            [(uint8_t)pPwmMap->instance]
                            [(uint8_t)pPwmMap->channel] =
                                WIRING_PWM_TRUE;

                        u8Result = WIRING_PWM_TRUE;
                    }
                    else
                    {
                        u8Result = WIRING_PWM_FALSE;
                    }
                }
                else
                {
                    u8Result = WIRING_PWM_TRUE;
                }

                if (WIRING_PWM_TRUE == u8Result)
                {
                    if (FTM_STATUS_OK !=
                        FTM_StartCounter(pPwmMap->instance))
                    {
                        u8Result = WIRING_PWM_FALSE;
                    }
                }
            }
        }
    }

    return u8Result;
}

/* ============================================================
 * Public API
 * ============================================================ */

/**
 * @copydoc WiringPwm_SetDutyPercent
 */
WiringPwm_Status_t WiringPwm_SetDutyPercent(uint8_t u8Pin,
                                            uint8_t u8DutyPercent)
{
    ArduinoPwmMap_t PwmMap = {0U};
    WiringPwm_Status_t Status = WIRING_PWM_STATUS_OK;

    if (WIRING_PWM_MAX_PERCENT < u8DutyPercent)
    {
        Status = WIRING_PWM_STATUS_INVALID_ARGUMENT;
    }
    else if (ARDUINO_VALID_TRUE != Arduino_IsValidPin(u8Pin))
    {
        Status = WIRING_PWM_STATUS_INVALID_ARGUMENT;
    }
    else if (ARDUINO_VALID_TRUE != Arduino_HasPwmCapability(u8Pin))
    {
        Status = WIRING_PWM_STATUS_INVALID_ARGUMENT;
    }
    else if (WIRING_PWM_TRUE !=
             Pwm_EnsurePinConfigured(u8Pin, &PwmMap))
    {
        Status = WIRING_PWM_STATUS_DRIVER_ERROR;
    }
    else
    {
        if (FTM_STATUS_OK !=
            FTM_SetPwmDutyPercent(
                PwmMap.instance,
                PwmMap.channel,
                u8DutyPercent))
        {
            Status = WIRING_PWM_STATUS_DRIVER_ERROR;
        }
    }

    return Status;
}

/**
 * @copydoc WiringPwm_Start
 */
WiringPwm_Status_t WiringPwm_Start(uint8_t u8Pin,
                                   uint32_t u32FrequencyHz,
                                   uint8_t u8DutyPercent)
{
    ArduinoPwmMap_t PwmMap = {0U};
    WiringPwm_Status_t Status = WIRING_PWM_STATUS_OK;

    if (0UL == u32FrequencyHz)
    {
        Status = WIRING_PWM_STATUS_INVALID_ARGUMENT;
    }
    else if (WIRING_PWM_MAX_PERCENT < u8DutyPercent)
    {
        Status = WIRING_PWM_STATUS_INVALID_ARGUMENT;
    }
    else if (ARDUINO_VALID_TRUE != Arduino_IsValidPin(u8Pin))
    {
        Status = WIRING_PWM_STATUS_INVALID_ARGUMENT;
    }
    else if (ARDUINO_VALID_TRUE != Arduino_HasPwmCapability(u8Pin))
    {
        Status = WIRING_PWM_STATUS_INVALID_ARGUMENT;
    }
    else if (WIRING_PWM_TRUE !=
             Pwm_EnsurePinConfigured(u8Pin, &PwmMap))
    {
        Status = WIRING_PWM_STATUS_DRIVER_ERROR;
    }
    else
    {
        if (FTM_STATUS_OK !=
            FTM_SetPwmFrequency(PwmMap.instance, u32FrequencyHz))
        {
            Status = WIRING_PWM_STATUS_DRIVER_ERROR;
        }
        else if (FTM_STATUS_OK !=
                 FTM_SetPwmDutyPercent(PwmMap.instance,
                                       PwmMap.channel,
                                       u8DutyPercent))
        {
            Status = WIRING_PWM_STATUS_DRIVER_ERROR;
        }
        else
        {
            Status = WIRING_PWM_STATUS_OK;
        }
    }

    return Status;
}

/**
 * @copydoc WiringPwm_Stop
 */
WiringPwm_Status_t WiringPwm_Stop(uint8_t u8Pin)
{
    ArduinoPwmMap_t PwmMap = {0U};
    WiringPwm_Status_t Status = WIRING_PWM_STATUS_OK;

    if (ARDUINO_VALID_TRUE != Arduino_IsValidPin(u8Pin))
    {
        Status = WIRING_PWM_STATUS_INVALID_ARGUMENT;
    }
    else if (ARDUINO_VALID_TRUE != Arduino_HasPwmCapability(u8Pin))
    {
        Status = WIRING_PWM_STATUS_INVALID_ARGUMENT;
    }
    else if (ARDUINO_VALID_TRUE != Arduino_GetPwmMap(u8Pin, &PwmMap))
    {
        Status = WIRING_PWM_STATUS_INVALID_ARGUMENT;
    }
    else if (WIRING_PWM_FALSE ==
             Pwm_IsValidResource(PwmMap.instance, PwmMap.channel))
    {
        Status = WIRING_PWM_STATUS_INVALID_ARGUMENT;
    }
    else if (WIRING_PWM_FALSE ==
             s_au8PwmChannelConfigured
                 [(uint8_t)PwmMap.instance]
                 [(uint8_t)PwmMap.channel])
    {
        /*
         * The channel has not been configured by the PWM service.
         * There is no active PWM output to stop.
         */
        Status = WIRING_PWM_STATUS_OK;
    }
    else
    {
        /*
         * LOW_TRUE PWM uses the maximum compare value as the inactive
         * output state in the current framework implementation.
         *
         * Do not stop the FTM counter because other channels on the same
         * instance may still depend on it.
         */
        if (FTM_STATUS_OK !=
            FTM_SetPwmDutyPercent(PwmMap.instance,
                                  PwmMap.channel,
                                  WIRING_PWM_MAX_PERCENT))
        {
            Status = WIRING_PWM_STATUS_DRIVER_ERROR;
        }
        else
        {
            Status = WIRING_PWM_STATUS_OK;
        }
    }

    return Status;
}
