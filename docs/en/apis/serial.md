# Serial API

## Overview

The Serial API is used for UART communication in EduFramework.

You can use it to:

- Print debug data to Hercules
- Send and receive characters
- Send text strings
- Send integers and floating-point numbers
- Communicate with external modules or sensors via UART

EduFramework provides two Serial interfaces:

| Serial    | Purpose                                                 |
| --------- | ------------------------------------------------------- |
| `Serial1` | Debug via J-Link/USB, typically used with Hercules      |
| `Serial2` | Physical UART via TX/RX pins for external communication |

---

## Baudrate Notes

Currently, the Serial API is recommended to be used at low baudrates.

Recommended values:

| Baudrate      | Status              |
| ------------- | ------------------- |
| `9600`        | Recommended         |
| `19200`       | Acceptable          |
| `38400`       | Maximum recommended |
| Above `38400` | Not recommended     |

This limitation is due to the current clock configuration not being optimized for high-speed UART.

---

## Function List

### Serial1

| Function                 | Description                            |
| ------------------------ | -------------------------------------- |
| `Serial1_begin()`        | Initialize Serial1                     |
| `Serial1_available()`    | Check if data is available             |
| `Serial1_read()`         | Read a character                       |
| `Serial1_readString()`   | Read a string                          |
| `Serial1_write()`        | Send a character                       |
| `Serial1_print()`        | Send a string                          |
| `Serial1_println()`      | Send a string with newline             |
| `Serial1_printInt()`     | Send an integer                        |
| `Serial1_printlnInt()`   | Send an integer with newline           |
| `Serial1_printFloat()`   | Send a floating-point number           |
| `Serial1_printlnFloat()` | Send a floating-point number + newline |

### Serial2

Serial2 provides the same set of functions as Serial1, but is used for physical UART via TX/RX pins.

---

## Serial1_begin() / Serial2_begin()

### Purpose

Initialize UART with a specified baudrate.

### Syntax

```c
Serial1_begin(baudRate);
Serial2_begin(baudRate);
```

### Parameters

| Parameter  | Description   |
| ---------- | ------------- |
| `baudRate` | UART baudrate |

### Example

```c
Serial1_begin(9600);
Serial2_begin(9600);
```

### Notes

- Always call `begin()` before using other Serial functions.
- Recommended baudrate is `9600`.
- Avoid using baudrates higher than `38400` in the current version.

---

## Serial1_available() / Serial2_available()

### Purpose

Check whether new data is available to read.

### Syntax

```c
bool available = Serial1_available();
bool available = Serial2_available();
```

### Return Value

| Value   | Description       |
| ------- | ----------------- |
| `true`  | Data is available |
| `false` | No data available |

### Example

```c
if (Serial1_available())
{
    char ch = Serial1_read();
}
```

### Notes

- Always check `available()` before calling `read()`.
- Reading without available data may produce unexpected results.

---

## Serial1_read() / Serial2_read()

### Purpose

Read a single character from UART.

### Syntax

```c
char ch = Serial1_read();
char ch = Serial2_read();
```

### Return Value

| Value  | Description            |
| ------ | ---------------------- |
| `char` | The received character |

### Example

```c
if (Serial1_available())
{
    char ch = Serial1_read();
    Serial1_write(ch);
}
```

### Notes

- This function reads only one character.
- To read multiple characters, use `readString()`.

---

## Serial1_readString() / Serial2_readString()

### Purpose

Read a string from UART into a buffer.

### Syntax

```c
uint32_t len = Serial1_readString(buffer, maxLength);
uint32_t len = Serial2_readString(buffer, maxLength);
```

### Parameters

| Parameter   | Description                          |
| ----------- | ------------------------------------ |
| `buffer`    | Buffer to store received string      |
| `maxLength` | Maximum number of characters to read |

### Return Value

| Value      | Description               |
| ---------- | ------------------------- |
| `uint32_t` | Number of characters read |

### Example

```c
char buffer[32];

uint32_t len = Serial1_readString(buffer, sizeof(buffer));
```

### Notes

- Make sure the buffer is large enough.
- Do not pass a `maxLength` larger than the buffer size.
- Suitable for reading short commands or terminal input.

---

## Serial1_write() / Serial2_write()

### Purpose

Send a single character via UART.

### Syntax

```c
Serial1_write(ch);
Serial2_write(ch);
```

### Parameters

| Parameter | Description       |
| --------- | ----------------- |
| `ch`      | Character to send |

### Example

```c
Serial1_write('A');
```

---

## Serial1_print() / Serial2_print()

### Purpose

Send a string via UART.

### Syntax

```c
Serial1_print(str);
Serial2_print(str);
```

### Parameters

| Parameter | Description    |
| --------- | -------------- |
| `str`     | String to send |

### Example

```c
Serial1_print("Hello EduFramework");
```

---

## Serial1_println() / Serial2_println()

### Purpose

Send a string and automatically append a newline.

### Syntax

```c
Serial1_println(str);
Serial2_println(str);
```

### Example

```c
Serial1_println("System started");
```

### Notes

- `print()` does not append a newline.
- `println()` adds a newline, making logs easier to read.

---

## Serial1_printInt() / Serial2_printInt()

### Purpose

Send an integer via UART.

### Syntax

```c
Serial1_printInt(value);
Serial2_printInt(value);
```

### Example

```c
int value = 123;
Serial1_printInt(value);
```

---

## Serial1_printlnInt() / Serial2_printlnInt()

### Purpose

Send an integer with a newline.

### Syntax

```c
Serial1_printlnInt(value);
Serial2_printlnInt(value);
```

### Example

```c
int adcValue = 2048;
Serial1_printlnInt(adcValue);
```

---

## Serial1_printFloat() / Serial2_printFloat()

### Purpose

Send a floating-point number via UART.

### Syntax

```c
Serial1_printFloat(value);
Serial2_printFloat(value);
```

### Example

```c
float voltage = 3.3f;
Serial1_printFloat(voltage);
```

---

## Serial1_printlnFloat() / Serial2_printlnFloat()

### Purpose

Send a floating-point number with a newline.

### Syntax

```c
Serial1_printlnFloat(value);
Serial2_printlnFloat(value);
```

### Example

```c
float temperature = 27.5f;
Serial1_printlnFloat(temperature);
```

---

## Serial1 vs Serial2

| Port      | Purpose              | When to use                        |
| --------- | -------------------- | ---------------------------------- |
| `Serial1` | Debug via J-Link/USB | Logging, testing with Hercules     |
| `Serial2` | Physical UART TX/RX  | Communication with sensors/modules |

---

## Tutorials

You can learn how to use the Serial API through tutorials such as UART echo or UART LED control.

##
