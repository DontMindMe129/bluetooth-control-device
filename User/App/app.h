/**
 * @file app.h
 * @brief Điểm tích hợp cấp ứng dụng cho các driver, service và callback HAL.
 */

#ifndef USER_APP_APP_H_
#define USER_APP_APP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "adxl345.h"
#include "app_commands.h"
#include "command_console.h"
#include "dht11.h"
#include "environment_display.h"
#include "environment_feedback.h"
#include "environment_monitor.h"
#include "i2c_bus.h"
#include "i2c_bus_recovery.h"
#include "motion_display.h"
#include "motion_monitor.h"
#include "output_control.h"
#include "output_display.h"
#include "pwm_output.h"
#include "ssd1306.h"
#include "stm32f1xx_hal.h"
#include "uart_stream.h"
#include "uart_log.h"
#include "warning_feedback.h"

/** @brief Các bước khởi động và vận hành giao diện OLED tại địa chỉ cố định. */
typedef enum
{
    APP_OLED_NOT_STARTED = 0, /**< Chưa có I2C handle hợp lệ để bắt đầu. */
    APP_OLED_INITIALIZING,    /**< Đang khởi tạo SSD1306 tại địa chỉ đã xác nhận. */
    APP_OLED_ACTIVE,          /**< OLED sẵn sàng và page được chọn đang hoạt động. */
    APP_OLED_RETRY_WAIT,      /**< Chờ trước khi thử nhanh lại giao tiếp OLED. */
    APP_OLED_BUS_RECOVERY_WAIT, /**< Chờ trước lần bus-clear vật lý kế tiếp. */
    APP_OLED_BUS_RECOVERING,  /**< PB6/PB7 đang được state machine bus-clear điều khiển. */
    APP_OLED_OFFLINE_WAIT,    /**< OLED offline; chờ đến chu kỳ probe 2 giây. */
    APP_OLED_OFFLINE_PROBING, /**< Đang probe địa chỉ OLED mà không gửi lại framebuffer. */
    APP_OLED_ERROR            /**< Lỗi cấu hình/phần mềm không thể tự phục hồi. */
} App_OledState_t;

/** @brief Nguyên nhân gần nhất của lỗi OLED, kể cả lỗi đang được tự phục hồi. */
typedef enum
{
    APP_OLED_ERROR_NONE = 0,       /**< Chưa có lỗi. */
    APP_OLED_ERROR_INVALID_I2C,    /**< Handle không phải peripheral I2C dùng chung của board. */
    APP_OLED_ERROR_BUS_INIT,       /**< Không thể tạo context I2C bus. */
    APP_OLED_ERROR_DISPLAY_INIT,   /**< SSD1306 từ chối cấu hình hoặc init thất bại. */
    APP_OLED_ERROR_DRAW,           /**< Không thể vẽ giao diện vào framebuffer. */
    APP_OLED_ERROR_REFRESH_REQUEST,/**< Driver từ chối yêu cầu refresh framebuffer. */
    APP_OLED_ERROR_REFRESH_RESULT  /**< Refresh đã bắt đầu nhưng kết thúc với lỗi. */
} App_OledError_t;

/** @brief Các page người dùng có thể chọn bằng nút Left/Right. */
typedef enum
{
    APP_DISPLAY_PAGE_ENVIRONMENT = 0, /**< Nhiệt độ, độ ẩm và trạng thái môi trường. */
    APP_DISPLAY_PAGE_MOTION,          /**< Gia tốc ba trục từ ADXL345. */
    APP_DISPLAY_PAGE_OUTPUTS,         /**< Lựa chọn và bật/tắt năm ngõ ra số. */
    APP_DISPLAY_PAGE_COUNT            /**< Số page; không phải một page hợp lệ. */
} App_DisplayPage_t;

/** @brief Các mức sáng tuần tự của kịch bản kiểm thử ngõ PWM bằng LED. */
typedef enum
{
    APP_SERVO_PWM_LED_TEST_OFF = 0, /**< LED tắt, duty cycle 0%. */
    APP_SERVO_PWM_LED_TEST_100_PERCENT, /**< LED sáng gần duty cycle 100%. */
    APP_SERVO_PWM_LED_TEST_25_PERCENT, /**< LED sáng ở duty cycle 25%. */
    APP_SERVO_PWM_LED_TEST_50_PERCENT, /**< LED sáng ở duty cycle 50%. */
    APP_SERVO_PWM_LED_TEST_75_PERCENT, /**< LED sáng ở duty cycle 75%. */
    APP_SERVO_PWM_LED_TEST_STEP_COUNT /**< Số bước; không phải một bước hợp lệ. */
} App_ServoPwmLedTestStep_t;

