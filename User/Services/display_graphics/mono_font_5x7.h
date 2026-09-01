/**
 * @file mono_font_5x7.h
 * @brief Font ASCII printable fixed-width 5x7 dùng với mono_graphics.
 */

#ifndef USER_SERVICES_DISPLAY_GRAPHICS_MONO_FONT_5X7_H_
#define USER_SERVICES_DISPLAY_GRAPHICS_MONO_FONT_5X7_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "mono_graphics.h"

/** @brief Font ASCII từ ký tự space (32) đến tilde (126), cell 6x8 pixel. */
extern const MonoGraphics_Font_t g_mono_font_5x7;

#ifdef __cplusplus
}
#endif

#endif /* USER_SERVICES_DISPLAY_GRAPHICS_MONO_FONT_5X7_H_ */
