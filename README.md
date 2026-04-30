# EduFramework – S32K144 Embedded Framework

EduFramework is a bare-metal embedded framework for **NXP S32K144 (MaaZEDU)**.

* No vendor SDK
* Register-level drivers
* Arduino-style API
* Designed for learning and teaching

---

## Overview

EduFramework is designed with two main goals:

* Easy to use (like Arduino)
* Easy to understand (for learning embedded systems)

This repository contains:

* Core framework (drivers + API)
* Example demos
* Documentation

---

## Getting Started

The recommended way to use this framework is through the documentation.

### If you want to USE the framework

Follow this path:

1. Start with:

   * `docs/vi/getting-started.md`

2. Then read API:

   * `docs/en/apis/`

3. Then learn through examples:

   * `docs/en/tutorials/`

---

### If you want to LEARN how it works

Follow this path:

1. Arduino-style API:

   * `firmware/arduino/`

2. Low-level drivers:

   * `firmware/drivers/`

3. Trace the flow from API → driver → register

---

## Example

```c
#include "Arduino.h"

int main(void)
{
    setup();

    pinMode(LED_RED, OUTPUT);

    while (1)
    {
        digitalWrite(LED_RED, LOW);
        delay(500);

        digitalWrite(LED_RED, HIGH);
        delay(500);
    }
}
```

---

## Project Structure

```
firmware/
├── drivers/      # register-level drivers
├── arduino/      # Arduino-style API
├── hal/          # sensor libraries (future)

tests/
└── demo/         # examples

docs/
├── vi/
└── en/
```

---

## Philosophy

* Driver = hardware control
* API = easy to use
* Demo = learning through practice

Code makes it work
Documentation makes it usable

---

## Documentation

All detailed guides are in the `docs/` folder:

* `docs/vi/` – Vietnamese documentation
* `docs/en/` – English documentation

Start here:

* `docs/vi/getting-started.md`
