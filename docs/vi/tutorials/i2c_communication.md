# I2C Communication (Master – Slave)

## Mục tiêu

Trong bài này, bạn sẽ học cách:

* Hiểu cách hoạt động của I2C
* Thiết lập giao tiếp giữa 2 board
* Gửi dữ liệu từ Master đến Slave
* Nhận và xử lý dữ liệu ở Slave
* Sử dụng callback khi Slave nhận dữ liệu

---

## Giới thiệu

I2C là một giao thức giao tiếp phổ biến trong embedded.

Trong bài này, bạn sẽ:

* Kết nối **2 board S32K144**
* 1 board đóng vai trò **Master**
* 1 board đóng vai trò **Slave**

> Nhấn nút → Master gửi lệnh → Slave nhận lệnh → LED thay đổi

---

## Kiến thức cần biết

### I2C (Inter-Integrated Circuit)

* Giao tiếp đồng bộ
* Sử dụng 2 dây chính
* Hỗ trợ nhiều thiết bị trên cùng một bus
* Master chọn Slave thông qua địa chỉ

---

### Các chân I2C

* SDA – Serial Data
* SCL – Serial Clock

---

### Master vs Slave

#### Master

* Chủ động bắt đầu giao tiếp
* Tạo clock
* Gửi dữ liệu đến Slave
* Chọn Slave bằng địa chỉ

#### Slave

* Có một địa chỉ riêng
* Chờ Master gọi đúng địa chỉ
* Nhận dữ liệu và xử lý

---

## Nguyên lý hoạt động

```text
Nhấn nút (Master) → gửi 0xA5 qua I2C → Slave nhận → LED_RED toggle
```

---

## Code

```c
#define I2C_DEMO_IS_MASTER    0

#define I2C_SLAVE_ADDR        0x12U
#define I2C_CMD_TOGGLE        0xA5U
```

---

## Giải thích

### Chọn vai trò

```c
#define I2C_DEMO_IS_MASTER 0
```

* `1` → Master
* `0` → Slave

👉 Bạn cần chạy **2 board**:

* 1 board set = `1`
* 1 board set = `0`

---

### Địa chỉ Slave

```c
#define I2C_SLAVE_ADDR 0x12U
```

Đây là địa chỉ của Slave trên bus I2C.

Master sẽ dùng địa chỉ này để gửi dữ liệu đến đúng board Slave.

---

### Lệnh điều khiển

```c
#define I2C_CMD_TOGGLE 0xA5U
```

Đây là byte lệnh được Master gửi sang Slave.

Trong demo này:

* Nếu Slave nhận được `0xA5`
* Slave sẽ toggle `LED_RED`

---

## Khởi tạo I2C

```c
static void Demo_InitI2C(void)
{
#if I2C_DEMO_IS_MASTER
    Wire_begin();
#else
    Wire_beginAddress(I2C_SLAVE_ADDR);
#endif
}
```

### Giải thích

Nếu board là Master:

```c
Wire_begin();
```

* Khởi tạo I2C ở chế độ Master
* Master có quyền bắt đầu truyền dữ liệu

Nếu board là Slave:

```c
Wire_beginAddress(I2C_SLAVE_ADDR);
```

* Khởi tạo I2C ở chế độ Slave
* Gán địa chỉ `0x12` cho board Slave
* Master sẽ gửi dữ liệu đến địa chỉ này

---

## Master – Gửi dữ liệu

```c
Wire_beginTransmission(I2C_SLAVE_ADDR);
Wire_write(I2C_CMD_TOGGLE);
Wire_endTransmission();

digitalToggle(LED_GREEN);
```

### Giải thích

```c
Wire_beginTransmission(I2C_SLAVE_ADDR);
```

* Bắt đầu truyền đến Slave có địa chỉ `0x12`

---

```c
Wire_write(I2C_CMD_TOGGLE);
```

* Ghi byte `0xA5` vào buffer truyền

---

```c
Wire_endTransmission();
```

* Kết thúc truyền
* Dữ liệu thật sự được gửi qua bus I2C tại bước này

---

```c
digitalToggle(LED_GREEN);
```

* LED_GREEN trên Master thay đổi để báo rằng Master đã gửi lệnh

---

## Trigger bằng button

```c
if (ButtonPressed(BTN0) == 0U)
{
    return;
}
```

### Giải thích

* Master chỉ gửi dữ liệu khi nhấn `BTN0`
* Nếu không nhấn nút, hàm sẽ thoát và không gửi gì

---

## Slave – Đăng ký callback

```c
Wire_onReceive(Demo_OnReceive);
```

### Giải thích

* Slave không cần polling liên tục trong `while(1)`
* Khi Master gửi dữ liệu đến đúng địa chỉ
* Hàm `Demo_OnReceive()` sẽ được gọi tự động

---

## Slave – Nhận dữ liệu

```c
static void Demo_OnReceive(int len)
{
    uint8_t data;

    if (len <= 0)
    {
        return;
    }

    data = (uint8_t)Wire_read();

    if (data == I2C_CMD_TOGGLE)
    {
        digitalToggle(LED_RED);
    }
}
```

### Giải thích

```c
if (len <= 0)
{
    return;
}
```

* Kiểm tra có dữ liệu được nhận hay không
* Nếu không có dữ liệu thì thoát

---

```c
data = (uint8_t)Wire_read();
```

* Đọc 1 byte dữ liệu từ Master

---

```c
if (data == I2C_CMD_TOGGLE)
{
    digitalToggle(LED_RED);
}
```

* Nếu dữ liệu nhận được là `0xA5`
* Slave sẽ toggle `LED_RED`

---

## Vòng lặp chính

```c
while (1)
{
#if I2C_DEMO_IS_MASTER
    Demo_MasterHandleButton();
#else
    /* Slave runs by interrupt callback */
#endif
}
```

### Giải thích

Ở Master:

* Chương trình liên tục kiểm tra nút nhấn
* Khi nhấn nút, Master gửi lệnh sang Slave

Ở Slave:

* `while(1)` không cần xử lý gì
* Slave nhận dữ liệu thông qua callback

---

## Kết quả

Khi chạy chương trình:

* Master:

  * Nhấn `BTN0`
  * Gửi `0xA5` đến Slave
  * `LED_GREEN` toggle

* Slave:

  * Nhận `0xA5`
  * `LED_RED` toggle

---

## Lưu ý

* Cần **2 board** để test
* 1 board phải là Master
* 1 board phải là Slave
* Master và Slave phải dùng cùng địa chỉ Slave
* Cần nối chung GND
* Cần nối đúng dây I2C:

  * SDA ↔ SDA
  * SCL ↔ SCL
* I2C cần pull-up trên SDA và SCL

---

## Gợi ý mở rộng

Bạn có thể thử:

* Gửi nhiều byte thay vì 1 byte
* Thêm nhiều command khác nhau
* Cho Slave phản hồi dữ liệu về Master
* Dùng LCD I2C để hiển thị trạng thái
* Thử tốc độ 100 kHz và 400 kHz

---

## Liên quan

```text
docs/vi/apis/i2c.md
tests/demo/i2c_demo.c
```