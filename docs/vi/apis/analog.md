# Analog API

## Tổng quan

Analog API dùng để làm việc với tín hiệu analog trong EduFramework.

Bạn có thể dùng để:

* Đọc tín hiệu analog từ cảm biến
* Đọc giá trị ADC theo dạng số
* Đọc giá trị điện áp theo millivolt
* Thực hiện ADC theo kiểu blocking hoặc non-blocking
* Xuất tín hiệu PWM để điều khiển độ sáng LED hoặc module ngoài

Analog API gồm 2 nhóm chính:

| Nhóm | Mục đích                              |
| ---- | ------------------------------------- |
| ADC  | Đọc tín hiệu analog input             |
| PWM  | Xuất tín hiệu analog giả lập bằng PWM |

---

## Lưu ý quan trọng

`analogWrite()` trong EduFramework không tạo ra điện áp analog thật.

Hàm này tạo tín hiệu PWM.

PWM thay đổi độ rộng xung để tạo hiệu ứng giống analog, ví dụ:

* Điều chỉnh độ sáng LED
* Điều khiển tốc độ motor nhỏ
* Điều khiển buzzer hoặc module đơn giản

---

## Danh sách các hàm

| Hàm                      | Mô tả                                          |
| ------------------------ | ---------------------------------------------- |
| `analogRead()`           | Đọc analog theo kiểu blocking                  |
| `analogStart()`          | Bắt đầu một lần đọc analog non-blocking        |
| `analogAvailable()`      | Kiểm tra kết quả non-blocking đã sẵn sàng chưa |
| `analogGetResult()`      | Lấy kết quả non-blocking mới nhất              |
| `analogReadMilliVolts()` | Đọc analog và trả về millivolt                 |
| `analogWrite()`          | Xuất PWM                                       |

---

## Blocking và Non-blocking

Analog API hỗ trợ 2 cách đọc ADC:

| Cách đọc     | Đặc điểm                                       | Khi dùng                                             |
| ------------ | ---------------------------------------------- | ---------------------------------------------------- |
| Blocking     | Gọi hàm và chờ đến khi có kết quả              | Dễ dùng, phù hợp beginner                            |
| Non-blocking | Bắt đầu chuyển đổi trước, kiểm tra kết quả sau | Phù hợp khi chương trình cần làm nhiều việc cùng lúc |

---

## analogRead()

### Mục đích

Đọc giá trị analog từ một chân ADC theo kiểu blocking.

Khi gọi hàm này, chương trình sẽ chờ cho đến khi ADC chuyển đổi xong.

### Cú pháp

```c
int value = analogRead(pin);
```

### Tham số

| Tham số | Mô tả               |
| ------- | ------------------- |
| `pin`   | Chân analog cần đọc |

### Chân hỗ trợ

Hiện tại `analogRead()` hỗ trợ các chân sau:

| Chân        | Mô tả       |
| ----------- | ----------- |
| `ADC0_SE12` | Kênh ADC 12 |
| `ADC0_SE13` | Kênh ADC 13 |

### Giá trị trả về

| Giá trị | Mô tả                |
| ------- | -------------------- |
| `int`   | Giá trị ADC đọc được |

### Ví dụ

```c
int value = analogRead(ADC0_SE12);
```

### Lưu ý

* Đây là cách đọc dễ nhất cho beginner.
* Vì là blocking, chương trình sẽ đứng chờ trong lúc ADC chuyển đổi.
* Phù hợp với các bài đọc biến trở, cảm biến analog đơn giản.
* Chỉ nên truyền vào các chân có hỗ trợ ADC.

---

## analogReadMilliVolts()

### Mục đích

Đọc giá trị analog và chuyển đổi sang millivolt.

Hàm này giúp bạn dễ hiểu giá trị điện áp hơn thay vì chỉ nhìn giá trị ADC thô.

### Cú pháp

```c
int mv = analogReadMilliVolts(pin);
```

### Tham số

| Tham số | Mô tả               |
| ------- | ------------------- |
| `pin`   | Chân analog cần đọc |

### Chân hỗ trợ

Hiện tại `analogReadMilliVolts()` hỗ trợ các chân sau:

| Chân        | Mô tả       |
| ----------- | ----------- |
| `ADC0_SE12` | Kênh ADC 12 |
| `ADC0_SE13` | Kênh ADC 13 |

### Giá trị trả về

| Giá trị | Mô tả                                 |
| ------- | ------------------------------------- |
| `int`   | Điện áp đọc được, tính bằng millivolt |

### Ví dụ

```c
int voltage = analogReadMilliVolts(ADC0_SE12);
```

### Lưu ý

* Giá trị trả về có đơn vị là mV.
* Hàm này phù hợp khi muốn hiển thị điện áp ra Serial.
* Kết quả phụ thuộc vào điện áp tham chiếu ADC của hệ thống.
* Chỉ nên truyền vào các chân có hỗ trợ ADC.

---

## analogStart()

### Mục đích

Bắt đầu một lần chuyển đổi ADC theo kiểu non-blocking.

