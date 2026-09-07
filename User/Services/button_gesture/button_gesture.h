/**
 * @file button_gesture.h
 * @brief Phân biệt nhấn ngắn và nhấn giữ từ trạng thái một nút đã debounce cạnh nhấn.
 */

#ifndef USER_SERVICES_BUTTON_GESTURE_BUTTON_GESTURE_H_
#define USER_SERVICES_BUTTON_GESTURE_BUTTON_GESTURE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/** @brief Cấu hình thời gian cho bộ nhận dạng gesture. */
typedef struct
{
    uint32_t long_press_time_ms; /**< Thời gian giữ tối thiểu để tạo long press. */
    uint32_t release_debounce_time_ms; /**< Thời gian nhả ổn định trước khi kết thúc gesture. */
} ButtonGesture_Config_t;

/** @brief Trạng thái nội bộ của một gesture đang diễn ra. */
typedef enum
{
    BUTTON_GESTURE_IDLE = 0,
    BUTTON_GESTURE_PRESSED,
    BUTTON_GESTURE_RELEASE_DEBOUNCE,
    BUTTON_GESTURE_WAIT_LONG_RELEASE,
    BUTTON_GESTURE_LONG_RELEASE_DEBOUNCE
} ButtonGesture_State_t;

/** @brief Context tĩnh và các event one-shot của bộ nhận dạng. */
typedef struct
{
    ButtonGesture_Config_t config;
    ButtonGesture_State_t state;
    uint32_t press_start_tick_ms;
    uint32_t release_start_tick_ms;
    bool short_press_pending;
    bool long_press_pending;
    bool is_initialized;
} ButtonGesture_t;

/** @brief Khởi tạo bộ nhận dạng gesture. */
bool ButtonGesture_Initialize(ButtonGesture_t *gesture,
                              const ButtonGesture_Config_t *config);

/**
 * @brief Tiến state machine bằng event cạnh nhấn đã debounce và mức nút tức thời.
 * @note Nhấn ngắn chỉ được phát sau khi nút đã nhả ổn định.
 */
void ButtonGesture_Service(ButtonGesture_t *gesture,
                           uint32_t current_tick_ms,
                           bool pressed_event,
                           bool button_is_pressed);

/** @brief Lấy và xóa event nhấn ngắn. */
bool ButtonGesture_TakeShortPress(ButtonGesture_t *gesture);

/** @brief Lấy và xóa event nhấn giữ. */
bool ButtonGesture_TakeLongPress(ButtonGesture_t *gesture);

#ifdef __cplusplus
}
#endif

#endif
