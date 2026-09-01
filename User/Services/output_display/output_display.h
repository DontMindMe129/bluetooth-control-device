/**
 * @file output_display.h
 * @brief Trình bày trạng thái năm ngõ ra trên canvas đơn sắc 128x64.
 *
 * Service thuần phần mềm này không truy cập GPIO, SSD1306, I2C hay HAL.
 */

#ifndef USER_SERVICES_OUTPUT_DISPLAY_OUTPUT_DISPLAY_H_
#define USER_SERVICES_OUTPUT_DISPLAY_OUTPUT_DISPLAY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "mono_graphics.h"
#include "output_control.h"

/** @brief Khoảng cách tối thiểu giữa hai lần vẽ framebuffer liên tiếp. */
#define OUTPUT_DISPLAY_MIN_REFRESH_INTERVAL_MS (100UL)

/** @brief Kết quả một lần thử vẽ trang Outputs. */
typedef enum
{
    OUTPUT_DISPLAY_RENDER_IDLE = 0, /**< Không có nội dung mới cần vẽ. */
    OUTPUT_DISPLAY_RENDER_WAIT_INTERVAL, /**< Đang chờ đủ khoảng cách refresh. */
    OUTPUT_DISPLAY_RENDER_DRAWN, /**< Frame mới đã được vẽ thành công. */
    OUTPUT_DISPLAY_RENDER_FAILED /**< Tham số hoặc primitive đồ họa bị lỗi. */
} OutputDisplay_RenderResult_t;

/** @brief Snapshot công khai phục vụ application và debugger. */
typedef struct
{
    bool is_initialized;       /**< Context đã được khởi tạo. */
    bool has_desired_content;  /**< Đã nhận trạng thái output để hiển thị. */
    bool redraw_is_pending;    /**< Có nội dung thay đổi đang chờ vẽ. */
    bool has_rendered_frame;   /**< Đã vẽ thành công ít nhất một frame. */
    uint32_t last_render_tick_ms; /**< Tick lần vẽ framebuffer gần nhất. */
    uint32_t rendered_frame_count; /**< Tổng số frame, tăng bão hòa. */
    OutputDisplay_RenderResult_t last_render_result; /**< Kết quả render gần nhất. */
} OutputDisplay_Status_t;

/** @brief Context tĩnh của trang Outputs. */
typedef struct
{
    OutputDisplay_Status_t status; /**< Trạng thái công khai của service. */
    OutputControl_Status_t desired_outputs; /**< Snapshot cần trình bày. */
} OutputDisplay_t;

/** @brief Callback xóa framebuffer do backend OLED cung cấp. */
typedef bool (*OutputDisplay_ClearCanvasFunction_t)(void *context);

/** @brief Khởi tạo context và yêu cầu vẽ frame đầu tiên. */
bool OutputDisplay_Initialize(OutputDisplay_t *display);

/** @brief Cập nhật snapshot và yêu cầu redraw khi lựa chọn hoặc ON/OFF thay đổi. */
void OutputDisplay_Update(OutputDisplay_t *display,
                          const OutputControl_Status_t *output_status);

/** @brief Vẽ frame đang chờ nếu đã qua khoảng cách refresh tối thiểu. */
OutputDisplay_RenderResult_t OutputDisplay_RenderIfDue(
    OutputDisplay_t *display,
    const MonoGraphics_Canvas_t *canvas,
    OutputDisplay_ClearCanvasFunction_t clear_canvas,
    uint32_t current_tick_ms);

/** @brief Đánh dấu trang cần được vẽ lại, ví dụ sau khi chuyển page. */
void OutputDisplay_RequestRedraw(OutputDisplay_t *display);

/** @brief Sao chép snapshot service cho application hoặc debugger. */
void OutputDisplay_GetStatus(const OutputDisplay_t *display,
                             OutputDisplay_Status_t *output_status);

#ifdef __cplusplus
}
#endif

#endif /* USER_SERVICES_OUTPUT_DISPLAY_OUTPUT_DISPLAY_H_ */
