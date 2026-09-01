/**
 * @file environment_display.h
 * @brief Trinh bay trang thai moi truong tren canvas don sac 128x64.
 *
 * Service thuan phan mem nay khong truy cap SSD1306, I2C hay HAL. Caller cung cap
 * snapshot monitor, feedback va canvas; service chi quyet dinh khi nao can ve lai
 * va chuyen du lieu thanh noi dung ASCII co the hien thi.
 */

#ifndef USER_SERVICES_ENVIRONMENT_DISPLAY_ENVIRONMENT_DISPLAY_H_
#define USER_SERVICES_ENVIRONMENT_DISPLAY_ENVIRONMENT_DISPLAY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "environment_feedback.h"
#include "environment_monitor.h"
#include "mono_graphics.h"

/** @brief Khoang cach toi thieu giua hai lan ve framebuffer lien tiep. */
#define ENVIRONMENT_DISPLAY_MIN_REFRESH_INTERVAL_MS (250UL)

/** @brief Thời gian mỗi pha hiện/ẩn phần chữ của cảnh báo đang nhấp nháy. */
#define ENVIRONMENT_DISPLAY_WARNING_BLINK_INTERVAL_MS (500UL)

/** @brief Cảnh báo ngắn gọn đang được trình bày trên trang môi trường. */
typedef enum
{
    ENVIRONMENT_DISPLAY_WARNING_NONE = 0, /**< Dữ liệu bình thường; không hiện badge. */
    ENVIRONMENT_DISPLAY_WARNING_WAITING,  /**< Đang chờ mẫu DHT11 hợp lệ đầu tiên. */
    ENVIRONMENT_DISPLAY_WARNING_ALERT,    /**< Cảnh báo manual hoặc warm-and-humid. */
    ENVIRONMENT_DISPLAY_WARNING_STALE,    /**< Mẫu cuối không còn mới. */
    ENVIRONMENT_DISPLAY_WARNING_SENSOR    /**< DHT11 đang báo lỗi giao dịch/cấu hình. */
} EnvironmentDisplay_Warning_t;

/** @brief Ket qua cua mot lan thu ve noi dung moi truong. */
typedef enum
{
    ENVIRONMENT_DISPLAY_RENDER_IDLE = 0, /**< Noi dung hien tai khong can ve lai. */
    ENVIRONMENT_DISPLAY_RENDER_WAIT_INTERVAL, /**< Dang cho du khoang cach refresh toi thieu. */
    ENVIRONMENT_DISPLAY_RENDER_DRAWN, /**< Frame moi da duoc ve thanh cong vao canvas. */
    ENVIRONMENT_DISPLAY_RENDER_FAILED /**< Tham so hoac primitive do hoa bi loi. */
} EnvironmentDisplay_RenderResult_t;

/** @brief Snapshot cong khai phuc vu application va debugger. */
typedef struct
{
    bool is_initialized;       /**< Context da duoc khoi tao. */
    bool has_desired_content;  /**< Da nhan snapshot moi truong de hien thi. */
    bool redraw_is_pending;    /**< Co thay doi dang cho duoc ve vao framebuffer. */
    bool has_rendered_frame;   /**< Da ve thanh cong it nhat mot frame. */
    bool warning_text_is_visible; /**< Pha hiện tại có vẽ phần chữ của badge. */
    bool warning_blink_timer_is_active; /**< Timer blink chỉ chạy khi page được render. */
    uint32_t last_render_tick_ms; /**< Tick cua lan ve framebuffer gan nhat. */
    uint32_t warning_blink_tick_ms; /**< Tick bắt đầu pha blink hiện tại. */
    uint32_t rendered_frame_count; /**< Tong so frame da ve, tang bao hoa tai UINT32_MAX. */
    EnvironmentDisplay_Warning_t active_warning; /**< Cảnh báo page-local đã rút gọn. */
    EnvironmentDisplay_RenderResult_t last_render_result; /**< Ket qua lan thu ve gan nhat. */
} EnvironmentDisplay_Status_t;

