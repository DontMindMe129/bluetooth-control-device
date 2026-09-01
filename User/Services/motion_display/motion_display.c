/**
 * @file motion_display.c
 * @brief Hiện thực trang dữ liệu gia tốc ADXL345 trên OLED 128x64.
 */

#include "motion_display.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "mono_font_5x7.h"

#define MOTION_DISPLAY_WIDTH_PIXELS  (128U)
#define MOTION_DISPLAY_HEIGHT_PIXELS (64U)
#define MOTION_DISPLAY_TEXT_CAPACITY (22U)
#define MOTION_DISPLAY_HEADER_HEIGHT (8U)

/** @brief Nối chuỗi có kiểm tra sức chứa và luôn giữ ký tự kết thúc NULL. */
static bool motion_display_append_text(char *buffer,
                                       uint8_t capacity,
                                       uint8_t *length,
                                       const char *text)
{
    if ((buffer == NULL) || (length == NULL) || (text == NULL))
    {
        return false;
    }

    while (*text != '\0')
    {
        if ((uint16_t)(*length + 1U) >= capacity)
        {
            return false;
        }
        buffer[*length] = *text;
        (*length)++;
        text++;
    }

    buffer[*length] = '\0';
    return true;
}

/** @brief Nối một số uint32 dạng thập phân mà không dùng snprintf. */
static bool motion_display_append_u32(char *buffer,
                                      uint8_t capacity,
                                      uint8_t *length,
                                      uint32_t value)
{
    char digits[10];
    uint8_t count = 0U;

    do
    {
        digits[count] = (char)('0' + (value % 10U));
        count++;
        value /= 10U;
    } while ((value != 0U) && (count < sizeof(digits)));

    while (count > 0U)
    {
        char character[2];

        count--;
        character[0] = digits[count];
        character[1] = '\0';
        if (!motion_display_append_text(buffer, capacity, length, character))
        {
            return false;
        }
    }
    return true;
}

/** @brief Tạo độ lớn tổng theo g với hai chữ số thập phân dễ đọc. */
static bool motion_display_build_acceleration_line(char *line,
                                                   int32_t value_mg)
{
    uint8_t length = 0U;
    uint32_t magnitude = (value_mg > 0) ? (uint32_t)value_mg : 0U;
    uint32_t centi_g = (magnitude + 5U) / 10U;
    uint32_t integer_part = centi_g / 100U;
    uint8_t decimal_part = (uint8_t)(centi_g % 100U);
    char decimal_text[3] =
    {
        (char)('0' + (decimal_part / 10U)),
        (char)('0' + (decimal_part % 10U)),
        '\0'
    };

    line[0] = '\0';
    return motion_display_append_text(line,
                                      MOTION_DISPLAY_TEXT_CAPACITY,
                                      &length,
                                      "ACCEL: ") &&
           motion_display_append_u32(line,
                                     MOTION_DISPLAY_TEXT_CAPACITY,
                                     &length,
                                     integer_part) &&
           motion_display_append_text(line,
                                      MOTION_DISPLAY_TEXT_CAPACITY,
                                      &length,
                                      ".") &&
           motion_display_append_text(line,
                                      MOTION_DISPLAY_TEXT_CAPACITY,
                                      &length,
                                      decimal_text) &&
           motion_display_append_text(line,
                                      MOTION_DISPLAY_TEXT_CAPACITY,
                                      &length,
                                      " g");
}

/** @brief Ánh xạ tư thế theo trục thành nhãn ngắn vừa màn hình. */
static const char *motion_display_orientation_text(
    MotionMonitor_Orientation_t orientation)
{
    switch (orientation)
    {
        case MOTION_MONITOR_ORIENTATION_X_POSITIVE:
            return "POSE : X+";
        case MOTION_MONITOR_ORIENTATION_X_NEGATIVE:
            return "POSE : X-";
        case MOTION_MONITOR_ORIENTATION_Y_POSITIVE:
            return "POSE : Y+";
        case MOTION_MONITOR_ORIENTATION_Y_NEGATIVE:
            return "POSE : Y-";
        case MOTION_MONITOR_ORIENTATION_Z_POSITIVE:
            return "POSE : Z+";
        case MOTION_MONITOR_ORIENTATION_Z_NEGATIVE:
            return "POSE : Z-";
        case MOTION_MONITOR_ORIENTATION_UNKNOWN:
        default:
            return "POSE : UNKNOWN";
    }
}

