/**
 * @file button_input.c
 * @brief Hiện thực driver nút nhấn EXTI với xử lý debounce không blocking.
 */

#include "button_input.h"

#include <stddef.h>

/** @brief Tạo critical section ngắn khi main lấy cờ do EXTI ISR ghi. */
static uint32_t button_input_enter_critical(void)
{
    uint32_t previous_primask = __get_PRIMASK();

    __disable_irq();
    return previous_primask;
}

/** @brief Khôi phục trạng thái ngắt trước critical section. */
static void button_input_exit_critical(uint32_t previous_primask)
{
    if ((previous_primask & 1UL) == 0UL)
    {
        __enable_irq();
    }
}

/** @brief Kiểm tra thời gian trôi qua an toàn khi HAL tick tràn. */
static bool button_input_has_elapsed(uint32_t now,
                                     uint32_t start,
                                     uint32_t duration)
{
    return ((uint32_t)(now - start) >= duration);
}

/** @brief Kiểm tra bitmask có chứa đúng một GPIO pin hay không. */
static bool button_input_is_single_pin(uint16_t pin)
{
    return ((pin != 0U) && ((pin & (uint16_t)(pin - 1U)) == 0U));
}

/** @brief Lấy và xóa cờ cạnh EXTI một cách nguyên tử trong main context. */
static bool button_input_take_edge(ButtonInput_t *button)
{
    uint32_t previous_primask = button_input_enter_critical();
    bool edge_pending = button->edge_pending;

    button->edge_pending = false;
    button_input_exit_critical(previous_primask);
    return edge_pending;
}

bool ButtonInput_Initialize(ButtonInput_t *button,
                            const ButtonInput_Config_t *config)
{
    if ((button == NULL) ||
        (config == NULL) ||
        (config->port == NULL) ||
        !button_input_is_single_pin(config->pin) ||
        ((config->pressed_level != GPIO_PIN_RESET) &&
         (config->pressed_level != GPIO_PIN_SET)))
    {
        return false;
    }

    *button = (ButtonInput_t){0};
    button->config = *config;
    button->is_initialized = true;
    return true;
}

void ButtonInput_HandleExtiInterrupt(ButtonInput_t *button, uint16_t gpio_pin)
{
    if ((button != NULL) &&
        button->is_initialized &&
        (gpio_pin == button->config.pin))
    {
        button->edge_pending = true;
    }
}

void ButtonInput_Service(ButtonInput_t *button, uint32_t current_tick_ms)
{
    if ((button == NULL) || !button->is_initialized)
    {
        return;
    }

    if (button_input_take_edge(button))
    {
        button->debounce_active = true;
        button->debounce_start_tick_ms = current_tick_ms;
    }

    if (button->debounce_active &&
        button_input_has_elapsed(current_tick_ms,
                                 button->debounce_start_tick_ms,
                                 button->config.debounce_time_ms))
    {
        button->debounce_active = false;

        if (HAL_GPIO_ReadPin(button->config.port, button->config.pin) ==
            button->config.pressed_level)
        {
            button->pressed_event_pending = true;
        }
    }
}

bool ButtonInput_TakePressedEvent(ButtonInput_t *button)
{
    bool pressed_event;

    if ((button == NULL) || !button->is_initialized)
    {
        return false;
    }

    pressed_event = button->pressed_event_pending;
    button->pressed_event_pending = false;
    return pressed_event;
}
