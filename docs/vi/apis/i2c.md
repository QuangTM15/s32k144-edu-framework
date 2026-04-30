# I2C API

## Tổng quan

I2C API dùng để giao tiếp với các thiết bị ngoại vi sử dụng chuẩn I2C trong EduFramework.

Bạn có thể dùng để:

* Giao tiếp với cảm biến I2C
* Giao tiếp với LCD I2C
* Đọc dữ liệu từ module ngoài
* Gửi dữ liệu từ Master đến Slave
* Nhận dữ liệu từ Slave về Master
* Làm việc theo chế độ master hoặc slave

---

## Kiến thức cơ bản

I2C sử dụng 2 dây chính:

| Tên | Ý nghĩa      |
| --- | ------------ |
| SDA | Serial Data  |
| SCL | Serial Clock |

---

## Lưu ý quan trọng

* I2C là giao thức **half-duplex**
  → Tại một thời điểm, bus chỉ truyền theo một hướng

* I2C dùng địa chỉ để chọn thiết bị
  → Mỗi slave cần có một địa chỉ riêng

* Một bus I2C có thể có nhiều slave

---

## Danh sách các hàm

| Hàm                        | Mô tả                                                |
| -------------------------- | ---------------------------------------------------- |
| `Wire_begin()`             | Khởi tạo I2C ở chế độ Master                         |
| `Wire_beginAddress()`      | Khởi tạo I2C ở chế độ Slave với địa chỉ riêng        |
| `Wire_setClock()`          | Đặt tốc độ I2C                                       |
| `Wire_beginTransmission()` | Bắt đầu truyền dữ liệu đến một slave                 |
| `Wire_write()`             | Ghi 1 byte vào buffer truyền                         |
| `Wire_writeBuffer()`       | Ghi nhiều byte vào buffer truyền                     |
| `Wire_endTransmission()`   | Kết thúc truyền và gửi dữ liệu                       |
| `Wire_requestFrom()`       | Master yêu cầu đọc dữ liệu từ slave                  |
| `Wire_available()`         | Kiểm tra số byte có thể đọc                          |
| `Wire_read()`              | Đọc 1 byte dữ liệu                                   |
| `Wire_onReceive()`         | Đăng ký callback khi Slave nhận dữ liệu              |
| `Wire_onRequest()`         | Đăng ký callback khi Master yêu cầu dữ liệu từ Slave |
| `Wire_transferAsync()`     | Bắt đầu truyền bất đồng bộ                           |
| `Wire_isBusy()`            | Kiểm tra I2C đang bận                                |
| `Wire_isDone()`            | Kiểm tra truyền bất đồng bộ đã xong chưa             |
| `Wire_getStatus()`         | Lấy trạng thái I2C gần nhất                          |

---

## Wire_begin()

### Mục đích

Khởi tạo I2C ở chế độ Master.

### Cú pháp

```c
Wire_begin();
```

### Ví dụ

```c
Wire_begin();
```

### Lưu ý

* Dùng cho board đóng vai trò Master
* Master là thiết bị chủ động bắt đầu truyền hoặc đọc dữ liệu
* Sau khi gọi hàm này, có thể dùng các hàm như:

  * `Wire_beginTransmission()`
  * `Wire_requestFrom()`

---

## Wire_beginAddress()

### Mục đích

Khởi tạo I2C ở chế độ Slave với địa chỉ riêng.

### Cú pháp

```c
Wire_beginAddress(address);
```

### Tham số

| Tham số   | Mô tả                 |
| --------- | --------------------- |
| `address` | Địa chỉ I2C của Slave |

### Ví dụ

```c
Wire_beginAddress(0x08);
```

### Lưu ý

* Dùng cho board đóng vai trò Slave
* Địa chỉ slave phải khác nhau nếu có nhiều thiết bị trên cùng bus
* Master sẽ dùng địa chỉ này để giao tiếp với Slave

---

## Wire_setClock()

### Mục đích

Đặt tốc độ I2C.

### Cú pháp

```c
Wire_setClock(frequency);
```

### Tham số

| Tham số     | Mô tả                    |
| ----------- | ------------------------ |
| `frequency` | Tốc độ I2C, tính bằng Hz |

### Ví dụ

```c
Wire_setClock(100000);
```

### Lưu ý

* `100000` tương ứng 100 kHz
* `400000` tương ứng 400 kHz
* Nên gọi sau `Wire_begin()`
* Không phải thiết bị nào cũng hỗ trợ tốc độ cao

---

## Wire_beginTransmission()

### Mục đích

Bắt đầu một phiên truyền dữ liệu từ Master đến một Slave.

### Cú pháp

```c
Wire_beginTransmission(address);
```

### Tham số

| Tham số   | Mô tả                                 |
| --------- | ------------------------------------- |
| `address` | Địa chỉ I2C của Slave cần gửi dữ liệu |

### Ví dụ

```c
Wire_beginTransmission(0x08);
```

### Lưu ý

* Hàm này chỉ bắt đầu phiên truyền
* Dữ liệu chưa được gửi ngay tại thời điểm gọi hàm này
* Sau đó cần gọi `Wire_write()` hoặc `Wire_writeBuffer()`
* Kết thúc bằng `Wire_endTransmission()`

