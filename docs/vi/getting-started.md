# EduFramework – Getting Started

## EduFramework là gì?

EduFramework là một dự án cá nhân được xây dựng với mục tiêu tạo ra một framework embedded đơn giản, dễ tiếp cận cho vi điều khiển S32K144 (MaaZEDU).

Dự án này hướng tới:

* Người mới bắt đầu học embedded
* Sinh viên cần một nền tảng dễ hiểu để thực hành
* Người đã có nền tảng muốn đọc code để học cách xây dựng driver từ thanh ghi

EduFramework không thay thế hoàn toàn SDK của hãng, mà đóng vai trò như một lớp trung gian giúp việc học và sử dụng embedded trở nên dễ tiếp cận hơn.

---

## Giới thiệu

Trong quá trình học embedded, với vi điều khiển S32K144, một trong những khó khăn lớn nhất đối với người mới là:

* SDK của hãng phức tạp
* Cấu hình nhiều bước khó hiểu
* Phải tiếp xúc với quá nhiều khái niệm ngay từ đầu

Điều này khiến nhiều người:

* Mất rất nhiều thời gian để bắt đầu
* Khó tiếp cận bản chất của hệ thống
* Dễ nản khi mới học

---

## Mục tiêu của EduFramework

EduFramework được xây dựng với mục tiêu:

* Giúp người mới bắt đầu nhanh với embedded
* Cho phép viết code dễ dàng như Arduino
* Đồng thời vẫn giữ khả năng học sâu từ thanh ghi (register-level)

Nói cách khác:

> Bạn có thể dùng ngay, hoặc học sâu — tùy theo nhu cầu.

---

## Cấu trúc dự án

EduFramework nằm trong một project duy nhất, nhưng bao gồm 2 phần chính:

---

### 1. Library (dùng trực tiếp)

Thư viện dùng để viết chương trình:

* `eduframework/include/` → header files
* `eduframework/lib/libeduframework.a` → static library

Người dùng chỉ cần:

* include thư viện
* gọi API

Ví dụ:

```c
pinMode(LED_RED, OUTPUT);
digitalWrite(LED_RED, LOW);
```

---

### 2. Source code (để học sâu)

Toàn bộ framework được viết từ mức thanh ghi:

* `firmware/drivers/` → driver
* `firmware/arduino/` → Arduino-style API

Nếu bạn muốn:

* hiểu cách driver hoạt động
* học cách viết embedded từ đầu

→ đọc trực tiếp source code

---

Lưu ý:

Tài liệu này tập trung vào cách sử dụng thư viện.
Nếu bạn muốn hiểu sâu, hãy đọc code.

---

## Bắt đầu với chương trình đầu tiên (Blink LED)

Mục tiêu:

Tạo project và làm LED nháy

---

## Yêu cầu

* S32 Design Studio (S32DS)
* Board: MaaZEDU S32K144

---

## Bước 1: Tạo project mới

1. Mở S32 Design Studio
2. Chọn:

```
File → New → S32DS Application Project
```

3. Cấu hình:

* Device: S32K144
* Core: Cortex-M4F
* Toolchain: NXP GCC 10.2

4. Finish

---

## Bước 2: Cấu hình EduFramework

Không cần copy thư viện
Sử dụng đường dẫn trực tiếp (absolute path)

---

### 2.1 Add include path

Vào:

```
Project Properties → C/C++ Build → Settings → Includes
```

Thêm đường dẫn tới thư mục include của thư viện:

```text
D:\S32K144_MaaZEDU_Framework\eduframework\include
```

---

### 2.2 Add library path

Vào:

```
Project Properties → C/C++ Build → Settings → Linker → Libraries
```

Thêm đường dẫn tới thư mục lib của thư viện:

```text
D:\S32K144_MaaZEDU_Framework\eduframework\lib
```

---

### 2.3 Link thư viện

Trong mục Libraries (-l), thêm:

```text
eduframework
```

---

## Bước 3: Viết chương trình đầu tiên

Mở `main.c`:

```c
#include "Arduino.h"
#include "S32K144.h"

int main(void)
{
    setup();
    pinMode(LED_RED, OUTPUT);

    while (1)
    {
        digitalWrite(LED_RED, LOW);
        delay(500);
        digitalWrite(LED_RED, HIGH);
        delay(500);
    }
}
```

---

## Bước 4: Build & Run

1. Build project
2. Nạp project bằng JLink
3. Flash xuống board
4. Quan sát:

LED sẽ nháy mỗi 500ms

---

## Kết quả

Bạn đã:

* Import thành công một embedded library
* Viết chương trình đầu tiên
* Không cần cấu hình phức tạp

---

## Bước tiếp theo

Sau khi hoàn thành:

### Hiểu mapping phần cứng

Đọc:

```
docs/vi/pin-mapping.md
```

### Hiểu các API của thư viện

Đọc:

```
docs/vi/api/
```

### Học cách dùng API trong thực tế

Đọc:

```
docs/vi/tutorials/
```

---

## Tổng kết

EduFramework được thiết kế để:

* Giảm rào cản khi bắt đầu embedded
* Giúp việc học trở nên dễ tiếp cận hơn
* Đồng thời không đánh mất khả năng hiểu sâu hệ thống

Bạn có thể bắt đầu đơn giản, và tiến dần tới chuyên sâu — trong cùng một framework.
