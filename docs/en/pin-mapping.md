# Pin Mapping

## Purpose

This document provides a list of logical pin names used in EduFramework.

When programming, you do not need to worry about the physical pins of the microcontroller.
Instead, you use the predefined names provided by the framework.

---

## Usage

EduFramework maps board pins into easy-to-understand and user-friendly names.

Examples:

* To control the red LED → use `LED_RED`
* To read buttons → use `BTN0`, `BTN1`
* To use external pins → use `GPIOx`

---

## Pin List

### GPIO Header

These are extended GPIO pins used to connect external devices such as LEDs, buttons, and sensors.

| Pin Name | Description     |
| -------- | --------------- |
| `GPIO0`  | External GPIO 0 |
| `GPIO1`  | External GPIO 1 |
| `GPIO2`  | External GPIO 2 |
| `GPIO3`  | External GPIO 3 |
| `GPIO4`  | External GPIO 4 |
| `GPIO5`  | External GPIO 5 |
| `GPIO6`  | External GPIO 6 |
| `GPIO7`  | External GPIO 7 |
| `GPIO8`  | External GPIO 8 |
| `GPIO9`  | External GPIO 9 |

---

### On-board LEDs

LEDs available on the board.

| Pin Name    | Description |
| ----------- | ----------- |
| `LED_RED`   | Red LED     |
| `LED_BLUE`  | Blue LED    |
| `LED_GREEN` | Green LED   |

Note:

The LEDs are **active LOW**:

* `LOW` → LED ON
* `HIGH` → LED OFF

---

### Buttons

Buttons available on the board.

| Pin Name | Description |
| -------- | ----------- |
| `BTN0`   | Button 0    |
| `BTN1`   | Button 1    |

---

### SPI

Pins used for SPI communication.

| Pin Name   | Description |
| ---------- | ----------- |
| `SPI_SCK`  | Clock       |
| `SPI_SIN`  | Data input  |
| `SPI_SOUT` | Data output |
| `SPI_PCS0` | Chip select |

---

### I2C

Pins used for I2C communication.

| Pin Name  | Description |
| --------- | ----------- |
| `I2C_SCL` | Clock       |
| `I2C_SDA` | Data        |

---

### ADC

Pins used for analog signal reading.

| Pin Name    | Description    |
| ----------- | -------------- |
| `ADC0_SE12` | ADC Channel 12 |
| `ADC0_SE13` | ADC Channel 13 |

---

## Notes

* These pin names are defined in `Arduino_pins.h`
* Users only need to use these names when calling APIs
* All hardware configurations are handled internally by the framework

##