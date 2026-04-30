# I2C API

## Overview

The I2C API is used to communicate with peripherals using the I2C protocol in EduFramework.

You can use it to:

- Communicate with I2C sensors
- Communicate with I2C LCD modules
- Read data from external modules
- Send data from Master to Slave
- Receive data from Slave to Master
- Work in both master and slave modes

---

## Basic Concepts

I2C uses 2 main lines:

| Name | Description  |
| ---- | ------------ |
| SDA  | Serial Data  |
| SCL  | Serial Clock |

---

## Important Notes

- I2C is a **half-duplex** protocol
  → At any given time, the bus transfers data in only one direction

- I2C uses addressing to select devices
  → Each slave must have a unique address

- One I2C bus can have multiple slaves

---

## API List

| Function                   | Description                                            |
| -------------------------- | ------------------------------------------------------ |
| `Wire_begin()`             | Initialize I2C in Master mode                          |
| `Wire_beginAddress()`      | Initialize I2C in Slave mode with a specific address   |
| `Wire_setClock()`          | Set I2C clock speed                                    |
| `Wire_beginTransmission()` | Begin transmission to a slave                          |
| `Wire_write()`             | Write 1 byte to TX buffer                              |
| `Wire_writeBuffer()`       | Write multiple bytes to TX buffer                      |
| `Wire_endTransmission()`   | End transmission and send data                         |
| `Wire_requestFrom()`       | Master requests data from a slave                      |
| `Wire_available()`         | Check available bytes to read                          |
| `Wire_read()`              | Read 1 byte                                            |
| `Wire_onReceive()`         | Register callback when Slave receives data             |
| `Wire_onRequest()`         | Register callback when Master requests data from Slave |
| `Wire_transferAsync()`     | Start asynchronous transfer                            |
| `Wire_isBusy()`            | Check if I2C is busy                                   |
| `Wire_isDone()`            | Check if async transfer is completed                   |
| `Wire_getStatus()`         | Get last I2C status                                    |

---

## Wire_begin()

### Purpose

Initialize I2C in Master mode.

### Syntax

```c
Wire_begin();
```

### Example

```c
Wire_begin();
```

### Notes

- Used for Master devices
- Master initiates communication
- After calling this function, you can use:
  - `Wire_beginTransmission()`
  - `Wire_requestFrom()`

---

## Wire_beginAddress()

### Purpose

Initialize I2C in Slave mode with a specific address.

### Syntax

```c
Wire_beginAddress(address);
```

### Parameters

| Parameter | Description       |
| --------- | ----------------- |
| `address` | I2C slave address |

### Example

```c
Wire_beginAddress(0x08);
```

### Notes

- Used for Slave devices
- Each slave must have a unique address on the bus
- Master uses this address to communicate

---

## Wire_setClock()

### Purpose

Set I2C clock speed.

### Syntax

```c
Wire_setClock(frequency);
```

### Parameters

| Parameter   | Description     |
| ----------- | --------------- |
| `frequency` | I2C speed in Hz |

### Example

```c
Wire_setClock(100000);
```

### Notes

- `100000` → 100 kHz
- `400000` → 400 kHz
- Should be called after `Wire_begin()`
- Not all devices support high speeds

---

## Wire_beginTransmission()

### Purpose

Start a transmission from Master to a Slave.

### Syntax

```c
Wire_beginTransmission(address);
```

### Parameters

| Parameter | Description          |
| --------- | -------------------- |
| `address` | Target slave address |

### Example

```c
Wire_beginTransmission(0x08);
```

### Notes

- This only starts the transmission
- Data is not sent immediately
- Use `Wire_write()` or `Wire_writeBuffer()` next
- Finish with `Wire_endTransmission()`

---

## Wire_write()

### Purpose

Write 1 byte into the TX buffer.

### Syntax

```c
uint8_t result = Wire_write(data);
```

### Parameters

| Parameter | Description  |
| --------- | ------------ |
| `data`    | Byte to send |

### Return value

| Value     | Description             |
| --------- | ----------------------- |
| `uint8_t` | Number of bytes written |

### Example

```c
Wire_write(0x55);
```

### Notes

- Typically used between:
  - `Wire_beginTransmission()`
  - `Wire_endTransmission()`

- Data is buffered first
- Actually sent when calling `Wire_endTransmission()`

---

## Wire_writeBuffer()

### Purpose

Write multiple bytes into the TX buffer.

### Syntax

```c
uint8_t result = Wire_writeBuffer(data, length);
```

### Parameters

| Parameter | Description            |
| --------- | ---------------------- |
| `data`    | Pointer to data buffer |
| `length`  | Number of bytes        |

### Return value

| Value     | Description             |
| --------- | ----------------------- |
| `uint8_t` | Number of bytes written |

### Example

```c
uint8_t data[3] = {0x11, 0x22, 0x33};

Wire_writeBuffer(data, 3);
```

### Notes

- Used for sending multiple bytes
- Buffer should not be `NULL`
- `length` must match actual data size

---

## Wire_endTransmission()

### Purpose

End transmission and send data from Master to Slave.

### Syntax

```c
uint8_t status = Wire_endTransmission();
```

### Return value

| Value                 | Description          |
| --------------------- | -------------------- |
| `WIRE_STATUS_OK`      | Success              |
| `WIRE_STATUS_BUSY`    | I2C busy             |
| `WIRE_STATUS_TIMEOUT` | Timeout              |
| `WIRE_STATUS_NACK`    | Slave not responding |
| `WIRE_STATUS_ERROR`   | Other error          |

### Example

```c
Wire_beginTransmission(0x08);
Wire_write(0x55);
status = Wire_endTransmission();
```

### Notes