---

## Wire_write()

### Mục đích

Ghi 1 byte dữ liệu vào buffer truyền.

### Cú pháp

```c
uint8_t result = Wire_write(data);
```

### Tham số

| Tham số | Mô tả        |
| ------- | ------------ |
| `data`  | Byte cần gửi |

### Giá trị trả về

| Giá trị   | Mô tả                     |
| --------- | ------------------------- |
| `uint8_t` | Số byte đã ghi vào buffer |

### Ví dụ

```c
Wire_write(0x55);
```

### Lưu ý

* Hàm này thường dùng giữa:

  * `Wire_beginTransmission()`
  * `Wire_endTransmission()`
* Dữ liệu được đưa vào buffer trước
* Dữ liệu thật sự được gửi khi gọi `Wire_endTransmission()`

---

## Wire_writeBuffer()

### Mục đích

Ghi nhiều byte dữ liệu vào buffer truyền.

### Cú pháp

```c
uint8_t result = Wire_writeBuffer(data, length);
```

### Tham số

| Tham số  | Mô tả                      |
| -------- | -------------------------- |
| `data`   | Con trỏ đến buffer dữ liệu |
| `length` | Số byte cần ghi            |

### Giá trị trả về

| Giá trị   | Mô tả                     |
| --------- | ------------------------- |
| `uint8_t` | Số byte đã ghi vào buffer |

### Ví dụ

```c
uint8_t data[3] = {0x11, 0x22, 0x33};

Wire_writeBuffer(data, 3);
```

### Lưu ý

* Dùng khi cần gửi nhiều byte liên tiếp
* Buffer không nên là `NULL`
* `length` phải đúng với số byte cần gửi

---

## Wire_endTransmission()

### Mục đích

Kết thúc phiên truyền và gửi dữ liệu từ Master đến Slave.

### Cú pháp

```c
uint8_t status = Wire_endTransmission();
```

### Giá trị trả về

| Giá trị               | Mô tả                |
| --------------------- | -------------------- |
| `WIRE_STATUS_OK`      | Truyền thành công    |
| `WIRE_STATUS_BUSY`    | I2C đang bận         |
| `WIRE_STATUS_TIMEOUT` | Truyền quá thời gian |
| `WIRE_STATUS_NACK`    | Slave không phản hồi |
| `WIRE_STATUS_ERROR`   | Lỗi khác             |

### Ví dụ

```c
Wire_beginTransmission(0x08);
Wire_write(0x55);
status = Wire_endTransmission();
```

### Lưu ý

* Đây là hàm thực sự gửi dữ liệu đã ghi bằng `Wire_write()`
* Nên kiểm tra giá trị trả về để biết truyền thành công hay lỗi
* Nếu sai địa chỉ Slave, thường sẽ nhận `WIRE_STATUS_NACK`

---

## Wire_requestFrom()

### Mục đích

Master yêu cầu đọc dữ liệu từ một Slave.

### Cú pháp

```c
uint8_t count = Wire_requestFrom(address, quantity);
```

### Tham số

| Tham số    | Mô tả                 |
| ---------- | --------------------- |
| `address`  | Địa chỉ I2C của Slave |
| `quantity` | Số byte muốn đọc      |

### Giá trị trả về

| Giá trị   | Mô tả             |
| --------- | ----------------- |
| `uint8_t` | Số byte nhận được |

### Ví dụ

```c
uint8_t count = Wire_requestFrom(0x08, 3);
```

### Lưu ý

* Dùng ở phía Master
* Sau khi gọi hàm này, dùng `Wire_available()` và `Wire_read()` để đọc dữ liệu
* Số byte nhận được có thể ít hơn số byte yêu cầu nếu có lỗi

---

## Wire_available()

### Mục đích

Kiểm tra số byte dữ liệu đang có sẵn để đọc.

### Cú pháp

```c
int count = Wire_available();
```

### Giá trị trả về

| Giá trị | Mô tả              |
| ------- | ------------------ |
| `int`   | Số byte có thể đọc |

### Ví dụ

```c
if (Wire_available() > 0)
{
    int data = Wire_read();
}
```

### Lưu ý

* Nên kiểm tra `Wire_available()` trước khi gọi `Wire_read()`
* Dùng sau `Wire_requestFrom()` hoặc trong xử lý nhận dữ liệu

---

## Wire_read()

### Mục đích

Đọc 1 byte dữ liệu từ buffer nhận.

### Cú pháp

```c
int data = Wire_read();
```

### Giá trị trả về

| Giá trị  | Mô tả            |
| -------- | ---------------- |
| `0..255` | Byte đọc được    |
| `-1`     | Không có dữ liệu |

### Ví dụ

```c
int data = Wire_read();
```

### Lưu ý

* Nên gọi sau khi `Wire_available()` lớn hơn 0
* Nếu không có dữ liệu, hàm có thể trả về `-1`

---

## Wire_onReceive()

### Mục đích

Đăng ký hàm callback khi Slave nhận dữ liệu từ Master.

### Cú pháp

