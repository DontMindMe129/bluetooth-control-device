/**
 * @file warning_history_display.c
 * @brief Giao diện danh sách và chi tiết lịch sử cảnh báo trên OLED 128x64.
 */

#include "warning_history_display.h"

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

/** @brief Tên sự kiện để hiển thị, không thay đổi lịch sử. */
static const char *history_event_text(WarningHistory_Event_t event)
{
    switch (event)
    {
        case WARNING_HISTORY_EVENT_STARTED: return "START";
        case WARNING_HISTORY_EVENT_SOURCE_CHANGED: return "CHANGE";
        case WARNING_HISTORY_EVENT_CLEARED: return "CLEAR";
        default: return "?";
    }
}

/** @brief Nguồn sau chuyển tiếp; CLEAR có mask 0 nên hiển thị NONE. */
static const char *history_source_text(uint8_t mask)
{
    static const char *const names[8] = {
        "NONE", "MANUAL", "ENV", "MANUAL|ENV",
        "SHAKING", "MANUAL|SHAKING", "ENV|SHAKING", "MANUAL|ENV|SHAKING"
    };
    return names[mask & 7U];
}

static bool history_draw_header(const WarningHistoryDisplay_t *display,
                                const MonoGraphics_Canvas_t *canvas,
                                uint16_t selected_index)
{
    char line[HISTORY_DISPLAY_LINE_CAPACITY] = {0};
    uint8_t position = 0U;

    position = history_append_text(line, position, "WARN ");
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

    return MonoGraphics_FillRectangle(canvas, 0U, 0U,
                                      HISTORY_DISPLAY_WIDTH_PIXELS,
                                      HISTORY_DISPLAY_HEADER_HEIGHT,
                                      MONO_GRAPHICS_PIXEL_ON) &&
           MonoGraphics_DrawText(canvas, 1U, 1U, line, &g_mono_font_5x7,
                                 MONO_GRAPHICS_PIXEL_OFF,
                                 MONO_GRAPHICS_PIXEL_ON, false);
}

/** @brief Một dòng gồm thời điểm, sự kiện và nguồn viết tắt M/E/S. */
static void history_build_list_line(char *line,
                                    const WarningHistory_Record_t *record,
                                    bool selected)
{
    uint8_t position = history_append_text(line, 0U, selected ? ">" : " ");
    position = history_append_time(line, position, record->tick_ms);
    position = history_append_text(line, position, " ");
    position = history_append_text(line, position, history_event_text(record->event));
    position = history_append_text(line, position, " ");
    if (record->source_mask == 0U)
        (void)history_append_text(line, position, "-");
    else {
        if (record->source_mask & 1U) position = history_append_text(line, position, "M");
        if (record->source_mask & 2U) position = history_append_text(line, position, "E");
        if (record->source_mask & 4U) (void)history_append_text(line, position, "S");
    }
}

