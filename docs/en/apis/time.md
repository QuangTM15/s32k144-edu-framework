# Time API

## Overview

The Time API is used to create delays and measure program runtime in EduFramework.

You can use it to:

* Create delays in milliseconds
* Read system uptime
* Write programs without configuring timers manually
* Build time-based logic

The Time API is implemented using hardware timers, so it provides stable and accurate timing compared to software delay loops.

---

## Function List

| Function   | Description                                       |
| ---------- | ------------------------------------------------- |
| `millis()` | Returns system uptime in milliseconds             |
| `delay()`  | Pauses the program for a specified amount of time |

---

## millis()

### Purpose

`millis()` returns the number of milliseconds since the time system was initialized.

This function is commonly used to:

* Measure elapsed time
* Check if a certain duration has passed
* Write non-blocking programs

### Syntax

```c
uint32_t currentTime = millis();
```

### Return Value

| Value      | Description          |
| ---------- | -------------------- |
| `uint32_t` | Time in milliseconds |

### Example

```c
uint32_t start = millis();

while ((millis() - start) < 1000)
{
    // wait for 1000 ms
}
```

### Notes

* The unit of `millis()` is milliseconds.
* The returned value increases continuously over time.
* `millis()` is suitable for writing programs that require timing without blocking.
* When comparing time, use `millis() - start` instead of comparing absolute values.

---

## delay()

### Purpose

`delay()` pauses the program for a specified duration.

This function is useful for simple timing operations.

### Syntax

```c
delay(ms);
```

### Parameters

| Parameter | Description                |
| --------- | -------------------------- |
| `ms`      | Delay time in milliseconds |

### Example

```c
delay(500);
```

This will pause the program for approximately 500 ms.

### Notes

* `delay()` is a blocking function.
* During the delay, the program will stop executing other tasks.
* It is not recommended to use `delay()` in programs that require multitasking.
* For simple examples like blinking an LED, `delay()` is easy and effective.
* For more advanced applications, use `millis()` for flexible timing control.

---

## Timing Accuracy

The Time API uses a hardware timer inside the framework.

In the current implementation:

* The timer tick is configured to 1 ms
* Each tick increments the system time counter
* `millis()` reads this counter
* `delay()` waits based on `millis()`

Because it uses a hardware timer, the timing is much more stable than CPU-based delay loops.

---

## Tutorials

You can learn how to use the Time API through the blink LED tutorial.

##