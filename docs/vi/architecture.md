# Kiến trúc EduFramework

## Tổng quan

EduFramework là một embedded framework dành cho **S32K144 (MaaZEDU)**.

Mục tiêu của project là:

* Không phụ thuộc vào vendor SDK
* Viết driver ở mức thanh ghi
* Cung cấp API dễ dùng theo phong cách Arduino
* Phục vụ học tập, thực hành và giảng dạy embedded systems

---

## Triết lý thiết kế

EduFramework được thiết kế theo hướng phân lớp rõ ràng:

```text
Application / Demo
        ↓
Arduino-style API
        ↓
Driver
        ↓
Register / Hardware
```

Ý tưởng chính:

* Người mới có thể dùng API đơn giản giống Arduino
* Người muốn học sâu có thể đọc xuống driver
* Driver chỉ làm việc với phần cứng
* Arduino API che giấu phần cứng và cung cấp hàm dễ dùng

---

## Các thư mục chính

```text
docs/
eduframework/
firmware/
s32ds/
tests/
```

---

## docs/

`docs/` là nơi chứa tài liệu của project.

Thư mục này gồm 2 phần:

```text
docs/
├── vi/
└── en/
```

| Thư mục | Mục đích            |
| ------- | ------------------- |
| `vi/`   | Tài liệu tiếng Việt |
| `en/`   | Tài liệu tiếng Anh  |

Hai bản tài liệu có cùng nội dung, chỉ khác ngôn ngữ.

---

## docs/apis/

`apis/` chứa tài liệu hướng dẫn sử dụng các API Arduino-style.

Ví dụ:

```text
docs/vi/apis/
├── digital.md
├── time.md
├── serial.md
├── analog.md
├── spi.md
└── i2c.md
```

Các file này dành cho người muốn **dùng framework** mà không cần hiểu chi tiết driver bên dưới.

---

## docs/tutorials/

`tutorials/` chứa các bài hướng dẫn thực hành.

Ví dụ:

```text
docs/vi/tutorials/
├── blink_led.md
├── button_control_led.md
├── uart_echo.md
├── analog-potentiometer-led.md
├── spi_communication.md
└── i2c_communication.md
```

Mỗi tutorial giải thích:

* Mục tiêu bài học
* Kiến thức cần biết
* Nguyên lý hoạt động
* Code mẫu
* Giải thích từng phần code
* Kết quả mong đợi

---

## eduframework/

`eduframework/` là thư mục thư viện thật dùng để import vào project khác.

```text
eduframework/
├── include/
└── lib/
```

### include/

`include/` chứa các header file public.

Người dùng include các file này để gọi API của framework.

Ví dụ:

```c
#include "Arduino.h"
```

### lib/

`lib/` chứa thư viện đã build sẵn.

```text
libeduframework.a
```

File `.a` này được import vào IDE hoặc project khác để người dùng có thể sử dụng EduFramework mà không cần build lại toàn bộ source code.

---

## firmware/

`firmware/` là source code chính của framework.

Đây là nơi phát triển logic thật của EduFramework.

```text
firmware/
├── arduino/
├── drivers/
└── board/
```

---

## firmware/arduino/

`firmware/arduino/` chứa các API theo phong cách Arduino.

Ví dụ:

```text
firmware/arduino/
├── inc/
└── src/
```

Các API ở tầng này gồm:

* `pinMode()`
* `digitalWrite()`
* `digitalRead()`
* `delay()`
* `millis()`
* `Serial`
* `analogRead()`
* `analogWrite()`
* `SPI_*`
* `Wire_*`

Tầng Arduino-style API có nhiệm vụ:

* Cung cấp hàm dễ dùng
* Che giấu chi tiết phần cứng
* Gọi xuống driver khi cần
* Giúp người mới tiếp cận S32K144 dễ hơn

Ví dụ:

```c
digitalWrite(LED_RED, LOW);
```

Người dùng không cần biết LED nằm ở port nào, pin nào, hay phải ghi thanh ghi nào.

