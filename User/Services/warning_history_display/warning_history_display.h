/** @file warning_history_display.h
 * @brief Danh sách và chi tiết sự kiện cảnh báo trên canvas 128x64.
 * @note M/E/S trong danh sách là MANUAL/ENV/SHAKING; nguồn là mask sau sự kiện.
 */

#ifndef USER_SERVICES_WARNING_HISTORY_DISPLAY_WARNING_HISTORY_DISPLAY_H_
#define USER_SERVICES_WARNING_HISTORY_DISPLAY_WARNING_HISTORY_DISPLAY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "warning_history.h"
#include "mono_graphics.h"

/** @brief Khoảng cách tối thiểu giữa hai lần vẽ framebuffer liên tiếp. */
#define WARNING_HISTORY_DISPLAY_MIN_REFRESH_INTERVAL_MS (100UL)

/** @brief Chế độ trình bày của trang History. */
typedef enum
{
    WARNING_HISTORY_DISPLAY_LIST = 0, /**< Danh sách tối đa năm sự kiện. */
    WARNING_HISTORY_DISPLAY_DETAIL   /**< Chi tiết record đang chọn. */
} WarningHistoryDisplay_View_t;

/** @brief Kết quả một lần thử vẽ trang History. */
typedef enum
{
    WARNING_HISTORY_DISPLAY_RENDER_IDLE = 0,
    WARNING_HISTORY_DISPLAY_RENDER_WAIT_INTERVAL,
    WARNING_HISTORY_DISPLAY_RENDER_DRAWN,
    WARNING_HISTORY_DISPLAY_RENDER_FAILED
} WarningHistoryDisplay_RenderResult_t;

/** @brief Snapshot công khai để quan sát service bằng debugger. */
typedef struct
{
    bool is_initialized; /**< Context đã được khởi tạo. */
    bool redraw_is_pending; /**< Nội dung đang chờ được vẽ. */
    bool has_rendered_frame; /**< Đã vẽ thành công ít nhất một frame. */
    bool monitoring_is_active; /**< Trạng thái giám sát trình bày trên header. */
    bool has_newer_record; /**< Có record mới hơn record đang chọn. */
    uint16_t history_count; /**< Số record hiện có trong ring buffer. */
    uint16_t selected_age_from_newest; /**< 0 là record mới nhất, 1 là record liền trước. */
    WarningHistoryDisplay_View_t view; /**< Danh sách hay chi tiết. */
    uint32_t last_render_tick_ms; /**< Tick của lần vẽ thành công gần nhất. */
    uint32_t rendered_frame_count; /**< Tổng số frame đã vẽ, tăng bão hòa. */
    WarningHistoryDisplay_RenderResult_t last_render_result;
} WarningHistoryDisplay_Status_t;

/** @brief Context tĩnh của trang History. */
typedef struct
{
    const WarningHistory_t *history; /**< Ring buffer chỉ-đọc do tầng App sở hữu. */
    WarningHistoryDisplay_Status_t status;
    uint32_t last_total_event_count; /**< Bộ đếm dùng nhận biết event mới. */
    uint16_t last_write_index; /**< Nhận biết ghi mới khi bộ đếm bão hòa. */
} WarningHistoryDisplay_t;

/** @brief Callback xóa framebuffer do backend OLED cung cấp. */
typedef bool (*WarningHistoryDisplay_ClearCanvasFunction_t)(void *context);

/**
 * @brief Khởi tạo context và yêu cầu frame đầu tiên.
 * @param display Context trang do application sở hữu.
 * @param history Ring buffer chỉ-đọc phải tồn tại suốt vòng đời trang.
 * @return true khi cả hai context hợp lệ.
 */
bool WarningHistoryDisplay_Initialize(WarningHistoryDisplay_t *display,
                                     const WarningHistory_t *history);

/**
 * @brief Đồng bộ số record, record mới nhất và trạng thái warning hiện tại.
 * @note Khi người dùng đang xem record cũ, record mới không làm nhảy con trỏ.
 */
void WarningHistoryDisplay_Update(
    WarningHistoryDisplay_t *display,
    bool monitoring_is_active);

/** @brief Đưa trang về list view và chọn record mới nhất mỗi khi đi vào page. */
void WarningHistoryDisplay_EnterPage(WarningHistoryDisplay_t *display);

/**
 * @brief Xử lý đúng một sự kiện Up, Down hoặc OK.
 * @return true nếu con trỏ hoặc chế độ xem đã thay đổi.
 */
bool WarningHistoryDisplay_HandleInput(WarningHistoryDisplay_t *display,
                                           bool up_pressed,
                                           bool down_pressed,
                                           bool ok_pressed);

/** @brief Vẽ frame đang chờ nếu đã qua khoảng cách refresh tối thiểu. */
WarningHistoryDisplay_RenderResult_t
WarningHistoryDisplay_RenderIfDue(
    WarningHistoryDisplay_t *display,
    const MonoGraphics_Canvas_t *canvas,
    WarningHistoryDisplay_ClearCanvasFunction_t clear_canvas,
    uint32_t current_tick_ms);

/** @brief Đánh dấu trang cần vẽ lại, ví dụ sau khi chuyển page. */
void WarningHistoryDisplay_RequestRedraw(
    WarningHistoryDisplay_t *display);

/** @brief Sao chép trạng thái service cho application hoặc debugger. */
void WarningHistoryDisplay_GetStatus(
    const WarningHistoryDisplay_t *display,
    WarningHistoryDisplay_Status_t *output_status);

#ifdef __cplusplus
}
#endif

#endif
