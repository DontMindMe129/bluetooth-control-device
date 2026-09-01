/**
 * @file output_display.c
 * @brief Hiện thực giao diện điều khiển năm ngõ ra trên OLED 128x64.
 */

#include "output_display.h"

#include <limits.h>
#include <stddef.h>

#include "mono_font_5x7.h"

#define OUTPUT_DISPLAY_WIDTH_PIXELS  (128U)
#define OUTPUT_DISPLAY_HEIGHT_PIXELS (64U)
#define OUTPUT_DISPLAY_HEADER_HEIGHT (9U)
#define OUTPUT_DISPLAY_LINE_CAPACITY (13U)

/** @brief Vẽ thanh tiêu đề đảo màu của trang. */
static bool output_display_draw_header(const MonoGraphics_Canvas_t *canvas)
{
    return MonoGraphics_FillRectangle(canvas,
                                      0U,
                                      0U,
                                      OUTPUT_DISPLAY_WIDTH_PIXELS,
                                      OUTPUT_DISPLAY_HEADER_HEIGHT,
                                      MONO_GRAPHICS_PIXEL_ON) &&
           MonoGraphics_DrawText(canvas,
                                 1U,
                                 1U,
                                 "OUTPUTS",
                                 &g_mono_font_5x7,
                                 MONO_GRAPHICS_PIXEL_OFF,
                                 MONO_GRAPHICS_PIXEL_ON,
                                 false);
}

/** @brief Tạo dòng dạng "> OUT1  ON" mà không dùng printf. */
static void output_display_build_line(char *line,
                                      uint8_t output_index,
                                      bool selected,
                                      bool is_on)
{
    line[0] = selected ? '>' : ' ';
    line[1] = ' ';
    line[2] = 'O';
    line[3] = 'U';
    line[4] = 'T';
    line[5] = (char)('1' + output_index);
    line[6] = ' ';
    line[7] = ' ';
    line[8] = 'O';
    line[9] = is_on ? 'N' : 'F';
    line[10] = is_on ? '\0' : 'F';
    line[11] = '\0';
}

/** @brief Vẽ toàn bộ frame từ snapshot output hiện tại. */
static bool output_display_draw_frame(
    const OutputDisplay_t *display,
    const MonoGraphics_Canvas_t *canvas,
    OutputDisplay_ClearCanvasFunction_t clear_canvas)
{
    char line[OUTPUT_DISPLAY_LINE_CAPACITY];
    uint8_t output_index;

    if ((canvas == NULL) ||
        (clear_canvas == NULL) ||
        (canvas->width < OUTPUT_DISPLAY_WIDTH_PIXELS) ||
        (canvas->height < OUTPUT_DISPLAY_HEIGHT_PIXELS) ||
        !clear_canvas(canvas->context) ||
        !output_display_draw_header(canvas))
    {
        return false;
    }

    for (output_index = 0U;
         output_index < OUTPUT_CONTROL_COUNT;
         output_index++)
    {
        output_display_build_line(
            line,
            output_index,
            display->desired_outputs.selected_output == output_index,
            (display->desired_outputs.output_on_mask &
             (uint8_t)(1U << output_index)) != 0U);

        if (!MonoGraphics_DrawText(canvas,
                                   1U,
                                   (uint16_t)(12U + (output_index * 10U)),
                                   line,
                                   &g_mono_font_5x7,
                                   MONO_GRAPHICS_PIXEL_ON,
                                   MONO_GRAPHICS_PIXEL_OFF,
                                   false))
        {
            return false;
        }
    }

    return true;
}

bool OutputDisplay_Initialize(OutputDisplay_t *display)
{
    if (display == NULL)
    {
        return false;
    }

    *display = (OutputDisplay_t){0};
    display->status.is_initialized = true;
    display->status.redraw_is_pending = true;
    display->status.last_render_result = OUTPUT_DISPLAY_RENDER_IDLE;
    return true;
}

void OutputDisplay_Update(OutputDisplay_t *display,
                          const OutputControl_Status_t *output_status)
{
    bool content_changed;

    if ((display == NULL) ||
        !display->status.is_initialized ||
        (output_status == NULL))
    {
        return;
    }

    content_changed =
        !display->status.has_desired_content ||
        (display->desired_outputs.selected_output !=
         output_status->selected_output) ||
        (display->desired_outputs.output_on_mask !=
         output_status->output_on_mask);

    display->desired_outputs = *output_status;
    display->status.has_desired_content = true;
    if (content_changed)
    {
        display->status.redraw_is_pending = true;
    }
}

OutputDisplay_RenderResult_t OutputDisplay_RenderIfDue(
    OutputDisplay_t *display,
    const MonoGraphics_Canvas_t *canvas,
    OutputDisplay_ClearCanvasFunction_t clear_canvas,
    uint32_t current_tick_ms)
{
    if ((display == NULL) ||
        !display->status.is_initialized ||
        !display->status.has_desired_content)
    {
        if (display != NULL)
        {
            display->status.last_render_result = OUTPUT_DISPLAY_RENDER_IDLE;
        }
        return OUTPUT_DISPLAY_RENDER_IDLE;
    }

    if (!display->status.redraw_is_pending)
    {
        display->status.last_render_result = OUTPUT_DISPLAY_RENDER_IDLE;
        return OUTPUT_DISPLAY_RENDER_IDLE;
    }

    if (display->status.has_rendered_frame &&
        ((uint32_t)(current_tick_ms - display->status.last_render_tick_ms) <
         OUTPUT_DISPLAY_MIN_REFRESH_INTERVAL_MS))
    {
        display->status.last_render_result =
            OUTPUT_DISPLAY_RENDER_WAIT_INTERVAL;
        return OUTPUT_DISPLAY_RENDER_WAIT_INTERVAL;
    }

    if (!output_display_draw_frame(display, canvas, clear_canvas))
    {
        display->status.last_render_result = OUTPUT_DISPLAY_RENDER_FAILED;
        return OUTPUT_DISPLAY_RENDER_FAILED;
    }

    display->status.redraw_is_pending = false;
    display->status.has_rendered_frame = true;
    display->status.last_render_tick_ms = current_tick_ms;
    if (display->status.rendered_frame_count < UINT32_MAX)
    {
        display->status.rendered_frame_count++;
    }
    display->status.last_render_result = OUTPUT_DISPLAY_RENDER_DRAWN;
    return OUTPUT_DISPLAY_RENDER_DRAWN;
}

void OutputDisplay_RequestRedraw(OutputDisplay_t *display)
{
    if ((display != NULL) && display->status.is_initialized)
    {
        display->status.redraw_is_pending = true;
    }
}

void OutputDisplay_GetStatus(const OutputDisplay_t *display,
                             OutputDisplay_Status_t *output_status)
{
    if ((display != NULL) && (output_status != NULL))
    {
        *output_status = display->status;
    }
}
