/**
 * @file motion_display.h
 * @brief Trình bày dữ liệu ADXL345 và kết quả motion monitor trên OLED 128x64.
 *
 * Service thuần phần mềm này không truy cập HAL, I2C hay SSD1306. Nó chỉ giữ
 * snapshot cần hiển thị, định dạng text và vẽ thông qua MonoGraphics.
 */

#ifndef USER_SERVICES_MOTION_DISPLAY_MOTION_DISPLAY_H_
#define USER_SERVICES_MOTION_DISPLAY_MOTION_DISPLAY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "adxl345.h"
#include "mono_graphics.h"
#include "motion_monitor.h"

/** @brief Khoảng cách tối thiểu giữa hai frame motion liên tiếp. */
#define MOTION_DISPLAY_MIN_REFRESH_INTERVAL_MS (250UL)

/** @brief Thời gian mỗi pha hiện/ẩn phần chữ của cảnh báo nhấp nháy. */
#define MOTION_DISPLAY_WARNING_BLINK_INTERVAL_MS (500UL)

/** @brief Cảnh báo ngắn gọn dành riêng cho trang motion. */
typedef enum
{
    MOTION_DISPLAY_WARNING_NONE = 0, /**< Dữ liệu bình thường; không hiện badge. */
    MOTION_DISPLAY_WARNING_WAITING,  /**< ADXL345 đang khởi tạo hoặc chưa có mẫu. */
    MOTION_DISPLAY_WARNING_STALE,    /**< Mẫu cuối không còn mới. */
    MOTION_DISPLAY_WARNING_SENSOR_MISSING, /**< Không nhận diện được ADXL345. */
    MOTION_DISPLAY_WARNING_SENSOR_ERROR /**< ADXL345 hoặc giao dịch I2C đang lỗi. */
} MotionDisplay_Warning_t;

/** @brief Kết quả một lần thử vẽ trang motion. */
typedef enum
{
    MOTION_DISPLAY_RENDER_IDLE = 0, /**< Nội dung hiện tại không cần vẽ lại. */
    MOTION_DISPLAY_RENDER_WAIT_INTERVAL, /**< Đang chờ đủ khoảng cách refresh. */
    MOTION_DISPLAY_RENDER_DRAWN, /**< Frame mới đã được vẽ vào framebuffer. */
    MOTION_DISPLAY_RENDER_FAILED /**< Tham số hoặc primitive đồ họa bị lỗi. */
} MotionDisplay_RenderResult_t;

/** @brief Snapshot công khai phục vụ App và debugger. */
typedef struct
{
    bool is_initialized;       /**< Context đã được khởi tạo. */
    bool has_desired_content;  /**< Đã nhận snapshot ADXL345 để hiển thị. */
    bool redraw_is_pending;    /**< Có thay đổi đang chờ vẽ. */
    bool has_rendered_frame;   /**< Đã vẽ thành công ít nhất một frame. */
    bool warning_text_is_visible; /**< Pha hiện tại có vẽ phần chữ cần nhấp nháy. */
    bool warning_blink_timer_is_active; /**< Timer blink chỉ tiến khi page được render. */
    uint32_t last_render_tick_ms; /**< Tick lần vẽ gần nhất. */
    uint32_t warning_blink_tick_ms; /**< Tick bắt đầu pha blink hiện tại. */
    uint32_t rendered_frame_count; /**< Tổng số frame, tăng bão hòa. */
    MotionDisplay_Warning_t active_warning; /**< Cảnh báo page-local đã rút gọn. */
    MotionDisplay_RenderResult_t last_render_result; /**< Kết quả lần thử vẽ gần nhất. */
} MotionDisplay_Status_t;

/** @brief Context tĩnh của trang hiển thị motion. */
typedef struct
{
    MotionDisplay_Status_t status; /**< Trạng thái công khai của service. */
    Adxl345_Status_t desired_sensor; /**< Snapshot ADXL345 cần hiển thị. */
    MotionMonitor_Status_t desired_monitor; /**< Snapshot kết quả suy ra cần hiển thị. */
} MotionDisplay_t;

/** @brief Callback xóa framebuffer do backend OLED cung cấp. */
typedef bool (*MotionDisplay_ClearCanvasFunction_t)(void *context);

/** @brief Khởi tạo context và yêu cầu vẽ frame đầu tiên. */
bool MotionDisplay_Initialize(MotionDisplay_t *display);

/** @brief Cập nhật hai snapshot và đánh dấu redraw khi nội dung nhìn thấy thay đổi. */
void MotionDisplay_Update(MotionDisplay_t *display,
                          const Adxl345_Status_t *sensor_status,
                          const MotionMonitor_Status_t *monitor_status);

/**
 * @brief Vẽ frame đang chờ nếu đã qua khoảng cách refresh tối thiểu.
 *
 * Timer blink page-local chỉ tiến khi hàm này được gọi. Application chỉ gọi hàm
 * cho page đang chọn nên cảnh báo của page ẩn không tạo refresh OLED.
 * @return Kết quả để App quyết định có yêu cầu SSD1306 refresh hay không.
 */
MotionDisplay_RenderResult_t MotionDisplay_RenderIfDue(
    MotionDisplay_t *display,
    const MonoGraphics_Canvas_t *canvas,
    MotionDisplay_ClearCanvasFunction_t clear_canvas,
    uint32_t current_tick_ms);

/**
 * @brief Yêu cầu vẽ lại trang, ví dụ khi vừa chuyển page hoặc refresh lỗi.
 * @note Badge đang nhấp nháy được đưa về pha hiện đầy đủ khi gọi hàm này.
 */
void MotionDisplay_RequestRedraw(MotionDisplay_t *display);

/** @brief Sao chép snapshot service cho App hoặc debugger. */
void MotionDisplay_GetStatus(const MotionDisplay_t *display,
                             MotionDisplay_Status_t *output_status);

#ifdef __cplusplus
}
#endif

#endif /* USER_SERVICES_MOTION_DISPLAY_MOTION_DISPLAY_H_ */
