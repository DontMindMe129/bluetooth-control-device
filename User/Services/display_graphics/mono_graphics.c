/**
 * @file mono_graphics.c
 * @brief Hiện thực thuật toán line, rectangle, bitmap và text cho canvas 1-bit.
 */

#include "mono_graphics.h"

#include <stddef.h>

/** @brief Kiểm tra operation pixel công khai hợp lệ. */
static bool mono_graphics_operation_is_valid(
    MonoGraphics_PixelOperation_t operation)
{
    return ((operation == MONO_GRAPHICS_PIXEL_OFF) ||
            (operation == MONO_GRAPHICS_PIXEL_ON) ||
            (operation == MONO_GRAPHICS_PIXEL_TOGGLE));
}

/** @brief Kiểm tra canvas đã có đủ kích thước, context và callback. */
static bool mono_graphics_canvas_is_valid(
    const MonoGraphics_Canvas_t *canvas)
{
    return ((canvas != NULL) &&
            (canvas->width > 0U) &&
            (canvas->height > 0U) &&
            (canvas->context != NULL) &&
            (canvas->set_pixel != NULL));
}

/** @brief Kiểm tra hình chữ nhật khác rỗng và nằm hoàn toàn trong canvas. */
static bool mono_graphics_rectangle_is_inside_canvas(
    const MonoGraphics_Canvas_t *canvas,
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height)
{
    if (!mono_graphics_canvas_is_valid(canvas) ||
        (width == 0U) ||
        (height == 0U) ||
        (x >= canvas->width) ||
        (y >= canvas->height))
    {
        return false;
    }

    return ((width <= (uint16_t)(canvas->width - x)) &&
            (height <= (uint16_t)(canvas->height - y)));
}

/** @brief Kiểm tra mô tả font nhất quán và glyph vừa trong một byte theo chiều cao. */
static bool mono_graphics_font_is_valid(const MonoGraphics_Font_t *font)
{
    return ((font != NULL) &&
            (font->glyph_data != NULL) &&
            (font->first_character <= font->last_character) &&
            (font->glyph_width > 0U) &&
            (font->glyph_height > 0U) &&
            (font->glyph_height <= 8U) &&
            (font->horizontal_advance >= font->glyph_width) &&
            (font->line_advance >= font->glyph_height));
}

bool MonoGraphics_InitializeCanvas(MonoGraphics_Canvas_t *canvas,
                                   uint16_t width,
                                   uint16_t height,
                                   void *context,
                                   MonoGraphics_SetPixelFunction_t set_pixel)
{
    if ((canvas == NULL) ||
        (width == 0U) ||
        (height == 0U) ||
        (context == NULL) ||
        (set_pixel == NULL))
    {
        return false;
    }

    canvas->width = width;
    canvas->height = height;
    canvas->context = context;
    canvas->set_pixel = set_pixel;
    return true;
}

bool MonoGraphics_DrawPixel(const MonoGraphics_Canvas_t *canvas,
                            uint16_t x,
                            uint16_t y,
                            MonoGraphics_PixelOperation_t operation)
{
    if (!mono_graphics_canvas_is_valid(canvas) ||
        !mono_graphics_operation_is_valid(operation) ||
        (x >= canvas->width) ||
        (y >= canvas->height))
    {
        return false;
    }

    return canvas->set_pixel(canvas->context, x, y, operation);
}

bool MonoGraphics_DrawLine(const MonoGraphics_Canvas_t *canvas,
                           uint16_t x0,
                           uint16_t y0,
                           uint16_t x1,
                           uint16_t y1,
                           MonoGraphics_PixelOperation_t operation)
{
    int32_t current_x;
    int32_t current_y;
    const int32_t target_x = x1;
    const int32_t target_y = y1;
    const int32_t delta_x = (x1 >= x0)
                                ? (int32_t)(x1 - x0)
                                : (int32_t)(x0 - x1);
    const int32_t delta_y = (y1 >= y0)
                                ? (int32_t)(y1 - y0)
                                : (int32_t)(y0 - y1);
    const int32_t step_x = (x0 < x1) ? 1 : -1;
    const int32_t step_y = (y0 < y1) ? 1 : -1;
    int32_t error;

    if (!mono_graphics_canvas_is_valid(canvas) ||
        !mono_graphics_operation_is_valid(operation) ||
        (x0 >= canvas->width) ||
        (x1 >= canvas->width) ||
        (y0 >= canvas->height) ||
        (y1 >= canvas->height))
    {
        return false;
    }

    current_x = x0;
    current_y = y0;
    error = delta_x - delta_y;

    for (;;)
    {
        int32_t doubled_error;

        if (!MonoGraphics_DrawPixel(canvas,
                                    (uint16_t)current_x,
                                    (uint16_t)current_y,
                                    operation))
        {
            return false;
        }

        if ((current_x == target_x) && (current_y == target_y))
        {
            break;
        }

        doubled_error = error * 2;
        if (doubled_error > -delta_y)
        {
            error -= delta_y;
            current_x += step_x;
        }
        if (doubled_error < delta_x)
        {
            error += delta_x;
            current_y += step_y;
        }
    }

    return true;
}

