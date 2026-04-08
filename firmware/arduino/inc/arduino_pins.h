#ifndef ARDUINO_PINS_H
#define ARDUINO_PINS_H

#include "S32K144.h"
#include <stdint.h>

/* =========================================
 * Logical Pin Numbers
 * ========================================= */

/* Digital GPIO header */
#define D0              0U
#define D1              1U
#define D2              2U
#define D3              3U
#define D4              4U
#define D5              5U
#define D6              6U
#define D7              7U
#define D8              8U
#define D9              9U

/* On-board resources */
#define LED_RED         10U
#define LED_BLUE        11U
#define LED_GREEN       12U
#define BTN0            13U
#define BTN1            14U

/* CAN */
#define CAN_TX_PIN      15U
#define CAN_RX_PIN      16U

/* SPI / SBC */
#define SPI_SCK_PIN     17U
#define SPI_MISO_PIN    18U
#define SPI_MOSI_PIN    19U
#define SPI_CS_PIN      20U

/* I2C */
#define I2C_SCL_PIN     21U
#define I2C_SDA_PIN     22U

/* UART */
#define UART1_RX_PIN    23U
#define UART1_TX_PIN    24U
#define UART2_RX_PIN    25U
#define UART2_TX_PIN    26U

/* Analog */
#define A0              27U
#define A1              28U

#define NUM_LOGICAL_PINS    29U

/* Arduino-style alias */
#define LED_BUILTIN     LED_BLUE

/* =========================================
 * Pin Function Type
 * ========================================= */
typedef enum
{
    PIN_FUNC_DIGITAL = 0U,
    PIN_FUNC_LED,
    PIN_FUNC_BUTTON,
    PIN_FUNC_CAN,
    PIN_FUNC_SPI,
    PIN_FUNC_I2C,
    PIN_FUNC_UART,
    PIN_FUNC_ANALOG
} PinFunction_t;

/* =========================================
 * Arduino Pin Map Type
 * ========================================= */
typedef struct
{
    PORT_Type      *portBase;
    GPIO_Type      *gpioBase;
    uint8_t         pinNumber;
    PinFunction_t   function;
} ArduinoPinMap_t;

/* =========================================
 * Global Pin Map Table
 * ========================================= */
extern const ArduinoPinMap_t g_arduinoPinMap[NUM_LOGICAL_PINS];

/* =========================================
 * Validation Macros
 * ========================================= */
#define ARDUINO_IS_VALID_PIN(pin)    ((uint8_t)(pin) < NUM_LOGICAL_PINS)

/* =========================================
 * Helper Functions
 * ========================================= */
uint8_t Arduino_IsValidPin(uint8_t pin);
uint8_t Arduino_IsDigitalPin(uint8_t pin);
uint8_t Arduino_IsAnalogPin(uint8_t pin);

#endif /* ARDUINO_PINS_H */
