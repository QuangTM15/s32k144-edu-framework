# UART Echo

## Mục tiêu

Trong bài này, bạn sẽ học cách:

* Khởi tạo UART với `Serial1_begin()`
* Đọc dữ liệu từ UART
* Gửi dữ liệu ra UART
* Xử lý chuỗi ký tự đơn giản
* Xây dựng một chương trình echo (nhập gì in lại đó)

---

## Kiến thức cần biết

### UART là gì?

UART là giao tiếp nối tiếp dùng để:

* Gửi dữ liệu giữa vi điều khiển và máy tính
* Debug chương trình
* Giao tiếp với module hoặc cảm biến

Trong EduFramework:

* `Serial1` → dùng để debug qua USB (Hercules)
* `Serial2` → dùng cho UART vật lý TX/RX

---

### Cách hoạt động của bài này

Chương trình sẽ:

1. Đọc từng ký tự từ UART
2. Lưu vào buffer
3. Khi gặp ký tự xuống dòng (`\n`):

   * Kết thúc chuỗi
   * In lại nội dung đã nhập

---

### Vì sao cần buffer?

UART gửi dữ liệu theo từng ký tự.

Để xử lý chuỗi (ví dụ: "hello"), ta cần:

* Lưu từng ký tự vào mảng (`buffer`)
* Khi kết thúc, mới xử lý toàn bộ chuỗi

---

## Code

```c id="z3kq7h"
#include "Arduino.h"

#define RX_BUFFER_SIZE (64U)

int main(void)
{
    char rxBuffer[RX_BUFFER_SIZE];
    uint32_t index = 0U;
    char ch;

    setup();

    Serial1_begin(9600);

    Serial1_print("=== UART1 ECHO DEMO ===\n");
    Serial1_println("Type something and press Enter:");

    while (1)
    {
        if (Serial1_available())
        {
            ch = Serial1_read();

            if (ch == '\r')
            {
                continue;
            }

            if (ch == '\n')
            {
                rxBuffer[index] = '\0';

                Serial1_print("Echo: ");
                Serial1_println(rxBuffer);

                index = 0U;
            }
            else
            {
                if (index < (RX_BUFFER_SIZE - 1U))
                {
                    rxBuffer[index] = ch;
                    index++;
                }
                else
                {
                    /* overflow -> reset */
                    index = 0U;
                }
            }
        }
    }
}
```

---

## Giải thích

### Khởi tạo UART

```c id="1b9rwr"
Serial1_begin(9600);
```

* Khởi tạo UART với baudrate 9600
* Đây là baudrate khuyến nghị

---

### Kiểm tra dữ liệu

```c id="6gqnh6"
if (Serial1_available())
```

* Kiểm tra xem có dữ liệu mới không
* Tránh đọc khi chưa có dữ liệu

---

### Đọc từng ký tự

```c id="1l6qqs"
ch = Serial1_read();
```

* UART nhận từng ký tự một
* Cần đọc và xử lý từng bước

---

### Bỏ ký tự '\r'

```c id="c4s5ns"
if (ch == '\r')
{
    continue;
}
```

* Một số terminal gửi cả `\r\n`
* Bỏ `\r` để tránh xử lý sai

---

### Kết thúc chuỗi

```c id="9dfr0g"
if (ch == '\n')
```

* Khi nhấn Enter → nhận `\n`
* Đây là dấu hiệu kết thúc chuỗi

```c id="9drp0o"
rxBuffer[index] = '\0';
```

* Thêm ký tự kết thúc chuỗi

---

### In lại dữ liệu

```c id="j94r1f"
Serial1_print("Echo: ");
Serial1_println(rxBuffer);
```

* In lại nội dung người dùng nhập

---

### Lưu ký tự vào buffer

```c id="p6u8dn"
rxBuffer[index] = ch;
index++;
```

* Lưu từng ký tự
* Tăng index

---

### Xử lý overflow

```c id="uyr3ns"
if (index < (RX_BUFFER_SIZE - 1U))
```

* Tránh ghi tràn buffer

Nếu quá dài:

```c id="1f9lco"
index = 0U;
```

* Reset buffer

---

## Kết quả

Khi chạy chương trình:

* Gõ dữ liệu trên Hercules
* Nhấn Enter

Kết quả:

```text id="u1lhm1"
Echo: hello
Echo: test
```

---

## Lưu ý

* Baudrate nên để `9600`
* Terminal phải cấu hình đúng baudrate
* UART nhận dữ liệu theo từng ký tự
* Luôn kiểm tra `available()` trước khi đọc
* Cần buffer để xử lý chuỗi
* Tránh overflow khi nhập chuỗi dài

---

## Liên quan

Xem thêm:

```text id="pncl0q"
docs/vi/apis/serial.md
```

để hiểu rõ các hàm đã sử dụng trong bài này.