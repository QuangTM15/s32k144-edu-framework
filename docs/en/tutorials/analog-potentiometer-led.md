# Analog Potentiometer Control

## Objective

In this tutorial, you will learn how to:

* Read values from a potentiometer using ADC
* Convert ADC values to PWM
* Control LED brightness using analog signals
* Understand the relationship between analog input and output

---

## Introduction

A potentiometer is a common analog device.

When you rotate the potentiometer:

* The voltage changes continuously
* The ADC reads the corresponding value

You can then use this value to control an LED:

> Rotate the potentiometer → LED brightness changes accordingly

---

## Concepts

### ADC (Analog to Digital)

* Resolution: 12-bit
* Range: `0` to `4095`

---

### PWM (Simulated Analog Output)

* Resolution: 8-bit
* Range: `0` to `255`

PWM does not generate true analog voltage.
It changes the duty cycle to simulate analog behavior.

---

### Value Scaling

You need to scale ADC values to PWM:

```c
pwmValue = (adcValue * 255) / 4095;
```

---

## How it works

```text
Potentiometer → ADC → scale → PWM → LED
```

---

## Code

```c
#include "Arduino.h"
#include "analog_full_demo.h"

void Demo_AnalogFull(void)
{
    int adcValue;
    uint8_t pwmValue;

    setup();

    while (1)
    {
        adcValue = analogRead(ADC0_SE12);

        if (adcValue < 0)
        {
            continue;
        }

        /* Convert ADC 12-bit (0..4095) to PWM 8-bit (0..255) */
        pwmValue = (uint8_t)(((uint32_t)adcValue * 255U) / 4095U);

        /* LED brightness follows potentiometer */
        analogWrite(LED_RED, pwmValue);
    }
}
```

---

## Explanation

### Reading ADC

```c
adcValue = analogRead(ADC0_SE12);
```

* Reads value from the potentiometer
* Range is from 0 to 4095

---

### Scaling the value

```c
pwmValue = (adcValue * 255) / 4095;
```

* Converts 12-bit → 8-bit
* Matches PWM resolution

---

### Controlling the LED

```c
analogWrite(LED_RED, pwmValue);
```

* PWM value controls LED brightness
* Higher value → brighter LED

---

## Result

When running the program:

* Rotate the potentiometer in one direction → LED becomes brighter
* Rotate in the opposite direction → LED becomes dimmer

---

## Notes

* On-board LEDs are **active LOW**
* ADC readings may have small noise
* No need to use `delay()` in this example
* Higher ADC value → higher PWM value

---

## Further Ideas

You can try:

* Printing ADC values via Serial
* Controlling multiple LEDs
* Using non-blocking ADC
* Adding smoothing (filtering)

---

## Related

See also:

```text
docs/en/apis/analog.md
docs/en/apis/time.md

Or check other examples in the tests/demo folder
```