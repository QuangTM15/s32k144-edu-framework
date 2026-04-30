# UART Echo

## Objective

In this tutorial, you will learn how to:

* Initialize UART using `Serial1_begin()`
* Read data from UART
* Send data through UART
* Handle simple string input
* Build an echo program (what you type will be sent back)

---

## Concepts

### What is UART?

UART is a serial communication interface used to:

* Send data between a microcontroller and a computer
* Debug programs
* Communicate with external modules or sensors

In EduFramework:

* `Serial1` → used for debugging via USB (Hercules)
* `Serial2` → used for physical UART (TX/RX pins)

---

### How this program works

The program will:

1. Read characters from UART one by one
2. Store them into a buffer
3. When a newline (`\n`) is received:

   * End the string
   * Print it back to the terminal

---

### Why use a buffer?

UART receives data one character at a time.

To process a full string (e.g., "hello"), we need to:

* Store each character into a buffer
* Process the complete string when it is finished

---

## Code

```c id="djh2xk"
#include "Arduino.h"

#define RX_BUFFER_SIZE (64U)

int main(void)
{
    char rxBuffer[RX_BUFFER_SIZE];
    uint32_t index = 0U;
    char ch;

    setup();

    Serial1_begin(9600);

    Serial1_print("=== UART1 ECHO DEMO ===\n");
    Serial1_println("Type something and press Enter:");

    while (1)
    {
        if (Serial1_available())
        {
            ch = Serial1_read();

            if (ch == '\r')
            {
                continue;
            }

            if (ch == '\n')
            {
                rxBuffer[index] = '\0';

                Serial1_print("Echo: ");
                Serial1_println(rxBuffer);

                index = 0U;
            }
            else
            {
                if (index < (RX_BUFFER_SIZE - 1U))
                {
                    rxBuffer[index] = ch;
                    index++;
                }
                else
                {
                    /* overflow -> reset */
                    index = 0U;
                }
            }
        }
    }
}
```

---

## Explanation

### UART initialization

```c id="sz9c5e"
Serial1_begin(9600);
```

* Initializes UART with baudrate 9600
* This is the recommended baudrate

---

### Checking for data

```c id="a9j1zp"
if (Serial1_available())
```

* Checks if new data is available
* Prevents reading when no data is present

---

### Reading characters

```c id="7mxwz2"
ch = Serial1_read();
```

* UART receives data one character at a time
* Each character must be processed individually

---

### Ignoring '\r'

```c id="lm7t2d"
if (ch == '\r')
{
    continue;
}
```

* Some terminals send both `\r\n`
* Ignore `\r` to avoid incorrect processing

---

### Detecting end of string

```c id="0d2hpm"
if (ch == '\n')
```

* Pressing Enter sends `\n`
* This indicates the end of input

```c id="5d1yrt"
rxBuffer[index] = '\0';
```

* Add string terminator

---

### Echoing the input

```c id="y5y4hz"
Serial1_print("Echo: ");
Serial1_println(rxBuffer);
```

* Print back the received string

---

### Storing characters

```c id="9t6v9z"
rxBuffer[index] = ch;
index++;
```

* Store each character
* Increment buffer index

---

### Handling overflow

```c id="lntc1g"
if (index < (RX_BUFFER_SIZE - 1U))
```

* Prevent buffer overflow

If overflow occurs:

```c id="r2nhz3"
index = 0U;
```

* Reset buffer

---

## Result

When running the program:

* Type text in Hercules
* Press Enter

Output:

```text id="n8a3k1"
Echo: hello
Echo: test
```

---

## Notes

* Recommended baudrate is `9600`
* Terminal must be configured with the correct baudrate
* UART processes data character by character
* Always check `available()` before reading
* Use a buffer to handle strings
* Prevent buffer overflow when input is too long

---

## Related

See also:

```text id="yq4j7p"
docs/en/apis/serial.md
```

to understand the APIs used in this tutorial.
