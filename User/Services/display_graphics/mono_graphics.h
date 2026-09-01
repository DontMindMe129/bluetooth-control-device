/**
 * @file mono_graphics.h
 * @brief Thuật toán đồ họa 1-bit tổng quát hoạt động qua một canvas pixel trừu tượng.
 *
 * Module không phụ thuộc SSD1306, HAL, I2C hay cách bố trí framebuffer. Backend chỉ
 * cần cung cấp kích thước canvas, context và callback ghi một pixel.
 */

#ifndef USER_SERVICES_DISPLAY_GRAPHICS_MONO_GRAPHICS_H_
#define USER_SERVICES_DISPLAY_GRAPHICS_MONO_GRAPHICS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/** @brief Phép toán mà backend áp dụng lên một pixel đơn sắc. */
typedef enum
{
    MONO_GRAPHICS_PIXEL_OFF = 0, /**< Xóa pixel. */
    MONO_GRAPHICS_PIXEL_ON,      /**< Bật pixel. */
    MONO_GRAPHICS_PIXEL_TOGGLE   /**< Đảo pixel hiện tại. */
} MonoGraphics_PixelOperation_t;

/**
 * @brief Kiểu callback ghi pixel mà một backend canvas phải cung cấp.
 * @param context Context riêng của backend, ví dụ Ssd1306_t.
 * @param x Tọa độ ngang đã được graphics kiểm tra nằm trong canvas.
 * @param y Tọa độ dọc đã được graphics kiểm tra nằm trong canvas.
 * @param operation OFF, ON hoặc TOGGLE.
 * @return true khi backend ghi được pixel; false nếu framebuffer đang khóa hoặc lỗi.
 */
typedef bool (*MonoGraphics_SetPixelFunction_t)(
    void *context,
    uint16_t x,
    uint16_t y,
    MonoGraphics_PixelOperation_t operation);

/** @brief Bề mặt vẽ trừu tượng mà mọi thuật toán trong module sử dụng. */
typedef struct
{
    uint16_t width;                       /**< Chiều rộng canvas tính bằng pixel. */
    uint16_t height;                      /**< Chiều cao canvas tính bằng pixel. */
    void *context;                        /**< Context được chuyển nguyên vẹn cho callback. */
    MonoGraphics_SetPixelFunction_t set_pixel; /**< Primitive ghi pixel của backend. */
} MonoGraphics_Canvas_t;

/**
 * @brief Font bitmap fixed-width có dữ liệu lưu theo từng cột.
 *
 * Mỗi glyph có glyph_width byte liên tiếp. Bit 0 của mỗi byte là pixel trên cùng;
 * implementation hiện hỗ trợ glyph_height tối đa 8 pixel.
 */
typedef struct
{
    const uint8_t *glyph_data; /**< Bảng glyph liên tiếp trong Flash. */
    uint8_t first_character;   /**< Mã ASCII đầu tiên có trong bảng. */
    uint8_t last_character;    /**< Mã ASCII cuối cùng có trong bảng. */
    uint8_t glyph_width;       /**< Số cột bitmap thực của một glyph. */
    uint8_t glyph_height;      /**< Số hàng bitmap thực của một glyph. */
    uint8_t horizontal_advance;/**< Khoảng tăng x sau mỗi ký tự, gồm khoảng cách. */
    uint8_t line_advance;      /**< Khoảng tăng y khi gặp ký tự xuống dòng. */
} MonoGraphics_Font_t;

/**
 * @brief Khởi tạo một canvas với backend pixel cụ thể.
 * @param canvas Context canvas do caller cấp phát.
 * @param width Chiều rộng khác 0.
 * @param height Chiều cao khác 0.
 * @param context Context backend khác NULL.
 * @param set_pixel Callback backend khác NULL.
 * @return true khi cấu hình hợp lệ; false nếu có tham số không hợp lệ.
 */
bool MonoGraphics_InitializeCanvas(MonoGraphics_Canvas_t *canvas,
                                   uint16_t width,
                                   uint16_t height,
                                   void *context,
                                   MonoGraphics_SetPixelFunction_t set_pixel);

