# Digital API

## Tổng quan

Digital API dùng để điều khiển và đọc tín hiệu digital trong EduFramework.

Bạn có thể dùng để:

- Điều khiển LED
- Đọc nút nhấn
- Làm việc với GPIO ngoài
- Gửi và nhận tín hiệu HIGH/LOW

---

## Danh sách các hàm

| Hàm               | Mô tả                    |
| ----------------- | ------------------------ |
| `pinMode()`       | Cấu hình chế độ của chân |
| `digitalWrite()`  | Ghi mức HIGH hoặc LOW    |
| `digitalRead()`   | Đọc mức HIGH hoặc LOW    |
| `digitalToggle()` | Đảo trạng thái chân      |

---

## pinMode()

### Mục đích

Cấu hình chế độ hoạt động của một chân.

### Cú pháp

```c
pinMode(pin, mode);
```

### Các mode

| Mode             | Mô tả              | Khi dùng               |
| ---------------- | ------------------ | ---------------------- |
| `INPUT`          | Chân input         | Đọc tín hiệu           |
| `OUTPUT`         | Chân output        | Điều khiển LED, module |
| `INPUT_PULLUP`   | Input có kéo lên   | Button nối GND         |
| `INPUT_PULLDOWN` | Input có kéo xuống | Button nối VCC         |

### Ví dụ

```c
pinMode(LED_RED, OUTPUT);
pinMode(BTN0, INPUT_PULLUP);
```

### Lưu ý

- Nên gọi `pinMode()` trước khi dùng các hàm khác.
- Nếu không dùng pull-up/pull-down, tín hiệu input có thể bị nhiễu (floating).
- Với button, nên dùng `INPUT_PULLUP` hoặc `INPUT_PULLDOWN`.
- Framework tự bật lọc nhiễu cơ bản (passive filter) cho các nút nhấn trên board.
- Passive filter không thay thế hoàn toàn debounce bằng phần mềm.

---

## digitalWrite()

### Mục đích

Ghi mức logic ra một chân output.

### Cú pháp

```c
digitalWrite(pin, value);
```

### Giá trị

| Value  | Mô tả    |
| ------ | -------- |
| `HIGH` | Mức cao  |
| `LOW`  | Mức thấp |

### Ví dụ

```c
digitalWrite(LED_RED, LOW);
digitalWrite(GPIO1, HIGH);
```

### Lưu ý

- Chân nên được cấu hình là `OUTPUT` trước.
- LED trên board là **active LOW**:
  - `LOW` → bật LED
  - `HIGH` → tắt LED

- Với thiết bị ngoài, HIGH/LOW phụ thuộc vào cách đấu mạch.

---

## digitalRead()

### Mục đích

Đọc trạng thái logic của một chân input.

### Cú pháp

```c
bool state = digitalRead(pin);
```

### Giá trị trả về

| Giá trị | Mô tả |
| ------- | ----- |
| `true`  | HIGH  |
| `false` | LOW   |

### Ví dụ

```c
bool state = digitalRead(BTN0);
```

### Lưu ý

- Chân nên được cấu hình là `INPUT`, `INPUT_PULLUP` hoặc `INPUT_PULLDOWN`.
- Nếu không có điện trở kéo, tín hiệu có thể bị nhiễu.
- Với `INPUT_PULLUP`:
  - Chưa nhấn → HIGH
  - Nhấn → LOW

- Button có thể bị dội phím, cần debounce nếu xử lý nhiều lần.

---

## digitalToggle()

### Mục đích

Đảo trạng thái hiện tại của một chân output.

### Cú pháp

```c
digitalToggle(pin);
```

### Ví dụ

```c
digitalToggle(LED_BLUE);
```

### Lưu ý

- Chân nên được cấu hình là `OUTPUT`.
- Dùng để nháy LED hoặc đổi trạng thái nhanh.
- Hoạt động độc lập với việc LED là active LOW hay HIGH.

---

## Tutorials

Bạn có thể tìm hiểu về cách dùng các hàm xử lý tín hiệu digital qua bài tutorial về btn_led(button + led control).

##