/** @brief Snapshot tổng hợp để quan sát trạng thái ứng dụng bằng debugger. */
typedef struct
{
    UartStream_Result_t pc_serial_initialize_result; /**< Kết quả khởi tạo UART nối tiếp máy tính. */
    UartStream_Status_t pc_serial;                   /**< Hàng đợi, thống kê và lỗi UART hiện tại. */
    UartLog_Result_t pc_log_initialize_result;       /**< Kết quả ghép logger với PC serial. */
    UartLog_Status_t pc_log;                         /**< Thống kê định dạng và xếp log UART. */
    CommandConsole_Result_t command_console_initialize_result; /**< Kết quả tạo parser command. */
    CommandConsole_Status_t command_console;         /**< Dòng dở, timeout và command đã nhận. */
    AppCommands_Result_t app_commands_initialize_result; /**< Kết quả tạo menu command ứng dụng. */
    AppCommands_Status_t app_commands;               /**< Thống kê thực thi help/echo và lỗi. */
    DHT11_Result_t dht11_initialize_result;       /**< Kết quả khởi tạo driver DHT11. */
    DHT11_Status_t dht11_status;                  /**< Trạng thái DHT11 gần nhất. */
    PwmOutput_Result_t servo_pwm_initialize_result; /**< Kết quả khởi tạo PWM dành cho servo. */
    PwmOutput_Status_t servo_pwm;                 /**< Clock, chu kỳ và pulse hiện tại của servo. */
    bool servo_pwm_led_test_enabled;              /**< Kịch bản LED PWM đang được phép chạy. */
    App_ServoPwmLedTestStep_t servo_pwm_led_test_step; /**< Mức sáng hiện tại của test. */
    PwmOutput_Result_t servo_pwm_led_test_last_result; /**< Kết quả đổi duty cycle gần nhất. */
    EnvironmentMonitor_Status_t environment;     /**< Dữ liệu và phân loại môi trường. */
    EnvironmentFeedback_Status_t feedback;       /**< Pattern và cảnh báo manual hiện tại. */
    WarningFeedback_Status_t warning_feedback;   /**< Nguồn warning và pattern 5 LED hiện tại. */
    EnvironmentDisplay_Status_t environment_display; /**< Trạng thái render giao diện môi trường. */
    MotionDisplay_Status_t motion_display;       /**< Trạng thái render page gia tốc. */
    OutputControl_Status_t output_control;       /**< Ngõ ra đang chọn và bitmask ON/OFF. */
    OutputDisplay_Status_t output_display;       /**< Trạng thái render trang Outputs. */
    uint8_t effective_output_mask;               /**< Mask thực tế xuất GPIO sau khi phân xử warning. */
    Adxl345_InitializeResult_t adxl345_initialize_result; /**< Kết quả bắt đầu driver ADXL345. */
    Adxl345_Status_t adxl345;                    /**< Nhận dạng, dữ liệu và lỗi ADXL345. */
    MotionMonitor_Result_t motion_monitor_initialize_result; /**< Kết quả khởi tạo thuật toán chuyển động. */
    MotionMonitor_Status_t motion_monitor;       /**< Tư thế và mức chuyển động suy ra từ XYZ. */
    bool environment_display_initialized;        /**< Service trình bày môi trường đã khởi tạo. */
    bool motion_display_initialized;             /**< Service trình bày gia tốc đã khởi tạo. */
    bool output_control_initialized;             /**< Service điều khiển năm output đã khởi tạo. */
    bool output_display_initialized;             /**< Service trình bày trang Outputs đã khởi tạo. */
    bool warning_feedback_initialized;           /**< Bộ tổng hợp nguồn warning đã khởi tạo. */
    bool oled_canvas_initialized;                 /**< Canvas đồ họa đã ghép với framebuffer OLED. */
    uint8_t digital_output_initialized_mask;     /**< Bit n bằng 1 khi driver output n khởi tạo được. */
    bool heartbeat_led_initialized;               /**< Driver LED heartbeat đã khởi tạo. */
    bool ui_ok_button_initialized;                /**< Nút OK/manual warning trên PB0 đã khởi tạo. */
    bool ui_left_button_initialized;              /**< Nút Left trên PB3 đã khởi tạo. */
    bool ui_right_button_initialized;             /**< Nút Right trên PB4 đã khởi tạo. */
    bool ui_up_button_initialized;                /**< Nút Up trên PB5 đã khởi tạo. */
    bool ui_down_button_initialized;              /**< Nút Down trên PA15 đã khởi tạo. */
    bool heartbeat_generator_initialized;         /**< Bộ tạo chu kỳ heartbeat đã khởi tạo. */
    bool heartbeat_led_is_active;                 /**< LED heartbeat hiện đang sáng. */
    bool shared_i2c_bus_initialized;              /**< Context I2C1 dùng chung đã khởi tạo. */
    I2cBus_Status_t shared_i2c_bus;               /**< Snapshot giao dịch và lỗi của bus dùng chung. */
    I2cBusRecovery_State_t shared_i2c_bus_recovery_state; /**< Bước bus-clear I2C1 hiện tại. */
    uint8_t oled_fast_attempt_count;              /**< Số lần thử nhanh trong đợt lỗi hiện tại. */
    uint8_t oled_consecutive_nack_count;          /**< Số NACK OLED liên tiếp. */
    uint8_t shared_i2c_bus_recovery_attempt_count; /**< Số lần bus-clear trong đợt lỗi hiện tại. */
    uint8_t oled_selected_address_7bit;           /**< Địa chỉ SSD1306 7-bit cố định đang sử dụng. */
    Ssd1306_InitializeResult_t oled_initialize_result; /**< Kết quả kiểm tra cấu hình SSD1306. */
    Ssd1306_Status_t oled;                        /**< Snapshot driver SSD1306. */
    App_DisplayPage_t current_display_page;       /**< Page OLED đang được chọn. */
    App_OledState_t oled_state;                   /**< Bước hiện tại của luồng OLED. */
    App_OledError_t oled_error;                   /**< Nguyên nhân lỗi cụ thể của luồng OLED. */
} App_Status_t;