/** @brief Ánh xạ phân loại chuyển động thành một dòng hoàn chỉnh. */
static const char *motion_display_motion_text(MotionMonitor_State_t state)
{
    switch (state)
    {
        case MOTION_MONITOR_STATE_STILL:
            return "MOVE : STILL";
        case MOTION_MONITOR_STATE_MOVING:
            return "MOVE : MOVING";
        case MOTION_MONITOR_STATE_SHAKING:
            return "MOVE : SHAKING";
        case MOTION_MONITOR_STATE_UNKNOWN:
        default:
            return "MOVE : UNKNOWN";
    }
}

/** @brief Rút gọn snapshot thành một cảnh báo page-local duy nhất. */
static MotionDisplay_Warning_t motion_display_select_warning(
    const Adxl345_Status_t *sensor,
    const MotionMonitor_Status_t *monitor)
{
    if (sensor->has_error)
    {
        if ((sensor->last_error == ADXL345_ERROR_DEVICE_NOT_FOUND) ||
            (sensor->last_error == ADXL345_ERROR_INVALID_DEVICE_ID))
        {
            return MOTION_DISPLAY_WARNING_SENSOR_MISSING;
        }

        if (sensor->last_error == ADXL345_ERROR_DATA_STALE)
        {
            return MOTION_DISPLAY_WARNING_STALE;
        }

        return MOTION_DISPLAY_WARNING_SENSOR_ERROR;
    }

    if (!sensor->is_ready || !sensor->has_sample || !monitor->has_data)
    {
        return MOTION_DISPLAY_WARNING_WAITING;
    }

    if (!sensor->sample_is_fresh || !monitor->data_is_fresh)
    {
        return MOTION_DISPLAY_WARNING_STALE;
    }

    return MOTION_DISPLAY_WARNING_NONE;
}

/** @brief Cho biết badge có cần ẩn/hiện phần chữ theo thời gian hay không. */
static bool motion_display_warning_blinks(MotionDisplay_Warning_t warning)
{
    return (warning == MOTION_DISPLAY_WARNING_STALE) ||
           (warning == MOTION_DISPLAY_WARNING_SENSOR_MISSING) ||
           (warning == MOTION_DISPLAY_WARNING_SENSOR_ERROR);
}

/** @brief Nhãn cảnh báo ngắn; dấu chấm than được vẽ riêng và luôn hiện. */
static const char *motion_display_warning_text(
    MotionDisplay_Warning_t warning)
{
    switch (warning)
    {
        case MOTION_DISPLAY_WARNING_WAITING:
            return "WAIT";
        case MOTION_DISPLAY_WARNING_STALE:
            return "STALE";
        case MOTION_DISPLAY_WARNING_SENSOR_MISSING:
            return "NO IMU";
        case MOTION_DISPLAY_WARNING_SENSOR_ERROR:
            return "IMU";
        case MOTION_DISPLAY_WARNING_NONE:
        default:
            return "";
    }
}

/** @brief Tiến blink đúng một bước; chỉ được gọi khi trang motion đang render. */
static bool motion_display_update_warning_blink(MotionDisplay_t *display,
                                                uint32_t current_tick_ms)
{
    const bool should_blink = motion_display_warning_blinks(
        display->status.active_warning);
    const bool should_show_static =
        (display->status.active_warning == MOTION_DISPLAY_WARNING_WAITING);
    bool changed = false;

    if (!should_blink)
    {
        if (display->status.warning_text_is_visible != should_show_static)
        {
            display->status.warning_text_is_visible = should_show_static;
            changed = true;
        }
        display->status.warning_blink_timer_is_active = false;
        return changed;
    }

    if (!display->status.warning_blink_timer_is_active)
    {
        display->status.warning_blink_timer_is_active = true;
        display->status.warning_blink_tick_ms = current_tick_ms;
        if (!display->status.warning_text_is_visible)
        {
            display->status.warning_text_is_visible = true;
            changed = true;
        }
        return changed;
    }

    if ((uint32_t)(current_tick_ms - display->status.warning_blink_tick_ms) >=
        MOTION_DISPLAY_WARNING_BLINK_INTERVAL_MS)
    {
        display->status.warning_text_is_visible =
            !display->status.warning_text_is_visible;
        display->status.warning_blink_tick_ms = current_tick_ms;
        changed = true;
    }

    return changed;
}

