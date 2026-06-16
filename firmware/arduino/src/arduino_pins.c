/**
 * @file arduino_pins.c
 * @brief Logical pin mapping implementation for EduFramework.
 *
 * @details
 * This file stores the mapping tables used by Arduino-style APIs.
 *
 * The purpose of this module is to separate user-facing Arduino pin names
 * from real S32K144 hardware pin definitions.
 *
 * This design allows application code to use simple logical names:
 *
 * @code
 * pinMode(LED_RED, OUTPUT);
 * digitalWrite(LED_RED, HIGH);
 * @endcode
 *
 * while the framework internally translates them to real hardware resources:
 *
 * @code
 * LED_RED -> PORTD / GPIOD / pin 15
 * @endcode
 *
 * This file must not directly configure registers. It only stores mapping
 * data and provides helper functions for validating pin capability.
 */

#include "arduino_pins.h"

#include <stddef.h>

/* ========================================================================= */
/* Logical Pin Mapping Table                                                  */
/* ========================================================================= */

/**
 * @brief Logical Arduino pin mapping table.
 *
 * @details
 * Each entry maps one logical pin ID to its real hardware resources.
 *
 * The array index must match the logical pin value defined in arduino_pins.h.
 *
 * Example:
 *
 * @code
 * #define LED_RED (10U)
 *
 * g_arduinoPinMap[LED_RED]
 *     -> BOARD_LED_RED_PORT
 *     -> BOARD_LED_RED_GPIO
 *     -> BOARD_LED_RED_PIN
 * @endcode
 */
