# Serial API

## Tổng quan

Serial API dùng để giao tiếp UART trong EduFramework.

Bạn có thể dùng để:

* In dữ liệu debug ra Hercules
* Gửi và nhận ký tự
* Gửi chuỗi văn bản
* Gửi số nguyên và số thực
* Giao tiếp UART với module hoặc cảm biến bên ngoài

EduFramework cung cấp 2 cổng Serial:

| Serial    | Mục đích                                                    |
| --------- | ----------------------------------------------------------- |
| `Serial1` | Debug qua J-Link/USB, thường dùng với Hercules              |
| `Serial2` | UART vật lý đưa ra chân TX/RX để giao tiếp với module ngoài |

---

## Lưu ý về baudrate

Hiện tại Serial API nên dùng ở baudrate thấp.

Khuyến nghị:

| Baudrate     | Trạng thái          |
| ------------ | ------------------- |
| `9600`       | Khuyến nghị sử dụng |
| `19200`      | Có thể dùng         |
| `38400`      | Giới hạn nên dùng   |
| Trên `38400` | Không khuyến nghị   |

Lý do là cấu hình clock hiện tại của framework chưa được tối ưu cho UART baudrate cao.

---

## Danh sách các hàm

### Serial1

| Hàm                      | Mô tả                              |
| ------------------------ | ---------------------------------- |
| `Serial1_begin()`        | Khởi tạo Serial1                   |
| `Serial1_available()`    | Kiểm tra có dữ liệu nhận hay không |
| `Serial1_read()`         | Đọc một ký tự                      |
| `Serial1_readString()`   | Đọc một chuỗi                      |
| `Serial1_write()`        | Gửi một ký tự                      |
| `Serial1_print()`        | Gửi chuỗi                          |
| `Serial1_println()`      | Gửi chuỗi kèm xuống dòng           |
| `Serial1_printInt()`     | Gửi số nguyên                      |
| `Serial1_printlnInt()`   | Gửi số nguyên kèm xuống dòng       |
| `Serial1_printFloat()`   | Gửi số thực                        |
| `Serial1_printlnFloat()` | Gửi số thực kèm xuống dòng         |

### Serial2

Serial2 có cùng bộ hàm với Serial1, nhưng dùng cho UART vật lý đưa ra chân TX/RX.

---

## Serial1_begin() / Serial2_begin()

### Mục đích

Khởi tạo UART với baudrate mong muốn.

### Cú pháp

```c
Serial1_begin(baudRate);
Serial2_begin(baudRate);
```

### Tham số

| Tham số    | Mô tả              |
| ---------- | ------------------ |
| `baudRate` | Tốc độ truyền UART |

### Ví dụ

```c
Serial1_begin(9600);
Serial2_begin(9600);
```

### Lưu ý

* Nên gọi hàm `begin()` trước khi dùng các hàm Serial khác.
* Baudrate khuyến nghị là `9600`.
* Không nên dùng baudrate lớn hơn `38400` trong phiên bản hiện tại.

---

## Serial1_available() / Serial2_available()

### Mục đích

Kiểm tra xem UART có dữ liệu mới để đọc hay không.

### Cú pháp

```c
bool available = Serial1_available();
bool available = Serial2_available();
```

### Giá trị trả về

| Giá trị | Mô tả             |
| ------- | ----------------- |
| `true`  | Có dữ liệu để đọc |
| `false` | Chưa có dữ liệu   |

### Ví dụ

```c
if (Serial1_available())
{
    char ch = Serial1_read();
}
```

### Lưu ý

* Nên kiểm tra `available()` trước khi gọi `read()`.
* Nếu đọc khi chưa có dữ liệu, kết quả có thể không đúng như mong muốn.

---

## Serial1_read() / Serial2_read()

### Mục đích

Đọc một ký tự từ UART.

### Cú pháp

```c
char ch = Serial1_read();
char ch = Serial2_read();
```

### Giá trị trả về

| Giá trị | Mô tả              |
| ------- | ------------------ |
| `char`  | Ký tự vừa đọc được |

### Ví dụ

```c
if (Serial1_available())
{
    char ch = Serial1_read();
    Serial1_write(ch);
}
```

### Lưu ý

