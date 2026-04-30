# Blink LED

## Mục tiêu

Trong bài này, bạn sẽ học cách:

* Sử dụng `delay()` để tạo thời gian chờ
* Sử dụng `millis()` để điều khiển theo thời gian
* So sánh hai cách xử lý thời gian
* Điều khiển nhiều LED với chu kỳ khác nhau

---

## Kiến thức cần biết

### delay() – cách đơn giản

`delay()` dừng chương trình trong một khoảng thời gian.

Ví dụ:

```c
delay(500);
```

* Chương trình sẽ dừng 500 ms
* Dễ hiểu, dễ dùng
* Nhưng là **blocking** (chặn chương trình)

---

### millis() – cách linh hoạt

`millis()` trả về thời gian đã trôi qua (ms).

Ví dụ:

```c
if ((millis() - lastTime) >= 500)
```

* Không chặn chương trình
* Có thể chạy nhiều tác vụ song song
* Phù hợp với chương trình phức tạp hơn

---

### So sánh hai cách

| Cách       | Ưu điểm   | Nhược điểm        |
| ---------- | --------- | ----------------- |
| `delay()`  | Dễ dùng   | Chặn chương trình |
| `millis()` | Linh hoạt | Code phức tạp hơn |

---

## Ý tưởng bài toán

Điều khiển 3 LED với chu kỳ khác nhau:

| LED   | Chu kỳ  | Cách thực hiện  |
| ----- | ------- | --------------- |
| RED   | 500 ms  | Dùng `delay()`  |
| GREEN | 700 ms  | Dùng `millis()` |
| BLUE  | 1100 ms | Dùng `millis()` |

---

## Code

```c
#include "Arduino.h"

int main(void)
{
    uint32_t lastGreen = 0U;
    uint32_t lastBlue  = 0U;
    uint8_t redTick = 0U;

    setup();

    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);

    /* All OFF initially
       Note: board LED is active-low */
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, HIGH);

    while (1)
    {
        /* Base tick using delay(): 100 ms */
        delay(100);
        redTick++;

        /* RED LED: 500 ms using delay-based tick */
        if (redTick >= 5U)
        {
            redTick = 0U;
            digitalToggle(LED_RED);
        }

        /* GREEN LED: 700 ms using millis() */
        if ((millis() - lastGreen) >= 700U)
        {
            lastGreen = millis();
            digitalToggle(LED_GREEN);
        }

        /* BLUE LED: 1100 ms using millis() */
        if ((millis() - lastBlue) >= 1100U)
        {
            lastBlue = millis();
            digitalToggle(LED_BLUE);
        }
    }
}
```

---

## Giải thích

### Khởi tạo LED

* Tất cả LED được cấu hình là `OUTPUT`
* Ghi `HIGH` để tắt LED (do active LOW)

---

### LED đỏ – dùng delay()

```c
delay(100);
redTick++;
```

* Mỗi vòng lặp = 100 ms
* Sau 5 lần → 500 ms

```c
if (redTick >= 5U)
```

* Toggle LED mỗi 500 ms

---

### LED xanh lá – dùng millis()

```c
if ((millis() - lastGreen) >= 700U)
```

* Kiểm tra đã đủ 700 ms chưa
* Nếu đủ → toggle LED

```c
lastGreen = millis();
```

* Cập nhật mốc thời gian mới

---

### LED xanh dương – tương tự

```c
if ((millis() - lastBlue) >= 1100U)
```

* Chu kỳ 1100 ms
* Hoạt động độc lập với các LED khác

---

## Điểm quan trọng

* `delay()` làm chậm toàn bộ chương trình
* `millis()` không chặn chương trình
* Có thể dùng `millis()` để chạy nhiều tác vụ cùng lúc
* Đây là bước chuyển từ beginner sang intermediate

---

## Kết quả

* LED đỏ nháy mỗi 500 ms
* LED xanh lá nháy mỗi 700 ms
* LED xanh dương nháy mỗi 1100 ms

Tất cả hoạt động đồng thời trong cùng một vòng lặp

---

## Liên quan

Xem thêm:

```text
docs/vi/apis/time.md
docs/vi/apis/digital.md
```

để hiểu rõ các hàm đã sử dụng trong bài này.
