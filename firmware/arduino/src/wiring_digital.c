#include "wiring_digital.h"
#include "arduino_pins.h"
#include "arduino_defs.h"

/* Driver layer */
#include "port.h"
#include "gpio.h"

static void Arduino_EnablePortClock(PORT_Type *base)
{
    if (base == IP_PORTA)
    {
        PORT_EnableClock(PORT_NAME_A);
    }
    else if (base == IP_PORTB)
    {
        PORT_EnableClock(PORT_NAME_B);
    }
    else if (base == IP_PORTC)
    {
        PORT_EnableClock(PORT_NAME_C);
    }
    else if (base == IP_PORTD)
    {
        PORT_EnableClock(PORT_NAME_D);
    }
    else if (base == IP_PORTE)
    {
        PORT_EnableClock(PORT_NAME_E);
    }
    else
    {
        /* Invalid port base: do nothing */
    }
}

void pinMode(uint8_t pin, uint8_t mode)
{
    const ArduinoPinMap_t *p;

    if (!Arduino_IsValidPin(pin))
    {
        return;
    }

    p = &g_arduinoPinMap[pin];

    /* Enable PORT clock before PORT configuration */
    Arduino_EnablePortClock(p->portBase);

    /* Set pin mux to GPIO */
    PORT_SetPinMux(p->portBase, p->pinNumber, PORT_MUX_GPIO);

    /* Default: disable pull resistor */
    PORT_SetPinPull(p->portBase, p->pinNumber, PORT_PULL_DISABLED);

    /* Optional: default disable passive filter */
    PORT_SetPassiveFilter(p->portBase, p->pinNumber, false);

    switch (mode)
    {
        case OUTPUT:
            GPIO_SetPinDirection(p->gpioBase, p->pinNumber, GPIO_DIRECTION_OUTPUT);
            break;

        case INPUT_PULLUP:
            GPIO_SetPinDirection(p->gpioBase, p->pinNumber, GPIO_DIRECTION_INPUT);
            PORT_SetPinPull(p->portBase, p->pinNumber, PORT_PULL_UP);
            break;

        case INPUT_PULLDOWN:
            GPIO_SetPinDirection(p->gpioBase, p->pinNumber, GPIO_DIRECTION_INPUT);
            PORT_SetPinPull(p->portBase, p->pinNumber, PORT_PULL_DOWN);
            break;

        case INPUT:
        default:
            GPIO_SetPinDirection(p->gpioBase, p->pinNumber, GPIO_DIRECTION_INPUT);
            break;
    }

    /* Auto-enable passive filter for on-board buttons */
    /* Auto-enable passive filter for on-board buttons */
    if (p->role == PIN_ROLE_BUTTON)
    {
        PORT_SetPassiveFilter(p->portBase, p->pinNumber, true);
    }
}

void digitalWrite(uint8_t pin, uint8_t value)
{
    const ArduinoPinMap_t *p;

    if (!Arduino_IsValidPin(pin))
    {
        return;
    }

    p = &g_arduinoPinMap[pin];

    GPIO_WritePin(p->gpioBase, p->pinNumber, (value == HIGH) ? true : false);
}

bool digitalRead(uint8_t pin)
{
    const ArduinoPinMap_t *p;

    if (!Arduino_IsValidPin(pin))
    {
        return false;
    }

    p = &g_arduinoPinMap[pin];

    return GPIO_ReadPin(p->gpioBase, p->pinNumber);
}

void digitalToggle(uint8_t pin)
{
    const ArduinoPinMap_t *p;

    if (!Arduino_IsValidPin(pin))
    {
        return;
    }

    p = &g_arduinoPinMap[pin];

    GPIO_TogglePin(p->gpioBase, p->pinNumber);
}
