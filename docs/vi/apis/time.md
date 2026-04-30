# Time API

## Tổng quan

Time API dùng để tạo thời gian chờ và đo thời gian chạy của chương trình trong EduFramework.

Bạn có thể dùng để:

* Tạo delay theo mili giây
* Đọc thời gian hệ thống đã chạy
* Viết các chương trình không cần tự cấu hình timer
* Xây dựng logic theo thời gian

Time API được xây dựng dựa trên timer phần cứng, nên thời gian ổn định và chính xác hơn so với việc tạo delay bằng vòng lặp thủ công.

---

## Danh sách các hàm

| Hàm        | Mô tả                                                  |
| ---------- | ------------------------------------------------------ |
| `millis()` | Trả về thời gian hệ thống đã chạy, tính bằng mili giây |
| `delay()`  | Dừng chương trình trong một khoảng thời gian           |
|            |                                                        |

---

## millis()

### Mục đích

`millis()` trả về số mili giây đã trôi qua kể từ khi hệ thống time được khởi tạo.

Hàm này thường dùng để:

* Đo thời gian
* Kiểm tra một khoảng thời gian đã trôi qua hay chưa
* Viết chương trình không blocking

### Cú pháp

```c
uint32_t currentTime = millis();
```

### Giá trị trả về

| Giá trị    | Mô tả                         |
| ---------- | ----------------------------- |
| `uint32_t` | Thời gian tính bằng mili giây |

### Ví dụ

```c
uint32_t start = millis();

while ((millis() - start) < 1000)
{
    // wait for 1000 ms
}
```

### Lưu ý

* Đơn vị của `millis()` là mili giây.
* Giá trị trả về sẽ tăng liên tục theo thời gian.
* `millis()` phù hợp để viết các chương trình cần kiểm tra thời gian mà không muốn dùng `delay()`.
* Khi so sánh thời gian, nên dùng dạng `millis() - start` thay vì so sánh trực tiếp với một mốc tuyệt đối.

---

## delay()

### Mục đích

`delay()` dừng chương trình trong một khoảng thời gian nhất định.

Hàm này thường dùng khi bạn muốn tạo thời gian chờ đơn giản.

### Cú pháp

```c
delay(ms);
```

### Tham số

| Tham số | Mô tả                                  |
| ------- | -------------------------------------- |
| `ms`    | Thời gian cần chờ, tính bằng mili giây |

### Ví dụ

```c
delay(500);
```

Ví dụ trên sẽ dừng chương trình trong khoảng 500 ms.

### Lưu ý

* `delay()` là hàm blocking.
* Trong thời gian delay, chương trình sẽ đứng chờ tại đó.
* Không nên dùng `delay()` nếu chương trình cần xử lý nhiều tác vụ cùng lúc.
* Với các bài đơn giản như blink LED, `delay()` rất dễ dùng và phù hợp.
* Với chương trình phức tạp hơn, nên dùng `millis()` để kiểm soát thời gian linh hoạt hơn.

---

## Độ chính xác thời gian

Time API sử dụng timer phần cứng bên dưới framework.

Trong implementation hiện tại:

* Timer tick được cấu hình theo chu kỳ 1 ms
* Mỗi tick làm tăng biến thời gian hệ thống
* `millis()` đọc giá trị thời gian này
* `delay()` chờ dựa trên giá trị của `millis()`

Vì sử dụng timer phần cứng, thời gian ổn định hơn nhiều so với delay bằng vòng lặp CPU thủ công.

---

## Tutorials

Bạn có thể tìm hiểu cách dùng Time API qua bài tutorial về blink LED 

##