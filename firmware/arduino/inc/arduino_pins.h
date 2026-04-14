#ifndef ARDUINO_PINS_H
#define ARDUINO_PINS_H

#include "S32K144.h"
#include <stdint.h>

/* =========================================
 * User-visible pin names (match board labels)
 * ========================================= */

/* GPIO header */
#define GPIO0           0U
#define GPIO1           1U
#define GPIO2           2U
#define GPIO3           3U
#define GPIO4           4U
#define GPIO5           5U
#define GPIO6           6U
#define GPIO7           7U
#define GPIO8           8U
#define GPIO9           9U

/* On-board LEDs */
#define LED_RED         10U
#define LED_BLUE        11U
#define LED_GREEN       12U

/* On-board buttons */
#define BTN0            13U
#define BTN1            14U

/* SPI header */
#define SPI_SCK         15U
#define SPI_SIN         16U
#define SPI_SOUT        17U
#define SPI_PCS0        18U

/* I2C header */
#define I2C_SCL         19U
#define I2C_SDA         20U

/* ADC header */
#define ADC0_SE12       21U
#define ADC0_SE13       22U

#define NUM_LOGICAL_PINS    23U

#define LED_BUILTIN     LED_BLUE

/* =========================================
 * Pin role
 * ========================================= */
typedef enum
{
    PIN_ROLE_GPIO = 0U,
    PIN_ROLE_LED,
    PIN_ROLE_BUTTON,
    PIN_ROLE_SPI,
    PIN_ROLE_I2C,
    PIN_ROLE_ADC
} PinRole_t;

/* =========================================
 * Pin capability flags
 * ========================================= */
typedef enum
{
    PIN_CAP_NONE        = 0U,
    PIN_CAP_DIGITAL     = (1U << 0),
    PIN_CAP_ANALOG_IN   = (1U << 1),
    PIN_CAP_PWM         = (1U << 2)
} PinCapability_t;

/* =========================================
 * Arduino pin map type
 * ========================================= */
typedef struct
{
    PORT_Type   *portBase;
    GPIO_Type   *gpioBase;
    uint8_t      pinNumber;
    PinRole_t    role;
    uint8_t      capability;
} ArduinoPinMap_t;

/* =========================================
 * Global pin map table
 * ========================================= */
extern const ArduinoPinMap_t g_arduinoPinMap[NUM_LOGICAL_PINS];

/* =========================================
 * Validation macros
 * ========================================= */
#define ARDUINO_IS_VALID_PIN(pin)    ((uint8_t)(pin) < NUM_LOGICAL_PINS)

/* =========================================
 * Helper functions
 * ========================================= */
uint8_t Arduino_IsValidPin(uint8_t pin);
uint8_t Arduino_HasDigitalCapability(uint8_t pin);
uint8_t Arduino_HasAnalogInputCapability(uint8_t pin);
uint8_t Arduino_HasPwmCapability(uint8_t pin);

#endif /* ARDUINO_PINS_H */