/** @brief So sánh riêng các trường thật sự xuất hiện trên OLED. */
static bool motion_display_content_is_equal(const MotionDisplay_t *display,
                                             const Adxl345_Status_t *sensor,
                                             const MotionMonitor_Status_t *monitor)
{
    const Adxl345_Status_t *old = &display->desired_sensor;
    const MotionMonitor_Status_t *old_monitor = &display->desired_monitor;

    return (old->has_sample == sensor->has_sample) &&
           (old_monitor->has_data == monitor->has_data) &&
           (old_monitor->data_is_fresh == monitor->data_is_fresh) &&
           (old_monitor->total_acceleration_mg ==
            monitor->total_acceleration_mg) &&
           (old_monitor->orientation == monitor->orientation) &&
           (old_monitor->motion_state == monitor->motion_state) &&
           (display->status.active_warning ==
            motion_display_select_warning(sensor, monitor));
}

/** @brief Vẽ một dòng text thường. */
static bool motion_display_draw_line(const MonoGraphics_Canvas_t *canvas,
                                     uint16_t y,
                                     const char *text)
{
    return MonoGraphics_DrawText(canvas,
                                 1U,
                                 y,
                                 text,
                                 &g_mono_font_5x7,
                                 MONO_GRAPHICS_PIXEL_ON,
                                 MONO_GRAPHICS_PIXEL_OFF,
                                 false);
}

/** @brief Vẽ tiêu đề bên trái và badge cảnh báo nhỏ ở góc phải. */
static bool motion_display_draw_header(const MotionDisplay_t *display,
                                       const MonoGraphics_Canvas_t *canvas)
{
    const MotionDisplay_Warning_t warning = display->status.active_warning;
    const char *warning_text = motion_display_warning_text(warning);
    const bool warning_blinks = motion_display_warning_blinks(warning);
    size_t warning_length = strlen(warning_text);
    uint16_t warning_x;

    if (!MonoGraphics_FillRectangle(canvas,
                                    0U,
                                    0U,
                                    MOTION_DISPLAY_WIDTH_PIXELS,
                                    MOTION_DISPLAY_HEADER_HEIGHT,
                                    MONO_GRAPHICS_PIXEL_ON) ||
        !MonoGraphics_DrawText(canvas,
                               1U,
                               0U,
                               "MOTION",
                               &g_mono_font_5x7,
                               MONO_GRAPHICS_PIXEL_OFF,
                               MONO_GRAPHICS_PIXEL_ON,
                               false))
    {
        return false;
    }

    if (warning == MOTION_DISPLAY_WARNING_NONE)
    {
        return true;
    }

    warning_x = (uint16_t)(MOTION_DISPLAY_WIDTH_PIXELS - 1U -
                           ((warning_length + (warning_blinks ? 1U : 0U)) *
                            g_mono_font_5x7.horizontal_advance));

    if (warning_blinks &&
        !MonoGraphics_DrawText(canvas,
                               warning_x,
                               0U,
                               "!",
                               &g_mono_font_5x7,
                               MONO_GRAPHICS_PIXEL_OFF,
                               MONO_GRAPHICS_PIXEL_ON,
                               false))
    {
        return false;
    }

    if (!display->status.warning_text_is_visible)
    {
        return true;
    }

    if (warning_blinks)
    {
        warning_x = (uint16_t)(warning_x +
                               g_mono_font_5x7.horizontal_advance);
    }

    return MonoGraphics_DrawText(canvas,
                                 warning_x,
                                 0U,
                                 warning_text,
                                 &g_mono_font_5x7,
                                 MONO_GRAPHICS_PIXEL_OFF,
                                 MONO_GRAPHICS_PIXEL_ON,
                                 false);
}

/** @brief Vẽ toàn bộ frame từ snapshot ADXL345 hiện tại. */
static bool motion_display_draw_frame(
    const MotionDisplay_t *display,
    const MonoGraphics_Canvas_t *canvas,
    MotionDisplay_ClearCanvasFunction_t clear_canvas)
{
    char acceleration_line[MOTION_DISPLAY_TEXT_CAPACITY];
    const Adxl345_Status_t *sensor = &display->desired_sensor;
    const MotionMonitor_Status_t *monitor = &display->desired_monitor;
    const char *motion_line;
    const char *orientation_line;

    if ((canvas == NULL) ||
        (clear_canvas == NULL) ||
        (canvas->width < MOTION_DISPLAY_WIDTH_PIXELS) ||
        (canvas->height < MOTION_DISPLAY_HEIGHT_PIXELS) ||
        !clear_canvas(canvas->context))
    {
        return false;
    }

    if (!sensor->has_sample || !monitor->has_data)
    {
        (void)strcpy(acceleration_line, "ACCEL: --.-- g");
        motion_line = "MOVE : --";
        orientation_line = "POSE : --";
    }
    else if (!motion_display_build_acceleration_line(
                 acceleration_line,
                 monitor->total_acceleration_mg))
    {
        return false;
    }
    else
    {
        motion_line = motion_display_motion_text(monitor->motion_state);
        orientation_line =
            motion_display_orientation_text(monitor->orientation);
    }

    return motion_display_draw_header(display, canvas) &&
           motion_display_draw_line(canvas, 15U, motion_line) &&
           motion_display_draw_line(canvas, 29U, orientation_line) &&
           motion_display_draw_line(canvas, 45U, acceleration_line);
}

