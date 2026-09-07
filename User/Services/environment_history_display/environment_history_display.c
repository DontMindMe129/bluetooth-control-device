/**
 * @file environment_history_display.c
 * @brief Giao diện danh sách và chi tiết lịch sử DHT11 trên OLED 128x64.
 */

#include "environment_history_display.h"

#include <limits.h>
#include <stddef.h>

#include "mono_font_5x7.h"

#define HISTORY_DISPLAY_WIDTH_PIXELS   (128U)
#define HISTORY_DISPLAY_HEIGHT_PIXELS  (64U)
#define HISTORY_DISPLAY_HEADER_HEIGHT  (9U)
#define HISTORY_DISPLAY_VISIBLE_ROWS   (5U)
#define HISTORY_DISPLAY_LINE_CAPACITY  (22U)

static uint8_t history_append_text(char *line,
                                   uint8_t position,
                                   const char *text)
{
    while ((text != NULL) && (*text != '\0') &&
           (position < (HISTORY_DISPLAY_LINE_CAPACITY - 1U)))
    {
        line[position++] = *text++;
    }
    line[position] = '\0';
    return position;
}

static uint8_t history_append_uint(char *line,
                                   uint8_t position,
                                   uint32_t value,
                                   uint8_t minimum_digits)
{
    char reversed[10];
    uint8_t count = 0U;

    do
    {
        reversed[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while ((value != 0U) && (count < sizeof(reversed)));

    while ((count < minimum_digits) && (count < sizeof(reversed)))
    {
        reversed[count++] = '0';
    }
    while ((count > 0U) &&
           (position < (HISTORY_DISPLAY_LINE_CAPACITY - 1U)))
    {
        line[position++] = reversed[--count];
    }
    line[position] = '\0';
    return position;
}

static uint8_t history_append_time(char *line,
                                   uint8_t position,
                                   uint32_t tick_ms)
{
    uint32_t total_seconds = tick_ms / 1000UL;
    uint32_t minutes = total_seconds / 60UL;

    position = history_append_uint(line, position, minutes, 2U);
    line[position++] = ':';
    line[position] = '\0';
    return history_append_uint(line, position, total_seconds % 60UL, 2U);
}

static const char *history_error_text(uint8_t error)
{
    switch (error)
    {
        case DHT11_ERROR_BUS_STUCK_LOW:
        case DHT11_ERROR_START_DRIVE:
            return "BUS";
        case DHT11_ERROR_ACK_TIMEOUT:
        case DHT11_ERROR_ACK_TIMING:
            return "ACK";
        case DHT11_ERROR_BIT_TIMING:
            return "BIT";
        case DHT11_ERROR_CAPTURE_OVERRUN:
            return "OVR";
        case DHT11_ERROR_FRAME_TIMEOUT:
            return "TIME";
        case DHT11_ERROR_CHECKSUM:
            return "CRC";
        case DHT11_ERROR_INTERNAL_STATE:
            return "INT";
        case DHT11_ERROR_INVALID_CONFIG:
        case DHT11_ERROR_TIMER_START:
            return "CFG";
        case DHT11_ERROR_NONE:
        default:
            return "NONE";
    }
}

static const char *history_data_state_text(uint8_t state)
{
    if (state == ENVIRONMENT_DATA_FRESH)
    {
        return "FRESH";
    }
    if (state == ENVIRONMENT_DATA_STALE)
    {
        return "STALE";
    }
    return "NO DATA";
}

static bool history_draw_header(const EnvironmentHistoryDisplay_t *display,
                                const MonoGraphics_Canvas_t *canvas,
                                uint16_t selected_index)
{
    char line[HISTORY_DISPLAY_LINE_CAPACITY] = {0};
    uint8_t position = 0U;

    position = history_append_text(line, position, "HIST ");
    position = history_append_text(
        line,
        position,
        display->status.monitoring_is_active ? "ON " : "OFF ");
    position = history_append_uint(line, position,
        display->status.history_count == 0U ? 0U : (uint32_t)selected_index + 1U,
        3U);
    position = history_append_text(line, position, "/");
    position = history_append_uint(line, position,
                                   display->status.history_count, 3U);
    if (display->status.has_newer_record)
    {
        position = history_append_text(line, position, "+");
    }
    if (display->status.current_sensor_warning)
    {
        (void)history_append_text(line, position, "!");
    }

    return MonoGraphics_FillRectangle(canvas, 0U, 0U,
                                      HISTORY_DISPLAY_WIDTH_PIXELS,
                                      HISTORY_DISPLAY_HEADER_HEIGHT,
                                      MONO_GRAPHICS_PIXEL_ON) &&
           MonoGraphics_DrawText(canvas, 1U, 1U, line, &g_mono_font_5x7,
                                 MONO_GRAPHICS_PIXEL_OFF,
                                 MONO_GRAPHICS_PIXEL_ON, false);
}

static void history_build_list_line(
    char *line,
    const EnvironmentHistory_Record_t *record,
    bool selected)
{
    uint8_t position = 0U;
    line[position++] = selected ? '>' : ' ';
    position = history_append_time(line, position, record->completed_tick_ms);
    line[position++] = ' ';
    line[position] = '\0';

    if (record->measurement_error != DHT11_ERROR_NONE)
    {
        position = history_append_text(line, position, "ERR:");
        position = history_append_text(line, position,
                                       history_error_text(record->measurement_error));
        line[position++] = ' ';
        line[position++] =
            record->cached_data_state == ENVIRONMENT_DATA_FRESH ? 'F' :
            record->cached_data_state == ENVIRONMENT_DATA_STALE ? 'S' : 'N';
        line[position] = '\0';
        return;
    }

    position = history_append_uint(line, position,
                                   record->measured_data.temperature_integer,
                                   1U);
    position = history_append_text(line, position, "C ");
    position = history_append_uint(line, position,
                                   record->measured_data.humidity_integer,
                                   1U);
    (void)history_append_text(line, position, "%");
}

static bool history_draw_list(const EnvironmentHistoryDisplay_t *display,
                              const MonoGraphics_Canvas_t *canvas,
                              uint16_t selected_index)
{
    char line[HISTORY_DISPLAY_LINE_CAPACITY];
    uint16_t window_start = 0U;
    uint16_t row;

    if (display->status.history_count == 0U)
    {
        return MonoGraphics_DrawText(canvas, 1U, 15U, "NO RECORDS",
                                     &g_mono_font_5x7,
                                     MONO_GRAPHICS_PIXEL_ON,
                                     MONO_GRAPHICS_PIXEL_OFF, false);
    }

    if (display->status.history_count > HISTORY_DISPLAY_VISIBLE_ROWS)
    {
        if (selected_index <= 2U)
        {
            window_start = 0U;
        }
        else if ((uint16_t)(selected_index + 2U) >=
                 display->status.history_count)
        {
            window_start = (uint16_t)(display->status.history_count -
                                      HISTORY_DISPLAY_VISIBLE_ROWS);
        }
        else
        {
            window_start = (uint16_t)(selected_index - 2U);
        }
    }

    for (row = 0U; row < HISTORY_DISPLAY_VISIBLE_ROWS; row++)
    {
        uint16_t record_index = (uint16_t)(window_start + row);
        EnvironmentHistory_Record_t record;

        if (record_index >= display->status.history_count)
        {
            break;
        }
        if (!EnvironmentHistory_GetRecord(display->history,
                                          record_index,
                                          &record))
        {
            return false;
        }
        history_build_list_line(line, &record, record_index == selected_index);
        if (!MonoGraphics_DrawText(canvas, 1U,
                                   (uint16_t)(11U + (row * 10U)), line,
                                   &g_mono_font_5x7,
                                   MONO_GRAPHICS_PIXEL_ON,
                                   MONO_GRAPHICS_PIXEL_OFF, false))
        {
            return false;
        }
    }
    return true;
}

/** @brief Vẽ một dòng detail rồi tăng tọa độ y cho dòng kế tiếp. */
static bool history_draw_detail_line(const MonoGraphics_Canvas_t *canvas,
                                     uint16_t *y,
                                     const char *line)
{
    if ((y == NULL) ||
        !MonoGraphics_DrawText(canvas, 1U, *y, line, &g_mono_font_5x7,
                               MONO_GRAPHICS_PIXEL_ON,
                               MONO_GRAPHICS_PIXEL_OFF, false))
    {
        return false;
    }
    *y = (uint16_t)(*y + 9U);
    return true;
}

static bool history_draw_detail(const EnvironmentHistoryDisplay_t *display,
                                const MonoGraphics_Canvas_t *canvas,
                                uint16_t selected_index)
{
    EnvironmentHistory_Record_t record;
    char line[HISTORY_DISPLAY_LINE_CAPACITY];
    uint8_t position;
    uint16_t y = 11U;

    if (!EnvironmentHistory_GetRecord(display->history,
                                      selected_index,
                                      &record))
    {
        return false;
    }

    position = history_append_text(line, 0U, "TIME: ");
    (void)history_append_time(line, position, record.completed_tick_ms);
    if (!history_draw_detail_line(canvas, &y, line)) return false;

    if (record.measurement_error == DHT11_ERROR_NONE)
    {
        position = history_append_text(line, 0U, "TEMP: ");
        position = history_append_uint(line, position,
            record.measured_data.temperature_integer, 1U);
        line[position++] = '.';
        position = history_append_uint(line, position,
            record.measured_data.temperature_decimal, 1U);
        (void)history_append_text(line, position, " C");
        if (!history_draw_detail_line(canvas, &y, line)) return false;

        position = history_append_text(line, 0U, "HUM: ");
        position = history_append_uint(line, position,
            record.measured_data.humidity_integer, 1U);
        line[position++] = '.';
        position = history_append_uint(line, position,
            record.measured_data.humidity_decimal, 1U);
        (void)history_append_text(line, position, " %");
        if (!history_draw_detail_line(canvas, &y, line)) return false;
    }
    else
    {
        position = history_append_text(line, 0U, "RESULT: ");
        (void)history_append_text(line, position,
            history_error_text(record.measurement_error));
        if (!history_draw_detail_line(canvas, &y, line)) return false;

        position = history_append_text(line, 0U, "ERROR RUN: ");
        (void)history_append_uint(line, position,
            record.consecutive_error_count, 1U);
        if (!history_draw_detail_line(canvas, &y, line)) return false;

        (void)history_append_text(line, 0U, "NO NEW SAMPLE");
        if (!history_draw_detail_line(canvas, &y, line)) return false;
    }

    position = history_append_text(line, 0U, "DATA: ");
    (void)history_append_text(line, position,
        history_data_state_text(record.cached_data_state));
    if (!history_draw_detail_line(canvas, &y, line)) return false;

    position = history_append_text(line, 0U, "SESSION: ");
    (void)history_append_uint(line, position, record.session_id, 1U);
    if (!history_draw_detail_line(canvas, &y, line)) return false;
    return true;
}

bool EnvironmentHistoryDisplay_Initialize(
    EnvironmentHistoryDisplay_t *display,
    const EnvironmentHistory_t *history)
{
    if ((display == NULL) || (history == NULL))
    {
        return false;
    }
    *display = (EnvironmentHistoryDisplay_t){0};
    display->history = history;
    display->status.is_initialized = true;
    display->status.redraw_is_pending = true;
    return true;
}

void EnvironmentHistoryDisplay_Update(
    EnvironmentHistoryDisplay_t *display,
    const EnvironmentMonitor_Status_t *current_environment,
    bool monitoring_is_active)
{
    uint16_t new_count;
    bool warning;
    bool content_changed = false;
    EnvironmentHistory_Record_t latest;

    if ((display == NULL) || !display->status.is_initialized ||
        (current_environment == NULL))
    {
        return;
    }

    warning = (current_environment->data_state != ENVIRONMENT_DATA_FRESH) ||
              (current_environment->last_sensor_error != DHT11_ERROR_NONE);
    if (warning != display->status.current_sensor_warning)
    {
        display->status.current_sensor_warning = warning;
        content_changed = true;
    }
    if (monitoring_is_active != display->status.monitoring_is_active)
    {
        display->status.monitoring_is_active = monitoring_is_active;
        content_changed = true;
    }
    new_count = EnvironmentHistory_GetCount(display->history);
    if ((new_count > 0U) &&
        EnvironmentHistory_GetRecord(display->history,
                                     (uint16_t)(new_count - 1U),
                                     &latest))
    {
        bool has_new_latest = !display->latest_record_is_known ||
            (latest.completed_tick_ms != display->latest_record_tick_ms);

        if (has_new_latest && display->latest_record_is_known &&
            (display->status.selected_age_from_newest > 0U))
        {
            if (display->status.selected_age_from_newest < (new_count - 1U))
            {
                display->status.selected_age_from_newest++;
            }
        }
        if (has_new_latest)
        {
            display->latest_record_tick_ms = latest.completed_tick_ms;
            display->latest_record_is_known = true;
            content_changed = true;
        }
    }
    else
    {
        display->latest_record_is_known = false;
        display->status.selected_age_from_newest = 0U;
    }

    if (display->status.history_count != new_count)
    {
        display->status.history_count = new_count;
        content_changed = true;
    }
    if ((new_count > 0U) &&
        (display->status.selected_age_from_newest >= new_count))
    {
        display->status.selected_age_from_newest = (uint16_t)(new_count - 1U);
        content_changed = true;
    }
    display->status.has_newer_record =
        display->status.selected_age_from_newest > 0U;
    if (content_changed)
    {
        display->status.redraw_is_pending = true;
    }
}

void EnvironmentHistoryDisplay_EnterPage(EnvironmentHistoryDisplay_t *display)
{
    if ((display != NULL) && display->status.is_initialized)
    {
        display->status.view = ENVIRONMENT_HISTORY_DISPLAY_LIST;
        display->status.selected_age_from_newest = 0U;
        display->status.has_newer_record = false;
        display->status.redraw_is_pending = true;
    }
}

bool EnvironmentHistoryDisplay_HandleInput(EnvironmentHistoryDisplay_t *display,
                                           bool up_pressed,
                                           bool down_pressed,
                                           bool ok_pressed)
{
    uint8_t pressed_count = (uint8_t)(up_pressed ? 1U : 0U) +
                            (uint8_t)(down_pressed ? 1U : 0U) +
                            (uint8_t)(ok_pressed ? 1U : 0U);
    bool changed = false;

    if ((display == NULL) || !display->status.is_initialized ||
        (pressed_count != 1U))
    {
        return false;
    }
    if (up_pressed && (display->status.history_count > 0U) &&
        (display->status.selected_age_from_newest <
         (display->status.history_count - 1U)))
    {
        display->status.selected_age_from_newest++;
        changed = true;
    }
    else if (down_pressed &&
             (display->status.selected_age_from_newest > 0U))
    {
        display->status.selected_age_from_newest--;
        changed = true;
    }
    else if (ok_pressed && (display->status.history_count > 0U))
    {
        display->status.view =
            display->status.view == ENVIRONMENT_HISTORY_DISPLAY_LIST
                ? ENVIRONMENT_HISTORY_DISPLAY_DETAIL
                : ENVIRONMENT_HISTORY_DISPLAY_LIST;
        changed = true;
    }

    if (changed)
    {
        display->status.has_newer_record =
            display->status.selected_age_from_newest > 0U;
        display->status.redraw_is_pending = true;
    }
    return changed;
}

EnvironmentHistoryDisplay_RenderResult_t
EnvironmentHistoryDisplay_RenderIfDue(
    EnvironmentHistoryDisplay_t *display,
    const MonoGraphics_Canvas_t *canvas,
    EnvironmentHistoryDisplay_ClearCanvasFunction_t clear_canvas,
    uint32_t current_tick_ms)
{
    uint16_t selected_index = 0U;
    bool draw_succeeded;

    if ((display == NULL) || !display->status.is_initialized ||
        !display->status.redraw_is_pending)
    {
        return ENVIRONMENT_HISTORY_DISPLAY_RENDER_IDLE;
    }
    if (display->status.has_rendered_frame &&
        ((uint32_t)(current_tick_ms - display->status.last_render_tick_ms) <
         ENVIRONMENT_HISTORY_DISPLAY_MIN_REFRESH_INTERVAL_MS))
    {
        display->status.last_render_result =
            ENVIRONMENT_HISTORY_DISPLAY_RENDER_WAIT_INTERVAL;
        return display->status.last_render_result;
    }
    if ((canvas == NULL) || (clear_canvas == NULL) ||
        (canvas->width < HISTORY_DISPLAY_WIDTH_PIXELS) ||
        (canvas->height < HISTORY_DISPLAY_HEIGHT_PIXELS) ||
        !clear_canvas(canvas->context))
    {
        display->status.last_render_result =
            ENVIRONMENT_HISTORY_DISPLAY_RENDER_FAILED;
        return display->status.last_render_result;
    }

    if (display->status.history_count > 0U)
    {
        selected_index = (uint16_t)(display->status.history_count - 1U -
            display->status.selected_age_from_newest);
    }
    draw_succeeded = history_draw_header(display, canvas, selected_index);
    if (draw_succeeded)
    {
        draw_succeeded = display->status.view == ENVIRONMENT_HISTORY_DISPLAY_DETAIL &&
                         display->status.history_count > 0U
            ? history_draw_detail(display, canvas, selected_index)
            : history_draw_list(display, canvas, selected_index);
    }
    if (!draw_succeeded)
    {
        display->status.last_render_result =
            ENVIRONMENT_HISTORY_DISPLAY_RENDER_FAILED;
        return display->status.last_render_result;
    }

    display->status.redraw_is_pending = false;
    display->status.has_rendered_frame = true;
    display->status.last_render_tick_ms = current_tick_ms;
    if (display->status.rendered_frame_count < UINT32_MAX)
    {
        display->status.rendered_frame_count++;
    }
    display->status.last_render_result =
        ENVIRONMENT_HISTORY_DISPLAY_RENDER_DRAWN;
    return display->status.last_render_result;
}

void EnvironmentHistoryDisplay_RequestRedraw(
    EnvironmentHistoryDisplay_t *display)
{
    if ((display != NULL) && display->status.is_initialized)
    {
        display->status.redraw_is_pending = true;
    }
}

void EnvironmentHistoryDisplay_GetStatus(
    const EnvironmentHistoryDisplay_t *display,
    EnvironmentHistoryDisplay_Status_t *output_status)
{
    if ((display != NULL) && (output_status != NULL))
    {
        *output_status = display->status;
    }
}
