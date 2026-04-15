#include "arduino_pins.h"

const ArduinoPinMap_t g_arduinoPinMap[NUM_LOGICAL_PINS] =
{
    /* 0  - GPIO0 */
    { BOARD_GPIO0_PORT, BOARD_GPIO0_GPIO, BOARD_GPIO0_PIN,
      PIN_ROLE_GPIO, PIN_CAP_DIGITAL },

    /* 1  - GPIO1 */
    { BOARD_GPIO1_PORT, BOARD_GPIO1_GPIO, BOARD_GPIO1_PIN,
      PIN_ROLE_GPIO, PIN_CAP_DIGITAL },

    /* 2  - GPIO2 */
    { BOARD_GPIO2_PORT, BOARD_GPIO2_GPIO, BOARD_GPIO2_PIN,
      PIN_ROLE_GPIO, (PIN_CAP_DIGITAL | PIN_CAP_PWM) },

    /* 3  - GPIO3 */
    { BOARD_GPIO3_PORT, BOARD_GPIO3_GPIO, BOARD_GPIO3_PIN,
      PIN_ROLE_GPIO, (PIN_CAP_DIGITAL | PIN_CAP_PWM) },

    /* 4  - GPIO4 */
    { BOARD_GPIO4_PORT, BOARD_GPIO4_GPIO, BOARD_GPIO4_PIN,
      PIN_ROLE_GPIO, (PIN_CAP_DIGITAL | PIN_CAP_PWM) },

    /* 5  - GPIO5 */
    { BOARD_GPIO5_PORT, BOARD_GPIO5_GPIO, BOARD_GPIO5_PIN,
      PIN_ROLE_GPIO, (PIN_CAP_DIGITAL | PIN_CAP_PWM) },

    /* 6  - GPIO6 */
    { BOARD_GPIO6_PORT, BOARD_GPIO6_GPIO, BOARD_GPIO6_PIN,
      PIN_ROLE_GPIO, (PIN_CAP_DIGITAL | PIN_CAP_PWM) },

    /* 7  - GPIO7 */
    { BOARD_GPIO7_PORT, BOARD_GPIO7_GPIO, BOARD_GPIO7_PIN,
      PIN_ROLE_GPIO, (PIN_CAP_DIGITAL | PIN_CAP_PWM) },

    /* 8  - GPIO8 */
    { BOARD_GPIO8_PORT, BOARD_GPIO8_GPIO, BOARD_GPIO8_PIN,
      PIN_ROLE_GPIO, (PIN_CAP_DIGITAL | PIN_CAP_PWM) },

    /* 9  - GPIO9 */
    { BOARD_GPIO9_PORT, BOARD_GPIO9_GPIO, BOARD_GPIO9_PIN,
      PIN_ROLE_GPIO, PIN_CAP_DIGITAL },

    /* 10 - LED_RED */
    { BOARD_LED_RED_PORT, BOARD_LED_RED_GPIO, BOARD_LED_RED_PIN,
      PIN_ROLE_LED, (PIN_CAP_DIGITAL | PIN_CAP_PWM) },

    /* 11 - LED_BLUE */
    { BOARD_LED_BLUE_PORT, BOARD_LED_BLUE_GPIO, BOARD_LED_BLUE_PIN,
      PIN_ROLE_LED, (PIN_CAP_DIGITAL | PIN_CAP_PWM) },

    /* 12 - LED_GREEN */
    { BOARD_LED_GREEN_PORT, BOARD_LED_GREEN_GPIO, BOARD_LED_GREEN_PIN,
      PIN_ROLE_LED, (PIN_CAP_DIGITAL | PIN_CAP_PWM) },

    /* 13 - BTN0 */
    { BOARD_BUTTON0_PORT, BOARD_BUTTON0_GPIO, BOARD_BUTTON0_PIN,
      PIN_ROLE_BUTTON, PIN_CAP_DIGITAL },

    /* 14 - BTN1 */
    { BOARD_BUTTON1_PORT, BOARD_BUTTON1_GPIO, BOARD_BUTTON1_PIN,
      PIN_ROLE_BUTTON, PIN_CAP_DIGITAL },

    /* 15 - SPI_SCK */
    { BOARD_SBC_SCK_PORT, BOARD_SBC_SCK_GPIO, BOARD_SBC_SCK_PIN,
      PIN_ROLE_SPI, PIN_CAP_NONE },

    /* 16 - SPI_SIN */
    { BOARD_SBC_MISO_PORT, BOARD_SBC_MISO_GPIO, BOARD_SBC_MISO_PIN,
      PIN_ROLE_SPI, PIN_CAP_NONE },

    /* 17 - SPI_SOUT */
    { BOARD_SBC_MOSI_PORT, BOARD_SBC_MOSI_GPIO, BOARD_SBC_MOSI_PIN,
      PIN_ROLE_SPI, PIN_CAP_NONE },

    /* 18 - SPI_PCS0 */
    { BOARD_SBC_CS_PORT, BOARD_SBC_CS_GPIO, BOARD_SBC_CS_PIN,
      PIN_ROLE_SPI, PIN_CAP_NONE },

    /* 19 - I2C_SCL */
    { BOARD_I2C0_SCL_PORT, BOARD_I2C0_SCL_GPIO, BOARD_I2C0_SCL_PIN,
      PIN_ROLE_I2C, PIN_CAP_NONE },

    /* 20 - I2C_SDA */
    { BOARD_I2C0_SDA_PORT, BOARD_I2C0_SDA_GPIO, BOARD_I2C0_SDA_PIN,
      PIN_ROLE_I2C, PIN_CAP_NONE },

    /* 21 - ADC0_SE12 */
    { BOARD_ADC0_SE12_PORT, BOARD_ADC0_SE12_GPIO, BOARD_ADC0_SE12_PIN,
      PIN_ROLE_ADC, PIN_CAP_ANALOG_IN },

    /* 22 - ADC0_SE13 */
    { BOARD_ADC0_SE13_PORT, BOARD_ADC0_SE13_GPIO, BOARD_ADC0_SE13_PIN,
      PIN_ROLE_ADC, PIN_CAP_ANALOG_IN }
};

