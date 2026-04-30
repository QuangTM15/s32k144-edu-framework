# Analog API

## Overview

Analog API is used to work with analog signals in EduFramework.

You can use it to:

* Read analog signals from sensors
* Read ADC values in numeric form
* Read voltage values in millivolts
* Perform ADC in blocking or non-blocking mode
* Output PWM signals to control LED brightness or external modules

Analog API consists of two main groups:

| Group | Purpose                              |
| ----- | ------------------------------------ |
| ADC   | Read analog input signals            |
| PWM   | Output analog-like signals using PWM |

---

## Important Note

`analogWrite()` in EduFramework does not generate true analog voltage.

It generates a PWM signal.

PWM changes the duty cycle to simulate analog behavior, for example:

* Adjusting LED brightness
* Controlling small motor speed
* Driving simple modules like buzzers

---

## Function List

| Function                 | Description                           |
| ------------------------ | ------------------------------------- |
| `analogRead()`           | Blocking analog read                  |
| `analogStart()`          | Start non-blocking analog conversion  |
| `analogAvailable()`      | Check if non-blocking result is ready |
| `analogGetResult()`      | Get latest non-blocking result        |
| `analogReadMilliVolts()` | Read analog value in millivolts       |
| `analogWrite()`          | Output PWM                            |

---

## Blocking vs Non-blocking

Analog API supports two ADC reading methods:

| Mode         | Characteristics                           | When to use                         |
| ------------ | ----------------------------------------- | ----------------------------------- |
| Blocking     | Waits until conversion completes          | Easy to use, suitable for beginners |
| Non-blocking | Starts conversion and checks result later | Suitable for multitasking programs  |

---

## analogRead()

### Purpose

Reads analog value from a pin in blocking mode.

The program will wait until the ADC conversion is complete.

### Syntax

```c
int value = analogRead(pin);
```

### Parameters

| Parameter | Description        |
| --------- | ------------------ |
| `pin`     | Analog pin to read |

### Supported Pins

Currently, `analogRead()` supports:

| Pin         | Description    |
| ----------- | -------------- |
| `ADC0_SE12` | ADC channel 12 |
| `ADC0_SE13` | ADC channel 13 |

### Return Value

| Value | Description |
| ----- | ----------- |
| `int` | ADC value   |

### Example

```c
int value = analogRead(ADC0_SE12);
```

### Notes

* Easiest method for beginners
* Blocking: CPU waits during conversion
* Suitable for simple sensor readings
* Only use supported ADC pins

---

## analogReadMilliVolts()

### Purpose

Reads analog value and converts it to millivolts.

This makes it easier to understand the voltage level.

### Syntax

```c
int mv = analogReadMilliVolts(pin);
```

### Parameters

| Parameter | Description |
| --------- | ----------- |
| `pin`     | Analog pin  |

### Supported Pins

| Pin         | Description    |
| ----------- | -------------- |
| `ADC0_SE12` | ADC channel 12 |
| `ADC0_SE13` | ADC channel 13 |

### Return Value

| Value | Description           |
| ----- | --------------------- |
| `int` | Voltage in millivolts |

### Example

```c
int voltage = analogReadMilliVolts(ADC0_SE12);
```

### Notes

* Unit is millivolts (mV)
* Useful for displaying voltage via Serial
* Depends on ADC reference voltage
* Only use supported ADC pins

---

## analogStart()

### Purpose

Starts a non-blocking ADC conversion.

It begins the conversion but does not wait for the result.

### Syntax

```c
analogStart(pin);
```

### Parameters

| Parameter | Description |
| --------- | ----------- |
| `pin`     | Analog pin  |

### Supported Pins

| Pin         | Description    |
| ----------- | -------------- |
| `ADC0_SE12` | ADC channel 12 |
| `ADC0_SE13` | ADC channel 13 |

### Example

```c
analogStart(ADC0_SE12);
```

### Notes

* Use `analogAvailable()` to check when result is ready
* Suitable for multitasking applications
* More advanced than `analogRead()`
* Only use supported ADC pins

---

## analogAvailable()

### Purpose

Checks whether the non-blocking ADC result is ready.

### Syntax

```c
uint8_t ready = analogAvailable();
```

### Return Value

| Value | Description     |
| ----- | --------------- |
| `1`   | Result is ready |
| `0`   | Not ready       |

### Example

```c
if (analogAvailable())
{
    int value = analogGetResult();
}
```

### Notes

* Must be used after `analogStart()`
* Do not call `analogGetResult()` too early

---

## analogGetResult()

### Purpose

Retrieves the latest ADC result after non-blocking conversion.

### Syntax

```c
int value = analogGetResult();
```

### Return Value

| Value | Description      |
| ----- | ---------------- |
| `int` | Latest ADC value |

### Example

```c
analogStart(ADC0_SE12);

while (!analogAvailable())
{
    // do other tasks
}

int value = analogGetResult();
```

### Notes

* Call after `analogAvailable()` returns 1
* Calling too early may return invalid data

---

## analogWrite()

### Purpose

Outputs PWM signal on a supported pin.

Common uses:

* Adjust LED brightness
* Control power level
* Create fade effects

### Syntax

```c
analogWrite(pin, value);
```

### Parameters

| Parameter | Description           |
| --------- | --------------------- |
| `pin`     | PWM-capable pin       |
| `value`   | Duty cycle (0 to 255) |

### Supported Pins

Currently, `analogWrite()` supports:

| Pin         | Description   |
| ----------- | ------------- |
| `GPIO2`     | PWM supported |
| `GPIO3`     | PWM supported |
| `GPIO4`     | PWM supported |
| `GPIO5`     | PWM supported |
| `GPIO6`     | PWM supported |
| `GPIO7`     | PWM supported |
| `GPIO8`     | PWM supported |
| `LED_RED`   | PWM supported |
| `LED_BLUE`  | PWM supported |
| `LED_GREEN` | PWM supported |

### Value Meaning

| Value | Meaning         |
| ----- | --------------- |
| `0`   | 0% duty cycle   |
| `127` | ~50% duty cycle |
| `255` | 100% duty cycle |

### Example

```c
analogWrite(LED_BLUE, 128);
```

### Notes

* Generates PWM, not real analog voltage
* Only works on supported pins
* Higher value → higher duty cycle
* For active-low LEDs, brightness behavior may appear inverted

---

## API Usage Guide

| Requirement            | Recommended API                                             |
| ---------------------- | ----------------------------------------------------------- |
| Simple sensor reading  | `analogRead()`                                              |
| Read voltage directly  | `analogReadMilliVolts()`                                    |
| Non-blocking ADC       | `analogStart()` + `analogAvailable()` + `analogGetResult()` |
| LED brightness control | `analogWrite()`                                             |

---

## Tutorials

You can learn how to use Analog API through tests/demo source code:

* Analog read (blocking)
* Analog read (non-blocking)
* PWM fade LED

##