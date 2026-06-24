#ifndef ARDUINO_PINS_H
#define ARDUINO_PINS_H

/**
 * @file arduino_pins.h
 * @brief Logical pin mapping interface for EduFramework Arduino-style APIs.
 *
 * @details
 * This file defines the user-visible logical pin names used by the
 * Arduino-style API layer.
 *
 * The Arduino layer must not require users to know the real MCU port and
 * pin number. Instead, users work with logical names such as LED_RED,
 * GPIO0, BTN0, ADC0_SE12, and so on.
 *
 * This module maps each logical pin to:
 * - Real PORT peripheral base address
 * - Real GPIO peripheral base address
 * - Real pin number
 * - Pin role
 * - Pin capability flags
 *
 * Example:
 *
 * @code
 * digitalWrite(LED_RED, HIGH);
 * @endcode
 *
 * Internally:
 *
 * @code
 * LED_RED -> g_arduinoPinMap[LED_RED] -> PORTD / GPIOD / pin 15
 * @endcode
 */

#include "S32K144.h"
#include "board_pins.h"
#include "ftm.h"
#include "port.h"

#include <stdint.h>

/* ========================================================================= */
/* Logical Pin Definitions                                                    */
/* ========================================================================= */

/**
 * @brief General-purpose GPIO logical pins.
 *
 * @details
 * These names are exposed to user applications. The real MCU pin mapping is
 * defined in board_pins.h and connected through g_arduinoPinMap[].
 */
#define GPIO0                 (0U)
#define GPIO1                 (1U)
#define GPIO2                 (2U)
#define GPIO3                 (3U)
#define GPIO4                 (4U)
#define GPIO5                 (5U)
#define GPIO6                 (6U)
#define GPIO7                 (7U)
#define GPIO8                 (8U)
#define GPIO9                 (9U)

/**
 * @brief On-board LED logical pins.
 */
#define LED_RED               (10U)
#define LED_BLUE              (11U)
#define LED_GREEN             (12U)

/**
 * @brief On-board button logical pins.
 */
#define BTN0                  (13U)
#define BTN1                  (14U)

/**
 * @brief SPI header logical pins.
 *
 * @note
 * These pins are not marked as digital GPIO-capable in the current map because
 * they are reserved for SPI usage by the Arduino SPI wrapper.
 */
#define SPI_SCK               (15U)
#define SPI_SIN               (16U)
#define SPI_SOUT              (17U)
#define SPI_PCS0              (18U)

/**
 * @brief I2C header logical pins.
 *
 * @note
 * These pins are not marked as digital GPIO-capable in the current map because
 * they are reserved for I2C usage by the Arduino Wire wrapper.
 */
#define I2C_SCL               (19U)
#define I2C_SDA               (20U)

/**
 * @brief ADC logical pins.
 */
#define ADC0_SE12             (21U)
#define ADC0_SE13             (22U)

/**
 * @brief Total number of logical pins available in the Arduino pin map.
 */
#define NUM_LOGICAL_PINS      (23U)

/**
 * @brief Default built-in LED used by Arduino-style examples.
 */
#define LED_BUILTIN           (LED_BLUE)

/* ========================================================================= */
/* Pin Role Definitions                                                       */
/* ========================================================================= */

/**
 * @brief Pin role type.
 *
 * @details
 * This project avoids enum for new code because enum storage size is compiler
 * dependent. A fixed-width integer type is used instead.
 */
typedef uint8_t PinRole_t;

/**
 * @brief Pin is a general-purpose digital GPIO.
 */
#define PIN_ROLE_GPIO         ((PinRole_t)0U)

/**
 * @brief Pin is connected to an on-board LED.
 */
#define PIN_ROLE_LED          ((PinRole_t)1U)

/**
 * @brief Pin is connected to an on-board button.
 */
#define PIN_ROLE_BUTTON       ((PinRole_t)2U)

/**
 * @brief Pin is reserved for SPI peripheral usage.
 */
#define PIN_ROLE_SPI          ((PinRole_t)3U)

/**
 * @brief Pin is reserved for I2C peripheral usage.
 */
#define PIN_ROLE_I2C          ((PinRole_t)4U)

/**
 * @brief Pin is used as ADC analog input.
 */
#define PIN_ROLE_ADC          ((PinRole_t)5U)

/* ========================================================================= */
/* Pin Capability Definitions                                                 */
/* ========================================================================= */

/**
 * @brief Pin capability type.
 *
 * @details
 * Capability flags describe which Arduino-style APIs are allowed to use a
 * logical pin.
 *
 * Example:
 * - PIN_CAP_DIGITAL allows pinMode(), digitalWrite(), digitalRead().
 * - PIN_CAP_ANALOG_IN allows analogRead().
 * - PIN_CAP_PWM allows analogWrite().
 */
typedef uint8_t PinCapability_t;

/**
 * @brief Pin has no Arduino-style user capability.
 */
#define PIN_CAP_NONE          ((PinCapability_t)0U)

/**
 * @brief Pin supports digital I/O operations.
 */
