# Đóng góp vào project

## Quy trình đề xuất

1. Tạo branch từ `main`.
2. Thay đổi đúng một module hoặc một mục tiêu rõ ràng.
3. Giữ đường chạy firmware không blocking và không dùng cấp phát động.
4. Build preset phù hợp và xử lý toàn bộ warning.
5. Kiểm thử phần cứng liên quan.
6. Commit với nội dung mô tả hành vi, không chỉ tên file.

Ví dụ:

```text
feature/adxl345-recovery
fix: recover ADXL345 sampling after missed data-ready event
docs: explain OLED I2C recovery flow
```

## Ranh giới CubeMX

- `Bluetooth_Control_Device.ioc` là nguồn cấu hình pin, clock, peripheral và NVIC.
- `Core/` do CubeMX quản lý.
- Chỉ sửa generated file trong đúng cặp `USER CODE BEGIN/END`.
- Không sửa thư viện vendor trong `Drivers/` nếu không có lý do đặc biệt.
- Thư viện tự viết đặt dưới `User/`.

Sau khi generate lại, luôn xem diff trước khi commit để phát hiện CubeMX đổi clock, pin, IRQ hoặc xóa user code ngoài vùng bảo vệ.

## Phân tầng module

- `Board`: ánh xạ thiết bị vào tài nguyên đã cấu hình.
- `Drivers`: giao tiếp phần cứng và giao thức.
- `Services`: thuật toán, monitor, display, feedback.
- `App`: policy và điều phối liên-module.

Không để display tự điều khiển GPIO warning, driver quyết định page OLED, hoặc service tự cấu hình pin CubeMX.

## Quy tắc runtime

- Không dùng `malloc`, `calloc`, `realloc` hoặc `free`.
- Không busy-wait, không `HAL_Delay()` trong module tự viết.
- Mọi giao dịch có thể treo phải có timeout hữu hạn.
- ISR phải ngắn; hoãn xử lý dài về main context.
- Buffer truyền bất đồng bộ phải tồn tại và không bị ghi đè đến khi có kết quả.
- Mọi buffer phải có giới hạn rõ ràng.

## Trước khi mở Pull Request

- [ ] Source build không có error/warning.
- [ ] Flash/RAM còn trong giới hạn linker script.
- [ ] Không commit `build/`, ELF, map, bin hoặc hex.
- [ ] Không commit token, mật khẩu hoặc thông tin cá nhân.
- [ ] README/docs được cập nhật nếu hành vi người dùng thay đổi.
- [ ] Pin/peripheral trong tài liệu khớp `.ioc` và generated source.
- [ ] Có mô tả cách kiểm thử và kết quả thực tế.
