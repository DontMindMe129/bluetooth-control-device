/**
 * @file app_ui_controller.h
 * @brief Điều phối năm nút, gesture OK và lựa chọn page OLED ở tầng App.
 */

#ifndef USER_APP_APP_UI_CONTROLLER_H_
#define USER_APP_APP_UI_CONTROLLER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "button_gesture.h"
#include "button_input.h"

/** @brief Các page người dùng có thể chọn bằng nút Left/Right. */
typedef enum
{
    APP_DISPLAY_PAGE_ENVIRONMENT = 0, /**< Nhiệt độ, độ ẩm và trạng thái môi trường. */
    APP_DISPLAY_PAGE_HISTORY,         /**< Lịch sử các lần đo DHT11. */
    APP_DISPLAY_PAGE_MOTION,          /**< Gia tốc ba trục từ ADXL345. */
    APP_DISPLAY_PAGE_WARNING_HISTORY, /**< Lịch sử bắt đầu, đổi nguồn và kết thúc cảnh báo. */
    APP_DISPLAY_PAGE_OUTPUTS,         /**< Lựa chọn và bật/tắt năm ngõ ra số. */
    APP_DISPLAY_PAGE_COUNT            /**< Số page; không phải một page hợp lệ. */
} App_DisplayPage_t;

/** @brief Cấu hình phần cứng và gesture của toàn bộ cụm nút UI. */
typedef struct
{
    ButtonInput_Config_t ok_button; /**< Chân và debounce của nút OK. */
    ButtonInput_Config_t left_button; /**< Chân và debounce của nút Left. */
    ButtonInput_Config_t right_button; /**< Chân và debounce của nút Right. */
    ButtonInput_Config_t up_button; /**< Chân và debounce của nút Up. */
    ButtonInput_Config_t down_button; /**< Chân và debounce của nút Down. */
    ButtonGesture_Config_t ok_gesture; /**< Ngưỡng nhấn ngắn/giữ của nút OK. */
    App_DisplayPage_t initial_page; /**< Page được chọn ngay sau khi khởi tạo. */
} AppUiController_Config_t;

/** @brief Các yêu cầu one-shot tạo ra trong một lần service UI. */
typedef struct
{
    bool page_changed; /**< Left hoặc Right vừa đổi current_page. */
    bool up_requested; /**< Nút Up vừa được xác nhận. */
    bool down_requested; /**< Nút Down vừa được xác nhận. */
    bool ok_short_requested; /**< Gesture nhấn ngắn OK vừa hoàn tất. */
    bool ok_long_requested; /**< Gesture giữ OK vừa đạt ngưỡng. */
} AppUiController_Events_t;

/** @brief Snapshot công khai phục vụ App và debugger. */
typedef struct
{
    bool is_initialized; /**< Controller đã nhận cấu hình hợp lệ. */
    bool ok_button_initialized; /**< Button driver OK đã sẵn sàng. */
    bool left_button_initialized; /**< Button driver Left đã sẵn sàng. */
    bool right_button_initialized; /**< Button driver Right đã sẵn sàng. */
    bool up_button_initialized; /**< Button driver Up đã sẵn sàng. */
    bool down_button_initialized; /**< Button driver Down đã sẵn sàng. */
    bool ok_gesture_initialized; /**< Bộ nhận dạng gesture OK đã sẵn sàng. */
    App_DisplayPage_t current_page; /**< Page OLED được chọn hiện tại. */
} AppUiController_Status_t;

/** @brief Context tĩnh sở hữu năm button driver và gesture của nút OK. */
typedef struct
{
    ButtonInput_t ok_button; /**< Context nút OK. */
    ButtonInput_t left_button; /**< Context nút Left. */
    ButtonInput_t right_button; /**< Context nút Right. */
    ButtonInput_t up_button; /**< Context nút Up. */
    ButtonInput_t down_button; /**< Context nút Down. */
    ButtonGesture_t ok_gesture; /**< Context gesture riêng của nút OK. */
    AppUiController_Status_t status; /**< Snapshot trạng thái công khai. */
} AppUiController_t;

/**
 * @brief Khởi tạo toàn bộ cụm nút và page ban đầu.
 * @return true khi mọi button/gesture đều sẵn sàng; false nếu có thành phần lỗi.
 * @note Controller vẫn phục vụ các thành phần khởi tạo thành công khi một nút lỗi.
 */
bool AppUiController_Initialize(AppUiController_t *controller,
                                const AppUiController_Config_t *config);

/**
 * @brief Tiến debounce, gesture và page navigation đúng một lần.
 * @note events được xóa trước khi ghi; event chỉ tồn tại trong lần gọi hiện tại.
 */
void AppUiController_Service(AppUiController_t *controller,
                             uint32_t current_tick_ms,
                             AppUiController_Events_t *events);

/** @brief Chuyển tiếp một callback EXTI tới cả năm button instance. */
void AppUiController_HandleExtiInterrupt(AppUiController_t *controller,
                                         uint16_t gpio_pin);

/** @brief Sao chép snapshot UI cho App hoặc debugger. */
void AppUiController_GetStatus(const AppUiController_t *controller,
                               AppUiController_Status_t *output_status);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_APP_UI_CONTROLLER_H_ */
