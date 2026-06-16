/**
 * @file wiring_digital.c
 * @brief Arduino-style digital I/O implementation for EduFramework.
 *
 * @details
 * This file implements the Arduino-style digital I/O layer.
 *
 * The purpose of this layer is to keep user code simple while preserving a
 * clean separation from the low-level register drivers.
 *
 * Architecture:
 *
 * @code
 * User Application
 *      |
 *      v
 * pinMode() / digitalWrite() / digitalRead()
 *      |
 *      v
 * Arduino Pin Mapping
 *      |
 *      v
 * PORT Driver + GPIO Driver
 *      |
 *      v
 * S32K144 Hardware Registers
 * @endcode
 *
 * This file must not access S32K144 registers directly. All hardware access
 * must be done through the driver layer.
 */

#include "wiring_digital.h"
#include "arduino_defs.h"
#include "arduino_pins.h"

#include "gpio.h"
#include "port.h"

/* ========================================================================= */
/* Private Function Prototypes                                                */
/* ========================================================================= */

/**
 * @brief Enable PORT clock from a PORT base pointer.
 *
 * @param[in] pBase
 * Pointer to PORT peripheral base address.
 *
 * @return None.
 */
static void Arduino_EnablePortClock(PORT_Type *pBase);

/**
 * @brief Check whether a pin can be used by digital APIs.
 *
 * @param[in] u8Pin
 * Logical Arduino pin number.
 *
 * @return uint8_t
 * @retval 1U Pin is valid and supports digital capability.
 * @retval 0U Pin is invalid or does not support digital capability.
 */
static uint8_t Arduino_IsDigitalPinAvailable(uint8_t u8Pin);

/* ========================================================================= */
/* Private Functions                                                          */
/* ========================================================================= */

static void Arduino_EnablePortClock(PORT_Type *pBase)
{
    /*
     * The pin mapping table stores PORT base pointers because PORT functions
     * need the base address to configure PCR registers.
     *
     * PORT_EnableClock() works with logical port names, so this helper
     * translates from base pointer to port name.
     */
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
        /*
         * Invalid PORT base pointer.
         * Keep Arduino-style behavior: do nothing and allow caller to exit.
         */
    }

    return;
}

static uint8_t Arduino_IsDigitalPinAvailable(uint8_t u8Pin)
{
    uint8_t u8RetVal = 0U;

    /*
     * Digital APIs should only operate on pins explicitly marked with
     * PIN_CAP_DIGITAL. This prevents accidental use of ADC/SPI/I2C-only pins
     * through pinMode(), digitalWrite(), digitalRead(), or digitalToggle().
     */
    if (ARDUINO_VALID_TRUE == Arduino_HasDigitalCapability(u8Pin))
    {
        u8RetVal = ARDUINO_VALID_TRUE;
    }
    else
    {
        u8RetVal = ARDUINO_VALID_FALSE;
    }

    return u8RetVal;
}

/* ========================================================================= */
/* Public Functions                                                           */
/* ========================================================================= */