/** @brief Context tinh do caller so huu cho mot giao dien moi truong. */
typedef struct
{
    EnvironmentDisplay_Status_t status; /**< Trang thai cong khai cua service. */
    EnvironmentMonitor_Status_t desired_environment; /**< Snapshot monitor can hien thi. */
    EnvironmentFeedback_Status_t desired_feedback; /**< Snapshot feedback can hien thi. */
} EnvironmentDisplay_t;

/**
 * @brief Callback xoa nhanh toan bo canvas bang primitive toi uu cua backend.
 * @param context Context backend dang duoc luu trong MonoGraphics_Canvas_t.
 * @return true khi framebuffer da duoc xoa va co the tiep tuc ve.
 */
typedef bool (*EnvironmentDisplay_ClearCanvasFunction_t)(void *context);

/**
 * @brief Khoi tao context va danh dau can ve frame dau tien.
 * @param display Context do caller cap phat tinh.
 * @return true khi context hop le; false khi display la NULL.
 */
bool EnvironmentDisplay_Initialize(EnvironmentDisplay_t *display);

/**
 * @brief Cap nhat snapshot mong muon va danh dau redraw neu noi dung nhin thay thay doi.
 *
 * Trang thai sang/tat LED theo tung pha khong phai noi dung OLED, vi vay thay doi cua
 * Các phase LED do warning feedback xử lý riêng nên không làm OLED refresh liên tục.
 *
 * @param display Context da khoi tao.
 * @param environment Snapshot moi nhat tu environment monitor.
 * @param feedback Snapshot moi nhat tu environment feedback.
 */
void EnvironmentDisplay_Update(
    EnvironmentDisplay_t *display,
    const EnvironmentMonitor_Status_t *environment,
    const EnvironmentFeedback_Status_t *feedback);

/**
 * @brief Thu ve frame dang cho khi da qua khoang cach refresh toi thieu.
 *
 * Timer blink page-local chi tien khi ham nay duoc goi. Application vi vay chi can
 * goi RenderIfDue() cho page dang duoc chon de page an khong tao refresh OLED.
 * @param display Context da khoi tao va da nhan snapshot.
 * @param canvas Canvas don sac toi thieu 128x64 pixel.
 * @param clear_canvas Callback xoa framebuffer toi uu cua backend.
 * @param current_tick_ms HAL tick do application cung cap; service khong tu goi HAL.
 * @return Trang thai ve de application quyet dinh co yeu cau refresh OLED hay khong.
 */
EnvironmentDisplay_RenderResult_t EnvironmentDisplay_RenderIfDue(
    EnvironmentDisplay_t *display,
    const MonoGraphics_Canvas_t *canvas,
    EnvironmentDisplay_ClearCanvasFunction_t clear_canvas,
    uint32_t current_tick_ms);

/**
 * @brief Danh dau frame hien tai can duoc ve lai, vi du sau khi doi page.
 *
 * Neu dang co canh bao nhap nhay, ham dua badge ve pha hien day du de nguoi dung
 * thay ngay noi dung canh bao khi vua chuyen sang trang nay.
 * @param display Context da khoi tao.
 */
void EnvironmentDisplay_RequestRedraw(EnvironmentDisplay_t *display);

/**
 * @brief Sao chep snapshot service cho application hoac debugger.
 * @param display Context can doc.
 * @param output_status Vung nho nhan snapshot; NULL se duoc bo qua.
 */
void EnvironmentDisplay_GetStatus(
    const EnvironmentDisplay_t *display,
    EnvironmentDisplay_Status_t *output_status);

#ifdef __cplusplus
}
#endif

#endif /* USER_SERVICES_ENVIRONMENT_DISPLAY_ENVIRONMENT_DISPLAY_H_ */