/**
 * @brief Ghi một pixel thông qua backend của canvas.
 * @return true khi tọa độ, operation và backend hợp lệ; false nếu không ghi được.
 */
bool MonoGraphics_DrawPixel(const MonoGraphics_Canvas_t *canvas,
                            uint16_t x,
                            uint16_t y,
                            MonoGraphics_PixelOperation_t operation);

/**
 * @brief Vẽ đoạn thẳng bằng thuật toán Bresenham số nguyên.
 * @note Hai đầu mút phải cùng nằm trong canvas; hàm chưa thực hiện clipping.
 */
bool MonoGraphics_DrawLine(const MonoGraphics_Canvas_t *canvas,
                           uint16_t x0,
                           uint16_t y0,
                           uint16_t x1,
                           uint16_t y1,
                           MonoGraphics_PixelOperation_t operation);

/**
 * @brief Vẽ đường bao hình chữ nhật.
 * @param x Tọa độ cạnh trái.
 * @param y Tọa độ cạnh trên.
 * @param width Chiều rộng khác 0.
 * @param height Chiều cao khác 0.
 */
bool MonoGraphics_DrawRectangle(const MonoGraphics_Canvas_t *canvas,
                                uint16_t x,
                                uint16_t y,
                                uint16_t width,
                                uint16_t height,
                                MonoGraphics_PixelOperation_t operation);

/** @brief Tô kín một vùng chữ nhật nằm hoàn toàn trong canvas. */
bool MonoGraphics_FillRectangle(const MonoGraphics_Canvas_t *canvas,
                                uint16_t x,
                                uint16_t y,
                                uint16_t width,
                                uint16_t height,
                                MonoGraphics_PixelOperation_t operation);

/**
 * @brief Vẽ bitmap 1-bit row-major, bit cao trước trong từng byte.
 * @param bitmap Dữ liệu mỗi hàng có ceil(width / 8) byte.
 * @param bitmap_size Số byte thật sự của bitmap để kiểm tra biên đọc.
 * @param foreground Operation cho bit bằng 1.
 * @param background Operation cho bit bằng 0 khi opaque bằng true.
 * @param opaque true để vẽ cả bit 0; false để giữ nguyên nền.
 */
bool MonoGraphics_DrawBitmap(const MonoGraphics_Canvas_t *canvas,
                             uint16_t x,
                             uint16_t y,
                             uint16_t width,
                             uint16_t height,
                             const uint8_t *bitmap,
                             uint32_t bitmap_size,
                             MonoGraphics_PixelOperation_t foreground,
                             MonoGraphics_PixelOperation_t background,
                             bool opaque);

/**
 * @brief Vẽ một ký tự fixed-width bằng font cột 1-bit.
 * @param opaque true để ghi cả vùng nền horizontal_advance x line_advance.
 */
bool MonoGraphics_DrawCharacter(const MonoGraphics_Canvas_t *canvas,
                                uint16_t x,
                                uint16_t y,
                                char character,
                                const MonoGraphics_Font_t *font,
                                MonoGraphics_PixelOperation_t foreground,
                                MonoGraphics_PixelOperation_t background,
                                bool opaque);

/**
 * @brief Vẽ chuỗi kết thúc NULL; hỗ trợ ký tự '\n' để xuống dòng.
 * @note Hàm không tự wrap; toàn bộ cell ký tự phải nằm trong canvas.
 */
bool MonoGraphics_DrawText(const MonoGraphics_Canvas_t *canvas,
                           uint16_t x,
                           uint16_t y,
                           const char *text,
                           const MonoGraphics_Font_t *font,
                           MonoGraphics_PixelOperation_t foreground,
                           MonoGraphics_PixelOperation_t background,
                           bool opaque);

#ifdef __cplusplus
}
#endif

#endif /* USER_SERVICES_DISPLAY_GRAPHICS_MONO_GRAPHICS_H_ */
