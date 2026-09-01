# Phần cứng và kết nối

## Vi điều khiển và clock

- MCU: STM32F103C8T6, package LQFP48.
- Board đang dùng: Blue Pill development board.
- Clock runtime hiện tại: HSI 8 MHz.
- SWD được giữ trên PA13/PA14; JTAG không được sử dụng.
- Linker script khai báo 64 KiB Flash và 20 KiB RAM.

## Bảng chân

| Chân | Label CubeMX | Vai trò hiện tại | Cấu hình chính |
|---|---|---|---|
| PC13 | `BOARD_LED` | Heartbeat | Output, active-low |
| PA0 | `PWM_OUTPUT_1` | PWM/servo | TIM2 CH1 |
| PA1 | `INTERRUPT_INPUT_6` | ADXL345 INT1 | EXTI1, no-pull |
| PA2 | `UART_PORT_1_TX` | PC UART TX | USART2 |
| PA3 | `UART_PORT_1_RX` | PC UART RX | USART2 |
| PA4 | `INTERRUPT_INPUT_1` | OK | EXTI falling, pull-up |
| PA5 | `INTERRUPT_INPUT_2` | Left | EXTI falling, pull-up |
| PA6 | `INTERRUPT_INPUT_3` | Right | EXTI falling, pull-up |
| PA7 | `INTERRUPT_INPUT_4` | Up | EXTI falling, pull-up |
| PB0 | `INTERRUPT_INPUT_5` | Down | EXTI falling, pull-up |
| PB1 | `CAPTURE_IO_1` | DHT11 DATA | TIM3 CH4 falling capture |
| PB6 | `I2C_PORT_1_SCL` | OLED SCL | I2C1, 100 kHz |
| PB7 | `I2C_PORT_1_SDA` | OLED SDA | I2C1, 100 kHz |
| PB10 | `I2C_PORT_2_SCL` | ADXL345 SCL | I2C2, 100 kHz |
| PB11 | `I2C_PORT_2_SDA` | ADXL345 SDA | I2C2, 100 kHz |
| PB12–PB15 | `DIGITAL_OUTPUT_1..4` | Output 1–4 | GPIO active-high |
| PA8 | `DIGITAL_OUTPUT_5` | Output 5 | GPIO active-high |
| PA13 | — | SWDIO | System debug |
| PA14 | — | SWCLK | System debug |

## Timer

TIM2 và TIM3 đều dùng prescaler 7 với clock timer 8 MHz:

```text
timer tick = 8 MHz / (7 + 1) = 1 MHz
1 counter tick = 1 µs
ARR = 19999 → chu kỳ tràn = 20 ms
```

- TIM2 CH1 tạo PWM 20 ms trên PA0.
- TIM3 CH4 đo pulse DHT11 trên PB1. Phép trừ capture có xử lý wrap nên ARR 19999 vẫn đủ cho các pulse DHT11 cỡ microsecond.

## Nút và LED output

Năm nút dùng pull-up, nên nút được xem là nhấn khi chân xuống mức 0. Mỗi nút nối giữa input tương ứng và GND.

Năm LED ngoài là active-high:

```text
GPIO output ── điện trở hạn dòng ── LED ── GND
```

Không nối LED trực tiếp mà thiếu điện trở hạn dòng.

## I2C và pull-up

I2C là bus open-drain nên SCL/SDA cần điện trở pull-up. Nhiều breakout OLED/ADXL345 đã có sẵn, nhưng cần kiểm tra schematic hoặc module thực tế thay vì mặc định.

DHT11 module trong project đã có pull-up trên DATA theo xác nhận phần cứng hiện tại.

## Nguồn servo

Không nên cấp servo SG90 từ chân 5 V của ST-Link nếu chưa xác nhận dòng cấp và sụt áp. Dòng khởi động/stall có thể làm MCU reset hoặc gây nhiễu cảm biến.

Firmware hiện bật bài test PWM bằng LED với duty 0–100%. Phải đặt:

```c
#define APP_SERVO_PWM_LED_TEST_ENABLED (0U)
```

trước khi nối servo thật và chuyển sang pulse width an toàn cho servo.
