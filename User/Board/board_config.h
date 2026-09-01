/**
 * @file board_config.h
 * @brief Ánh xạ thiết bị của ứng dụng vào các tài nguyên đã đặt label trong CubeMX.
 *
 * File này không cấu hình GPIO, timer, I2C, UART hoặc NVIC. Cấu hình phần cứng
 * thực tế vẫn do Bluetooth_Control_Device.ioc và mã CubeMX sinh ra quản lý.
 * Khi đổi vị trí một thiết bị, hãy cấu hình lại CubeMX nếu cần rồi chỉ cập nhật
 * ánh xạ vai trò tương ứng tại đây.
 */

#ifndef USER_BOARD_BOARD_CONFIG_H_
#define USER_BOARD_BOARD_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* -------------------------------------------------------------------------- */
/* Kết nối truyền thông                                                       */
/* -------------------------------------------------------------------------- */

/** @brief Peripheral UART hiện được dùng cho liên kết nối tiếp với máy tính. */
#define BOARD_PC_SERIAL_UART_INSTANCE                 USART2

/** @brief Peripheral I2C dành cố định cho màn hình OLED. */
#define BOARD_OLED_I2C_INSTANCE                       I2C1

/** @brief GPIO port SCL của cổng OLED; dùng khi bus-clear tạm chiếm chân I2C1. */
#define BOARD_OLED_I2C_SCL_PORT                       I2C_PORT_1_SCL_GPIO_Port

/** @brief GPIO pin SCL của cổng OLED. */
#define BOARD_OLED_I2C_SCL_PIN                        I2C_PORT_1_SCL_Pin

/** @brief GPIO port SDA của cổng OLED; dùng khi bus-clear tạm chiếm chân I2C1. */
#define BOARD_OLED_I2C_SDA_PORT                       I2C_PORT_1_SDA_GPIO_Port

/** @brief GPIO pin SDA của cổng OLED. */
#define BOARD_OLED_I2C_SDA_PIN                        I2C_PORT_1_SDA_Pin

/** @brief Peripheral I2C dành cố định cho cảm biến ADXL345. */
#define BOARD_ADXL345_I2C_INSTANCE                    I2C2

/** @brief Cổng interrupt đa dụng hiện được gán cho tín hiệu interrupt ADXL345. */
#define BOARD_ADXL345_INTERRUPT_PORT                  INTERRUPT_INPUT_6_GPIO_Port

/** @brief Pin interrupt đa dụng hiện được gán cho tín hiệu interrupt ADXL345. */
#define BOARD_ADXL345_INTERRUPT_PIN                   INTERRUPT_INPUT_6_Pin

/* -------------------------------------------------------------------------- */
/* Cảm biến DHT11                                                             */
/* -------------------------------------------------------------------------- */

/** @brief Timer được gán cho input capture của DHT11. */
#define BOARD_DHT11_TIMER_INSTANCE                    TIM3

/** @brief Channel input capture được gán cho DHT11. */
#define BOARD_DHT11_TIMER_CHANNEL                     TIM_CHANNEL_4

/** @brief GPIO port của cổng capture đa dụng hiện được gán cho DATA DHT11. */
#define BOARD_DHT11_DATA_PORT                         CAPTURE_IO_1_GPIO_Port

/** @brief GPIO pin của cổng capture đa dụng hiện được gán cho DATA DHT11. */
#define BOARD_DHT11_DATA_PIN                          CAPTURE_IO_1_Pin

/* -------------------------------------------------------------------------- */
/* Servo                                                                      */
/* -------------------------------------------------------------------------- */

/** @brief Timer riêng được gán cho tín hiệu PWM điều khiển servo. */
#define BOARD_SERVO_TIMER_INSTANCE                    TIM2

/**
 * @brief Channel PWM hiện được gán cho servo.
 * @note TIM2 tách riêng khỏi TIM3 đang phục vụ input capture DHT11.
 *       Các driver không được tự thay đổi timer base sau khi đã khởi tạo.
 */