#define PIN_CAP_DIGITAL       ((PinCapability_t)(1U << 0U))

/**
 * @brief Pin supports analog input operations.
 */
#define PIN_CAP_ANALOG_IN     ((PinCapability_t)(1U << 1U))

/**
 * @brief Pin supports PWM output operations.
 */
#define PIN_CAP_PWM           ((PinCapability_t)(1U << 2U))

/* ========================================================================= */
/* Common Return Values                                                       */
/* ========================================================================= */

/**
 * @brief Arduino helper return value: true.
 */
#define ARDUINO_VALID_TRUE    (1U)

/**
 * @brief Arduino helper return value: false.
 */
#define ARDUINO_VALID_FALSE   (0U)

/**
 * @brief Check whether a logical pin index is inside the pin map range.
 */
#define ARDUINO_IS_VALID_PIN(u8Pin)    ((uint8_t)(u8Pin) < NUM_LOGICAL_PINS)

/* ========================================================================= */
/* Structure Definitions                                                      */
/* ========================================================================= */

/**
 * @brief Logical Arduino pin mapping entry.
 *
 * @details
 * Each logical pin has one entry in g_arduinoPinMap[].
 *
 * The Arduino layer uses this table to translate from user-facing pin names
 * into real MaaZEDU/S32K144 hardware resources.
 */
typedef struct
{
    /**
     * @brief PORT peripheral used for mux, pull resistor, and filter control.
     */
    PORT_Type *portBase;

    /**
     * @brief GPIO peripheral used for digital input/output register access.
     */
    GPIO_Type *gpioBase;

    /**
     * @brief Pin number inside the selected PORT/GPIO module.
     */
    uint8_t pinNumber;

    /**
     * @brief Logical role of the pin.
     */
    PinRole_t role;

    /**
     * @brief Capability flags describing allowed Arduino-style operations.
     */
    PinCapability_t capability;
} ArduinoPinMap_t;

/**
 * @brief PWM mapping entry for a logical Arduino pin.
 *
 * @details
 * This table is used by analogWrite() to translate a logical pin into an FTM
 * instance, FTM channel, and required PORT mux value.
 */
typedef struct
{
    /**
     * @brief FTM peripheral instance used for PWM output.
     */
    FTM_Instance_t instance;

    /**
     * @brief FTM channel used for PWM output.
     */
    FTM_Channel_t channel;

    /**
     * @brief PORT mux value required to route the FTM output to the pin.
     */
    port_mux_t mux;
} ArduinoPwmMap_t;

/* ========================================================================= */
/* Global Mapping Tables                                                      */
/* ========================================================================= */

/**
 * @brief Logical pin to hardware pin mapping table.
 */
extern const ArduinoPinMap_t g_arduinoPinMap[NUM_LOGICAL_PINS];

/**
 * @brief Logical pin to PWM hardware mapping table.
 */
extern const ArduinoPwmMap_t g_arduinoPwmMap[NUM_LOGICAL_PINS];

/* ========================================================================= */
/* Public API Prototypes                                                      */
/* ========================================================================= */

/**
 * @brief Check whether a logical Arduino pin number is valid.
 *
 * @param[in] u8Pin
 * Logical Arduino pin number.
 *
 * @return uint8_t
 * @retval 1U Pin is valid.
 * @retval 0U Pin is invalid.
 */
uint8_t Arduino_IsValidPin(uint8_t u8Pin);

/**
 * @brief Check whether a logical pin supports digital I/O.
 *
 * @param[in] u8Pin
 * Logical Arduino pin number.
 *
 * @return uint8_t
 * @retval 1U Pin supports digital I/O.
 * @retval 0U Pin does not support digital I/O or pin is invalid.
 */
uint8_t Arduino_HasDigitalCapability(uint8_t u8Pin);

/**
 * @brief Check whether a logical pin supports analog input.
 *
 * @param[in] u8Pin
 * Logical Arduino pin number.
 *
 * @return uint8_t
 * @retval 1U Pin supports analog input.
 * @retval 0U Pin does not support analog input or pin is invalid.
 */
uint8_t Arduino_HasAnalogInputCapability(uint8_t u8Pin);

/**
 * @brief Check whether a logical pin supports PWM output.
 *
 * @param[in] u8Pin
 * Logical Arduino pin number.
 *
 * @return uint8_t
 * @retval 1U Pin supports PWM output.
 * @retval 0U Pin does not support PWM output or pin is invalid.
 */
uint8_t Arduino_HasPwmCapability(uint8_t u8Pin);

/**
 * @brief Get PWM mapping information for a logical pin.
 *
 * @param[in] u8Pin
 * Logical Arduino pin number.
 *
 * @param[out] pPwmMap
 * Pointer to destination PWM map structure.
 *
 * @return uint8_t
 * @retval 1U PWM map is valid and copied successfully.
 * @retval 0U Pin is invalid, pin does not support PWM, or output pointer is NULL.
 */
uint8_t Arduino_GetPwmMap(uint8_t u8Pin,
                          ArduinoPwmMap_t *pPwmMap);

#endif /* ARDUINO_PINS_H */