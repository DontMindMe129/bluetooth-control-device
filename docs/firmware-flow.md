# Luồng hoạt động và luồng dữ liệu

## Một vòng superloop

```mermaid
flowchart TD
    A[App_Service tick hiện tại] --> B[Console UART]
    B --> C[DHT11 service]
    C --> D[I2C2 + ADXL345]
    D --> E[Motion monitor]
    E --> F[Environment monitor]
    F --> G[Debounce và lấy sự kiện 5 nút]
    G --> H[Điều hướng UI / manual warning / output control]
    H --> I[Tổng hợp warning]
    I --> J[Cập nhật display model]
    J --> K[I2C1 + OLED state machine]
    K --> L[Heartbeat + PWM + status snapshot]
    L --> A
```

Không bước nào chờ một peripheral hoàn thành. Nếu I2C đang truyền, vòng lặp vẫn phục vụ cảm biến, nút, UART và heartbeat.

## DHT11 → Environment → phản hồi

```mermaid
flowchart LR
    D[DHT11 raw 4 byte] --> M[Environment Monitor]
    M -->|temperature, humidity, fresh/stale, condition| DISP[Environment Display]
    M --> FB[Environment Feedback]
    FB -->|manual / automatic warning| WF[Warning Feedback]
    DISP --> OLED[OLED page Environment]
    WF --> OUT[5 digital outputs]
```

Các ngưỡng hiện tại:

- Warm từ 28 °C.
- Humid từ 65%.
- Chỉ `WARM_AND_HUMID` là automatic warning.
- Không có mẫu hợp lệ mới trong 6 giây thì dữ liệu stale; mẫu hợp lệ cuối vẫn được giữ.

Manual warning được bật bằng OK tại trang Environment hoặc Motion. Mẫu DHT11 hợp lệ tiếp theo gỡ nguồn manual; nếu môi trường thật sự warm-and-humid thì nguồn automatic tiếp tục giữ warning.

## ADXL345 → Motion → phản hồi

```mermaid
flowchart LR
    A[ADXL345 XYZ mg] --> M[Motion Monitor]
    M -->|orientation| D[Motion Display]
    M -->|still / moving / shaking| D
    M -->|shaking| W[Warning Feedback]
    D --> O[OLED page Motion]
    W --> OUT[5 digital outputs]
```

Motion monitor lọc thành phần trọng lực để suy ra hướng ±X/±Y/±Z và dùng phần chuyển động còn lại để phân loại still, moving hoặc shaking.

## Nút và ba trang OLED

```text
Left/Right: Environment ⇄ Motion ⇄ Outputs

Trang Environment/Motion:
    OK → manual warning

Trang Outputs:
    Up/Down → đổi output đang chọn
    OK      → bật/tắt output đó
```

Khi warning chạy, `output_control` vẫn giữ mask người dùng. `effective_output_mask` tạm lấy pattern cảnh báo. Khi tất cả nguồn warning biến mất, output trở lại mask đã lưu.

## OLED và phục hồi I2C1

Framebuffer SSD1306 gồm 1024 byte. Driver khóa framebuffer trong lúc truyền để graphics không ghi đè dữ liệu HAL đang mượn.

```mermaid
stateDiagram-v2
    [*] --> Scanning
    Scanning --> Initializing: tìm thấy 0x3C hoặc 0x3D
    Scanning --> Offline: không thấy OLED
    Initializing --> Active: init thành công
    Active --> RetryWait: lỗi giao dịch
    RetryWait --> Initializing: sau 200 ms
    RetryWait --> Offline: 3 NACK liên tiếp
    RetryWait --> BusRecovery: lỗi bus / hết lượt retry
    BusRecovery --> Initializing: bus-clear thành công
    BusRecovery --> Offline: tối đa 2 lần thất bại
    Offline --> Initializing: probe 2 giây tìm thấy OLED
```

Bus-clear tạm DeInit I2C1, chuyển PB6/PB7 thành GPIO open-drain, phát tối đa 9 xung SCL, tạo STOP rồi gọi lại `HAL_I2C_Init()`. I2C2 của ADXL345 độc lập nên không bị chiếm chân.

## UART

USART2 RX nhận từng byte bằng ngắt và đưa vào ring buffer. `command_console` lấy byte trong main context, ghép dòng khi gặp `\r`/`\n`, sau đó `app_commands` tìm handler.

TX cũng dùng ring buffer và interrupt. `UartLog_Printf()` định dạng vào buffer tĩnh rồi xếp byte để gửi; nó không chờ UART truyền xong.

Log OLED recovery chỉ được phát khi trạng thái thay đổi, không phát định kỳ ở trạng thái ổn định.
