# SPI Communication (Master – Slave)

## Mục tiêu

Trong bài này, bạn sẽ học cách:

* Hiểu cách hoạt động của SPI
* Thiết lập giao tiếp giữa 2 board
* Gửi dữ liệu từ Master
* Nhận và xử lý dữ liệu ở Slave
* Sử dụng các API SPI cơ bản

---

## Giới thiệu

SPI là một giao thức giao tiếp tốc độ cao giữa các thiết bị.

Trong bài này, bạn sẽ:

* Kết nối **2 board S32K144**
* 1 board đóng vai trò **Master**
* 1 board đóng vai trò **Slave**

> Nhấn nút → Master gửi dữ liệu → Slave nhận → LED thay đổi

---

## Kiến thức cần biết

### SPI (Serial Peripheral Interface)

* Giao tiếp đồng bộ (có clock)
* Full-duplex (gửi và nhận cùng lúc)
* Tốc độ cao

---

### Các chân SPI

* SCK – Clock
* MOSI – Master → Slave
* MISO – Slave → Master
* CS – Chọn Slave

---

### Master vs Slave

#### Master

* Tạo clock
* Chủ động gửi dữ liệu
* Điều khiển toàn bộ giao tiếp

#### Slave

* Chờ Master
* Nhận dữ liệu
* Xử lý dữ liệu

---

### SPI Mode

Trong bài này sử dụng:

```c id="g2q7n1"
SPI_MODE0
```

---

## Nguyên lý hoạt động

```text id="yhh3wl"
Nhấn nút (Master) → SPI → Slave → LED thay đổi
```

---

## Code

```c id="demo_spi_full"
#define SPI_DEMO_IS_MASTER    1

#define SPI_DEMO_CASE_8BIT    0
#define SPI_DEMO_CASE_16BIT   1
#define SPI_DEMO_CASE_BUFFER  2

#define SPI_DEMO_CASE         SPI_DEMO_CASE_BUFFER
```

---

## Giải thích

### Chọn vai trò

```c id="role_select"
#define SPI_DEMO_IS_MASTER 1
```

* `1` → Master
* `0` → Slave

👉 Bạn cần chạy **2 board**:

* 1 board set = 1
* 1 board set = 0

---

### Chọn kiểu truyền

```c id="case_select"
#define SPI_DEMO_CASE SPI_DEMO_CASE_BUFFER
```

Có 3 kiểu:

* 8-bit → truyền 1 byte
* 16-bit → truyền 2 byte
* Buffer → truyền nhiều byte

---

## Khởi tạo SPI

```c id="init_spi"
SPI_beginEx(SPI_ROLE_MASTER, 1000000U, SPI_MODE0, SPI_MSBFIRST);
```

### Giải thích

* `SPI_ROLE_MASTER` → chọn Master
* `1000000U` → tốc độ 1 MHz
* `SPI_MODE0` → mode SPI
* `SPI_MSBFIRST` → bit cao trước

👉 Slave vẫn gọi `SPI_beginEx` nhưng không cần frequency

---

## Master – Gửi dữ liệu

### Gửi 8-bit

```c id="send_8bit"
SPI_transfer(0xA5);
```

---

### Gửi 16-bit

```c id="send_16bit"
SPI_transfer16(0x55AA);
```

---

### Gửi buffer

```c id="send_buffer"
SPI_transferBuffer(g_testBuf, 0, 3);
```

---

### Trigger bằng button

```c id="button_trigger"
if (ButtonPressed(BTN0))
{
    SPI_transfer(...);
}
```

👉 Master chỉ gửi khi nhấn nút

---

## Slave – Nhận dữ liệu

### Kiểm tra dữ liệu

```c id="check_available"
if (!SPI_available())
{
    return;
}
```

👉 Chỉ đọc khi có dữ liệu

---

### Đọc 8-bit

```c id="read_8bit"
uint8_t data = SPI_read();
```

---

### Đọc 16-bit

```c id="read_16bit"
uint16_t data = SPI_read16();
```

---

### Đọc buffer

```c id="read_buffer"
buf[0] = SPI_read();

for (i = 1; i < 3; i++)
{
    while (!SPI_available());
    buf[i] = SPI_read();
}
```

---

## Xử lý dữ liệu

```c id="process_data"
if (data == 0xA5)
{
    digitalToggle(LED_RED);
}
```

👉 Khi Slave nhận đúng dữ liệu → LED thay đổi

---

## Kết quả

Khi chạy chương trình:

* Master:

  * Nhấn nút → gửi dữ liệu

* Slave:

  * Nhận dữ liệu → LED bật/tắt

---

## Lưu ý

* Cần **2 board** để test
* Master và Slave phải cùng:

  * SPI mode
  * bit order
* Nối dây đúng:

  * MOSI ↔ MOSI
  * MISO ↔ MISO
  * SCK ↔ SCK
  * GND chung

---

## Gợi ý mở rộng

Bạn có thể thử:

* Thêm phản hồi từ Slave → Master
* Gửi nhiều dữ liệu hơn
* Dùng interrupt thay vì polling
* Thử các SPI mode khác

---

## Liên quan

```text id="related_spi"
docs/vi/apis/spi.md
tests/demo/spi_full_demo.c
```