# Pin Mapping

## Mục đích

Tài liệu này cung cấp danh sách các tên chân logic được sử dụng trong EduFramework.

Khi lập trình, bạn không cần quan tâm đến chân vật lý của vi điều khiển.
Thay vào đó, bạn sử dụng các tên đã được định nghĩa sẵn trong framework.

---

## Cách sử dụng

EduFramework ánh xạ các chân trên board thành các tên dễ hiểu và dễ sử dụng.

Ví dụ:

* Muốn điều khiển LED đỏ → dùng `LED_RED`
* Muốn đọc nút nhấn → dùng `BTN0`, `BTN1`
* Muốn dùng chân mở rộng → dùng `GPIOx`

---

## Danh sách chân

### GPIO Header

Các chân GPIO mở rộng dùng để kết nối thiết bị bên ngoài như LED, button, cảm biến.

| Tên chân | Mô tả          |
| -------- | -------------- |
| `GPIO0`  | GPIO mở rộng 0 |
| `GPIO1`  | GPIO mở rộng 1 |
| `GPIO2`  | GPIO mở rộng 2 |
| `GPIO3`  | GPIO mở rộng 3 |
| `GPIO4`  | GPIO mở rộng 4 |
| `GPIO5`  | GPIO mở rộng 5 |
| `GPIO6`  | GPIO mở rộng 6 |
| `GPIO7`  | GPIO mở rộng 7 |
| `GPIO8`  | GPIO mở rộng 8 |
| `GPIO9`  | GPIO mở rộng 9 |

---

### On-board LEDs

Các LED có sẵn trên board.

| Tên chân    | Mô tả          |
| ----------- | -------------- |
| `LED_RED`   | LED đỏ         |
| `LED_BLUE`  | LED xanh dương |
| `LED_GREEN` | LED xanh lá    |

Lưu ý:

Các LED hoạt động theo kiểu **active LOW**:

* `LOW` → bật LED
* `HIGH` → tắt LED

---

### Buttons

Các nút nhấn trên board.

| Tên chân | Mô tả      |
| -------- | ---------- |
| `BTN0`   | Nút nhấn 0 |
| `BTN1`   | Nút nhấn 1 |

---

### SPI

Các chân dùng cho giao tiếp SPI.

| Tên chân   | Mô tả       |
| ---------- | ----------- |
| `SPI_SCK`  | Clock       |
| `SPI_SIN`  | Data input  |
| `SPI_SOUT` | Data output |
| `SPI_PCS0` | Chip select |

---

### I2C

Các chân dùng cho giao tiếp I2C.

| Tên chân  | Mô tả |
| --------- | ----- |
| `I2C_SCL` | Clock |
| `I2C_SDA` | Data  |

---

### ADC

Các chân dùng để đọc tín hiệu analog.

| Tên chân    | Mô tả       |
| ----------- | ----------- |
| `ADC0_SE12` | Kênh ADC 12 |
| `ADC0_SE13` | Kênh ADC 13 |

---

## Ghi chú

* Các tên chân này được định nghĩa trong `Arduino_pins.h`
* Người dùng chỉ cần sử dụng các tên này khi gọi API
* Mọi cấu hình phần cứng đã được xử lý bên trong framework

##
