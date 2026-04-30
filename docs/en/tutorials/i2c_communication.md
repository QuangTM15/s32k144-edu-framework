# I2C Communication (Master – Slave)

## Objective

In this tutorial, you will learn how to:

- Understand how I2C works
- Set up communication between two boards
- Send data from Master to Slave
- Receive and process data on Slave
- Use callback for Slave data handling

---

## Introduction

I2C is a widely used communication protocol in embedded systems.

In this tutorial, you will:

- Connect **2 S32K144 boards**
- One board acts as **Master**
- One board acts as **Slave**

> Press button → Master sends command → Slave receives → LED changes

---

## Required Knowledge

### I2C (Inter-Integrated Circuit)

- Synchronous communication
- Uses only 2 wires
- Supports multiple devices on the same bus
- Master selects Slave by address

---

### I2C Pins

- SDA – Serial Data
- SCL – Serial Clock

---

### Master vs Slave

#### Master

- Initiates communication
- Generates clock
- Sends data to Slave
- Selects Slave using address

#### Slave

- Has its own address
- Waits for Master
- Receives and processes data

---

## Working Principle

```text
Button (Master) → send 0xA5 via I2C → Slave receives → LED_RED toggles
```

---

## Code

```c
#define I2C_DEMO_IS_MASTER    0

#define I2C_SLAVE_ADDR        0x12U
#define I2C_CMD_TOGGLE        0xA5U
```

---

## Explanation

### Select Role

```c
#define I2C_DEMO_IS_MASTER 0
```

- `1` → Master
- `0` → Slave

👉 You need **2 boards**:

- One board set to `1`
- One board set to `0`

---

### Slave Address

```c
#define I2C_SLAVE_ADDR 0x12U
```

This is the address of the Slave on the I2C bus.

The Master uses this address to send data to the correct Slave.

---

### Command

```c
#define I2C_CMD_TOGGLE 0xA5U
```

This is the command sent by the Master.

In this demo:

- If Slave receives `0xA5`
- It toggles `LED_RED`

---

## I2C Initialization

```c
static void Demo_InitI2C(void)
{
#if I2C_DEMO_IS_MASTER
    Wire_begin();
#else
    Wire_beginAddress(I2C_SLAVE_ADDR);
#endif
}
```

### Explanation

If the board is Master:

```c
Wire_begin();
```

- Initialize I2C in Master mode
- Master controls communication

If the board is Slave:

```c
Wire_beginAddress(I2C_SLAVE_ADDR);
```

- Initialize I2C in Slave mode
- Assign address `0x12`
- Master will communicate using this address

---

## Master – Sending Data

```c
Wire_beginTransmission(I2C_SLAVE_ADDR);
Wire_write(I2C_CMD_TOGGLE);
Wire_endTransmission();

digitalToggle(LED_GREEN);
```

### Explanation

```c
Wire_beginTransmission(I2C_SLAVE_ADDR);
```

- Start transmission to Slave address `0x12`

---

```c
Wire_write(I2C_CMD_TOGGLE);
```

- Write command `0xA5` to TX buffer

---

```c
Wire_endTransmission();
```

- End transmission
- Data is actually sent at this step

---

```c
digitalToggle(LED_GREEN);
```

- Toggle LED on Master to indicate data sent

---

## Trigger with Button

```c
if (ButtonPressed(BTN0) == 0U)
{
    return;
}
```

### Explanation

- Master sends data only when `BTN0` is pressed
- Otherwise, it does nothing

---

## Slave – Register Callback

```c
Wire_onReceive(Demo_OnReceive);
```

### Explanation

- Slave does not need polling
- When Master sends data to this Slave
- `Demo_OnReceive()` is automatically called

---

## Slave – Receiving Data

```c
static void Demo_OnReceive(int len)
{
    uint8_t data;

    if (len <= 0)
    {
        return;
    }

    data = (uint8_t)Wire_read();

    if (data == I2C_CMD_TOGGLE)
    {
        digitalToggle(LED_RED);
    }
}
```

### Explanation

```c
if (len <= 0)
{
    return;
}
```

- Check if any data was received

---

```c
data = (uint8_t)Wire_read();
```

- Read 1 byte from Master

---

```c
if (data == I2C_CMD_TOGGLE)
{
    digitalToggle(LED_RED);
}
```

- If received `0xA5`
- Toggle LED on Slave

---

## Main Loop

```c
while (1)
{
#if I2C_DEMO_IS_MASTER
    Demo_MasterHandleButton();
#else
    /* Slave runs by interrupt callback */
#endif
}
```

### Explanation

Master:

- Continuously checks button
- Sends data when pressed

Slave:

- No need to do anything in loop
- Works via callback

---

## Result

When running:

- Master:
  - Press `BTN0`
  - Sends `0xA5`
  - `LED_GREEN` toggles

- Slave:
  - Receives `0xA5`
  - `LED_RED` toggles

---

## Notes

- Requires **2 boards**
- One must be Master, one must be Slave
- Must use the same Slave address
- Common GND is required
- Proper wiring:
  - SDA ↔ SDA
  - SCL ↔ SCL

- I2C requires pull-up resistors on SDA and SCL

---

## Further Exploration

You can try:

- Send multiple bytes instead of one
- Define multiple commands
- Let Slave send data back to Master
- Use I2C LCD to display data
- Try different speeds (100 kHz, 400 kHz)

---

## Related

```text
docs/vi/apis/i2c.md
tests/demo/i2c_demo.c
```