---

## firmware/drivers/

`firmware/drivers/` chứa các driver cấp thấp.

Driver làm việc trực tiếp với thanh ghi của S32K144.

Ví dụ:

```text
firmware/drivers/
├── gpio/
├── port/
├── clock/
├── lpit/
├── lpuart/
├── adc/
├── ftm/
├── lpspi/
├── lpi2c/
└── irq/
```

Tầng driver có nhiệm vụ:

* Cấu hình peripheral
* Ghi và đọc thanh ghi
* Cung cấp API cấp thấp cho tầng Arduino sử dụng
* Không chứa logic thân thiện với người dùng cuối

Nguyên tắc:

> Driver chỉ điều khiển phần cứng.
> Arduino API mới là nơi tạo giao diện dễ dùng.

---

## firmware/board/

`firmware/board/` chứa thông tin liên quan đến board.

Phần này dùng để mô tả mapping giữa tên dễ hiểu và phần cứng thật.

Ví dụ:

* `LED_RED`
* `LED_BLUE`
* `LED_GREEN`
* `BTN0`
* `BTN1`
* Các chân GPIO
* Các chân ADC, PWM, SPI, I2C

Nhờ tầng board mapping, người dùng có thể viết:

```c
pinMode(LED_RED, OUTPUT);
```

thay vì phải nhớ LED_RED là port nào, pin nào.

---

## s32ds/

`s32ds/` là project dùng trong **S32 Design Studio**.

Thư mục này dùng để:

* Test driver
* Test Arduino API
* Test demo
* Build project
* Nạp code vào chip thật

Đây là project thực tế để kiểm tra EduFramework trên phần cứng S32K144.

---

## tests/

`tests/` chứa các chương trình test và demo.

```text
tests/
├── demo/
└── example/
```

---

## tests/demo/

`tests/demo/` chứa các demo nhỏ cho từng phần của framework.

Ví dụ:

* Blink LED
* Button control LED
* UART echo
* Analog read
* PWM fade LED
* SPI communication
* I2C communication

Các demo này dùng để:

* Kiểm tra API đã hoạt động đúng chưa
* Làm ví dụ học tập
* Làm tài liệu tham khảo cho tutorials

---

## tests/example/

`tests/example/` dành cho các ví dụ lớn hơn trong tương lai.

Ở version 2, thư mục này sẽ chứa các mini project sử dụng cảm biến và module ngoài.

Ví dụ tương lai:

* LCD I2C display
* Ultrasonic distance measurement
* RFID reader
* Bluetooth communication
* Light sensor application
* Sensor-based mini projects

---

## HAL trong tương lai

Trong tương lai, EduFramework có thể có thêm tầng `hal/`.

Tầng này sẽ dùng để xây dựng thư viện cho cảm biến và module ngoài.

Kiến trúc dự kiến:

```text
Application / Mini Project
        ↓
HAL / Sensor Library
        ↓
Arduino-style API
        ↓
Driver
        ↓
Register / Hardware
```

---

## Vai trò của HAL

HAL sẽ bọc các API Arduino-style để tạo ra các hàm dễ dùng hơn cho từng module cụ thể.

Ví dụ:

```c
distance = Ultrasonic_readDistance();
```

hoặc:

```c
LCD_print("Hello");
```

Người dùng không cần biết phía dưới dùng GPIO, Timer, UART, SPI hay I2C.

Trong một số trường hợp cần tối ưu hoặc cần tính năng đặc biệt, HAL cũng có thể gọi trực tiếp xuống driver.

---

## Tóm tắt vai trò các layer

| Layer              | Vai trò                                  |
| ------------------ | ---------------------------------------- |
| Application / Demo | Code người dùng hoặc code demo           |
| HAL                | Thư viện cảm biến/module trong tương lai |
| Arduino-style API  | API dễ dùng, che giấu phần cứng          |
| Driver             | Điều khiển peripheral bằng thanh ghi     |
| Hardware           | S32K144 và board MaaZEDU                 |

---

##