#define BOARD_SERVO_TIMER_CHANNEL                     TIM_CHANNEL_1

/** @brief Cổng PWM đa dụng hiện được gán cho servo. */
#define BOARD_SERVO_PWM_PORT                          PWM_OUTPUT_1_GPIO_Port

/** @brief Pin PWM đa dụng hiện được gán cho servo. */
#define BOARD_SERVO_PWM_PIN                           PWM_OUTPUT_1_Pin

/* -------------------------------------------------------------------------- */
/* Năm nút điều hướng giao diện                                              */
/* -------------------------------------------------------------------------- */

/** @brief Cổng nút OK/Enter; nút này cũng phát cảnh báo manual ngoài trang Outputs. */
#define BOARD_UI_OK_BUTTON_PORT                       INTERRUPT_INPUT_1_GPIO_Port

/** @brief Pin nút OK/Enter, hiện ánh xạ vào PA4. */
#define BOARD_UI_OK_BUTTON_PIN                        INTERRUPT_INPUT_1_Pin

/** @brief Mức GPIO xác nhận nút OK/Enter đang được nhấn. */
#define BOARD_UI_OK_BUTTON_PRESSED_LEVEL              GPIO_PIN_RESET

/** @brief Cổng nút chuyển sang page OLED bên trái, hiện ánh xạ vào PA5. */
#define BOARD_UI_LEFT_BUTTON_PORT                     INTERRUPT_INPUT_2_GPIO_Port

/** @brief Pin nút chuyển sang page OLED bên trái, hiện ánh xạ vào PA5. */
#define BOARD_UI_LEFT_BUTTON_PIN                      INTERRUPT_INPUT_2_Pin

/** @brief Mức GPIO xác nhận nút Left đang được nhấn. */
#define BOARD_UI_LEFT_BUTTON_PRESSED_LEVEL            GPIO_PIN_RESET

/** @brief Cổng nút chuyển sang page OLED bên phải, hiện ánh xạ vào PA6. */
#define BOARD_UI_RIGHT_BUTTON_PORT                    INTERRUPT_INPUT_3_GPIO_Port

/** @brief Pin nút chuyển sang page OLED bên phải, hiện ánh xạ vào PA6. */
#define BOARD_UI_RIGHT_BUTTON_PIN                     INTERRUPT_INPUT_3_Pin

/** @brief Mức GPIO xác nhận nút Right đang được nhấn. */
#define BOARD_UI_RIGHT_BUTTON_PRESSED_LEVEL           GPIO_PIN_RESET

/** @brief Cổng nút di chuyển lựa chọn lên, hiện ánh xạ vào PA7. */
#define BOARD_UI_UP_BUTTON_PORT                       INTERRUPT_INPUT_4_GPIO_Port

/** @brief Pin nút di chuyển lựa chọn lên, hiện ánh xạ vào PA7. */
#define BOARD_UI_UP_BUTTON_PIN                        INTERRUPT_INPUT_4_Pin

/** @brief Mức GPIO xác nhận nút Up đang được nhấn. */
#define BOARD_UI_UP_BUTTON_PRESSED_LEVEL              GPIO_PIN_RESET

/** @brief Cổng nút di chuyển lựa chọn xuống, hiện ánh xạ vào PB0. */
#define BOARD_UI_DOWN_BUTTON_PORT                     INTERRUPT_INPUT_5_GPIO_Port

/** @brief Pin nút di chuyển lựa chọn xuống, hiện ánh xạ vào PB0. */
#define BOARD_UI_DOWN_BUTTON_PIN                      INTERRUPT_INPUT_5_Pin

/** @brief Mức GPIO xác nhận nút Down đang được nhấn. */
#define BOARD_UI_DOWN_BUTTON_PRESSED_LEVEL            GPIO_PIN_RESET

