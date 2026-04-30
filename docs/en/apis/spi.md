# SPI API

## Overview

The SPI API is used to communicate with peripherals using the SPI protocol in EduFramework.

You can use it to:

* Send and receive data between microcontrollers
* Communicate with sensors and modules (RFID, SPI LCD, flash…)
* Perform high-speed data transfer
* Work in both master and slave modes

---

## Basic Concepts

SPI uses 4 main signals:

| Name | Description         |
| ---- | ------------------- |
| MOSI | Master Out Slave In |
| MISO | Master In Slave Out |
| SCK  | Clock               |
| CS   | Chip Select         |

---

## Important Notes

* SPI is a **full-duplex** protocol
  → Sending and receiving happen at the same time

* When calling `SPI_transfer()`:

  * You **always send 1 byte**
  * You **always receive 1 byte**

---

## API List

| Function               | Description                                |
| ---------------------- | ------------------------------------------ |
| `SPI_begin()`          | Initialize SPI with default configuration  |
| `SPI_beginEx()`        | Initialize SPI with advanced configuration |
| `SPI_end()`            | Disable SPI                                |
| `SPI_setFrequency()`   | Set SPI speed                              |
| `SPI_setDataMode()`    | Set SPI mode                               |
| `SPI_setBitOrder()`    | Set bit order                              |
| `SPI_transfer()`       | Send & receive 8-bit                       |
| `SPI_transfer16()`     | Send & receive 16-bit                      |
| `SPI_transferBuffer()` | Send & receive buffer                      |
| `SPI_available()`      | Check received data                        |
| `SPI_read()`           | Read 8-bit                                 |
| `SPI_read16()`         | Read 16-bit                                |
| `SPI_write()`          | Send 8-bit                                 |
| `SPI_write16()`        | Send 16-bit                                |
| `SPI_getRole()`        | Get current role                           |

---

## SPI_begin()

### Purpose

Initialize SPI with default configuration.

### Syntax

```c
SPI_begin(role);
```

### Parameters

| Parameter | Description                           |
| --------- | ------------------------------------- |
| `role`    | `SPI_ROLE_MASTER` or `SPI_ROLE_SLAVE` |

### Example

```c
SPI_begin(SPI_ROLE_MASTER);
```

### Notes

* This is the simplest way for beginners
* Default configuration:

  * Mode 0
  * MSB first
  * Default frequency

---

## SPI_beginEx()

### Purpose

Initialize SPI with full configuration.

### Syntax

```c
SPI_beginEx(role, frequency, mode, bitOrder);
```

### Parameters

| Parameter   | Description           |
| ----------- | --------------------- |
| `role`      | Master or Slave       |
| `frequency` | SPI speed (Hz)        |
| `mode`      | SPI_MODE0 → SPI_MODE3 |
| `bitOrder`  | MSB or LSB            |

### Example

```c
SPI_beginEx(SPI_ROLE_MASTER, 1000000, SPI_MODE0, SPI_MSBFIRST);
```

### Notes

* Use when precise configuration is required
* Must match:

  * Mode
  * Clock
  * Bit order

---

## SPI_setFrequency()

### Purpose

Change SPI speed.

### Syntax

```c
SPI_setFrequency(frequency);
```

### Example

```c
SPI_setFrequency(2000000);
```

### Notes

* Must be called after `SPI_begin()`
* Not all devices support high speeds

---

## SPI_setDataMode()

### Purpose

Set SPI mode.

### Syntax

```c
SPI_setDataMode(mode);
```

### Modes

| Mode  | CPOL | CPHA |
| ----- | ---- | ---- |
| MODE0 | 0    | 0    |
| MODE1 | 0    | 1    |
| MODE2 | 1    | 0    |
| MODE3 | 1    | 1    |

### Example

```c
SPI_setDataMode(SPI_MODE3);
```

### Notes

* Must match the slave device
* Wrong mode → completely incorrect data

---

## SPI_setBitOrder()

### Purpose

Set bit order.

### Syntax

```c
SPI_setBitOrder(order);
```

### Example

```c
SPI_setBitOrder(SPI_MSBFIRST);
```

### Notes

* Usually MSB first
* LSB is less common

---

## SPI_transfer()

### Purpose

Send and receive 1 byte.

### Syntax

```c
uint8_t data = SPI_transfer(tx);
```

### Example

```c
uint8_t rx = SPI_transfer(0xA5);
```

### Notes

* Always returns data
* If only reading → send dummy (0xFF)

---

## SPI_transfer16()

### Purpose

Send and receive 16-bit data.

### Syntax

```c
uint16_t data = SPI_transfer16(tx);
```

### Example

```c
uint16_t rx = SPI_transfer16(0x1234);
```

---

## SPI_transferBuffer()

### Purpose

Send and receive multiple bytes.

### Syntax

```c
SPI_transferBuffer(txBuf, rxBuf, length);
```

### Example

```c
SPI_transferBuffer(txData, rxData, 10);
```

### Notes

* Can pass NULL if only sending or only receiving

---

## SPI_write() / SPI_write16()

### Purpose

Send data only.

### Example

```c
SPI_write(0x55);
SPI_write16(0x1234);
```

---

## SPI_read() / SPI_read16()

### Purpose

Read received data.

### Example

```c
uint8_t data = SPI_read();
```

---

## SPI_available()

### Purpose

Check if data is available.

### Example

```c
if (SPI_available())
{
    uint8_t data = SPI_read();
}
```

---

## SPI_getRole()

### Purpose

Get current role.

### Example

```c
if (SPI_getRole() == SPI_ROLE_MASTER)
{
    // do something
}
```

---

## Usage Suggestions

| Use case            | Recommended API        |
| ------------------- | ---------------------- |
| Quick start         | `SPI_begin()`          |
| Full configuration  | `SPI_beginEx()`        |
| Send 1 byte         | `SPI_transfer()`       |
| Send multiple bytes | `SPI_transferBuffer()` |
| Write only          | `SPI_write()`          |
| Read only           | `SPI_read()`           |

---

## Tutorials

You can learn SPI through:

* Full SPI demo in `tests/demo/spi_full_demo.c`
* SPI tutorials in the documentation