void pinMode(uint8_t pin,
             uint8_t mode)
{
    const ArduinoPinMap_t *pPinMap = (const ArduinoPinMap_t *)0;

    /*
     * The Arduino API does not return error codes.
     * Invalid pins or non-digital pins are ignored safely.
     */
    if (ARDUINO_VALID_TRUE == Arduino_IsDigitalPinAvailable(pin))
    {
        pPinMap = &g_arduinoPinMap[pin];

        /*
         * PORT clock must be enabled before accessing PCR registers.
         * Without PORT clock, mux/pull/filter configuration may not take effect.
         */
        Arduino_EnablePortClock(pPinMap->portBase);

        /*
         * Digital I/O requires the pin mux to be configured as GPIO.
         * Peripheral functions such as SPI, I2C, ADC, or PWM use other mux values.
         */
        PORT_SetPinMux(pPinMap->portBase,
                       pPinMap->pinNumber,
                       PORT_MUX_GPIO);

        /*
         * Start from a neutral pull configuration.
         * Specific input modes will enable pull-up or pull-down below.
         */
        PORT_SetPinPull(pPinMap->portBase,
                        pPinMap->pinNumber,
                        PORT_PULL_DISABLED);

        /*
         * Passive filter is disabled by default.
         * Mechanical button pins enable it again later in this function.
         */
        PORT_SetPassiveFilter(pPinMap->portBase,
                              pPinMap->pinNumber,
                              false);

        switch (mode)
        {
        case OUTPUT:
            /*
             * OUTPUT mode:
             * GPIO PDDR bit is set to 1 so the pin can drive output level.
             */
            GPIO_SetPinDirection(pPinMap->gpioBase,
                                 pPinMap->pinNumber,
                                 GPIO_DIRECTION_OUTPUT);
            break;

        case INPUT_PULLUP:
            /*
             * INPUT_PULLUP mode:
             * GPIO direction is input and internal pull-up resistor is enabled.
             */
            GPIO_SetPinDirection(pPinMap->gpioBase,
                                 pPinMap->pinNumber,
                                 GPIO_DIRECTION_INPUT);

            PORT_SetPinPull(pPinMap->portBase,
                            pPinMap->pinNumber,
                            PORT_PULL_UP);
            break;

        case INPUT_PULLDOWN:
            /*
             * INPUT_PULLDOWN mode:
             * GPIO direction is input and internal pull-down resistor is enabled.
             */
            GPIO_SetPinDirection(pPinMap->gpioBase,
                                 pPinMap->pinNumber,
                                 GPIO_DIRECTION_INPUT);

            PORT_SetPinPull(pPinMap->portBase,
                            pPinMap->pinNumber,
                            PORT_PULL_DOWN);
            break;

        case INPUT:
        default:
            /*
             * INPUT mode and unsupported mode:
             * Default safely to input without pull resistor.
             */
            GPIO_SetPinDirection(pPinMap->gpioBase,
                                 pPinMap->pinNumber,
                                 GPIO_DIRECTION_INPUT);
            break;
        }

        /*
         * On-board buttons are mechanical inputs and may generate short glitches.
         * Enable the PORT passive filter automatically for button-role pins.
         */
        if (PIN_ROLE_BUTTON == pPinMap->role)
        {
            PORT_SetPassiveFilter(pPinMap->portBase,
                                  pPinMap->pinNumber,
                                  true);
        }
        else
        {
            /* Non-button digital pins keep passive filter disabled. */
        }
    }
    else
    {
        /* Invalid or non-digital logical pin. Keep Arduino behavior: do nothing. */
    }

    return;
}

void digitalWrite(uint8_t pin,
                  uint8_t value)
{
    const ArduinoPinMap_t *pPinMap = (const ArduinoPinMap_t *)0;
    bool bOutputLevel = false;

    /*
     * The function intentionally checks digital capability instead of only
     * checking pin range. This protects ADC/SPI/I2C-only logical pins from
     * accidental GPIO writes.
     */
    if (ARDUINO_VALID_TRUE == Arduino_IsDigitalPinAvailable(pin))
    {
        pPinMap = &g_arduinoPinMap[pin];

        /*
         * Arduino-compatible behavior:
         * - HIGH means logic high.
         * - Any non-HIGH value is treated as LOW.
         */
        if (HIGH == value)
        {
            bOutputLevel = true;
        }
        else
        {
            bOutputLevel = false;
        }

        GPIO_WritePin(pPinMap->gpioBase,
                      pPinMap->pinNumber,
                      bOutputLevel);
    }
    else
    {
        /* Invalid or non-digital logical pin. Keep Arduino behavior: do nothing. */
    }

    return;
}

bool digitalRead(uint8_t pin)
{
    const ArduinoPinMap_t *pPinMap = (const ArduinoPinMap_t *)0;
    bool bInputLevel = false;

    /*
     * Invalid or non-digital pins return LOW-equivalent value because this API
     * follows Arduino style and has no separate error return.
     */
    if (ARDUINO_VALID_TRUE == Arduino_IsDigitalPinAvailable(pin))
    {
        pPinMap = &g_arduinoPinMap[pin];

        bInputLevel = GPIO_ReadPin(pPinMap->gpioBase,
                                   pPinMap->pinNumber);
    }
    else
    {
        bInputLevel = false;
    }

    return bInputLevel;
}

void digitalToggle(uint8_t pin)
{
    const ArduinoPinMap_t *pPinMap = (const ArduinoPinMap_t *)0;

    /*
     * Only pins that are valid and digital-capable may be toggled.
     */
    if (ARDUINO_VALID_TRUE == Arduino_IsDigitalPinAvailable(pin))
    {
        pPinMap = &g_arduinoPinMap[pin];

        GPIO_TogglePin(pPinMap->gpioBase,
                       pPinMap->pinNumber);
    }
    else
    {
        /* Invalid or non-digital logical pin. Keep Arduino behavior: do nothing. */
    }

    return;
}