/**
 * @brief Khởi tạo toàn bộ module ứng dụng sau các hàm MX_*_Init().
 * @param dht11_timer Handle TIM3 dành cho input capture DHT11.
 * @param servo_timer Handle TIM1 dành cho PWM servo trên PA8.
 * @param pc_serial_uart Handle UART dành cho liên kết nối tiếp với máy tính.
 * @param shared_i2c Handle I2C1 dùng chung cho OLED và ADXL345.
 * @param current_tick_ms HAL tick hiện tại.
 */
void App_Initialize(TIM_HandleTypeDef *dht11_timer,
                    TIM_HandleTypeDef *servo_timer,
                    UART_HandleTypeDef *pc_serial_uart,
                    I2C_HandleTypeDef *shared_i2c,
                    uint32_t current_tick_ms);

/**
 * @brief Tiến toàn bộ state machine của ứng dụng đúng một lần.
 * @param current_tick_ms HAL tick hiện tại.
 * @note Gọi thường xuyên trong superloop; hàm không blocking.
 */
void App_Service(uint32_t current_tick_ms);

/**
 * @brief Chuyển tiếp callback input capture đến driver đang sở hữu timer/channel.
 * @param timer Handle timer do HAL callback cung cấp.
 * @note Gọi từ HAL_TIM_IC_CaptureCallback().
 */
void App_HandleTimerInputCaptureInterrupt(TIM_HandleTypeDef *timer);

/**
 * @brief Chuyển tiếp callback GPIO EXTI đến các input driver đã đăng ký.
 * @param gpio_pin Pin do HAL callback cung cấp.
 * @note Gọi từ HAL_GPIO_EXTI_Callback().
 */
void App_HandleGpioExtiInterrupt(uint16_t gpio_pin);

/**
 * @brief Chuyển tiếp callback hoàn thành I2C master transmit đến bus tương ứng.
 * @param i2c HAL handle do callback cung cấp.
 * @note Gọi từ HAL_I2C_MasterTxCpltCallback().
 */
void App_HandleI2cMasterTransmitCompleteInterrupt(I2C_HandleTypeDef *i2c);

/**
 * @brief Chuyển tiếp callback hoàn thành I2C memory read đến bus tương ứng.
 * @param i2c HAL handle do callback cung cấp.
 * @note Gọi từ HAL_I2C_MemRxCpltCallback().
 */
void App_HandleI2cMemoryReadCompleteInterrupt(I2C_HandleTypeDef *i2c);

/**
 * @brief Chuyển tiếp callback lỗi I2C đến bus tương ứng.
 * @param i2c HAL handle do callback cung cấp.
 * @note Gọi từ HAL_I2C_ErrorCallback().
 */
void App_HandleI2cErrorInterrupt(I2C_HandleTypeDef *i2c);

/**
 * @brief Chuyển tiếp callback hoàn thành abort I2C đến bus tương ứng.
 * @param i2c HAL handle do callback cung cấp.
 * @note Gọi từ HAL_I2C_AbortCpltCallback().
 */
void App_HandleI2cAbortCompleteInterrupt(I2C_HandleTypeDef *i2c);

/** @brief Chuyển tiếp callback nhận xong một byte UART đến driver PC serial. */
void App_HandleUartReceiveCompleteInterrupt(UART_HandleTypeDef *uart);

/** @brief Chuyển tiếp callback truyền xong một đoạn UART đến driver PC serial. */
void App_HandleUartTransmitCompleteInterrupt(UART_HandleTypeDef *uart);

/** @brief Chuyển tiếp callback lỗi UART đến driver PC serial. */
void App_HandleUartErrorInterrupt(UART_HandleTypeDef *uart);

/**
 * @brief Sao chép snapshot trạng thái tổng hợp của ứng dụng.
 * @param output_status Vùng nhớ nhận snapshot; NULL sẽ được bỏ qua.
 */
void App_GetStatus(App_Status_t *output_status);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_APP_H_ */