static bool history_draw_list(const WarningHistoryDisplay_t *display,
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
        WarningHistory_Record_t record;

        if (record_index >= display->status.history_count)
        {
            break;
        }
        if (!WarningHistory_GetRecord(display->history,
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

static bool history_draw_detail(const WarningHistoryDisplay_t *display,
                                const MonoGraphics_Canvas_t *canvas,
                                uint16_t selected_index)
{
    WarningHistory_Record_t record;
    char line[HISTORY_DISPLAY_LINE_CAPACITY];
    uint8_t position;
    uint16_t y = 11U;

    if (!WarningHistory_GetRecord(display->history,
                                  selected_index,
                                  &record))
    {
        return false;
    }

    position = history_append_text(line, 0U, "TIME: ");
    (void)history_append_time(line, position, record.tick_ms);
    if (!history_draw_detail_line(canvas, &y, line)) return false;

    position = history_append_text(line, 0U, "EVENT: ");
    (void)history_append_text(line, position, history_event_text(record.event));
    if (!history_draw_detail_line(canvas, &y, line)) return false;
    (void)history_append_text(line, 0U, "SOURCE AFTER:");
    if (!history_draw_detail_line(canvas, &y, line)) return false;
    (void)history_append_text(line, 0U, history_source_text(record.source_mask));
    if (!history_draw_detail_line(canvas, &y, line)) return false;

    position = history_append_text(line, 0U, "SESSION: ");
    (void)history_append_uint(line, position, record.session_id, 1U);
    if (!history_draw_detail_line(canvas, &y, line)) return false;
    return true;
}

bool WarningHistoryDisplay_Initialize(WarningHistoryDisplay_t *display,
                                     const WarningHistory_t *history)
{
    if ((display == NULL) || (history == NULL))
    {
        return false;
    }
    *display = (WarningHistoryDisplay_t){0};
    display->history = history;
    display->status.is_initialized = true;
    display->status.redraw_is_pending = true;
    return true;
}

/** @brief Đồng bộ ring buffer, giữ record đang xem khi có nhiều event mới. */
void WarningHistoryDisplay_Update(WarningHistoryDisplay_t *display,
                                  bool monitoring_is_active)
{
    WarningHistory_Status_t history;
    if ((display == NULL) || !display->status.is_initialized) return;
    WarningHistory_GetStatus(display->history, &history);
    bool changed = (history.total_event_count != display->last_total_event_count) ||
                   (history.count != display->status.history_count) ||
                   (history.write_index != display->last_write_index);
    if ((history.count == 0U) ||
        (history.total_event_count < display->last_total_event_count))
        display->status.selected_age_from_newest = 0U;
    else if (changed && display->status.selected_age_from_newest > 0U) {
        uint32_t added = history.total_event_count - display->last_total_event_count;
        /* Khi bộ đếm bão hòa, vẫn nhận biết một lần ghi qua write_index. */
        if (added == 0U && history.write_index != display->last_write_index) added = 1U;
        uint32_t room = (uint32_t)(history.count - 1U) -
            (display->status.selected_age_from_newest < history.count ?
             display->status.selected_age_from_newest : history.count - 1U);
        display->status.selected_age_from_newest =
            added > room ? history.count - 1U :
            (uint16_t)(display->status.selected_age_from_newest + added);
    }
    if (history.count > 0U && display->status.selected_age_from_newest >= history.count)
        display->status.selected_age_from_newest = history.count - 1U;
    display->status.redraw_is_pending |= changed ||
        (monitoring_is_active != display->status.monitoring_is_active);
    display->status.monitoring_is_active = monitoring_is_active;
    display->status.history_count = history.count;
    display->status.has_newer_record = display->status.selected_age_from_newest > 0U;
    display->last_total_event_count = history.total_event_count;
    display->last_write_index = history.write_index;
}

void WarningHistoryDisplay_EnterPage(WarningHistoryDisplay_t *display)
{
    if ((display != NULL) && display->status.is_initialized)
    {
        display->status.view = WARNING_HISTORY_DISPLAY_LIST;
        display->status.selected_age_from_newest = 0U;
        display->status.has_newer_record = false;
        display->status.redraw_is_pending = true;
    }
}

bool WarningHistoryDisplay_HandleInput(WarningHistoryDisplay_t *display,
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
            display->status.view == WARNING_HISTORY_DISPLAY_LIST
                ? WARNING_HISTORY_DISPLAY_DETAIL
                : WARNING_HISTORY_DISPLAY_LIST;
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

WarningHistoryDisplay_RenderResult_t
WarningHistoryDisplay_RenderIfDue(
    WarningHistoryDisplay_t *display,
    const MonoGraphics_Canvas_t *canvas,
    WarningHistoryDisplay_ClearCanvasFunction_t clear_canvas,
    uint32_t current_tick_ms)
{
    uint16_t selected_index = 0U;
    bool draw_succeeded;

    if ((display == NULL) || !display->status.is_initialized ||
        !display->status.redraw_is_pending)
    {
        return WARNING_HISTORY_DISPLAY_RENDER_IDLE;
    }
    if (display->status.has_rendered_frame &&
        ((uint32_t)(current_tick_ms - display->status.last_render_tick_ms) <
         WARNING_HISTORY_DISPLAY_MIN_REFRESH_INTERVAL_MS))
    {
        display->status.last_render_result =
            WARNING_HISTORY_DISPLAY_RENDER_WAIT_INTERVAL;
        return display->status.last_render_result;
    }
    if ((canvas == NULL) || (clear_canvas == NULL) ||
        (canvas->width < HISTORY_DISPLAY_WIDTH_PIXELS) ||
        (canvas->height < HISTORY_DISPLAY_HEIGHT_PIXELS) ||
        !clear_canvas(canvas->context))
    {
        display->status.last_render_result =
            WARNING_HISTORY_DISPLAY_RENDER_FAILED;
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
        draw_succeeded = display->status.view == WARNING_HISTORY_DISPLAY_DETAIL &&
                         display->status.history_count > 0U
            ? history_draw_detail(display, canvas, selected_index)
            : history_draw_list(display, canvas, selected_index);
    }
    if (!draw_succeeded)
    {
        display->status.last_render_result =
            WARNING_HISTORY_DISPLAY_RENDER_FAILED;
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
        WARNING_HISTORY_DISPLAY_RENDER_DRAWN;
    return display->status.last_render_result;
}

void WarningHistoryDisplay_RequestRedraw(
    WarningHistoryDisplay_t *display)
{
    if ((display != NULL) && display->status.is_initialized)
    {
        display->status.redraw_is_pending = true;
    }
}

void WarningHistoryDisplay_GetStatus(
    const WarningHistoryDisplay_t *display,
    WarningHistoryDisplay_Status_t *output_status)
{
    if ((display != NULL) && (output_status != NULL))
    {
        *output_status = display->status;
    }
}
