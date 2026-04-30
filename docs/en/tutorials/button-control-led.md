# Button Control LED

## Objective

In this tutorial, you will learn how to:

* Read button states
* Understand how to use `INPUT_PULLUP`
* Detect a single button press
* Control LEDs using two methods:

  * `digitalToggle()`
  * `digitalWrite()`

---

## Concepts

### Button states

A button is a digital device with two states:

| State       | Value                                  |
| ----------- | -------------------------------------- |
| Not pressed | HIGH or LOW (depends on configuration) |
| Pressed     | The opposite value                     |

---

### Pull-up and Pull-down

An input pin should not be left floating, as it can produce unstable readings.

EduFramework supports two modes:

| Mode             | Default state | When pressed |
| ---------------- | ------------- | ------------ |
| `INPUT_PULLUP`   | HIGH          | LOW          |
| `INPUT_PULLDOWN` | LOW           | HIGH         |

In this tutorial, we use:

```c
pinMode(BTN0, INPUT_PULLUP);
```

This means:

* Not pressed → HIGH
* Pressed → LOW

---

### Detecting a single press

If you only use:

```c
if (digitalRead(BTN0) == LOW)
```

the LED will toggle continuously while the button is held.

To avoid this, we detect a **falling edge**:

| Previous state | Current state | Meaning             |
| -------------- | ------------- | ------------------- |
| HIGH           | LOW           | Button just pressed |

---

### Noise and bouncing

Mechanical buttons can generate noise (bouncing):

* One press may be detected as multiple rapid changes

In this framework:

* A basic **passive filter** is enabled
* For more advanced applications, software debounce may still be required

---

## Code

```c
#include "Arduino.h"

int main(void)
{
    setup();

    bool lastBtn0 = true;
    bool lastBtn1 = true;
    bool redState = false;

    pinMode(BTN0, INPUT_PULLUP);
    pinMode(BTN1, INPUT_PULLUP);
    pinMode(LED_BLUE, OUTPUT);
    pinMode(LED_RED, OUTPUT);

    digitalWrite(LED_BLUE, LOW);
    digitalWrite(LED_RED, LOW);

    while (1)
    {
        bool btn0 = digitalRead(BTN0);
        bool btn1 = digitalRead(BTN1);

        if ((lastBtn0 == true) && (btn0 == false))
        {
            digitalToggle(LED_BLUE);
        }

        if ((lastBtn1 == true) && (btn1 == false))
        {
            redState = !redState;
            digitalWrite(LED_RED, redState ? HIGH : LOW);
        }

        lastBtn0 = btn0;
        lastBtn1 = btn1;
    }
}
```

---

## Explanation

### Pin configuration

* `BTN0`, `BTN1` use `INPUT_PULLUP`
* `LED_BLUE`, `LED_RED` use `OUTPUT`

---

### Reading button

```c
bool btn0 = digitalRead(BTN0);
```

* `true` → HIGH
* `false` → LOW

---

### Detecting press

```c
if ((lastBtn0 == true) && (btn0 == false))
```

This condition ensures:

* The action happens only when the button is just pressed
* No repeated triggering while holding the button

---

### Two ways to toggle LED

| Method            | Code                       | Description        |
| ----------------- | -------------------------- | ------------------ |
| `digitalToggle()` | `digitalToggle(LED_BLUE);` | Simple and concise |
| `digitalWrite()`  | uses `redState` variable   | More flexible      |

---

## Result

* Press `BTN0` → LED_BLUE toggles
* Press `BTN1` → LED_RED toggles

---

## Related

See also:

```text
docs/en/api/digital.md
```

to understand the APIs used in this tutorial.