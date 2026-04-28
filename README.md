# EduFramework – S32K144 Embedded Framework

EduFramework is a bare-metal embedded framework for **NXP S32K144 (MaaZEDU)**.

- No vendor SDK  
- Register-level drivers  
- Arduino-style API (easy to use)  
- Designed for learning and teaching  

---

## 🎯 Who is this for?

EduFramework supports **two types of users**:

---

### 🔹 1. You just want to USE it (like Arduino)

👉 You do NOT need to understand drivers or hardware

Just follow this path:

1. Read:
   - `docs/vi/library-usage.md`

2. Then check API:
   - `docs/vi/api/`

3. Then copy example:
   - `docs/vi/examples/`

👉 That’s it. You can use the framework immediately.

---

### 🔹 2. You want to LEARN how it works

👉 You want to understand embedded systems deeply

Follow this path:

1. Read API implementation:
   - `firmware/core/`

2. Then read drivers:
   - `firmware/drivers/`

3. Then trace flow:


👉 This is where real learning happens.

---

## ⚡ Quick Example

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
## 📂 Project Structure

```
firmware/
├── drivers/     # register-level drivers
├── core/        # Arduino-style API
├── hal/         # sensor libraries (future)

tests/
└── demo/        # examples

docs/
├── vi/
└── en/
```
---
## 🧠 Philosophy
Driver = hardware control
API = easy to use
Demo = how to learn

Code makes it work  
Docs make it usable