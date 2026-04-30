# EduFramework Architecture

## Overview

EduFramework is an embedded framework designed for **S32K144 (MaaZEDU)**.

Project goals:

* No dependency on vendor SDK
* Register-level driver implementation
* Provide easy-to-use Arduino-style APIs
* Designed for learning, practice, and teaching embedded systems

---

## Design Philosophy

EduFramework is built with a clear layered architecture:

```text
Application / Demo
        ↓
Arduino-style API
        ↓
Driver
        ↓
Register / Hardware
```

Core ideas:

* Beginners can use simple Arduino-like APIs
* Advanced users can go deeper into drivers
* Drivers interact directly with hardware
* Arduino API hides hardware complexity and provides easy-to-use functions

---

## Main Directories

```text
docs/
eduframework/
firmware/
s32ds/
tests/
```

---

## docs/

`docs/` contains all project documentation.

Structure:

```text
docs/
├── vi/
└── en/
```

| Folder | Purpose                  |
| ------ | ------------------------ |
| `vi/`  | Vietnamese documentation |
| `en/`  | English documentation    |

Both versions have the same content, only different languages.

---

## docs/apis/

`apis/` contains documentation for Arduino-style APIs.

Example:

```text
docs/vi/apis/
├── digital.md
├── time.md
├── serial.md
├── analog.md
├── spi.md
└── i2c.md
```

These files are for users who want to **use the framework** without understanding low-level drivers.

---

## docs/tutorials/

`tutorials/` contains hands-on learning guides.

Example:

```text
docs/vi/tutorials/
├── blink_led.md
├── button_control_led.md
├── uart_echo.md
├── analog-potentiometer-led.md
├── spi_communication.md
└── i2c_communication.md
```

Each tutorial includes:

* Learning objectives
* Required knowledge
* Working principle
* Example code
* Step-by-step explanation
* Expected results

---

## eduframework/

`eduframework/` is the actual library used by end users.

```text
eduframework/
├── include/
└── lib/
```

### include/

Contains public header files.

Users include these headers to use the framework APIs.

Example:

```c
#include "Arduino.h"
```

---

### lib/

Contains the prebuilt library:

```text
libeduframework.a
```

This `.a` file can be imported into IDEs or projects so users can use EduFramework without rebuilding the entire source.

---

## firmware/

`firmware/` is the main source code of the framework.

This is where the core logic is implemented.

```text
firmware/
├── arduino/
├── drivers/
└── board/
```

---

## firmware/arduino/

`firmware/arduino/` contains Arduino-style APIs.

Example:

```text
firmware/arduino/
├── inc/
└── src/
```

APIs in this layer include:

* `pinMode()`
* `digitalWrite()`
* `digitalRead()`
* `delay()`
* `millis()`
* `Serial`
* `analogRead()`
* `analogWrite()`
* `SPI_*`
* `Wire_*`

Responsibilities:

* Provide easy-to-use functions
* Hide hardware complexity
* Call driver layer when needed
* Help beginners work with S32K144 easily

Example:

```c
digitalWrite(LED_RED, LOW);
```

Users do not need to know port, pin, or registers.

---

## firmware/drivers/

`firmware/drivers/` contains low-level drivers.

Drivers interact directly with S32K144 registers.

Example:

```text
firmware/drivers/
├── gpio/
├── port/
├── clock/
├── lpit/
├── lpuart/
├── adc/
├── ftm/
├── lpspi/
├── lpi2c/
└── irq/
```

Responsibilities:

* Configure peripherals
* Read/write registers
* Provide low-level APIs for Arduino layer
* No user-friendly logic

Principle:

> Drivers control hardware only.
> Arduino API provides usability.

---

## firmware/board/

`firmware/board/` contains board-specific definitions.

This layer maps human-readable names to real hardware.

Examples:

* `LED_RED`
* `LED_BLUE`
* `LED_GREEN`
* `BTN0`
* `BTN1`
* GPIO pins
* ADC, PWM, SPI, I2C pins

Thanks to this mapping, users can write:

```c
pinMode(LED_RED, OUTPUT);
```

instead of dealing with raw ports and pins.

---

## s32ds/

`s32ds/` is the project used in **S32 Design Studio**.

Used for:

* Testing drivers
* Testing Arduino APIs
* Running demos
* Building projects
* Flashing code to hardware

This is the real project used to validate EduFramework on S32K144 hardware.

---

## tests/

`tests/` contains test programs and demos.

```text
tests/
├── demo/
└── example/
```

---

## tests/demo/

`tests/demo/` contains small demos for each feature.

Examples:

* Blink LED
* Button control LED
* UART echo
* Analog read
* PWM fade LED
* SPI communication
* I2C communication

Used for:

* Verifying APIs
* Learning examples
* Supporting tutorials

---

## tests/example/

`tests/example/` is reserved for larger applications.

In version 2, this will include mini-projects using sensors and modules.

Future examples:

* I2C LCD display
* Ultrasonic distance measurement
* RFID reader
* Bluetooth communication
* Light sensor applications
* Sensor-based projects

---

## Future HAL Layer

In the future, EduFramework may include a `hal/` layer.

This layer will provide libraries for sensors and modules.

Planned architecture:

```text
Application / Mini Project
        ↓
HAL / Sensor Library
        ↓
Arduino-style API
        ↓
Driver
        ↓
Register / Hardware
```

---

## Role of HAL

HAL wraps Arduino APIs to provide higher-level functions for specific modules.

Example:

```c
distance = Ultrasonic_readDistance();
```

or:

```c
LCD_print("Hello");
```

Users do not need to know whether GPIO, Timer, UART, SPI, or I2C is used underneath.

In advanced cases, HAL may directly access driver layer for optimization.

---

## Layer Summary

| Layer              | Role                             |
| ------------------ | -------------------------------- |
| Application / Demo | User code or demo code           |
| HAL                | Sensor/module libraries (future) |
| Arduino-style API  | Easy-to-use abstraction          |
| Driver             | Register-level hardware control  |
| Hardware           | S32K144 + MaaZEDU board          |

---

##