Hàm này chỉ bắt đầu quá trình đọc ADC, không chờ kết quả ngay.

### Cú pháp

```c
analogStart(pin);
```

### Tham số

| Tham số | Mô tả               |
| ------- | ------------------- |
| `pin`   | Chân analog cần đọc |

### Chân hỗ trợ

Hiện tại `analogStart()` hỗ trợ các chân sau:

| Chân        | Mô tả       |
| ----------- | ----------- |
| `ADC0_SE12` | Kênh ADC 12 |
| `ADC0_SE13` | Kênh ADC 13 |

### Ví dụ

```c
analogStart(ADC0_SE12);
```

### Lưu ý

* Sau khi gọi `analogStart()`, cần dùng `analogAvailable()` để kiểm tra khi nào có kết quả.
* Phù hợp khi chương trình cần làm việc khác trong lúc ADC đang chuyển đổi.
* Đây là cách dùng nâng cao hơn `analogRead()`.
* Chỉ nên truyền vào các chân có hỗ trợ ADC.

---

## analogAvailable()

### Mục đích

Kiểm tra kết quả chuyển đổi ADC non-blocking đã sẵn sàng hay chưa.

### Cú pháp

```c
uint8_t ready = analogAvailable();
```

### Giá trị trả về

| Giá trị | Mô tả           |
| ------- | --------------- |
| `1`     | Đã có kết quả   |
| `0`     | Chưa có kết quả |

### Ví dụ

```c
if (analogAvailable())
{
    int value = analogGetResult();
}
```

### Lưu ý

* Chỉ dùng sau khi đã gọi `analogStart()`.
* Không nên gọi `analogGetResult()` nếu `analogAvailable()` chưa báo sẵn sàng.

---

## analogGetResult()

### Mục đích

Lấy kết quả ADC mới nhất sau khi chuyển đổi non-blocking hoàn thành.

### Cú pháp

```c
int value = analogGetResult();
```

### Giá trị trả về

| Giá trị | Mô tả                |
| ------- | -------------------- |
| `int`   | Giá trị ADC mới nhất |

### Ví dụ

```c
analogStart(ADC0_SE12);

while (!analogAvailable())
{
    // có thể làm việc khác ở đây
}

int value = analogGetResult();
```

### Lưu ý

* Nên gọi sau khi `analogAvailable()` trả về `1`.
* Nếu gọi quá sớm, kết quả có thể chưa phải giá trị mới nhất.

---

## analogWrite()

### Mục đích

Xuất tín hiệu PWM ra một chân có hỗ trợ PWM.

Hàm này thường dùng để:

* Điều chỉnh độ sáng LED
* Điều khiển mức công suất đơn giản
* Tạo hiệu ứng fade

### Cú pháp

```c
analogWrite(pin, value);
```

### Tham số

| Tham số | Mô tả                        |
| ------- | ---------------------------- |
| `pin`   | Chân có hỗ trợ PWM           |
| `value` | Duty cycle, từ `0` đến `255` |

### Chân hỗ trợ

Hiện tại `analogWrite()` hỗ trợ các chân sau:

| Chân        | Mô tả      |
| ----------- | ---------- |
| `GPIO2`     | Hỗ trợ PWM |
| `GPIO3`     | Hỗ trợ PWM |
| `GPIO4`     | Hỗ trợ PWM |
| `GPIO5`     | Hỗ trợ PWM |
| `GPIO6`     | Hỗ trợ PWM |
| `GPIO7`     | Hỗ trợ PWM |
| `GPIO8`     | Hỗ trợ PWM |
| `LED_RED`   | Hỗ trợ PWM |
| `LED_BLUE`  | Hỗ trợ PWM |
| `LED_GREEN` | Hỗ trợ PWM |

### Giá trị `value`

| Value | Ý nghĩa               |
| ----- | --------------------- |
| `0`   | 0% duty cycle         |
| `127` | Khoảng 50% duty cycle |
| `255` | 100% duty cycle       |

### Ví dụ

```c
analogWrite(LED_BLUE, 128);
```

### Lưu ý

* `analogWrite()` tạo PWM, không phải điện áp analog thật.
* Chân truyền vào phải hỗ trợ PWM.
* Giá trị càng lớn thì duty cycle càng cao.
* Với LED active LOW, cảm giác sáng/tối có thể ngược tùy cách phần cứng được đấu.

---

## Gợi ý cách chọn API

| Nhu cầu                                    | Hàm nên dùng                                                |
| ------------------------------------------ | ----------------------------------------------------------- |
| Đọc cảm biến đơn giản                      | `analogRead()`                                              |
| Đọc điện áp dễ hiểu hơn                    | `analogReadMilliVolts()`                                    |
| Đọc ADC nhưng không muốn chặn chương trình | `analogStart()` + `analogAvailable()` + `analogGetResult()` |
| Điều chỉnh độ sáng LED                     | `analogWrite()`                                             |

---

## Tutorials

Bạn có thể tìm hiểu cách dùng Analog API qua các bài tutorial:

* Analog read blocking
* Analog read non-blocking
* PWM fade LED

##