bool MonoGraphics_DrawRectangle(const MonoGraphics_Canvas_t *canvas,
                                uint16_t x,
                                uint16_t y,
                                uint16_t width,
                                uint16_t height,
                                MonoGraphics_PixelOperation_t operation)
{
    uint16_t right;
    uint16_t bottom;

    if (!mono_graphics_rectangle_is_inside_canvas(canvas,
                                                   x,
                                                   y,
                                                   width,
                                                   height) ||
        !mono_graphics_operation_is_valid(operation))
    {
        return false;
    }

    right = (uint16_t)(x + width - 1U);
    bottom = (uint16_t)(y + height - 1U);

    if (!MonoGraphics_DrawLine(canvas, x, y, right, y, operation))
    {
        return false;
    }

    if ((height > 1U) &&
        !MonoGraphics_DrawLine(canvas, x, bottom, right, bottom, operation))
    {
        return false;
    }

    if ((height > 2U) &&
        !MonoGraphics_DrawLine(canvas,
                               x,
                               (uint16_t)(y + 1U),
                               x,
                               (uint16_t)(bottom - 1U),
                               operation))
    {
        return false;
    }

    if ((width > 1U) && (height > 2U) &&
        !MonoGraphics_DrawLine(canvas,
                               right,
                               (uint16_t)(y + 1U),
                               right,
                               (uint16_t)(bottom - 1U),
                               operation))
    {
        return false;
    }

    return true;
}

bool MonoGraphics_FillRectangle(const MonoGraphics_Canvas_t *canvas,
                                uint16_t x,
                                uint16_t y,
                                uint16_t width,
                                uint16_t height,
                                MonoGraphics_PixelOperation_t operation)
{
    uint16_t row;

    if (!mono_graphics_rectangle_is_inside_canvas(canvas,
                                                   x,
                                                   y,
                                                   width,
                                                   height) ||
        !mono_graphics_operation_is_valid(operation))
    {
        return false;
    }

    for (row = 0U; row < height; row++)
    {
        if (!MonoGraphics_DrawLine(canvas,
                                   x,
                                   (uint16_t)(y + row),
                                   (uint16_t)(x + width - 1U),
                                   (uint16_t)(y + row),
                                   operation))
        {
            return false;
        }
    }

    return true;
}

bool MonoGraphics_DrawBitmap(const MonoGraphics_Canvas_t *canvas,
                             uint16_t x,
                             uint16_t y,
                             uint16_t width,
                             uint16_t height,
                             const uint8_t *bitmap,
                             uint32_t bitmap_size,
                             MonoGraphics_PixelOperation_t foreground,
                             MonoGraphics_PixelOperation_t background,
                             bool opaque)
{
    uint16_t bitmap_x;
    uint16_t bitmap_y;
    uint32_t stride;
    uint32_t required_size;

    if (!mono_graphics_rectangle_is_inside_canvas(canvas,
                                                   x,
                                                   y,
                                                   width,
                                                   height) ||
        (bitmap == NULL) ||
        !mono_graphics_operation_is_valid(foreground) ||
        (opaque && !mono_graphics_operation_is_valid(background)))
    {
        return false;
    }

    stride = ((uint32_t)width + 7U) / 8U;
    required_size = stride * height;
    if (bitmap_size < required_size)
    {
        return false;
    }

    for (bitmap_y = 0U; bitmap_y < height; bitmap_y++)
    {
        for (bitmap_x = 0U; bitmap_x < width; bitmap_x++)
        {
            const uint32_t byte_index =
                ((uint32_t)bitmap_y * stride) + ((uint32_t)bitmap_x / 8U);
            const uint8_t bit_mask =
                (uint8_t)(0x80U >> ((uint32_t)bitmap_x % 8U));
            const bool bit_is_set = ((bitmap[byte_index] & bit_mask) != 0U);

            if (bit_is_set)
            {
                if (!MonoGraphics_DrawPixel(canvas,
                                            (uint16_t)(x + bitmap_x),
                                            (uint16_t)(y + bitmap_y),
                                            foreground))
                {
                    return false;
                }
            }
            else if (opaque &&
                     !MonoGraphics_DrawPixel(canvas,
                                             (uint16_t)(x + bitmap_x),
                                             (uint16_t)(y + bitmap_y),
                                             background))
            {
                return false;
            }
        }
    }

    return true;
}

