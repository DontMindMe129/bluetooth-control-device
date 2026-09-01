/**
 * @file environment_display.c
 * @brief Hien thuc giao dien moi truong ASCII tren canvas don sac 128x64.
 */

#include "environment_display.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "mono_font_5x7.h"

#define ENVIRONMENT_DISPLAY_WIDTH_PIXELS       (128U)
#define ENVIRONMENT_DISPLAY_HEIGHT_PIXELS      (64U)
#define ENVIRONMENT_DISPLAY_TEXT_CAPACITY      (22U)
#define ENVIRONMENT_DISPLAY_HEADER_HEIGHT      (9U)

/** @brief Them chuoi vao buffer co gioi han va luon giu ky tu ket thuc NULL. */
static bool environment_display_append_text(char *buffer,
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

/** @brief Them gia tri uint8 dang thap phan ma khong can snprintf hoac so thuc. */
static bool environment_display_append_u8(char *buffer,
                                          uint8_t capacity,
                                          uint8_t *length,
                                          uint8_t value)
{
    char digits[3];
    uint8_t digit_count = 0U;

    do
    {
        digits[digit_count] = (char)('0' + (value % 10U));
        digit_count++;
        value = (uint8_t)(value / 10U);
    } while ((value != 0U) && (digit_count < sizeof(digits)));

    while (digit_count > 0U)
    {
        char character[2];

        digit_count--;
        character[0] = digits[digit_count];
        character[1] = '\0';
        if (!environment_display_append_text(buffer,
                                             capacity,
                                             length,
                                             character))
        {
            return false;
        }
    }

    return true;
}

/** @brief Tao mot dong nhiet do hoac do am tu hai byte nguyen/thap phan DHT11. */
static bool environment_display_build_measurement(
    char *line,
    const char *label,
    uint8_t integer_part,
    uint8_t decimal_part,
    const char *unit)
{
    uint8_t length = 0U;

    line[0] = '\0';
    return environment_display_append_text(line,
                                           ENVIRONMENT_DISPLAY_TEXT_CAPACITY,
                                           &length,
                                           label) &&
           environment_display_append_u8(line,
                                         ENVIRONMENT_DISPLAY_TEXT_CAPACITY,
                                         &length,
                                         integer_part) &&
           environment_display_append_text(line,
                                           ENVIRONMENT_DISPLAY_TEXT_CAPACITY,
                                           &length,
                                           ".") &&
           environment_display_append_u8(line,
                                         ENVIRONMENT_DISPLAY_TEXT_CAPACITY,
                                         &length,
                                         decimal_part) &&
           environment_display_append_text(line,
                                           ENVIRONMENT_DISPLAY_TEXT_CAPACITY,
                                           &length,
                                           unit);
}

/** @brief Anh xa ket luan monitor sang dong trang thai co y nghia voi nguoi dung. */
static const char *environment_display_condition_line(
    const EnvironmentMonitor_Status_t *environment,
    const EnvironmentFeedback_Status_t *feedback)
{
    if (feedback->warning_active)
    {
        return "STATUS: ALERT";
    }

    if (environment->data_state == ENVIRONMENT_DATA_NO_DATA)
    {
        return "STATUS: WAITING";
    }

    switch (environment->condition)
    {
        case ENVIRONMENT_CONDITION_NORMAL:
            return "STATUS: NORMAL";
        case ENVIRONMENT_CONDITION_WARM:
            return "STATUS: WARM";
        case ENVIRONMENT_CONDITION_HUMID:
            return "STATUS: HUMID";
        case ENVIRONMENT_CONDITION_WARM_AND_HUMID:
            return "STATUS: WARM+HUMID";
        case ENVIRONMENT_CONDITION_UNKNOWN:
        default:
            return "STATUS: UNKNOWN";
    }
}

/** @brief Rut gon snapshot thanh mot canh bao page-local duy nhat. */
static EnvironmentDisplay_Warning_t environment_display_select_warning(
    const EnvironmentMonitor_Status_t *environment,
    const EnvironmentFeedback_Status_t *feedback)
{
    if (environment->last_sensor_error != DHT11_ERROR_NONE)
    {
        return ENVIRONMENT_DISPLAY_WARNING_SENSOR;
    }

    if (environment->data_state == ENVIRONMENT_DATA_STALE)
    {
        return ENVIRONMENT_DISPLAY_WARNING_STALE;
    }

    if (feedback->warning_active)
    {
        return ENVIRONMENT_DISPLAY_WARNING_ALERT;
    }

    if (environment->data_state == ENVIRONMENT_DATA_NO_DATA)
    {
        return ENVIRONMENT_DISPLAY_WARNING_WAITING;
    }

    return ENVIRONMENT_DISPLAY_WARNING_NONE;
}

/** @brief Cho biet badge co can an/hien phan chu theo thoi gian hay khong. */
static bool environment_display_warning_blinks(
    EnvironmentDisplay_Warning_t warning)
{
    return (warning == ENVIRONMENT_DISPLAY_WARNING_ALERT) ||
           (warning == ENVIRONMENT_DISPLAY_WARNING_STALE) ||
           (warning == ENVIRONMENT_DISPLAY_WARNING_SENSOR);
}

/** @brief Nhan ngan gon cua canh bao; dau cham than duoc ve rieng va luon hien. */
static const char *environment_display_warning_text(
    EnvironmentDisplay_Warning_t warning)
{
    switch (warning)
    {
        case ENVIRONMENT_DISPLAY_WARNING_WAITING:
            return "WAIT";
        case ENVIRONMENT_DISPLAY_WARNING_ALERT:
            return "ALERT";
        case ENVIRONMENT_DISPLAY_WARNING_STALE:
            return "STALE";
        case ENVIRONMENT_DISPLAY_WARNING_SENSOR:
            return "DHT";
        case ENVIRONMENT_DISPLAY_WARNING_NONE:
        default:
            return "";
    }
}

/** @brief Tien blink dung mot buoc; ham chi duoc goi khi page nay dang duoc render. */
static bool environment_display_update_warning_blink(
    EnvironmentDisplay_t *display,
    uint32_t current_tick_ms)
{
    const bool should_blink = environment_display_warning_blinks(
        display->status.active_warning);
    const bool should_show_static =
        (display->status.active_warning ==
         ENVIRONMENT_DISPLAY_WARNING_WAITING);
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

    if ((uint32_t)(current_tick_ms -
                   display->status.warning_blink_tick_ms) >=
        ENVIRONMENT_DISPLAY_WARNING_BLINK_INTERVAL_MS)
    {
        display->status.warning_text_is_visible =
            !display->status.warning_text_is_visible;
        display->status.warning_blink_tick_ms = current_tick_ms;
        changed = true;
    }

    return changed;
}

/** @brief So sanh rieng cac truong thuc su xuat hien tren OLED. */
static bool environment_display_content_is_equal(
    const EnvironmentDisplay_t *display,
    const EnvironmentMonitor_Status_t *environment,
    const EnvironmentFeedback_Status_t *feedback)
{
    const DHT11_Data_t *old_data = &display->desired_environment.latest_data;
    const DHT11_Data_t *new_data = &environment->latest_data;

    return
        (old_data->humidity_integer == new_data->humidity_integer) &&
        (old_data->humidity_decimal == new_data->humidity_decimal) &&
        (old_data->temperature_integer == new_data->temperature_integer) &&
        (old_data->temperature_decimal == new_data->temperature_decimal) &&
        (display->desired_environment.data_state == environment->data_state) &&
        (display->desired_environment.condition == environment->condition) &&
        (display->desired_feedback.warning_active == feedback->warning_active) &&
        (display->status.active_warning ==
         environment_display_select_warning(environment, feedback));
}

/** @brief Ve mot dong text thuong tren nen da duoc xoa. */
static bool environment_display_draw_line(
    const MonoGraphics_Canvas_t *canvas,
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

/** @brief Ve tieu de ben trai va badge canh bao nho o goc phai. */
static bool environment_display_draw_header(
    const EnvironmentDisplay_t *display,
    const MonoGraphics_Canvas_t *canvas)
{
    const EnvironmentDisplay_Warning_t warning =
        display->status.active_warning;
    const char *warning_text = environment_display_warning_text(warning);
    const bool warning_blinks = environment_display_warning_blinks(warning);
    size_t warning_length = strlen(warning_text);
    uint16_t warning_x;

    if (!MonoGraphics_FillRectangle(canvas,
                                    0U,
                                    0U,
                                    ENVIRONMENT_DISPLAY_WIDTH_PIXELS,
                                    ENVIRONMENT_DISPLAY_HEADER_HEIGHT,
                                    MONO_GRAPHICS_PIXEL_ON) ||
        !MonoGraphics_DrawText(canvas,
                               1U,
                               1U,
                               "ENVIRONMENT",
                               &g_mono_font_5x7,
                               MONO_GRAPHICS_PIXEL_OFF,
                               MONO_GRAPHICS_PIXEL_ON,
                               false))
    {
        return false;
    }

    if (warning == ENVIRONMENT_DISPLAY_WARNING_NONE)
    {
        return true;
    }

    warning_x = (uint16_t)(ENVIRONMENT_DISPLAY_WIDTH_PIXELS - 1U -
                           ((warning_length + (warning_blinks ? 1U : 0U)) *
                            g_mono_font_5x7.horizontal_advance));

    if (warning_blinks &&
        !MonoGraphics_DrawText(canvas,
                               warning_x,
                               1U,
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
                                 1U,
                                 warning_text,
                                 &g_mono_font_5x7,
                                 MONO_GRAPHICS_PIXEL_OFF,
                                 MONO_GRAPHICS_PIXEL_ON,
                                 false);
}

/** @brief Ve toan bo frame tu snapshot mong muon hien tai. */
static bool environment_display_draw_frame(
    const EnvironmentDisplay_t *display,
    const MonoGraphics_Canvas_t *canvas,
    EnvironmentDisplay_ClearCanvasFunction_t clear_canvas)
{
    char temperature_line[ENVIRONMENT_DISPLAY_TEXT_CAPACITY];
    char humidity_line[ENVIRONMENT_DISPLAY_TEXT_CAPACITY];
    if ((canvas == NULL) ||
        (clear_canvas == NULL) ||
        (canvas->width < ENVIRONMENT_DISPLAY_WIDTH_PIXELS) ||
        (canvas->height < ENVIRONMENT_DISPLAY_HEIGHT_PIXELS) ||
        !clear_canvas(canvas->context))
    {
        return false;
    }

    if (display->desired_environment.data_state == ENVIRONMENT_DATA_NO_DATA)
    {
        (void)strcpy(temperature_line, "TEMP : --.- C");
        (void)strcpy(humidity_line, "HUM  : --.- %");
    }
    else if (!environment_display_build_measurement(
                 temperature_line,
                 "TEMP : ",
                 display->desired_environment.latest_data.temperature_integer,
                 display->desired_environment.latest_data.temperature_decimal,
                 " C") ||
             !environment_display_build_measurement(
                 humidity_line,
                 "HUM  : ",
                 display->desired_environment.latest_data.humidity_integer,
                 display->desired_environment.latest_data.humidity_decimal,
                 " %"))
    {
        return false;
    }

    return environment_display_draw_header(display, canvas) &&
        environment_display_draw_line(canvas, 15U, temperature_line) &&
        environment_display_draw_line(canvas, 29U, humidity_line) &&
        environment_display_draw_line(
            canvas,
            45U,
            environment_display_condition_line(
                &display->desired_environment,
                &display->desired_feedback));
}

bool EnvironmentDisplay_Initialize(EnvironmentDisplay_t *display)
{
    if (display == NULL)
    {
        return false;
    }

    *display = (EnvironmentDisplay_t){0};
    display->status.is_initialized = true;
    display->status.redraw_is_pending = true;
    display->status.active_warning = ENVIRONMENT_DISPLAY_WARNING_NONE;
    display->status.last_render_result = ENVIRONMENT_DISPLAY_RENDER_IDLE;
    return true;
}

void EnvironmentDisplay_Update(
    EnvironmentDisplay_t *display,
    const EnvironmentMonitor_Status_t *environment,
    const EnvironmentFeedback_Status_t *feedback)
{
    bool content_changed;
    EnvironmentDisplay_Warning_t new_warning;

    if ((display == NULL) ||
        !display->status.is_initialized ||
        (environment == NULL) ||
        (feedback == NULL))
    {
        return;
    }

    new_warning = environment_display_select_warning(environment, feedback);
    content_changed =
        !display->status.has_desired_content ||
        !environment_display_content_is_equal(display,
                                              environment,
                                              feedback);

    display->desired_environment = *environment;
    display->desired_feedback = *feedback;
    display->status.has_desired_content = true;

    if (display->status.active_warning != new_warning)
    {
        display->status.active_warning = new_warning;
        display->status.warning_text_is_visible =
            (new_warning != ENVIRONMENT_DISPLAY_WARNING_NONE);
        display->status.warning_blink_timer_is_active = false;
    }

    if (content_changed)
    {
        display->status.redraw_is_pending = true;
    }
}

EnvironmentDisplay_RenderResult_t EnvironmentDisplay_RenderIfDue(
    EnvironmentDisplay_t *display,
    const MonoGraphics_Canvas_t *canvas,
    EnvironmentDisplay_ClearCanvasFunction_t clear_canvas,
    uint32_t current_tick_ms)
{
    if ((display == NULL) || !display->status.is_initialized ||
        !display->status.has_desired_content)
    {
        if (display != NULL)
        {
            display->status.last_render_result =
                ENVIRONMENT_DISPLAY_RENDER_IDLE;
        }
        return ENVIRONMENT_DISPLAY_RENDER_IDLE;
    }

    if (environment_display_update_warning_blink(display, current_tick_ms))
    {
        display->status.redraw_is_pending = true;
    }

    if (!display->status.redraw_is_pending)
    {
        display->status.last_render_result = ENVIRONMENT_DISPLAY_RENDER_IDLE;
        return ENVIRONMENT_DISPLAY_RENDER_IDLE;
    }

    if (display->status.has_rendered_frame &&
        ((uint32_t)(current_tick_ms - display->status.last_render_tick_ms) <
         ENVIRONMENT_DISPLAY_MIN_REFRESH_INTERVAL_MS))
    {
        display->status.last_render_result =
            ENVIRONMENT_DISPLAY_RENDER_WAIT_INTERVAL;
        return ENVIRONMENT_DISPLAY_RENDER_WAIT_INTERVAL;
    }

    if (!environment_display_draw_frame(display, canvas, clear_canvas))
    {
        display->status.last_render_result =
            ENVIRONMENT_DISPLAY_RENDER_FAILED;
        return ENVIRONMENT_DISPLAY_RENDER_FAILED;
    }

    display->status.redraw_is_pending = false;
    display->status.has_rendered_frame = true;
    display->status.last_render_tick_ms = current_tick_ms;
    if (display->status.rendered_frame_count < UINT32_MAX)
    {
        display->status.rendered_frame_count++;
    }
    display->status.last_render_result = ENVIRONMENT_DISPLAY_RENDER_DRAWN;
    return ENVIRONMENT_DISPLAY_RENDER_DRAWN;
}

void EnvironmentDisplay_RequestRedraw(EnvironmentDisplay_t *display)
{
    if ((display != NULL) && display->status.is_initialized)
    {
        display->status.redraw_is_pending = true;
        if (environment_display_warning_blinks(
                display->status.active_warning))
        {
            display->status.warning_text_is_visible = true;
            display->status.warning_blink_timer_is_active = false;
        }
    }
}

void EnvironmentDisplay_GetStatus(
    const EnvironmentDisplay_t *display,
    EnvironmentDisplay_Status_t *output_status)
{
    if ((display != NULL) && (output_status != NULL))
    {
        *output_status = display->status;
    }
}
