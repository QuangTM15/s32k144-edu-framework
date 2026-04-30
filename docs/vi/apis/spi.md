# SPI API

## Tổng quan

SPI API dùng để giao tiếp với các thiết bị ngoại vi sử dụng chuẩn SPI trong EduFramework.

Bạn có thể dùng để:

* Gửi và nhận dữ liệu giữa các vi điều khiển
* Giao tiếp với cảm biến, module (RFID, LCD SPI, flash…)
* Truyền dữ liệu tốc độ cao
* Làm việc theo chế độ master hoặc slave

---

## Kiến thức cơ bản

SPI sử dụng 4 dây chính:

| Tên  | Ý nghĩa             |
| ---- | ------------------- |
| MOSI | Master Out Slave In |
| MISO | Master In Slave Out |
| SCK  | Clock               |
| CS   | Chip Select         |

---

## Lưu ý quan trọng

* SPI là giao thức **full-duplex**
  → Gửi và nhận xảy ra đồng thời

* Khi gọi `SPI_transfer()`:

  * Bạn **luôn gửi 1 byte**
  * Đồng thời **luôn nhận 1 byte**

---

## Danh sách các hàm

| Hàm                    | Mô tả                              |
| ---------------------- | ---------------------------------- |
| `SPI_begin()`          | Khởi tạo SPI với cấu hình mặc định |
| `SPI_beginEx()`        | Khởi tạo SPI với cấu hình nâng cao |
| `SPI_end()`            | Tắt SPI                            |
| `SPI_setFrequency()`   | Đặt tốc độ SPI                     |
| `SPI_setDataMode()`    | Đặt mode SPI                       |
| `SPI_setBitOrder()`    | Đặt thứ tự bit                     |
| `SPI_transfer()`       | Gửi & nhận 8-bit                   |
| `SPI_transfer16()`     | Gửi & nhận 16-bit                  |
| `SPI_transferBuffer()` | Gửi & nhận buffer                  |
| `SPI_available()`      | Kiểm tra dữ liệu nhận              |
| `SPI_read()`           | Đọc 8-bit                          |
| `SPI_read16()`         | Đọc 16-bit                         |
| `SPI_write()`          | Gửi 8-bit                          |
| `SPI_write16()`        | Gửi 16-bit                         |
| `SPI_getRole()`        | Lấy role hiện tại                  |

---

## SPI_begin()

### Mục đích

Khởi tạo SPI với cấu hình mặc định.

### Cú pháp

```c
SPI_begin(role);
```

### Tham số

| Tham số | Mô tả                                   |
| ------- | --------------------------------------- |
| `role`  | `SPI_ROLE_MASTER` hoặc `SPI_ROLE_SLAVE` |

### Ví dụ

```c
SPI_begin(SPI_ROLE_MASTER);
```

### Lưu ý

* Đây là cách dùng đơn giản nhất cho beginner
* Sử dụng cấu hình mặc định:

  * Mode 0
  * MSB first
  * Frequency mặc định

---

## SPI_beginEx()

### Mục đích

Khởi tạo SPI với cấu hình đầy đủ.

### Cú pháp

```c
SPI_beginEx(role, frequency, mode, bitOrder);
```

### Tham số

| Tham số     | Mô tả                 |
| ----------- | --------------------- |
| `role`      | Master hoặc Slave     |
| `frequency` | Tốc độ SPI (Hz)       |
| `mode`      | SPI_MODE0 → SPI_MODE3 |
| `bitOrder`  | MSB hoặc LSB          |

### Ví dụ

```c
SPI_beginEx(SPI_ROLE_MASTER, 1000000, SPI_MODE0, SPI_MSBFIRST);
```

### Lưu ý

* Dùng khi cần cấu hình chính xác theo thiết bị
* Phải match đúng:

  * Mode
  * Clock
  * Bit order

---

## SPI_setFrequency()

### Mục đích

Thay đổi tốc độ SPI.

### Cú pháp

```c
SPI_setFrequency(freq);
```

### Ví dụ

```c
SPI_setFrequency(2000000);
```

### Lưu ý

* Phải gọi sau `SPI_begin()`
* Không phải thiết bị nào cũng chịu được tốc độ cao

---

## SPI_setDataMode()

### Mục đích

Đặt mode SPI.

### Cú pháp

```c
SPI_setDataMode(mode);
```

### Mode

| Mode  | CPOL | CPHA |
| ----- | ---- | ---- |
| MODE0 | 0    | 0    |
| MODE1 | 0    | 1    |
| MODE2 | 1    | 0    |
| MODE3 | 1    | 1    |

### Ví dụ

```c
SPI_setDataMode(SPI_MODE3);
```

### Lưu ý

* Phải trùng với thiết bị slave
* Sai mode → dữ liệu sai hoàn toàn

---

## SPI_setBitOrder()

### Mục đích

Đặt thứ tự bit.

### Cú pháp

```c
SPI_setBitOrder(order);
```

### Ví dụ

```c
SPI_setBitOrder(SPI_MSBFIRST);
```

### Lưu ý

* Thường dùng MSB first
* LSB ít gặp hơn

---

## SPI_transfer()

### Mục đích

Gửi và nhận 1 byte.

### Cú pháp

```c
uint8_t data = SPI_transfer(tx);
```

### Ví dụ

```c
uint8_t rx = SPI_transfer(0xA5);
```

### Lưu ý

* Luôn có dữ liệu trả về
* Nếu chỉ muốn đọc → gửi dummy (0xFF)

---

## SPI_transfer16()

### Mục đích

Gửi và nhận 16-bit.

### Cú pháp

```c
uint16_t data = SPI_transfer16(tx);
```

### Ví dụ

```c
uint16_t rx = SPI_transfer16(0x1234);
```

---

## SPI_transferBuffer()

### Mục đích

Gửi và nhận nhiều byte.

### Cú pháp

```c
SPI_transferBuffer(txBuf, rxBuf, length);
```

### Ví dụ

```c
SPI_transferBuffer(txData, rxData, 10);
```

### Lưu ý

* Có thể truyền NULL nếu chỉ gửi hoặc chỉ nhận

---

## SPI_write() / SPI_write16()

### Mục đích

Chỉ gửi dữ liệu.

### Ví dụ

```c
SPI_write(0x55);
SPI_write16(0x1234);
```

---

## SPI_read() / SPI_read16()

### Mục đích

Đọc dữ liệu nhận được.

### Ví dụ

```c
uint8_t data = SPI_read();
```

---

## SPI_available()

### Mục đích

Kiểm tra có dữ liệu nhận hay chưa.

### Ví dụ

```c
if (SPI_available())
{
    uint8_t data = SPI_read();
}
```

---

## SPI_getRole()

### Mục đích

Lấy role hiện tại.

### Ví dụ

```c
if (SPI_getRole() == SPI_ROLE_MASTER)
{
    // do something
}
```

---

## Gợi ý cách dùng API

| Nhu cầu         | Hàm nên dùng           |
| --------------- | ---------------------- |
| Dùng nhanh      | `SPI_begin()`          |
| Cấu hình đầy đủ | `SPI_beginEx()`        |
| Gửi 1 byte      | `SPI_transfer()`       |
| Gửi nhiều byte  | `SPI_transferBuffer()` |
| Chỉ gửi         | `SPI_write()`          |
| Chỉ đọc         | `SPI_read()`           |

---

## Tutorials

Bạn có thể học SPI qua các bài spi_full_demo trong source code tests/demo/spi_full_demo.h/.c hoặc đọc tutorials để hiểu hơn về code.