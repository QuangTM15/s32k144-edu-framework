# SPI Communication (Master – Slave)

## Objective

In this tutorial, you will learn how to:

* Understand how SPI works
* Set up communication between two boards
* Send data from Master
* Receive and process data on Slave
* Use basic SPI APIs

---

## Introduction

SPI is a high-speed communication protocol used between devices.

In this tutorial, you will:

* Connect **2 S32K144 boards**
* One board acts as **Master**
* One board acts as **Slave**

> Press button → Master sends data → Slave receives → LED changes

---

## Required Knowledge

### SPI (Serial Peripheral Interface)

* Synchronous communication (with clock)
* Full-duplex (send and receive at the same time)
* High speed

---

### SPI Pins

* SCK – Clock
* MOSI – Master → Slave
* MISO – Slave → Master
* CS – Chip Select

---

### Master vs Slave

#### Master

* Generates clock
* Actively sends data
* Controls the communication

#### Slave

* Waits for Master
* Receives data
* Processes data

---

### SPI Mode

This tutorial uses:

```c id="g2q7n1"
SPI_MODE0
```

---

## Working Principle

```text id="yhh3wl"
Button (Master) → SPI → Slave → LED toggle
```

---

## Code

```c id="demo_spi_full"
#define SPI_DEMO_IS_MASTER    1

#define SPI_DEMO_CASE_8BIT    0
#define SPI_DEMO_CASE_16BIT   1
#define SPI_DEMO_CASE_BUFFER  2

#define SPI_DEMO_CASE         SPI_DEMO_CASE_BUFFER
```

---

## Explanation

### Select Role

```c id="role_select"
#define SPI_DEMO_IS_MASTER 1
```

* `1` → Master
* `0` → Slave

👉 You need **2 boards**:

* One board set to `1`
* One board set to `0`

---

### Select Transfer Mode

```c id="case_select"
#define SPI_DEMO_CASE SPI_DEMO_CASE_BUFFER
```

There are 3 modes:

* 8-bit → transfer 1 byte
* 16-bit → transfer 2 bytes
* Buffer → transfer multiple bytes

---

## SPI Initialization

```c id="init_spi"
SPI_beginEx(SPI_ROLE_MASTER, 1000000U, SPI_MODE0, SPI_MSBFIRST);
```

### Explanation

* `SPI_ROLE_MASTER` → select Master
* `1000000U` → 1 MHz
* `SPI_MODE0` → SPI mode
* `SPI_MSBFIRST` → MSB first

👉 Slave also calls `SPI_beginEx`, but does not need frequency

---

## Master – Sending Data

### Send 8-bit

```c id="send_8bit"
SPI_transfer(0xA5);
```

---

### Send 16-bit

```c id="send_16bit"
SPI_transfer16(0x55AA);
```

---

### Send buffer

```c id="send_buffer"
SPI_transferBuffer(g_testBuf, 0, 3);
```

---

### Trigger with button

```c id="button_trigger"
if (ButtonPressed(BTN0))
{
    SPI_transfer(...);
}
```

👉 Master only sends when the button is pressed

---

## Slave – Receiving Data

### Check data availability

```c id="check_available"
if (!SPI_available())
{
    return;
}
```

👉 Only read when data is available

---

### Read 8-bit

```c id="read_8bit"
uint8_t data = SPI_read();
```

---

### Read 16-bit

```c id="read_16bit"
uint16_t data = SPI_read16();
```

---

### Read buffer

```c id="read_buffer"
buf[0] = SPI_read();

for (i = 1; i < 3; i++)
{
    while (!SPI_available());
    buf[i] = SPI_read();
}
```

---

## Data Processing

```c id="process_data"
if (data == 0xA5)
{
    digitalToggle(LED_RED);
}
```

👉 When Slave receives correct data → LED toggles

---

## Result

When running the program:

* Master:

  * Press button → send data

* Slave:

  * Receive data → LED toggles

---

## Notes

* Requires **2 boards**
* Master and Slave must match:

  * SPI mode
  * bit order
* Correct wiring:

  * MOSI ↔ MOSI
  * MISO ↔ MISO
  * SCK ↔ SCK
  * Common GND

---

## Further Exploration

You can try:

* Add response from Slave → Master
* Send more complex data
* Use interrupt instead of polling
* Test different SPI modes

---

## Related

```text id="related_spi"
docs/vi/apis/spi.md
tests/demo/spi_full_demo.c
```