bool MotionDisplay_Initialize(MotionDisplay_t *display)
{
    if (display == NULL)
    {
        return false;
    }

    *display = (MotionDisplay_t){0};
    display->status.is_initialized = true;
    display->status.redraw_is_pending = true;
    display->status.active_warning = MOTION_DISPLAY_WARNING_NONE;
    display->status.last_render_result = MOTION_DISPLAY_RENDER_IDLE;
    return true;
}

void MotionDisplay_Update(MotionDisplay_t *display,
                          const Adxl345_Status_t *sensor_status,
                          const MotionMonitor_Status_t *monitor_status)
{
    bool content_changed;
    MotionDisplay_Warning_t new_warning;

    if ((display == NULL) ||
        !display->status.is_initialized ||
        (sensor_status == NULL) ||
        (monitor_status == NULL))
    {
        return;
    }

    new_warning = motion_display_select_warning(sensor_status, monitor_status);
    content_changed =
        !display->status.has_desired_content ||
        !motion_display_content_is_equal(display,
                                         sensor_status,
                                         monitor_status);
    display->desired_sensor = *sensor_status;
    display->desired_monitor = *monitor_status;
    display->status.has_desired_content = true;

    if (display->status.active_warning != new_warning)
    {
        display->status.active_warning = new_warning;
        display->status.warning_text_is_visible =
            (new_warning != MOTION_DISPLAY_WARNING_NONE);
        display->status.warning_blink_timer_is_active = false;
    }

    if (content_changed)
    {
        display->status.redraw_is_pending = true;
    }
}

MotionDisplay_RenderResult_t MotionDisplay_RenderIfDue(
    MotionDisplay_t *display,
    const MonoGraphics_Canvas_t *canvas,
    MotionDisplay_ClearCanvasFunction_t clear_canvas,
    uint32_t current_tick_ms)
{
    if ((display == NULL) ||
        !display->status.is_initialized ||
        !display->status.has_desired_content)
    {
        if (display != NULL)
        {
            display->status.last_render_result = MOTION_DISPLAY_RENDER_IDLE;
        }
        return MOTION_DISPLAY_RENDER_IDLE;
    }

    if (motion_display_update_warning_blink(display, current_tick_ms))
    {
        display->status.redraw_is_pending = true;
    }

    if (!display->status.redraw_is_pending)
    {
        display->status.last_render_result = MOTION_DISPLAY_RENDER_IDLE;
        return MOTION_DISPLAY_RENDER_IDLE;
    }

    if (display->status.has_rendered_frame &&
        ((uint32_t)(current_tick_ms - display->status.last_render_tick_ms) <
         MOTION_DISPLAY_MIN_REFRESH_INTERVAL_MS))
    {
        display->status.last_render_result =
            MOTION_DISPLAY_RENDER_WAIT_INTERVAL;
        return MOTION_DISPLAY_RENDER_WAIT_INTERVAL;
    }

    if (!motion_display_draw_frame(display, canvas, clear_canvas))
    {
        display->status.last_render_result = MOTION_DISPLAY_RENDER_FAILED;
        return MOTION_DISPLAY_RENDER_FAILED;
    }

    display->status.redraw_is_pending = false;
    display->status.has_rendered_frame = true;
    display->status.last_render_tick_ms = current_tick_ms;
    if (display->status.rendered_frame_count < UINT32_MAX)
    {
        display->status.rendered_frame_count++;
    }
    display->status.last_render_result = MOTION_DISPLAY_RENDER_DRAWN;
    return MOTION_DISPLAY_RENDER_DRAWN;
}

void MotionDisplay_RequestRedraw(MotionDisplay_t *display)
{
    if ((display != NULL) && display->status.is_initialized)
    {
        display->status.redraw_is_pending = true;
        if (motion_display_warning_blinks(display->status.active_warning))
        {
            display->status.warning_text_is_visible = true;
            display->status.warning_blink_timer_is_active = false;
        }
    }
}

void MotionDisplay_GetStatus(const MotionDisplay_t *display,
                             MotionDisplay_Status_t *output_status)
{
    if ((display != NULL) && (output_status != NULL))
    {
        *output_status = display->status;
    }
}
