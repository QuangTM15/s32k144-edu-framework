# Digital API

## Overview

The Digital API is used to control and read digital signals in EduFramework.

You can use it to:

* Control LEDs
* Read buttons
* Work with external GPIO pins
* Send and receive HIGH/LOW signals

---

## Function List

| Function          | Description                 |
| ----------------- | --------------------------- |
| `pinMode()`       | Configure pin mode          |
| `digitalWrite()`  | Write HIGH or LOW to a pin  |
| `digitalRead()`   | Read HIGH or LOW from a pin |
| `digitalToggle()` | Toggle pin state            |

---

## pinMode()

### Purpose

Configures the operating mode of a pin.

### Syntax

```c
pinMode(pin, mode);
```

### Modes

| Mode             | Description          | When to use             |
| ---------------- | -------------------- | ----------------------- |
| `INPUT`          | Input mode           | Read signals            |
| `OUTPUT`         | Output mode          | Control LEDs, modules   |
| `INPUT_PULLUP`   | Input with pull-up   | Button connected to GND |
| `INPUT_PULLDOWN` | Input with pull-down | Button connected to VCC |

### Example

```c
pinMode(LED_RED, OUTPUT);
pinMode(BTN0, INPUT_PULLUP);
```

### Notes

* Always call `pinMode()` before using other functions.
* Without pull-up or pull-down, input signals may float and become unstable.
* For buttons, it is recommended to use `INPUT_PULLUP` or `INPUT_PULLDOWN`.
* The framework automatically enables a basic passive filter for on-board buttons.
* The passive filter does not fully replace software debounce.

---

## digitalWrite()

### Purpose

Writes a logic level to an output pin.

### Syntax

```c
digitalWrite(pin, value);
```

### Values

| Value  | Description |
| ------ | ----------- |
| `HIGH` | Logic high  |
| `LOW`  | Logic low   |

### Example

```c
digitalWrite(LED_RED, LOW);
digitalWrite(GPIO1, HIGH);
```

### Notes

* The pin should be configured as `OUTPUT` before use.
* On-board LEDs are **active LOW**:

  * `LOW` → LED ON
  * `HIGH` → LED OFF
* For external devices, HIGH/LOW behavior depends on the hardware connection.

---

## digitalRead()

### Purpose

Reads the logic state of an input pin.

### Syntax

```c
bool state = digitalRead(pin);
```

### Return Value

| Value   | Description |
| ------- | ----------- |
| `true`  | HIGH        |
| `false` | LOW         |

### Example

```c
bool state = digitalRead(BTN0);
```

### Notes

* The pin should be configured as `INPUT`, `INPUT_PULLUP`, or `INPUT_PULLDOWN`.
* Without a pull resistor, the signal may become unstable.
* With `INPUT_PULLUP`:

  * Not pressed → HIGH
  * Pressed → LOW
* Mechanical buttons may cause bouncing; debounce may be required.

---

## digitalToggle()

### Purpose

Toggles the current state of an output pin.

### Syntax

```c
digitalToggle(pin);
```

### Example

```c
digitalToggle(LED_BLUE);
```

### Notes

* The pin should be configured as `OUTPUT`.
* Useful for blinking LEDs or quickly changing states.
* Works regardless of whether the LED is active LOW or HIGH.

---

## Example

You can learn how to use these digital functions through tutorials such as `btn_led` or `full_led`.

##