const ArduinoPwmMap_t g_arduinoPwmMap[NUM_LOGICAL_PINS] =
{
    /* 0  - GPIO0 */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_GPIO },

    /* 1  - GPIO1 */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_GPIO },

    /* 2  - GPIO2 : PTD14 -> FTM2_CH5 -> ALT2 */
    { IP_FTM_2, FTM_CHANNEL_5, PORT_MUX_ALT2 },

    /* 3  - GPIO3 : PTD13 -> FTM2_CH4 -> ALT2 */
    { IP_FTM_2, FTM_CHANNEL_4, PORT_MUX_ALT2 },

    /* 4  - GPIO4 : PTD12 -> FTM2_CH2 -> ALT2 */
    { IP_FTM_2, FTM_CHANNEL_2, PORT_MUX_ALT2 },

    /* 5  - GPIO5 : PTD11 -> FTM2_CH1 -> ALT2 */
    { IP_FTM_2, FTM_CHANNEL_1, PORT_MUX_ALT2 },

    /* 6  - GPIO6 : PTD10 -> FTM2_CH0 -> ALT2 */
    { IP_FTM_2, FTM_CHANNEL_0, PORT_MUX_ALT2 },

    /* 7  - GPIO7 : PTD9 -> FTM1_CH5 -> ALT6 */
    { IP_FTM_1, FTM_CHANNEL_5, PORT_MUX_ALT6 },

    /* 8  - GPIO8 : PTD8 -> FTM1_CH4 -> ALT6 */
    { IP_FTM_1, FTM_CHANNEL_4, PORT_MUX_ALT6 },

    /* 9  - GPIO9 */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_GPIO },

    /* 10 - LED_RED : PTD15 -> FTM0_CH0 -> ALT2 */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_ALT2 },

    /* 11 - LED_BLUE : PTD16 -> FTM0_CH1 -> ALT2 */
    { IP_FTM_0, FTM_CHANNEL_1, PORT_MUX_ALT2 },

    /* 12 - LED_GREEN : PTD0 -> FTM0_CH2 -> ALT2 */
    { IP_FTM_0, FTM_CHANNEL_2, PORT_MUX_ALT2 },

    /* 13 - BTN0 */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_GPIO },

    /* 14 - BTN1 */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_GPIO },

    /* 15 - SPI_SCK */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_GPIO },

    /* 16 - SPI_SIN */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_GPIO },

    /* 17 - SPI_SOUT */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_GPIO },

    /* 18 - SPI_PCS0 */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_GPIO },

    /* 19 - I2C_SCL */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_GPIO },

    /* 20 - I2C_SDA */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_GPIO },

    /* 21 - ADC0_SE12 */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_GPIO },

    /* 22 - ADC0_SE13 */
    { IP_FTM_0, FTM_CHANNEL_0, PORT_MUX_GPIO }
};

uint8_t Arduino_IsValidPin(uint8_t pin)
{
    return (ARDUINO_IS_VALID_PIN(pin)) ? 1U : 0U;
}

uint8_t Arduino_HasDigitalCapability(uint8_t pin)
{
    if (!Arduino_IsValidPin(pin))
    {
        return 0U;
    }

    return ((g_arduinoPinMap[pin].capability & PIN_CAP_DIGITAL) != 0U) ? 1U : 0U;
}

uint8_t Arduino_HasAnalogInputCapability(uint8_t pin)
{
    if (!Arduino_IsValidPin(pin))
    {
        return 0U;
    }

    return ((g_arduinoPinMap[pin].capability & PIN_CAP_ANALOG_IN) != 0U) ? 1U : 0U;
}

uint8_t Arduino_HasPwmCapability(uint8_t pin)
{
    if (!Arduino_IsValidPin(pin))
    {
        return 0U;
    }

    return ((g_arduinoPinMap[pin].capability & PIN_CAP_PWM) != 0U) ? 1U : 0U;
}

uint8_t Arduino_GetPwmMap(uint8_t pin, ArduinoPwmMap_t *pwmMap)
{
    if ((!Arduino_IsValidPin(pin)) || (pwmMap == (ArduinoPwmMap_t *)0))
    {
        return 0U;
    }

    if (Arduino_HasPwmCapability(pin) == 0U)
    {
        return 0U;
    }

    *pwmMap = g_arduinoPwmMap[pin];
    return 1U;
}