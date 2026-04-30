# Button Control LED

## Mục tiêu

Trong bài này, bạn sẽ học cách:

* Đọc trạng thái nút nhấn
* Hiểu cách dùng `INPUT_PULLUP`
* Phát hiện một lần nhấn nút
* Điều khiển LED theo 2 cách:

  * `digitalToggle()`
  * `digitalWrite()`

---

## Kiến thức cần biết

### Trạng thái của nút nhấn

Nút nhấn là một thiết bị digital có 2 trạng thái:

| Trạng thái | Giá trị                      |
| ---------- | ---------------------------- |
| Chưa nhấn  | HIGH hoặc LOW (tùy cấu hình) |
| Đang nhấn  | Giá trị còn lại              |

---

### Pull-up và Pull-down

Một chân input không nên để “trôi” tự do (floating), vì giá trị đọc được sẽ không ổn định.

EduFramework hỗ trợ 2 chế độ:

| Mode             | Trạng thái mặc định | Khi nhấn |
| ---------------- | ------------------- | -------- |
| `INPUT_PULLUP`   | HIGH                | LOW      |
| `INPUT_PULLDOWN` | LOW                 | HIGH     |

Trong bài này sử dụng:

```c
pinMode(BTN0, INPUT_PULLUP);
```

Điều này có nghĩa:

* Chưa nhấn → HIGH
* Nhấn → LOW

---

### Phát hiện một lần nhấn

Nếu chỉ dùng:

```c
if (digitalRead(BTN0) == LOW)
```

thì khi giữ nút, LED sẽ bị toggle liên tục.

Để tránh điều này, ta cần phát hiện **cạnh xuống (falling edge)**:

| Trạng thái trước | Trạng thái hiện tại | Ý nghĩa           |
| ---------------- | ------------------- | ----------------- |
| HIGH             | LOW                 | Nút vừa được nhấn |

---

### Nhiễu và dội phím

Nút nhấn cơ khí có thể tạo ra nhiễu (bouncing):

* Khi nhấn 1 lần nhưng đọc được nhiều lần HIGH/LOW rất nhanh

Trong framework:

* Đã có **passive filter cơ bản**
* Nhưng với ứng dụng phức tạp, vẫn nên dùng debounce bằng phần mềm

---

## Code

```c
#include "Arduino.h"

int main(void)
{
    setup();

    bool lastBtn0 = true;
    bool lastBtn1 = true;
    bool redState = false;

    pinMode(BTN0, INPUT_PULLUP);
    pinMode(BTN1, INPUT_PULLUP);
    pinMode(LED_BLUE, OUTPUT);
    pinMode(LED_RED, OUTPUT);

    digitalWrite(LED_BLUE, LOW);
    digitalWrite(LED_RED, LOW);

    while (1)
    {
        bool btn0 = digitalRead(BTN0);
        bool btn1 = digitalRead(BTN1);

        if ((lastBtn0 == true) && (btn0 == false))
        {
            digitalToggle(LED_BLUE);
        }

        if ((lastBtn1 == true) && (btn1 == false))
        {
            redState = !redState;
            digitalWrite(LED_RED, redState ? HIGH : LOW);
        }

        lastBtn0 = btn0;
        lastBtn1 = btn1;
    }
}
```

---

## Giải thích

### Cấu hình chân

* `BTN0`, `BTN1` dùng `INPUT_PULLUP`
* `LED_BLUE`, `LED_RED` dùng `OUTPUT`

---

### Đọc button

```c
bool btn0 = digitalRead(BTN0);
```

* `true` → HIGH
* `false` → LOW

---

### Phát hiện nhấn

```c
if ((lastBtn0 == true) && (btn0 == false))
```

Điều kiện này giúp:

* Chỉ xử lý khi nút vừa được nhấn
* Không bị lặp khi giữ nút

---

### Hai cách toggle LED

| Cách              | Code                       | Đặc điểm  |
| ----------------- | -------------------------- | --------- |
| `digitalToggle()` | `digitalToggle(LED_BLUE);` | Ngắn gọn  |
| `digitalWrite()`  | dùng biến `redState`       | Linh hoạt |

---

## Kết quả

* Nhấn `BTN0` → LED_BLUE đổi trạng thái
* Nhấn `BTN1` → LED_RED đổi trạng thái

---

## Liên quan

Xem thêm:

```text
docs/vi/apis/digital.md
```

để hiểu chi tiết các hàm đã sử dụng trong bài này.
