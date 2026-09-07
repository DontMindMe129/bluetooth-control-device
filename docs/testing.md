# Kịch bản kiểm thử toàn hệ thống

## Nguyên tắc

Kiểm thử theo thứ tự: nguồn và heartbeat → giao tiếp → cảm biến → UI → output → warning → fault injection. Sau mỗi bước, PC13 phải tiếp tục nháy; một ngoại vi lỗi không được làm toàn bộ superloop dừng.

> PWM đang ở chế độ LED test 0–100%. Không nối servo thật khi `APP_SERVO_PWM_LED_TEST_ENABLED` còn bằng 1.

## Checklist

### 1. Khởi động

- [ ] PC13 sáng 250 ms, tắt 750 ms.
- [ ] OLED vào trang Environment.
- [ ] DHT11 có mẫu sau chu kỳ đọc.
- [ ] ADXL345 có XYZ fresh.
- [ ] LED PWM đổi mức mỗi 2 giây.

### 2. UART

Gửi `help\r\n`, `echo one two 123\r\n` và một lệnh không tồn tại.

- [ ] `help` liệt kê command.
- [ ] `echo` giữ đúng nhiều argument.
- [ ] Lệnh sai trả `ERR unknown command`.
- [ ] Dòng không terminator bị hủy sau 10 giây.

### 3. OLED và nút

- [ ] Left/Right đi qua Environment, Motion, Outputs.
- [ ] Mỗi lần nhấn chỉ tạo một sự kiện sau debounce 30 ms.
- [ ] Up/Down đổi lựa chọn tại Outputs.
- [ ] OK bật/tắt đúng output đang chọn.

### 4. Environment

- [ ] Nhiệt độ và độ ẩm hiển thị đúng.
- [ ] Từ 28 °C được phân loại warm.
- [ ] Từ 65% được phân loại humid.
- [ ] Chỉ warm-and-humid kích hoạt automatic warning.
- [ ] Mất mẫu hơn 6 giây tạo cảnh báo stale nhưng giữ mẫu cuối.

### 5. Motion

- [ ] X/Y/Z đổi khi xoay board.
- [ ] Để yên đủ 500 ms cho trạng thái still.
- [ ] Lắc mạnh tạo shaking và warning.
- [ ] Để yên đủ khoảng thoát 300 ms làm warning shaking biến mất.

### 6. Warning và output arbitration

Đặt trước mask output dễ nhận biết, kích hoạt manual warning rồi tạo thêm shaking.

- [ ] Pattern lần lượt OUT1/3/5 và OUT2/4, mỗi pha 150 ms, sau đó tắt 400 ms.
- [ ] `active_source_mask` giữ đồng thời manual và shaking.
- [ ] Gỡ một nguồn không làm warning dừng nếu nguồn khác còn.
- [ ] Khi hết mọi nguồn, mask output người dùng được khôi phục.

### 7. OLED offline do NACK

- [ ] Rút OLED tạo tối đa 3 lần thử, cách nhau 200 ms.
- [ ] 3 NACK liên tiếp chuyển OLED offline mà không bus-clear.
- [ ] UART chỉ log khi trạng thái thay đổi.
- [ ] Kết nối lại: probe tối đa 2 giây và khôi phục trang hiện tại.

### 8. I2C1 dùng chung bị giữ thấp

Chỉ fault-injection khi hiểu mạch. Nên dùng điện trở khoảng 1 kΩ kéo SDA hoặc SCL xuống GND thay vì chập tùy tiện.

- [ ] Firmware bus-clear tối đa 2 lần, cách nhau 300 ms.
- [ ] Mỗi lần phát không quá 9 xung SCL rồi tạo STOP.
- [ ] Không phục hồi được thì cả hai device manager chuyển sang chính sách offline riêng.
- [ ] Trong lúc bus-clear, OLED và ADXL345 không mở giao dịch I2C mới; dữ liệu hiển thị/Motion có thể tạm stale.
- [ ] UART, DHT11, nút và heartbeat vẫn chạy vì không phụ thuộc I2C1.
- [ ] Bỏ lỗi: OLED online lại và ADXL345 được khởi tạo lại tại `0x53`.

### 9. Lỗi riêng của ADXL345

- [ ] Một hoặc hai lần đọc lỗi rời rạc không làm bus-clear và được xóa bộ đếm khi có mẫu tốt.
- [ ] 3 lần đọc lỗi liên tiếp làm ADXL345 restart sau 100 ms.
- [ ] Stale liên tục 2 giây cũng làm cảm biến restart.
- [ ] Khi không nhận dạng/cấu hình được, manager thử lại mỗi 2 giây mà OLED vẫn tiếp tục hoạt động.
- [ ] Sau khi nối lại, 3 mẫu tốt liên tiếp chuyển ADXL345 sang online.

## Biến debugger quan trọng

```text
s_app.status.oled.state
s_app.status.oled.error
s_app.status.oled.fast_attempt_count
s_app.status.oled.consecutive_nack_count
s_app.status.oled.driver
s_app.status.shared_i2c.state
s_app.status.shared_i2c.recovery_attempt_count
s_app.status.shared_i2c.bus
s_app.status.shared_i2c.physical_recovery_state
s_app.status.environment
s_app.status.adxl345.state
s_app.status.adxl345.consecutive_read_error_count
s_app.status.adxl345.good_sample_count
s_app.status.adxl345.driver
s_app.status.motion_monitor
s_app.status.warning.output_pattern.active_source_mask
s_app.status.output_control.output_on_mask
s_app.status.warning.effective_output_mask
```

## Tiêu chí đạt cuối cùng

Chạy xen kẽ chuyển trang, UART, cảm biến, nút, output và lỗi I2C1 trong ít nhất 5 phút. Không được có reset ngoài ý muốn, heartbeat dừng, output kẹt sau warning hoặc giao dịch OLED/ADXL345 lấy nhầm kết quả của nhau.