* Hàm này chỉ đọc một ký tự.
* Nếu muốn đọc nhiều ký tự, dùng `Serial1_readString()` hoặc `Serial2_readString()`.

---

## Serial1_readString() / Serial2_readString()

### Mục đích

Đọc một chuỗi ký tự từ UART vào buffer.

### Cú pháp

```c
uint32_t len = Serial1_readString(buffer, maxLength);
uint32_t len = Serial2_readString(buffer, maxLength);
```

### Tham số

| Tham số     | Mô tả                            |
| ----------- | -------------------------------- |
| `buffer`    | Mảng dùng để lưu chuỗi nhận được |
| `maxLength` | Số ký tự tối đa có thể đọc       |

### Giá trị trả về

| Giá trị    | Mô tả                |
| ---------- | -------------------- |
| `uint32_t` | Số ký tự đã đọc được |

### Ví dụ

```c
char buffer[32];

uint32_t len = Serial1_readString(buffer, sizeof(buffer));
```

### Lưu ý

* Cần cấp phát buffer đủ lớn.
* Không nên truyền `maxLength` lớn hơn kích thước thật của buffer.
* Hàm này phù hợp khi muốn đọc lệnh hoặc chuỗi ngắn từ terminal.

---

## Serial1_write() / Serial2_write()

### Mục đích

Gửi một ký tự qua UART.

### Cú pháp

```c
Serial1_write(ch);
Serial2_write(ch);
```

### Tham số

| Tham số | Mô tả         |
| ------- | ------------- |
| `ch`    | Ký tự cần gửi |

### Ví dụ

```c
Serial1_write('A');
```

---

## Serial1_print() / Serial2_print()

### Mục đích

Gửi một chuỗi ký tự qua UART.

### Cú pháp

```c
Serial1_print(str);
Serial2_print(str);
```

### Tham số

| Tham số | Mô tả         |
| ------- | ------------- |
| `str`   | Chuỗi cần gửi |

### Ví dụ

```c
Serial1_print("Hello EduFramework");
```

---

## Serial1_println() / Serial2_println()

### Mục đích

Gửi một chuỗi ký tự và tự động xuống dòng.

### Cú pháp

```c
Serial1_println(str);
Serial2_println(str);
```

### Ví dụ

```c
Serial1_println("System started");
```

### Lưu ý

* `print()` không tự xuống dòng.
* `println()` có xuống dòng, phù hợp để in log dễ đọc hơn.

---

## Serial1_printInt() / Serial2_printInt()

### Mục đích

Gửi một số nguyên qua UART.

### Cú pháp

```c
Serial1_printInt(value);
Serial2_printInt(value);
```

### Ví dụ

```c
int value = 123;
Serial1_printInt(value);
```

---

## Serial1_printlnInt() / Serial2_printlnInt()

### Mục đích

Gửi một số nguyên và tự động xuống dòng.

### Cú pháp

```c
Serial1_printlnInt(value);
Serial2_printlnInt(value);
```

### Ví dụ

```c
int adcValue = 2048;
Serial1_printlnInt(adcValue);
```

---

## Serial1_printFloat() / Serial2_printFloat()

### Mục đích

Gửi một số thực qua UART.

### Cú pháp

```c
Serial1_printFloat(value);
Serial2_printFloat(value);
```

### Ví dụ

```c
float voltage = 3.3f;
Serial1_printFloat(voltage);
```

---

## Serial1_printlnFloat() / Serial2_printlnFloat()

### Mục đích

Gửi một số thực và tự động xuống dòng.

### Cú pháp

```c
Serial1_printlnFloat(value);
Serial2_printlnFloat(value);
```

### Ví dụ

```c
float temperature = 27.5f;
Serial1_printlnFloat(temperature);
```

---

## Serial1 và Serial2 khác nhau thế nào?

| Cổng      | Mục đích             | Khi dùng                             |
| --------- | -------------------- | ------------------------------------ |
| `Serial1` | Debug qua J-Link/USB | In log, test bằng Hercules           |
| `Serial2` | UART vật lý TX/RX    | Giao tiếp cảm biến hoặc module ngoài |

---

## Tutorials

Bạn có thể tìm hiểu cách dùng Serial API qua các bài tutorial về UART echo hoặc UART điều khiển LED.

##