# EduFramework – Getting Started

## What is EduFramework?

EduFramework is a personal project aimed at building a simple and accessible embedded framework for the S32K144 (MaaZEDU).

This project is designed for:

* Beginners who are new to embedded systems
* Students who need a clear and practical learning platform
* Learners who want to explore how low-level drivers are implemented

EduFramework does not aim to replace the official SDK, but rather to act as a bridge that makes embedded development easier to approach and understand.

---

## Introduction

When learning embedded systems, especially with microcontrollers like S32K144, beginners often face several challenges:

* Vendor SDKs are complex
* Configuration steps are difficult to understand
* Too many concepts are introduced at once

As a result:

* It takes a long time to get started
* It is hard to grasp how the system really works
* Many learners feel overwhelmed early on

---

## Goals of EduFramework

EduFramework is built with the following goals:

* Help beginners get started quickly
* Provide an easy-to-use API similar to Arduino
* Still allow deep learning at the register level

In other words:

> You can use it immediately, or dive deeper — depending on your needs.

---

## Project Structure

EduFramework is contained in a single repository, but it consists of two main parts:

---

### 1. Library (for direct use)

This is the part you will use to build applications:

* `eduframework/include/` → header files
* `eduframework/lib/libeduframework.a` → static library

You only need to:

* include the library
* use the API

Example:

```c
pinMode(LED_RED, OUTPUT);
digitalWrite(LED_RED, LOW);
```

---

### 2. Source code (for learning)

The entire framework is implemented at the register level:

* `firmware/drivers/` → low-level drivers
* `firmware/arduino/` → Arduino-style API

If you want to:

* understand how drivers work
* learn how embedded systems are built from scratch

→ you can read the source code directly

---

Note:

This documentation focuses on using the library.
If you want deeper understanding, read the source code.

---

## Getting Started with Your First Program (Blink LED)

Goal:

Create a project and make an LED blink

---

## Requirements

* S32 Design Studio (S32DS)
* Board: MaaZEDU S32K144

---

## Step 1: Create a new project

1. Open S32 Design Studio
2. Select:

```
File → New → S32DS Application Project
```

3. Configure:

* Device: S32K144
* Core: Cortex-M4F
* Toolchain: NXP GCC 10.2

4. Finish

---

## Step 2: Configure EduFramework

No need to copy the library
Use absolute paths instead

---

### 2.1 Add include path

Go to:

```
Project Properties → C/C++ Build → Settings → Includes
```

Add the path to the include folder:

```text
D:\S32K144_MaaZEDU_Framework\eduframework\include
```

---

### 2.2 Add library path

Go to:

```
Project Properties → C/C++ Build → Settings → Linker → Libraries
```

Add the path to the lib folder:

```text
D:\S32K144_MaaZEDU_Framework\eduframework\lib
```

---

### 2.3 Link the library

In **Libraries (-l)**, add:

```text
eduframework
```

---

## Step 3: Write your first program

Open `main.c`:

```c
#include "Arduino.h"
#include "S32K144.h"

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

## Step 4: Build & Run

1. Build the project
2. Flash using JLink
3. Run on the board

Result:

The LED will blink every 500ms

---

## Result

You have successfully:

* Imported an embedded library
* Written your first program
* Run code without complex configuration

---

## Next Steps

After this:

### Understand hardware mapping

Read:

```
docs/en/pin-mapping.md
```

---

### Learn the API

Read:

```
docs/en/api/
```

---

### Learn how to use the API in real projects

Read:

```
docs/en/tutorials/
```

---

## Summary

EduFramework is designed to:

* Reduce the barrier to entry for embedded systems
* Make learning more accessible
* Still allow deep understanding of the system

You can start simple and gradually move to advanced topics — all within the same framework.
