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

#include "app_adxl_manager.h"
#include "app_commands.h"
#include "app_display_controller.h"
#include "app_oled_manager.h"
#include "app_shared_i2c_manager.h"
#include "app_ui_controller.h"
#include "app_warning_controller.h"
#include "command_console.h"
#include "dht11.h"
#include "environment_history.h"
#include "environment_monitor.h"
#include "motion_monitor.h"
#include "monitoring_session.h"
#include "output_control.h"
#include "pwm_output.h"
#include "stm32f1xx_hal.h"
#include "uart_stream.h"
#include "uart_log.h"

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
    EnvironmentHistory_Status_t environment_history; /**< Trạng thái ring buffer history. */
    MonitoringSession_Status_t monitoring_session; /**< Phiên giám sát hiện tại. */
    AppWarningController_Status_t warning;       /**< Policy, history, pattern và mask warning tập trung. */
    AppDisplayController_Status_t display;       /**< Trạng thái tập trung của năm page OLED. */
    OutputControl_Status_t output_control;       /**< Ngõ ra đang chọn và bitmask ON/OFF. */
    AppAdxlManager_Status_t adxl345;             /**< Vòng đời, dữ liệu và lỗi ADXL345 tập trung. */
    MotionMonitor_Result_t motion_monitor_initialize_result; /**< Kết quả khởi tạo thuật toán chuyển động. */
    MotionMonitor_Status_t motion_monitor;       /**< Tư thế và mức chuyển động suy ra từ XYZ. */
    bool output_control_initialized;             /**< Service điều khiển năm output đã khởi tạo. */
    bool monitoring_session_initialized;         /**< Bộ quản lý phiên giám sát đã khởi tạo. */
    AppUiController_Status_t ui;                 /**< Trạng thái nút, gesture và page hiện tại. */
    uint8_t digital_output_initialized_mask;     /**< Bit n bằng 1 khi driver output n khởi tạo được. */
    bool heartbeat_led_initialized;               /**< Driver LED heartbeat đã khởi tạo. */
    bool heartbeat_generator_initialized;         /**< Bộ tạo chu kỳ heartbeat đã khởi tạo. */
    bool heartbeat_led_is_active;                 /**< LED heartbeat hiện đang sáng. */
    AppSharedI2cManager_Status_t shared_i2c;      /**< Bus I2C1 và state machine bus-clear dùng chung. */
    AppOledManager_Status_t oled;                 /**< Vòng đời, lỗi, retry và driver OLED tập trung. */
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
