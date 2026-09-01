# Bắt đầu với project

## 1. Clone và mở project

```powershell
git clone <repository-url>
cd Bluetooth_Control_Device
code .
```

Mở `Bluetooth_Control_Device.ioc` bằng STM32CubeMX khi cần thay đổi clock, pin, peripheral hoặc NVIC.

## 2. Cài công cụ

- CMake 3.22+.
- Ninja.
- Arm GNU Toolchain.
- STM32CubeMX/Cube extension phù hợp.
- STM32Cube ST-Link GDB Server và ST-Link V2 nếu nạp/debug.

Kiểm tra nhanh:

```powershell
cmake --version
ninja --version
arm-none-eabi-gcc --version
```

## 3. Chọn preset

Mỗi cấu hình có build tree riêng:

```text
build/Debug/
build/SizeDebug/
build/Release/
```

Build tree không được commit lên Git.

### Debug

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

### SizeDebug

```powershell
cmake --preset SizeDebug
cmake --build --preset SizeDebug
```

`SizeDebug` phù hợp khi Flash chật nhưng vẫn cần symbols. Tối ưu hóa có thể khiến một số biến cục bộ khó quan sát hơn Debug.

### Release

```powershell
cmake --preset Release
cmake --build --preset Release
```

## 4. Nạp và debug

1. Nối ST-Link V2 với SWDIO PA13, SWCLK PA14, GND và reference voltage phù hợp.
2. Chọn đúng executable trong build tree mong muốn.
3. Chạy `STM32Cube: Launch ST-Link GDB Server` trong VS Code.
4. Xác nhận artifact và preset trước khi nạp.

File `.elf` chứa code, dữ liệu và symbols debug. MCU chỉ nhận các section cần nạp; metadata debug không chiếm Flash trên chip.

## 5. Khi thay đổi pin bằng CubeMX

1. Sửa `.ioc` và giữ label tổng quát như `DIGITAL_OUTPUT_1`.
2. Generate code lại.
3. Kiểm tra thay đổi trong `Core/` và CMake CubeMX.
4. Cập nhật `User/Board/board_config.h` nếu thiết bị đổi cổng.
5. Build và kiểm thử ngoại vi bị ảnh hưởng.

Không sửa generated code bên ngoài `USER CODE BEGIN/END`, vì lần generate sau có thể ghi đè.

## 6. Debug trạng thái hệ thống

Context ứng dụng nằm trong biến tĩnh `s_app` của `User/App/app.c`. Khi dừng tại `App_Service()`, có thể theo dõi:

```text
s_app.status.environment
s_app.status.adxl345
s_app.status.motion_monitor
s_app.status.warning_feedback
s_app.status.output_control
s_app.status.oled_state
s_app.status.oled_i2c_bus
s_app.status.pc_serial
```

Trong preset có tối ưu, debugger có thể hiển thị một số biểu thức kém ổn định hơn Debug. Hãy đặt breakpoint tại nơi biến được dùng thực tế.
