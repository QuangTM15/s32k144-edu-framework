# UART LED Control

## Mục tiêu

Trong bài này, bạn sẽ học cách:

* Nhận lệnh đơn giản từ UART
* Điều khiển LED bằng ký tự nhận được
* Kết hợp Serial API với Digital API
* In trạng thái LED ra terminal

---

## Kiến thức cần biết

### Ý tưởng điều khiển bằng UART

UART không chỉ dùng để debug hoặc echo dữ liệu.

Bạn có thể dùng UART để gửi lệnh từ máy tính xuống board.

Ví dụ:

| Lệnh gửi     | Hành động        |
| ------------ | ---------------- |
| `r` hoặc `R` | Toggle LED_RED   |
| `g` hoặc `G` | Toggle LED_GREEN |
| `b` hoặc `B` | Toggle LED_BLUE  |

---

### Vì sao dùng ký tự đơn?

Trong bài này, mỗi lệnh chỉ là một ký tự.

Cách này đơn giản và phù hợp cho beginner vì:

* Không cần xử lý chuỗi dài
* Không cần parser phức tạp
* Dễ test bằng Hercules
* Dễ mở rộng sau này

---

### Kết hợp UART và GPIO

Chương trình sử dụng:

| API                      | Vai trò              |
| ------------------------ | -------------------- |
| `Serial1_available()`    | Kiểm tra có lệnh mới |
| `Serial1_read()`         | Đọc ký tự lệnh       |
| `digitalToggle()`        | Đảo trạng thái LED   |
| `Serial1_printlnFloat()` | In trạng thái LED    |

---

## Code

```c
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

## Giải thích

### Khởi tạo LED

```c
pinMode(LED_RED, OUTPUT);
pinMode(LED_GREEN, OUTPUT);
pinMode(LED_BLUE, OUTPUT);
```

* Cấu hình 3 LED là output
* Cho phép điều khiển LED bằng `digitalWrite()` hoặc `digitalToggle()`

---

### Tắt LED ban đầu

```c
digitalWrite(LED_RED, HIGH);
digitalWrite(LED_GREEN, HIGH);
digitalWrite(LED_BLUE, HIGH);
```

LED trên board là **active LOW**:

| Mức ghi | Trạng thái LED |
| ------- | -------------- |
| LOW     | Bật            |
| HIGH    | Tắt            |

Vì vậy, ghi `HIGH` để tắt LED ban đầu.

---

### Khởi tạo Serial1

```c
Serial1_begin(9600);
```

* Khởi tạo UART debug
* Baudrate khuyến nghị: `9600`
* Dùng Hercules để gửi lệnh từ máy tính xuống board

---

### Kiểm tra lệnh mới

```c
if (Serial1_available())
```

* Chỉ đọc khi có dữ liệu mới
* Tránh đọc sai khi UART chưa nhận gì

---

### Đọc ký tự lệnh

```c
ch = Serial1_read();
```

* Đọc một ký tự từ UART
* Ví dụ: `r`, `g`, `b`

---

### Xử lý lệnh RED

```c
if ((ch == 'r') || (ch == 'R'))
```

Chấp nhận cả chữ thường và chữ hoa.

```c
digitalToggle(LED_RED);
```

Đảo trạng thái LED đỏ.

```c
redState = (redState == 0.0f) ? 1.0f : 0.0f;
```

Cập nhật biến trạng thái để in ra terminal.

---

### In trạng thái

```c
Serial1_print("RED: ");
Serial1_printlnFloat(redState);
```

Sau mỗi lần nhận lệnh, board sẽ gửi lại trạng thái LED.

Ví dụ:

```text
RED: 1.0
RED: 0.0
```

---

## Kết quả

Khi chạy chương trình và mở Hercules ở baudrate `9600`:

| Gửi lệnh | Kết quả                  |
| -------- | ------------------------ |
| `r`      | LED_RED đổi trạng thái   |
| `g`      | LED_GREEN đổi trạng thái |
| `b`      | LED_BLUE đổi trạng thái  |

Terminal sẽ in lại trạng thái LED sau mỗi lệnh.

---

## Lưu ý

* Nên dùng baudrate `9600`
* Hercules phải cấu hình đúng baudrate
* Lệnh trong bài này chỉ gồm một ký tự
* Có thể gửi chữ thường hoặc chữ hoa
* LED trên board là active LOW
* Biến `redState`, `greenState`, `blueState` chỉ dùng để hiển thị trạng thái ra UART

---

## Liên quan

Xem thêm:

```text
docs/vi/apis/serial.md
docs/vi/apis/digital.md
```

để hiểu rõ các hàm đã sử dụng trong bài này.