const ArduinoPinMap_t g_arduinoPinMap[NUM_LOGICAL_PINS] =
{
    /* GPIO0: External GPIO header pin. */
    { BOARD_GPIO0_PORT, BOARD_GPIO0_GPIO, BOARD_GPIO0_PIN,
      PIN_ROLE_GPIO, PIN_CAP_DIGITAL },

    /* GPIO1: External GPIO header pin. */
    { BOARD_GPIO1_PORT, BOARD_GPIO1_GPIO, BOARD_GPIO1_PIN,
      PIN_ROLE_GPIO, PIN_CAP_DIGITAL },

    /* GPIO2: Digital GPIO with PWM capability. */
    { BOARD_GPIO2_PORT, BOARD_GPIO2_GPIO, BOARD_GPIO2_PIN,
      PIN_ROLE_GPIO, (PinCapability_t)(PIN_CAP_DIGITAL | PIN_CAP_PWM) },

    /* GPIO3: Digital GPIO with PWM capability. */
    { BOARD_GPIO3_PORT, BOARD_GPIO3_GPIO, BOARD_GPIO3_PIN,
      PIN_ROLE_GPIO, (PinCapability_t)(PIN_CAP_DIGITAL | PIN_CAP_PWM) },

    /* GPIO4: Digital GPIO with PWM capability. */
    { BOARD_GPIO4_PORT, BOARD_GPIO4_GPIO, BOARD_GPIO4_PIN,
      PIN_ROLE_GPIO, (PinCapability_t)(PIN_CAP_DIGITAL | PIN_CAP_PWM) },

    /* GPIO5: Digital GPIO with PWM capability. */
    { BOARD_GPIO5_PORT, BOARD_GPIO5_GPIO, BOARD_GPIO5_PIN,
      PIN_ROLE_GPIO, (PinCapability_t)(PIN_CAP_DIGITAL | PIN_CAP_PWM) },

    /* GPIO6: Digital GPIO with PWM capability. */
    { BOARD_GPIO6_PORT, BOARD_GPIO6_GPIO, BOARD_GPIO6_PIN,
      PIN_ROLE_GPIO, (PinCapability_t)(PIN_CAP_DIGITAL | PIN_CAP_PWM) },

    /* GPIO7: Digital GPIO with PWM capability. */
    { BOARD_GPIO7_PORT, BOARD_GPIO7_GPIO, BOARD_GPIO7_PIN,
      PIN_ROLE_GPIO, (PinCapability_t)(PIN_CAP_DIGITAL | PIN_CAP_PWM) },

    /* GPIO8: Digital GPIO with PWM capability. */
    { BOARD_GPIO8_PORT, BOARD_GPIO8_GPIO, BOARD_GPIO8_PIN,
      PIN_ROLE_GPIO, (PinCapability_t)(PIN_CAP_DIGITAL | PIN_CAP_PWM) },

    /* GPIO9: External GPIO header pin. */
    { BOARD_GPIO9_PORT, BOARD_GPIO9_GPIO, BOARD_GPIO9_PIN,
      PIN_ROLE_GPIO, PIN_CAP_DIGITAL },

    /* LED_RED: On-board red LED. */
    { BOARD_LED_RED_PORT, BOARD_LED_RED_GPIO, BOARD_LED_RED_PIN,
      PIN_ROLE_LED, (PinCapability_t)(PIN_CAP_DIGITAL | PIN_CAP_PWM) },

    /* LED_BLUE: On-board blue LED and default LED_BUILTIN. */
    { BOARD_LED_BLUE_PORT, BOARD_LED_BLUE_GPIO, BOARD_LED_BLUE_PIN,
      PIN_ROLE_LED, (PinCapability_t)(PIN_CAP_DIGITAL | PIN_CAP_PWM) },

    /* LED_GREEN: On-board green LED. */
    { BOARD_LED_GREEN_PORT, BOARD_LED_GREEN_GPIO, BOARD_LED_GREEN_PIN,
      PIN_ROLE_LED, (PinCapability_t)(PIN_CAP_DIGITAL | PIN_CAP_PWM) },

    /* BTN0: On-board mechanical button. */
    { BOARD_BUTTON0_PORT, BOARD_BUTTON0_GPIO, BOARD_BUTTON0_PIN,
      PIN_ROLE_BUTTON, PIN_CAP_DIGITAL },

    /* BTN1: On-board mechanical button. */
    { BOARD_BUTTON1_PORT, BOARD_BUTTON1_GPIO, BOARD_BUTTON1_PIN,
      PIN_ROLE_BUTTON, PIN_CAP_DIGITAL },

    /* SPI_SCK: Reserved for SPI clock function. */
    { BOARD_SBC_SCK_PORT, BOARD_SBC_SCK_GPIO, BOARD_SBC_SCK_PIN,
      PIN_ROLE_SPI, PIN_CAP_NONE },

    /* SPI_SIN: Reserved for SPI input function. */
    { BOARD_SBC_MISO_PORT, BOARD_SBC_MISO_GPIO, BOARD_SBC_MISO_PIN,
      PIN_ROLE_SPI, PIN_CAP_NONE },

    /* SPI_SOUT: Reserved for SPI output function. */
    { BOARD_SBC_MOSI_PORT, BOARD_SBC_MOSI_GPIO, BOARD_SBC_MOSI_PIN,
      PIN_ROLE_SPI, PIN_CAP_NONE },

    /* SPI_PCS0: Reserved for SPI chip-select function. */
    { BOARD_SBC_CS_PORT, BOARD_SBC_CS_GPIO, BOARD_SBC_CS_PIN,
      PIN_ROLE_SPI, PIN_CAP_NONE },

    /* I2C_SCL: Reserved for I2C clock function. */
    { BOARD_I2C0_SCL_PORT, BOARD_I2C0_SCL_GPIO, BOARD_I2C0_SCL_PIN,
      PIN_ROLE_I2C, PIN_CAP_NONE },

    /* I2C_SDA: Reserved for I2C data function. */
    { BOARD_I2C0_SDA_PORT, BOARD_I2C0_SDA_GPIO, BOARD_I2C0_SDA_PIN,
      PIN_ROLE_I2C, PIN_CAP_NONE },

    /* ADC0_SE12: Analog input channel. */
    { BOARD_ADC0_SE12_PORT, BOARD_ADC0_SE12_GPIO, BOARD_ADC0_SE12_PIN,
      PIN_ROLE_ADC, PIN_CAP_ANALOG_IN },

    /* ADC0_SE13: Analog input channel. */
    { BOARD_ADC0_SE13_PORT, BOARD_ADC0_SE13_GPIO, BOARD_ADC0_SE13_PIN,
      PIN_ROLE_ADC, PIN_CAP_ANALOG_IN }
};

