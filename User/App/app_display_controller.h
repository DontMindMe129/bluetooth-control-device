/**
 * @file app_display_controller.h
 * @brief Điều phối năm page OLED và các display service ở tầng App.
 *
 * Controller chỉ cập nhật model giao diện và vẽ vào canvas do caller cung cấp.
 * Module không sở hữu framebuffer, không gửi I2C và không điều khiển phần cứng.
 */

#ifndef USER_APP_APP_DISPLAY_CONTROLLER_H_
#define USER_APP_APP_DISPLAY_CONTROLLER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "adxl345.h"
#include "app_ui_controller.h"
#include "environment_display.h"
#include "environment_history_display.h"
#include "motion_display.h"
#include "output_display.h"
#include "warning_history_display.h"

/** @brief Kết quả chuẩn hóa của một lần thử vẽ page hiện tại. */
typedef enum
{
    APP_DISPLAY_CONTROLLER_RENDER_IDLE = 0, /**< Chưa tới hạn hoặc page không cần vẽ lại. */
    APP_DISPLAY_CONTROLLER_RENDER_DRAWN,    /**< Framebuffer vừa được vẽ thành công. */
    APP_DISPLAY_CONTROLLER_RENDER_FAILED   /**< Dữ liệu/canvas không hợp lệ hoặc vẽ thất bại. */
} AppDisplayController_RenderResult_t;

/** @brief Snapshot dữ liệu chỉ-đọc dùng để cập nhật năm page. */
typedef struct
{
    const EnvironmentMonitor_Status_t *environment; /**< Dữ liệu môi trường mới nhất. */
    const EnvironmentFeedback_Status_t *environment_feedback; /**< Warning môi trường/manual hiện tại. */
    const Adxl345_Status_t *adxl345; /**< Dữ liệu và trạng thái driver ADXL345. */
    const MotionMonitor_Status_t *motion; /**< Kết quả phân loại chuyển động. */
    const OutputControl_Status_t *outputs; /**< Lựa chọn và mask output của người dùng. */
    bool monitoring_is_active; /**< Trạng thái phiên giám sát trình bày trên UI. */
} AppDisplayController_Input_t;

/** @brief Snapshot tập trung của toàn bộ tầng trình bày OLED. */
typedef struct
{
    bool is_initialized; /**< Cả năm display service đã khởi tạo thành công. */
    EnvironmentDisplay_Status_t environment; /**< Trạng thái page Environment. */
    EnvironmentHistoryDisplay_Status_t environment_history; /**< Trạng thái page Environment History. */
    MotionDisplay_Status_t motion; /**< Trạng thái page Motion. */
    WarningHistoryDisplay_Status_t warning_history; /**< Trạng thái page Warning History. */
    OutputDisplay_Status_t outputs; /**< Trạng thái page Outputs. */
    AppDisplayController_RenderResult_t last_render_result; /**< Kết quả render chuẩn hóa gần nhất. */
} AppDisplayController_Status_t;

/** @brief Context tĩnh sở hữu năm display service, không chứa framebuffer. */
typedef struct
{
    EnvironmentDisplay_t environment; /**< Context page Environment. */
    EnvironmentHistoryDisplay_t environment_history; /**< Context page Environment History. */
    MotionDisplay_t motion; /**< Context page Motion. */
    WarningHistoryDisplay_t warning_history; /**< Context page Warning History. */
    OutputDisplay_t outputs; /**< Context page Outputs. */
    AppDisplayController_Status_t status; /**< Snapshot tổng hợp được đồng bộ sau mỗi thao tác. */
} AppDisplayController_t;

/** @brief Callback xóa canvas do backend framebuffer cung cấp. */
typedef bool (*AppDisplayController_ClearCanvasFunction_t)(void *context);

/**
 * @brief Khởi tạo năm page và liên kết hai nguồn history chỉ-đọc.
 * @param controller Context do application cấp phát tĩnh.
 * @param environment_history Ring buffer lịch sử DHT11 phải tồn tại suốt vòng đời controller.
 * @param warning_history Ring buffer warning phải tồn tại suốt vòng đời controller.
 * @return true khi toàn bộ display service khởi tạo thành công.
 */
bool AppDisplayController_Initialize(
    AppDisplayController_t *controller,
    const EnvironmentHistory_t *environment_history,
    const WarningHistory_t *warning_history);

/**
 * @brief Cập nhật model của cả năm page từ snapshot hệ thống hiện tại.
 * @param controller Context đã khởi tạo.
 * @param input Các snapshot chỉ-đọc của vòng superloop hiện tại.
 * @note Hàm không vẽ framebuffer và không thực hiện giao dịch I2C.
 */
void AppDisplayController_Update(
    AppDisplayController_t *controller,
    const AppDisplayController_Input_t *input);

/**
 * @brief Chuẩn bị page vừa được chọn và yêu cầu page đó vẽ lại.
 * @param controller Context đã khởi tạo.
 * @param page Page vừa được chọn bởi UI controller.
 * @note Hai page history được đưa về danh sách và chọn record mới nhất.
 */
void AppDisplayController_EnterPage(AppDisplayController_t *controller,
                                    App_DisplayPage_t page);

/**
 * @brief Chuyển Up/Down/OK tới page history đang chọn.
 * @param controller Context đã khởi tạo.
 * @param page Page hiện tại; chỉ hai page history được xử lý.
 * @param up_requested Sự kiện Up one-shot.
 * @param down_requested Sự kiện Down one-shot.
 * @param ok_requested Sự kiện nhấn ngắn OK one-shot.
 * @return true khi page history đã thay đổi con trỏ hoặc chế độ xem.
 */
bool AppDisplayController_HandleHistoryInput(
    AppDisplayController_t *controller,
    App_DisplayPage_t page,
    bool up_requested,
    bool down_requested,
    bool ok_requested);

/**
 * @brief Đánh dấu đúng page được chỉ định cần vẽ lại.
 * @param controller Context đã khởi tạo.
 * @param page Page cần redraw.
 */
void AppDisplayController_RequestRedraw(AppDisplayController_t *controller,
                                        App_DisplayPage_t page);

/**
 * @brief Vẽ page được chọn vào canvas nếu page đang tới hạn cập nhật.
 * @param controller Context đã khởi tạo.
 * @param page Page cần thử render.
 * @param canvas Canvas ánh xạ tới framebuffer hiện tại.
 * @param clear_canvas Callback xóa framebuffer trước khi dựng frame.
 * @param current_tick_ms Tick dùng để giới hạn tần suất render.
 * @return DRAWN khi framebuffer vừa đổi, IDLE khi chưa cần vẽ, hoặc FAILED.
 */
AppDisplayController_RenderResult_t AppDisplayController_RenderIfDue(
    AppDisplayController_t *controller,
    App_DisplayPage_t page,
    const MonoGraphics_Canvas_t *canvas,
    AppDisplayController_ClearCanvasFunction_t clear_canvas,
    uint32_t current_tick_ms);

/**
 * @brief Sao chép snapshot tập trung cho application hoặc debugger.
 * @param controller Context nguồn.
 * @param output_status Vùng nhớ nhận snapshot; NULL sẽ được bỏ qua.
 */
void AppDisplayController_GetStatus(
    const AppDisplayController_t *controller,
    AppDisplayController_Status_t *output_status);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_APP_DISPLAY_CONTROLLER_H_ */