bool MonoGraphics_DrawCharacter(const MonoGraphics_Canvas_t *canvas,
                                uint16_t x,
                                uint16_t y,
                                char character,
                                const MonoGraphics_Font_t *font,
                                MonoGraphics_PixelOperation_t foreground,
                                MonoGraphics_PixelOperation_t background,
                                bool opaque)
{
    uint8_t cell_x;
    uint8_t cell_y;
    uint8_t character_code;
    uint32_t glyph_offset;

    if (!mono_graphics_font_is_valid(font) ||
        !mono_graphics_rectangle_is_inside_canvas(canvas,
                                                   x,
                                                   y,
                                                   font->horizontal_advance,
                                                   font->line_advance) ||
        !mono_graphics_operation_is_valid(foreground) ||
        (opaque && !mono_graphics_operation_is_valid(background)))
    {
        return false;
    }

    character_code = (uint8_t)character;
    if ((character_code < font->first_character) ||
        (character_code > font->last_character))
    {
        return false;
    }

    glyph_offset =
        (uint32_t)(character_code - font->first_character) * font->glyph_width;

    for (cell_y = 0U; cell_y < font->line_advance; cell_y++)
    {
        for (cell_x = 0U; cell_x < font->horizontal_advance; cell_x++)
        {
            bool glyph_pixel_is_set = false;

            if ((cell_x < font->glyph_width) &&
                (cell_y < font->glyph_height))
            {
                const uint8_t column_bits =
                    font->glyph_data[glyph_offset + cell_x];
                glyph_pixel_is_set =
                    ((column_bits & (uint8_t)(1U << cell_y)) != 0U);
            }

            if (glyph_pixel_is_set)
            {
                if (!MonoGraphics_DrawPixel(canvas,
                                            (uint16_t)(x + cell_x),
                                            (uint16_t)(y + cell_y),
                                            foreground))
                {
                    return false;
                }
            }
            else if (opaque &&
                     !MonoGraphics_DrawPixel(canvas,
                                             (uint16_t)(x + cell_x),
                                             (uint16_t)(y + cell_y),
                                             background))
            {
                return false;
            }
        }
    }

    return true;
}

bool MonoGraphics_DrawText(const MonoGraphics_Canvas_t *canvas,
                           uint16_t x,
                           uint16_t y,
                           const char *text,
                           const MonoGraphics_Font_t *font,
                           MonoGraphics_PixelOperation_t foreground,
                           MonoGraphics_PixelOperation_t background,
                           bool opaque)
{
    uint16_t cursor_x = x;
    uint16_t cursor_y = y;

    if (!mono_graphics_canvas_is_valid(canvas) ||
        !mono_graphics_font_is_valid(font) ||
        (text == NULL) ||
        !mono_graphics_operation_is_valid(foreground) ||
        (opaque && !mono_graphics_operation_is_valid(background)))
    {
        return false;
    }

    while (*text != '\0')
    {
        if (*text == '\n')
        {
            cursor_x = x;
            if (font->line_advance > (uint16_t)(UINT16_MAX - cursor_y))
            {
                return false;
            }
            cursor_y = (uint16_t)(cursor_y + font->line_advance);
        }
        else
        {
            if (!MonoGraphics_DrawCharacter(canvas,
                                            cursor_x,
                                            cursor_y,
                                            *text,
                                            font,
                                            foreground,
                                            background,
                                            opaque))
            {
                return false;
            }

            if (font->horizontal_advance >
                (uint16_t)(UINT16_MAX - cursor_x))
            {
                return false;
            }
            cursor_x = (uint16_t)(cursor_x + font->horizontal_advance);
        }

        text++;
    }

    return true;
}