```c
Wire_onReceive(callback);
```

### Tham số

| Tham số    | Mô tả                               |
| ---------- | ----------------------------------- |
| `callback` | Hàm được gọi khi Slave nhận dữ liệu |

### Ví dụ

```c
void receiveEvent(int length)
{
    while (Wire_available())
    {
        int data = Wire_read();
    }
}

Wire_onReceive(receiveEvent);
```

### Lưu ý

* Dùng ở phía Slave
* Callback nhận vào số byte đã nhận
* Không nên xử lý quá nặng trong callback

---

## Wire_onRequest()

### Mục đích

Đăng ký hàm callback khi Master yêu cầu dữ liệu từ Slave.

### Cú pháp

```c
Wire_onRequest(callback);
```

### Tham số

| Tham số    | Mô tả                                   |
| ---------- | --------------------------------------- |
| `callback` | Hàm được gọi khi Master yêu cầu dữ liệu |

### Ví dụ

```c
void requestEvent(void)
{
    Wire_write(0x55);
}

Wire_onRequest(requestEvent);
```

### Lưu ý

* Dùng ở phía Slave
* Trong callback này, Slave có thể dùng `Wire_write()` để gửi dữ liệu về Master
* Callback nên ngắn gọn

---

## Wire_transferAsync()

### Mục đích

Bắt đầu một phiên truyền I2C bất đồng bộ.

Hàm này không chờ truyền xong ngay, phù hợp khi chương trình cần tiếp tục làm việc khác trong lúc I2C đang truyền.

### Cú pháp

```c
uint8_t status = Wire_transferAsync(address, txData, txSize, rxData, rxSize);
```

### Tham số

| Tham số   | Mô tả                  |
| --------- | ---------------------- |
| `address` | Địa chỉ I2C của Slave  |
| `txData`  | Buffer dữ liệu cần gửi |
| `txSize`  | Số byte cần gửi        |
| `rxData`  | Buffer nhận dữ liệu    |
| `rxSize`  | Số byte cần nhận       |

### Giá trị trả về

| Giá trị             | Mô tả                       |
| ------------------- | --------------------------- |
| `WIRE_STATUS_OK`    | Bắt đầu transfer thành công |
| `WIRE_STATUS_BUSY`  | I2C đang bận                |
| `WIRE_STATUS_ERROR` | Không thể bắt đầu transfer  |

### Ví dụ

```c
uint8_t tx[1] = {0x01};
uint8_t rx[2];

Wire_transferAsync(0x08, tx, 1, rx, 2);
```

### Lưu ý

* Đây là API nâng cao
* Dùng cho truyền non-blocking / interrupt-based
* Sau khi gọi, dùng `Wire_isBusy()`, `Wire_isDone()` và `Wire_getStatus()` để kiểm tra trạng thái

---

## Wire_isBusy()

### Mục đích

Kiểm tra I2C có đang bận hay không.

### Cú pháp

```c
uint8_t busy = Wire_isBusy();
```

### Giá trị trả về

| Giá trị | Mô tả     |
| ------- | --------- |
| `1`     | Đang bận  |
| `0`     | Không bận |

---

## Wire_isDone()

### Mục đích

Kiểm tra truyền bất đồng bộ đã hoàn thành chưa.

### Cú pháp

```c
uint8_t done = Wire_isDone();
```

### Giá trị trả về

| Giá trị | Mô tả           |
| ------- | --------------- |
| `1`     | Đã hoàn thành   |
| `0`     | Chưa hoàn thành |

---

## Wire_getStatus()

### Mục đích

Lấy trạng thái I2C gần nhất.

### Cú pháp

```c
uint8_t status = Wire_getStatus();
```

### Giá trị trả về

| Giá trị               | Mô tả                |
| --------------------- | -------------------- |
| `WIRE_STATUS_OK`      | Thành công           |
| `WIRE_STATUS_BUSY`    | I2C đang bận         |
| `WIRE_STATUS_TIMEOUT` | Quá thời gian        |
| `WIRE_STATUS_NACK`    | Slave không phản hồi |
| `WIRE_STATUS_ERROR`   | Lỗi khác             |

---

## Gợi ý cách dùng API

| Nhu cầu             | Hàm nên dùng                                                           |
| ------------------- | ---------------------------------------------------------------------- |
| Khởi tạo Master     | `Wire_begin()`                                                         |
| Khởi tạo Slave      | `Wire_beginAddress()`                                                  |
| Đổi tốc độ I2C      | `Wire_setClock()`                                                      |
| Master gửi dữ liệu  | `Wire_beginTransmission()` + `Wire_write()` + `Wire_endTransmission()` |
| Master đọc dữ liệu  | `Wire_requestFrom()` + `Wire_available()` + `Wire_read()`              |
| Slave nhận dữ liệu  | `Wire_onReceive()`                                                     |
| Slave trả dữ liệu   | `Wire_onRequest()`                                                     |
| Truyền non-blocking | `Wire_transferAsync()`                                                 |

---

## Tutorials

Bạn có thể học I2C qua các bài tutorial hoặc đọc source code trong thư mục `tests/demo`.
