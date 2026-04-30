# UART LED Control

## Objective

In this tutorial, you will learn how to:

* Receive simple commands from UART
* Control LEDs using received characters
* Combine Serial API with Digital API
* Print LED states to the terminal

---

## Concepts

### Command control via UART

UART is not only used for debugging or echoing data.

You can also use it to send commands from a computer to the board.

Example:

| Command    | Action           |
| ---------- | ---------------- |
| `r` or `R` | Toggle LED_RED   |
| `g` or `G` | Toggle LED_GREEN |
| `b` or `B` | Toggle LED_BLUE  |

---

### Why use single characters?

Each command is just one character.

This approach is simple and beginner-friendly because:

* No need to handle long strings
* No complex parsing required
* Easy to test using Hercules
* Easy to extend later

---

### Combining UART and GPIO

This program uses:

| API                      | Purpose                |
| ------------------------ | ---------------------- |
| `Serial1_available()`    | Check for new command  |
| `Serial1_read()`         | Read command character |
| `digitalToggle()`        | Toggle LED state       |
| `Serial1_printlnFloat()` | Print LED state        |

---

## Code

```c id="k7l3d2"
#include "Arduino.h"

int main(void)
{
    char ch;
    float redState = 0.0f;
    float greenState = 0.0f;
    float blueState = 0.0f;

    setup();

    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);

    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, HIGH);

    Serial1_begin(9600);

    Serial1_println("UART LED SIMPLE DEMO");
    Serial1_println("Send: r / g / b");

    while (1)
    {
        if (Serial1_available())
        {
            ch = Serial1_read();

            if ((ch == 'r') || (ch == 'R'))
            {
                digitalToggle(LED_RED);
                redState = (redState == 0.0f) ? 1.0f : 0.0f;

                Serial1_print("RED: ");
                Serial1_printlnFloat(redState);
            }
            else if ((ch == 'g') || (ch == 'G'))
            {
                digitalToggle(LED_GREEN);
                greenState = (greenState == 0.0f) ? 1.0f : 0.0f;

                Serial1_print("GREEN: ");
                Serial1_printlnFloat(greenState);
            }
            else if ((ch == 'b') || (ch == 'B'))
            {
                digitalToggle(LED_BLUE);
                blueState = (blueState == 0.0f) ? 1.0f : 0.0f;

                Serial1_print("BLUE: ");
                Serial1_printlnFloat(blueState);
            }
        }
    }
}
```

---

## Explanation

### LED initialization

```c id="p9f1x2"
pinMode(LED_RED, OUTPUT);
pinMode(LED_GREEN, OUTPUT);
pinMode(LED_BLUE, OUTPUT);
```

* Configure all LEDs as output
* Allows control via `digitalWrite()` or `digitalToggle()`

---

### Initial LED state

```c id="a3d8m4"
digitalWrite(LED_RED, HIGH);
digitalWrite(LED_GREEN, HIGH);
digitalWrite(LED_BLUE, HIGH);
```

On-board LEDs are **active LOW**:

| Write value | LED state |
| ----------- | --------- |
| LOW         | ON        |
| HIGH        | OFF       |

So writing `HIGH` turns them OFF initially.

---

### UART initialization

```c id="c8l2k7"
Serial1_begin(9600);
```

* Initialize UART for debugging
* Recommended baudrate: `9600`
* Use Hercules to send commands

---

### Checking for new commands

```c id="z1x7v3"
if (Serial1_available())
```

* Only read when data is available
* Prevents invalid reads

---

### Reading command

```c id="u6b4n9"
ch = Serial1_read();
```

* Reads one character from UART
* Example: `r`, `g`, `b`

---

### Handling RED command

```c id="t4m8y1"
if ((ch == 'r') || (ch == 'R'))
```

Accept both lowercase and uppercase.

```c id="j2h5q6"
digitalToggle(LED_RED);
```

Toggle the LED.

```c id="v9n3w8"
redState = (redState == 0.0f) ? 1.0f : 0.0f;
```

Update state variable for display.

---

### Printing state

```c id="f7s2e4"
Serial1_print("RED: ");
Serial1_printlnFloat(redState);
```

After each command, the board sends back the LED state.

Example:

```text id="b6r1k2"
RED: 1.0
RED: 0.0
```

---

## Result

When running the program with Hercules at baudrate `9600`:

| Command | Result            |
| ------- | ----------------- |
| `r`     | LED_RED toggles   |
| `g`     | LED_GREEN toggles |
| `b`     | LED_BLUE toggles  |

The terminal displays the LED state after each command.

---

## Notes

* Recommended baudrate: `9600`
* Terminal must use the correct baudrate
* Commands are single characters
* Both uppercase and lowercase are supported
* LEDs are active LOW
* State variables are used only for display purposes

---

## Related

See also:

```text id="y8d2l5"
docs/en/apis/serial.md
docs/en/apis/digital.md
```

to understand the APIs used in this tutorial.