/* -------------------------------------------------------------------------- */
/* Năm ngõ ra số đa dụng                                                     */
/* -------------------------------------------------------------------------- */

/** @brief Cổng của ngõ ra số thứ nhất. */
#define BOARD_DIGITAL_OUTPUT_1_PORT                   DIGITAL_OUTPUT_1_GPIO_Port

/** @brief Pin của ngõ ra số thứ nhất. */
#define BOARD_DIGITAL_OUTPUT_1_PIN                    DIGITAL_OUTPUT_1_Pin

/** @brief Ngõ ra số thứ nhất active-high. */
#define BOARD_DIGITAL_OUTPUT_1_ACTIVE_LEVEL           GPIO_PIN_SET

/** @brief Cổng của ngõ ra số thứ hai. */
#define BOARD_DIGITAL_OUTPUT_2_PORT                   DIGITAL_OUTPUT_2_GPIO_Port

/** @brief Pin của ngõ ra số thứ hai. */
#define BOARD_DIGITAL_OUTPUT_2_PIN                    DIGITAL_OUTPUT_2_Pin

/** @brief Ngõ ra số thứ hai active-high. */
#define BOARD_DIGITAL_OUTPUT_2_ACTIVE_LEVEL           GPIO_PIN_SET

/** @brief Cổng của ngõ ra số thứ ba. */
#define BOARD_DIGITAL_OUTPUT_3_PORT                   DIGITAL_OUTPUT_3_GPIO_Port

/** @brief Pin của ngõ ra số thứ ba. */
#define BOARD_DIGITAL_OUTPUT_3_PIN                    DIGITAL_OUTPUT_3_Pin

/** @brief Ngõ ra số thứ ba active-high. */
#define BOARD_DIGITAL_OUTPUT_3_ACTIVE_LEVEL           GPIO_PIN_SET

/** @brief Cổng của ngõ ra số thứ tư. */
#define BOARD_DIGITAL_OUTPUT_4_PORT                   DIGITAL_OUTPUT_4_GPIO_Port

/** @brief Pin của ngõ ra số thứ tư. */
#define BOARD_DIGITAL_OUTPUT_4_PIN                    DIGITAL_OUTPUT_4_Pin

/** @brief Ngõ ra số thứ tư active-high. */
#define BOARD_DIGITAL_OUTPUT_4_ACTIVE_LEVEL           GPIO_PIN_SET

/** @brief Cổng của ngõ ra số thứ năm, hiện ánh xạ vào PA8. */
#define BOARD_DIGITAL_OUTPUT_5_PORT                   DIGITAL_OUTPUT_5_GPIO_Port

/** @brief Pin của ngõ ra số thứ năm, hiện ánh xạ vào PA8. */
#define BOARD_DIGITAL_OUTPUT_5_PIN                    DIGITAL_OUTPUT_5_Pin

/** @brief Ngõ ra số thứ năm active-high. */
#define BOARD_DIGITAL_OUTPUT_5_ACTIVE_LEVEL           GPIO_PIN_SET

/* -------------------------------------------------------------------------- */
/* LED heartbeat                                                             */
/* -------------------------------------------------------------------------- */

/** @brief GPIO port của LED tích hợp đang được dùng làm heartbeat. */
#define BOARD_HEARTBEAT_LED_PORT                      BOARD_LED_GPIO_Port

/** @brief GPIO pin của LED tích hợp đang được dùng làm heartbeat. */
#define BOARD_HEARTBEAT_LED_PIN                       BOARD_LED_Pin

/** @brief Mức GPIO làm LED heartbeat tích hợp trên Blue Pill sáng. */
#define BOARD_HEARTBEAT_LED_ACTIVE_LEVEL              GPIO_PIN_RESET

#ifdef __cplusplus
}
#endif

#endif /* USER_BOARD_BOARD_CONFIG_H_ */
