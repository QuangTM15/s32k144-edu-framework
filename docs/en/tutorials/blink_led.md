# Blink LED

## Objective

In this tutorial, you will learn how to:

* Use `delay()` to create time delays
* Use `millis()` for time-based control
* Compare two timing methods
* Control multiple LEDs with different intervals

---

## Concepts

### delay() – simple approach

`delay()` pauses the program for a specific duration.

Example:

```c
delay(500);
```

* The program stops for 500 ms
* Easy to understand and use
* But it is **blocking**

---

### millis() – flexible approach

`millis()` returns the elapsed time in milliseconds.

Example:

```c
if ((millis() - lastTime) >= 500)
```

* Does not block the program
* Allows multiple tasks to run in parallel
* Suitable for more complex applications

---

### Comparison

| Method     | Advantage     | Disadvantage       |
| ---------- | ------------- | ------------------ |
| `delay()`  | Simple to use | Blocks the program |
| `millis()` | Flexible      | More complex code  |

---

## Problem idea

Control 3 LEDs with different timing:

| LED   | Interval | Method     |
| ----- | -------- | ---------- |
| RED   | 500 ms   | `delay()`  |
| GREEN | 700 ms   | `millis()` |
| BLUE  | 1100 ms  | `millis()` |

---

## Code

```c
#include "Arduino.h"

int main(void)
{
    uint32_t lastGreen = 0U;
    uint32_t lastBlue  = 0U;
    uint8_t redTick = 0U;

    setup();

    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);

    /* All OFF initially
       Note: board LED is active-low */
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, HIGH);

    while (1)
    {
        /* Base tick using delay(): 100 ms */
        delay(100);
        redTick++;

        /* RED LED: 500 ms using delay-based tick */
        if (redTick >= 5U)
        {
            redTick = 0U;
            digitalToggle(LED_RED);
        }

        /* GREEN LED: 700 ms using millis() */
        if ((millis() - lastGreen) >= 700U)
        {
            lastGreen = millis();
            digitalToggle(LED_GREEN);
        }

        /* BLUE LED: 1100 ms using millis() */
        if ((millis() - lastBlue) >= 1100U)
        {
            lastBlue = millis();
            digitalToggle(LED_BLUE);
        }
    }
}
```

---

## Explanation

### LED initialization

* All LEDs are configured as `OUTPUT`
* Writing `HIGH` turns them OFF (active LOW)

---

### RED LED – using delay()

```c
delay(100);
redTick++;
```

* Each loop = 100 ms
* After 5 loops → 500 ms

```c
if (redTick >= 5U)
```

* Toggle LED every 500 ms

---

### GREEN LED – using millis()

```c
if ((millis() - lastGreen) >= 700U)
```

* Check if 700 ms has passed
* If yes → toggle LED

```c
lastGreen = millis();
```

* Update the time reference

---

### BLUE LED – similar approach

```c
if ((millis() - lastBlue) >= 1100U)
```

* Interval = 1100 ms
* Runs independently from other LEDs

---

## Key points

* `delay()` slows down the entire program
* `millis()` does not block execution
* `millis()` allows multiple tasks to run simultaneously
* This is an important step from beginner to intermediate level

---

## Result

* RED LED blinks every 500 ms
* GREEN LED blinks every 700 ms
* BLUE LED blinks every 1100 ms

All LEDs run simultaneously in the same loop

---

## Related

See also:

```text
docs/en/apis/time.md
docs/en/apis/digital.md
```

to understand the APIs used in this tutorial.
