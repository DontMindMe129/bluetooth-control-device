/**
 * @file environment_history_display.h
 * @brief Trình bày và điều hướng lịch sử đo môi trường trên canvas 128x64.
 *
 * Service đọc record từ EnvironmentHistory, nhận trạng thái hiện tại từ
 * EnvironmentMonitor và vẽ bằng các primitive đồ họa. Module không truy cập
 * trực tiếp SSD1306, I2C hay HAL.
 */

#ifndef USER_SERVICES_ENVIRONMENT_HISTORY_DISPLAY_ENVIRONMENT_HISTORY_DISPLAY_H_
#define USER_SERVICES_ENVIRONMENT_HISTORY_DISPLAY_ENVIRONMENT_HISTORY_DISPLAY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "environment_history.h"
#include "mono_graphics.h"

/** @brief Khoảng cách tối thiểu giữa hai lần vẽ framebuffer liên tiếp. */
#define ENVIRONMENT_HISTORY_DISPLAY_MIN_REFRESH_INTERVAL_MS (100UL)

/** @brief Chế độ trình bày của trang History. */
typedef enum
{
    ENVIRONMENT_HISTORY_DISPLAY_LIST = 0, /**< Danh sách tối đa năm lần đo. */
    ENVIRONMENT_HISTORY_DISPLAY_DETAIL   /**< Chi tiết record đang chọn. */
} EnvironmentHistoryDisplay_View_t;

/** @brief Kết quả một lần thử vẽ trang History. */
typedef enum
{
    ENVIRONMENT_HISTORY_DISPLAY_RENDER_IDLE = 0,
    ENVIRONMENT_HISTORY_DISPLAY_RENDER_WAIT_INTERVAL,
    ENVIRONMENT_HISTORY_DISPLAY_RENDER_DRAWN,
    ENVIRONMENT_HISTORY_DISPLAY_RENDER_FAILED
} EnvironmentHistoryDisplay_RenderResult_t;

/** @brief Snapshot công khai để quan sát service bằng debugger. */
typedef struct
{
    bool is_initialized; /**< Context đã được khởi tạo. */
    bool redraw_is_pending; /**< Nội dung đang chờ được vẽ. */
    bool has_rendered_frame; /**< Đã vẽ thành công ít nhất một frame. */
    bool current_sensor_warning; /**< DHT11 hiện không fresh hoặc vừa báo lỗi. */
    bool monitoring_is_active; /**< Trạng thái giám sát trình bày trên header. */
    bool has_newer_record; /**< Có record mới hơn record đang chọn. */
    uint16_t history_count; /**< Số record hiện có trong ring buffer. */
    uint16_t selected_age_from_newest; /**< 0 là record mới nhất, 1 là record liền trước. */
    EnvironmentHistoryDisplay_View_t view; /**< Danh sách hay chi tiết. */
    uint32_t last_render_tick_ms; /**< Tick của lần vẽ thành công gần nhất. */
    uint32_t rendered_frame_count; /**< Tổng số frame đã vẽ, tăng bão hòa. */
    EnvironmentHistoryDisplay_RenderResult_t last_render_result;
} EnvironmentHistoryDisplay_Status_t;

/** @brief Context tĩnh của trang History. */
typedef struct
{
    const EnvironmentHistory_t *history; /**< Ring buffer chỉ-đọc do tầng App sở hữu. */
    EnvironmentHistoryDisplay_Status_t status;
    uint32_t latest_record_tick_ms;
    bool latest_record_is_known;
} EnvironmentHistoryDisplay_t;

/** @brief Callback xóa framebuffer do backend OLED cung cấp. */
typedef bool (*EnvironmentHistoryDisplay_ClearCanvasFunction_t)(void *context);

/**
 * @brief Khởi tạo context và yêu cầu frame đầu tiên.
 * @param display Context trang do application sở hữu.
 * @param history Ring buffer chỉ-đọc phải tồn tại suốt vòng đời trang.
 * @return true khi cả hai context hợp lệ.
 */
bool EnvironmentHistoryDisplay_Initialize(
    EnvironmentHistoryDisplay_t *display,
    const EnvironmentHistory_t *history);

/**
 * @brief Đồng bộ số record, record mới nhất và trạng thái DHT11 hiện tại.
 * @note Khi người dùng đang xem record cũ, record mới không làm nhảy con trỏ.
 */
void EnvironmentHistoryDisplay_Update(
    EnvironmentHistoryDisplay_t *display,
    const EnvironmentMonitor_Status_t *current_environment,
    bool monitoring_is_active);

/** @brief Đưa trang về list view và chọn record mới nhất mỗi khi đi vào page. */
void EnvironmentHistoryDisplay_EnterPage(EnvironmentHistoryDisplay_t *display);

/**
 * @brief Xử lý đúng một sự kiện Up, Down hoặc OK.
 * @return true nếu con trỏ hoặc chế độ xem đã thay đổi.
 */
bool EnvironmentHistoryDisplay_HandleInput(EnvironmentHistoryDisplay_t *display,
                                           bool up_pressed,
                                           bool down_pressed,
                                           bool ok_pressed);

/** @brief Vẽ frame đang chờ nếu đã qua khoảng cách refresh tối thiểu. */
EnvironmentHistoryDisplay_RenderResult_t
EnvironmentHistoryDisplay_RenderIfDue(
    EnvironmentHistoryDisplay_t *display,
    const MonoGraphics_Canvas_t *canvas,
    EnvironmentHistoryDisplay_ClearCanvasFunction_t clear_canvas,
    uint32_t current_tick_ms);

/** @brief Đánh dấu trang cần vẽ lại, ví dụ sau khi chuyển page. */
void EnvironmentHistoryDisplay_RequestRedraw(
    EnvironmentHistoryDisplay_t *display);

/** @brief Sao chép trạng thái service cho application hoặc debugger. */
void EnvironmentHistoryDisplay_GetStatus(
    const EnvironmentHistoryDisplay_t *display,
    EnvironmentHistoryDisplay_Status_t *output_status);

#ifdef __cplusplus
}
#endif

#endif
