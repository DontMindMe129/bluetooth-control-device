/** @file button_gesture.c @brief Hiện thực state machine short/long press. */

#include "button_gesture.h"

#include <stddef.h>

static bool button_gesture_has_elapsed(uint32_t now,
                                       uint32_t start,
                                       uint32_t duration)
{
    return ((uint32_t)(now - start) >= duration);
}

bool ButtonGesture_Initialize(ButtonGesture_t *gesture,
                              const ButtonGesture_Config_t *config)
{
    if ((gesture == NULL) || (config == NULL) ||
        (config->long_press_time_ms == 0U))
    {
        return false;
    }

    *gesture = (ButtonGesture_t){0};
    gesture->config = *config;
    gesture->is_initialized = true;
    return true;
}

void ButtonGesture_Service(ButtonGesture_t *gesture,
                           uint32_t current_tick_ms,
                           bool pressed_event,
                           bool button_is_pressed)
{
    if ((gesture == NULL) || !gesture->is_initialized)
    {
        return;
    }

    switch (gesture->state)
    {
        case BUTTON_GESTURE_IDLE:
            if (pressed_event && button_is_pressed)
            {
                gesture->press_start_tick_ms = current_tick_ms;
                gesture->state = BUTTON_GESTURE_PRESSED;
            }
            break;

        case BUTTON_GESTURE_PRESSED:
            if (!button_is_pressed)
            {
                gesture->release_start_tick_ms = current_tick_ms;
                gesture->state = BUTTON_GESTURE_RELEASE_DEBOUNCE;
            }
            else if (button_gesture_has_elapsed(
                         current_tick_ms,
                         gesture->press_start_tick_ms,
                         gesture->config.long_press_time_ms))
            {
                gesture->long_press_pending = true;
                gesture->state = BUTTON_GESTURE_WAIT_LONG_RELEASE;
            }
            break;

        case BUTTON_GESTURE_RELEASE_DEBOUNCE:
            if (button_is_pressed)
            {
                gesture->state = BUTTON_GESTURE_PRESSED;
            }
            else if (button_gesture_has_elapsed(
                         current_tick_ms,
                         gesture->release_start_tick_ms,
                         gesture->config.release_debounce_time_ms))
            {
                gesture->short_press_pending = true;
                gesture->state = BUTTON_GESTURE_IDLE;
            }
            break;

        case BUTTON_GESTURE_WAIT_LONG_RELEASE:
            if (!button_is_pressed)
            {
                gesture->release_start_tick_ms = current_tick_ms;
                gesture->state = BUTTON_GESTURE_LONG_RELEASE_DEBOUNCE;
            }
            break;

        case BUTTON_GESTURE_LONG_RELEASE_DEBOUNCE:
            if (button_is_pressed)
            {
                gesture->state = BUTTON_GESTURE_WAIT_LONG_RELEASE;
            }
            else if (button_gesture_has_elapsed(
                         current_tick_ms,
                         gesture->release_start_tick_ms,
                         gesture->config.release_debounce_time_ms))
            {
                gesture->state = BUTTON_GESTURE_IDLE;
            }
            break;

        default:
            gesture->state = BUTTON_GESTURE_IDLE;
            break;
    }
}

bool ButtonGesture_TakeShortPress(ButtonGesture_t *gesture)
{
    bool pending;

    if ((gesture == NULL) || !gesture->is_initialized)
    {
        return false;
    }
    pending = gesture->short_press_pending;
    gesture->short_press_pending = false;
    return pending;
}

bool ButtonGesture_TakeLongPress(ButtonGesture_t *gesture)
{
    bool pending;

    if ((gesture == NULL) || !gesture->is_initialized)
    {
        return false;
    }
    pending = gesture->long_press_pending;
    gesture->long_press_pending = false;
    return pending;
}
