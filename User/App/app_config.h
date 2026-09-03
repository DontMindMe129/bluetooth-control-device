/**
 * @file app_config.h
 * @brief Các tham số hành vi cấp ứng dụng đã được duyệt cho firmware hiện tại.
 */

#ifndef USER_APP_APP_CONFIG_H_
#define USER_APP_APP_CONFIG_H_

/** @brief Số byte tối đa được giữ trong hàng đợi nhận UART từ máy tính. */
#define APP_PC_SERIAL_RX_BUFFER_CAPACITY         (128U)

/** @brief Số byte tối đa được giữ trong hàng đợi truyền UART tới máy tính. */
#define APP_PC_SERIAL_TX_BUFFER_CAPACITY         (256U)

/** @brief Số ký tự tối đa của một dòng command, không tính ký tự kết thúc chuỗi. */
#define APP_COMMAND_MAX_LENGTH                   (64U)

/** @brief Sức chứa buffer command gồm 64 ký tự và một ký tự '\0'. */
#define APP_COMMAND_LINE_BUFFER_CAPACITY         (APP_COMMAND_MAX_LENGTH + 1U)

/** @brief Số thành phần argc tối đa, tính cả tên command tại argv[0]. */
#define APP_COMMAND_MAX_ARGUMENTS                (8U)

/** @brief Dòng nhập dở quá thời gian này sẽ bị hủy, không được thực thi. */
#define APP_COMMAND_INCOMPLETE_TIMEOUT_MS        (10000UL)

/** @brief Số byte RX tối đa parser được lấy trong một vòng superloop. */
#define APP_COMMAND_MAX_BYTES_PER_SERVICE        (128U)

/** @brief Buffer tĩnh dùng cho mỗi lần định dạng log kiểu printf. */
#define APP_UART_LOG_FORMAT_BUFFER_CAPACITY      (128U)

/** @brief Thời gian debounce dùng chung cho năm nút điều hướng UI. */
#define APP_UI_BUTTON_DEBOUNCE_MS              (30UL)

/** @brief Timeout đã duyệt cho mỗi giao dịch I2C của ADXL345. */
#define APP_ADXL345_TRANSFER_TIMEOUT_MS        (20UL)

/** @brief Khoảng chờ đã duyệt giữa hai lần khởi tạo ADXL345. */
#define APP_ADXL345_INIT_RETRY_DELAY_MS        (100UL)

/** @brief Tổng số lần khởi tạo ADXL345 tối đa đã duyệt. */
#define APP_ADXL345_INIT_MAX_ATTEMPTS          (3U)

/** @brief Thời gian không có mẫu trước khi dữ liệu ADXL345 bị xem là stale. */
#define APP_ADXL345_STALE_TIMEOUT_MS           (200UL)

/** @brief Hệ số lọc trọng lực IIR đã duyệt: gravity += delta / 16. */
#define APP_MOTION_GRAVITY_FILTER_DIVISOR      (16U)

/** @brief Trục phải đạt ít nhất mức này mới được dùng để xác định tư thế. */
#define APP_MOTION_ORIENTATION_MINIMUM_MG      (700UL)

/** @brief Trục chiếm ưu thế phải hơn trục lớn thứ hai ít nhất mức này. */
#define APP_MOTION_ORIENTATION_MARGIN_MG       (200UL)

/** @brief Tín hiệu động không vượt mức này được xem là ứng viên STILL. */
#define APP_MOTION_STILL_THRESHOLD_MG          (80UL)

/** @brief Tín hiệu động đạt mức này sẽ chuyển ngay sang SHAKING. */
#define APP_MOTION_SHAKING_ENTER_THRESHOLD_MG (400UL)

/** @brief SHAKING chỉ bắt đầu thoát khi tín hiệu xuống dưới mức này. */
#define APP_MOTION_SHAKING_EXIT_THRESHOLD_MG  (250UL)

/** @brief Thời gian yên liên tục trước khi xác nhận trạng thái STILL. */
#define APP_MOTION_STILL_CONFIRMATION_MS       (500UL)

/** @brief Thời gian dịu liên tục trước khi thoát trạng thái SHAKING. */
#define APP_MOTION_SHAKING_EXIT_CONFIRMATION_MS (300UL)

/* -------------------------------------------------------------------------- */
/* Kiểm thử ngõ PWM servo bằng LED                                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief Bật kịch bản kiểm thử độ sáng LED trên ngõ PWM dành cho servo.
 * @warning Đặt về 0 trước khi nối servo thật vì test sử dụng duty cycle 0-100%.
 */
#define APP_SERVO_PWM_LED_TEST_ENABLED          (1U)

/** @brief Thời gian giữ mỗi mức sáng trong kịch bản kiểm thử PWM. */
#define APP_SERVO_PWM_LED_TEST_STEP_TIME_MS     (2000UL)

/** @brief Thời gian LED heartbeat sáng trong mỗi chu kỳ. */
#define APP_HEARTBEAT_ON_TIME_MS               (250UL)

/** @brief Thời gian LED heartbeat tắt trong mỗi chu kỳ. */
#define APP_HEARTBEAT_OFF_TIME_MS              (750UL)

/** @brief Timeout đã duyệt cho lần probe địa chỉ OLED cố định khi thiết bị offline. */
#define APP_OLED_PROBE_TIMEOUT_MS               (5UL)

/** @brief Timeout đã duyệt cho mỗi lần giành bus hoặc truyền dữ liệu SSD1306. */
#define APP_OLED_TRANSFER_TIMEOUT_MS            (150UL)

/** @brief Tổng số lần thử nhanh cho một đợt giao tiếp OLED, tính cả lần đầu. */
#define APP_OLED_FAST_ATTEMPT_LIMIT              (3U)

/** @brief Khoảng nghỉ giữa hai lần thử giao tiếp OLED liên tiếp. */
#define APP_OLED_RETRY_DELAY_MS                  (200UL)

/** @brief Số lỗi NACK liên tiếp trước khi xác nhận OLED offline. */
#define APP_OLED_NACK_OFFLINE_THRESHOLD          (3U)

/** @brief Số lần bus-clear tối đa trong một đợt lỗi vật lý của I2C1 dùng chung. */
#define APP_SHARED_I2C_BUS_RECOVERY_ATTEMPT_LIMIT (2U)

/** @brief Khoảng nghỉ giữa hai lần bus-clear I2C1 dùng chung. */
#define APP_SHARED_I2C_BUS_RECOVERY_DELAY_MS      (300UL)

/** @brief Chu kỳ probe OLED khi thiết bị đang được xem là offline. */
#define APP_OLED_OFFLINE_PROBE_PERIOD_MS         (2000UL)

/** @brief Contrast ban đầu của SSD1306 theo giá trị reset mặc định. */
#define APP_OLED_INITIAL_CONTRAST                (0x7FU)

#endif /* USER_APP_APP_CONFIG_H_ */
