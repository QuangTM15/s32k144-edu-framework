# Analog Potentiometer Control

## Mục tiêu

Trong bài này, bạn sẽ học cách:

* Đọc giá trị từ biến trở bằng ADC
* Chuyển đổi giá trị ADC sang PWM
* Điều khiển độ sáng LED theo tín hiệu analog
* Hiểu mối liên hệ giữa analog input và output

---

## Giới thiệu

Biến trở là một thiết bị analog phổ biến.

Khi bạn xoay biến trở:

* Điện áp thay đổi liên tục
* ADC sẽ đọc giá trị tương ứng

Sau đó, bạn có thể dùng giá trị này để điều khiển LED:

> Xoay biến trở → LED sáng/tối theo

---

## Kiến thức cần biết

### ADC (Analog to Digital)

* Độ phân giải: 12-bit
* Giá trị: từ `0` đến `4095`

---

### PWM (Analog giả lập)

* Độ phân giải: 8-bit
* Giá trị: từ `0` đến `255`

PWM không tạo điện áp thật, mà thay đổi độ rộng xung để tạo hiệu ứng analog.

---

### Chuyển đổi giá trị

Bạn cần scale từ ADC sang PWM:

```c
pwmValue = (adcValue * 255) / 4095;
```

---

## Nguyên lý hoạt động

```text
Biến trở → ADC → scale → PWM → LED
```

---

## Code

```c
#include "Arduino.h"
#include "analog_full_demo.h"

void Demo_AnalogFull(void)
{
    int adcValue;
    uint8_t pwmValue;

    setup();

    while (1)
    {
        adcValue = analogRead(ADC0_SE12);

        if (adcValue < 0)
        {
            continue;
        }

        /* Convert ADC 12-bit (0..4095) to PWM 8-bit (0..255) */
        pwmValue = (uint8_t)(((uint32_t)adcValue * 255U) / 4095U);

        /* LED brightness follows potentiometer */
        analogWrite(LED_RED, pwmValue);
    }
}
```

---

## Giải thích

### Đọc ADC

```c
adcValue = analogRead(ADC0_SE12);
```

* Đọc giá trị từ biến trở
* Giá trị nằm trong khoảng 0 → 4095

---

### Chuyển đổi giá trị

```c
pwmValue = (adcValue * 255) / 4095;
```

* Scale từ 12-bit → 8-bit
* Phù hợp với PWM

---

### Điều khiển LED

```c
analogWrite(LED_RED, pwmValue);
```

* Giá trị PWM quyết định độ sáng LED
* Giá trị càng lớn → LED càng sáng

---

## Kết quả

Khi chạy chương trình:

* Xoay biến trở sang một phía → LED sáng dần
* Xoay ngược lại → LED tối dần

---

## Lưu ý

* LED trên board là **active LOW**
* ADC có thể có nhiễu nhỏ
* Không cần dùng `delay()` trong bài này
* Giá trị ADC càng lớn → PWM càng lớn

---

## Gợi ý mở rộng

Bạn có thể thử:

* In giá trị ADC ra Serial
* Điều khiển nhiều LED
* Dùng non-blocking ADC
* Thêm smoothing (lọc nhiễu)

---

## Liên quan

Xem thêm:

```text
docs/vi/apis/analog.md
docs/vi/apis/time.md

Hoặc cái ví dụ khác có thể đọc source code trong folder tests/demo
```