/* ========================================================================= */
/* PWM Mapping Table                                                          */
/* ========================================================================= */

/**
 * @brief Logical Arduino pin to PWM hardware mapping table.
 *
 * @details
 * This table is used by analogWrite().
 *
 * Only pins with PIN_CAP_PWM should be used for PWM. Non-PWM pins keep a
 * placeholder entry to preserve direct indexing by logical pin number.
 */
const ArduinoPwmMap_t g_arduinoPwmMap[NUM_LOGICAL_PINS] =
{
    /* GPIO0: No PWM capability. Placeholder entry. */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_GPIO },

    /* GPIO1: No PWM capability. Placeholder entry. */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_GPIO },

    /* GPIO2: PTD14 -> FTM2_CH5 -> ALT2. */
    { IP_FTM_2, FTM_CHANNEL_5, PORT_MUX_ALT2 },

    /* GPIO3: PTD13 -> FTM2_CH4 -> ALT2. */
    { IP_FTM_2, FTM_CHANNEL_4, PORT_MUX_ALT2 },

    /* GPIO4: PTD12 -> FTM2_CH2 -> ALT2. */
    { IP_FTM_2, FTM_CHANNEL_2, PORT_MUX_ALT2 },

    /* GPIO5: PTD11 -> FTM2_CH1 -> ALT2. */
    { IP_FTM_2, FTM_CHANNEL_1, PORT_MUX_ALT2 },

    /* GPIO6: PTD10 -> FTM2_CH0 -> ALT2. */
    { IP_FTM_2, FTM_CHANNEL_0, PORT_MUX_ALT2 },

    /* GPIO7: PTD9 -> FTM1_CH5 -> ALT6. */
    { IP_FTM_1, FTM_CHANNEL_5, PORT_MUX_ALT6 },

    /* GPIO8: PTD8 -> FTM1_CH4 -> ALT6. */
    { IP_FTM_1, FTM_CHANNEL_4, PORT_MUX_ALT6 },

    /* GPIO9: No PWM capability. Placeholder entry. */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_GPIO },

    /* LED_RED: PTD15 -> FTM0_CH0 -> ALT2. */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_ALT2 },

    /* LED_BLUE: PTD16 -> FTM0_CH1 -> ALT2. */
    { IP_FTM_0, FTM_CHANNEL_1, PORT_MUX_ALT2 },

    /* LED_GREEN: PTD0 -> FTM0_CH2 -> ALT2. */
    { IP_FTM_0, FTM_CHANNEL_2, PORT_MUX_ALT2 },

    /* BTN0: No PWM capability. Placeholder entry. */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_GPIO },

    /* BTN1: No PWM capability. Placeholder entry. */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_GPIO },

    /* SPI_SCK: No PWM capability. Placeholder entry. */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_GPIO },

    /* SPI_SIN: No PWM capability. Placeholder entry. */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_GPIO },

    /* SPI_SOUT: No PWM capability. Placeholder entry. */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_GPIO },

    /* SPI_PCS0: No PWM capability. Placeholder entry. */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_GPIO },

    /* I2C_SCL: No PWM capability. Placeholder entry. */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_GPIO },

    /* I2C_SDA: No PWM capability. Placeholder entry. */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_GPIO },

    /* ADC0_SE12: No PWM capability. Placeholder entry. */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_GPIO },

    /* ADC0_SE13: No PWM capability. Placeholder entry. */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_GPIO }
};

/* ========================================================================= */
/* Public Functions                                                           */
/* ========================================================================= */