- This function actually sends the buffered data
- Always check return status
- Wrong address → usually returns `WIRE_STATUS_NACK`

---

## Wire_requestFrom()

### Purpose

Master requests data from a Slave.

### Syntax

```c
uint8_t count = Wire_requestFrom(address, quantity);
```

### Parameters

| Parameter  | Description             |
| ---------- | ----------------------- |
| `address`  | Slave address           |
| `quantity` | Number of bytes to read |

### Return value

| Value     | Description              |
| --------- | ------------------------ |
| `uint8_t` | Number of bytes received |

### Example

```c
uint8_t count = Wire_requestFrom(0x08, 3);
```

### Notes

- Used by Master
- After calling, use:
  - `Wire_available()`
  - `Wire_read()`

- Received bytes may be less than requested if errors occur

---

## Wire_available()

### Purpose

Check how many bytes are available to read.

### Syntax

```c
int count = Wire_available();
```

### Return value

| Value | Description     |
| ----- | --------------- |
| `int` | Number of bytes |

### Example

```c
if (Wire_available() > 0)
{
    int data = Wire_read();
}
```

### Notes

- Always check before calling `Wire_read()`
- Used after `Wire_requestFrom()` or in receive handling

---

## Wire_read()

### Purpose

Read 1 byte from RX buffer.

### Syntax

```c
int data = Wire_read();
```

### Return value

| Value    | Description       |
| -------- | ----------------- |
| `0..255` | Received byte     |
| `-1`     | No data available |

### Example

```c
int data = Wire_read();
```

### Notes

- Call after `Wire_available() > 0`
- Returns `-1` if no data

---

## Wire_onReceive()

### Purpose

Register callback when Slave receives data from Master.

### Syntax

```c
Wire_onReceive(callback);
```

### Parameters

| Parameter  | Description                |
| ---------- | -------------------------- |
| `callback` | Function called on receive |

### Example

```c
void receiveEvent(int length)
{
    while (Wire_available())
    {
        int data = Wire_read();
    }
}

Wire_onReceive(receiveEvent);
```

### Notes

- Used in Slave mode
- Callback receives number of bytes
- Keep callback lightweight

---

## Wire_onRequest()

### Purpose

Register callback when Master requests data from Slave.

### Syntax

```c
Wire_onRequest(callback);
```

### Parameters

| Parameter  | Description                |
| ---------- | -------------------------- |
| `callback` | Function called on request |

### Example

```c
void requestEvent(void)
{
    Wire_write(0x55);
}

Wire_onRequest(requestEvent);
```

### Notes

- Used in Slave mode
- Slave can send data using `Wire_write()`
- Keep callback short

---

## Wire_transferAsync()

### Purpose

Start an asynchronous I2C transfer.

This function does not block, allowing the CPU to perform other tasks.

### Syntax

```c
uint8_t status = Wire_transferAsync(address, txData, txSize, rxData, rxSize);
```

### Parameters

| Parameter | Description                |
| --------- | -------------------------- |
| `address` | Slave address              |
| `txData`  | TX buffer                  |
| `txSize`  | Number of bytes to send    |
| `rxData`  | RX buffer                  |
| `rxSize`  | Number of bytes to receive |

### Return value

| Value               | Description          |
| ------------------- | -------------------- |
| `WIRE_STATUS_OK`    | Started successfully |
| `WIRE_STATUS_BUSY`  | I2C busy             |
| `WIRE_STATUS_ERROR` | Failed to start      |

### Example

```c
uint8_t tx[1] = {0x01};
uint8_t rx[2];

Wire_transferAsync(0x08, tx, 1, rx, 2);
```

### Notes

- Advanced API
- Used for non-blocking / interrupt-based transfer
- Use:
  - `Wire_isBusy()`
  - `Wire_isDone()`
  - `Wire_getStatus()`

---

## Wire_isBusy()

### Purpose

Check if I2C is busy.

### Syntax

```c
uint8_t busy = Wire_isBusy();
```

### Return value

| Value | Description |
| ----- | ----------- |
| `1`   | Busy        |
| `0`   | Not busy    |

---

## Wire_isDone()

### Purpose

Check if async transfer is complete.

### Syntax

```c
uint8_t done = Wire_isDone();
```

### Return value

| Value | Description   |
| ----- | ------------- |
| `1`   | Completed     |
| `0`   | Not completed |

---

## Wire_getStatus()

### Purpose

Get last I2C status.

### Syntax

```c
uint8_t status = Wire_getStatus();
```

### Return value

| Value                 | Description          |
| --------------------- | -------------------- |
| `WIRE_STATUS_OK`      | Success              |
| `WIRE_STATUS_BUSY`    | I2C busy             |
| `WIRE_STATUS_TIMEOUT` | Timeout              |
| `WIRE_STATUS_NACK`    | Slave not responding |
| `WIRE_STATUS_ERROR`   | Other error          |

---

## Usage Suggestions

| Use case              | Recommended API                                                        |
| --------------------- | ---------------------------------------------------------------------- |
| Initialize Master     | `Wire_begin()`                                                         |
| Initialize Slave      | `Wire_beginAddress()`                                                  |
| Set clock             | `Wire_setClock()`                                                      |
| Master write          | `Wire_beginTransmission()` + `Wire_write()` + `Wire_endTransmission()` |
| Master read           | `Wire_requestFrom()` + `Wire_available()` + `Wire_read()`              |
| Slave receive         | `Wire_onReceive()`                                                     |
| Slave respond         | `Wire_onRequest()`                                                     |
| Non-blocking transfer | `Wire_transferAsync()`                                                 |

---

## Tutorials

You can learn I2C through tutorials or by reading source code in `tests/demo`.