uint8_t Arduino_IsValidPin(uint8_t u8Pin)
{
    uint8_t u8RetVal = ARDUINO_VALID_FALSE;

    /*
     * Logical pin numbers are used as direct indexes into g_arduinoPinMap[].
     * Therefore, the pin must be smaller than NUM_LOGICAL_PINS.
     */
    if (ARDUINO_IS_VALID_PIN(u8Pin))
    {
        u8RetVal = ARDUINO_VALID_TRUE;
    }
    else
    {
        u8RetVal = ARDUINO_VALID_FALSE;
    }

    return u8RetVal;
}

uint8_t Arduino_HasDigitalCapability(uint8_t u8Pin)
{
    uint8_t u8RetVal = ARDUINO_VALID_FALSE;

    /*
     * Capability flags are valid only if the logical pin index is valid.
     * This prevents out-of-range access to g_arduinoPinMap[].
     */
    if (ARDUINO_VALID_TRUE == Arduino_IsValidPin(u8Pin))
    {
        if (0U != (g_arduinoPinMap[u8Pin].capability & PIN_CAP_DIGITAL))
        {
            u8RetVal = ARDUINO_VALID_TRUE;
        }
        else
        {
            u8RetVal = ARDUINO_VALID_FALSE;
        }
    }
    else
    {
        u8RetVal = ARDUINO_VALID_FALSE;
    }

    return u8RetVal;
}

uint8_t Arduino_HasAnalogInputCapability(uint8_t u8Pin)
{
    uint8_t u8RetVal = ARDUINO_VALID_FALSE;

    /*
     * Analog capability is used by analogRead() and related ADC helpers.
     */
    if (ARDUINO_VALID_TRUE == Arduino_IsValidPin(u8Pin))
    {
        if (0U != (g_arduinoPinMap[u8Pin].capability & PIN_CAP_ANALOG_IN))
        {
            u8RetVal = ARDUINO_VALID_TRUE;
        }
        else
        {
            u8RetVal = ARDUINO_VALID_FALSE;
        }
    }
    else
    {
        u8RetVal = ARDUINO_VALID_FALSE;
    }

    return u8RetVal;
}

uint8_t Arduino_HasPwmCapability(uint8_t u8Pin)
{
    uint8_t u8RetVal = ARDUINO_VALID_FALSE;

    /*
     * PWM capability is checked before reading g_arduinoPwmMap[].
     * Non-PWM pins still have placeholder PWM entries, but those entries
     * must not be used by analogWrite().
     */
    if (ARDUINO_VALID_TRUE == Arduino_IsValidPin(u8Pin))
    {
        if (0U != (g_arduinoPinMap[u8Pin].capability & PIN_CAP_PWM))
        {
            u8RetVal = ARDUINO_VALID_TRUE;
        }
        else
        {
            u8RetVal = ARDUINO_VALID_FALSE;
        }
    }
    else
    {
        u8RetVal = ARDUINO_VALID_FALSE;
    }

    return u8RetVal;
}

uint8_t Arduino_GetPwmMap(uint8_t u8Pin,
                          ArduinoPwmMap_t *pPwmMap)
{
    uint8_t u8RetVal = ARDUINO_VALID_FALSE;

    /*
     * The output pointer must be valid because the function copies the PWM
     * mapping entry into the caller-provided structure.
     */
    if ((ARDUINO_VALID_TRUE == Arduino_IsValidPin(u8Pin)) &&
        (NULL != pPwmMap))
    {
        /*
         * Only pins explicitly marked with PIN_CAP_PWM are allowed to expose
         * PWM mapping information to analogWrite().
         */
        if (ARDUINO_VALID_TRUE == Arduino_HasPwmCapability(u8Pin))
        {
            *pPwmMap = g_arduinoPwmMap[u8Pin];
            u8RetVal = ARDUINO_VALID_TRUE;
        }
        else
        {
            u8RetVal = ARDUINO_VALID_FALSE;
        }
    }
    else
    {
        u8RetVal = ARDUINO_VALID_FALSE;
    }

    return u8RetVal;
}
