/**
 * @file button_input.h
 * @brief Driver nút nhấn EXTI không blocking với debounce trong main context.
 */

#ifndef USER_DRIVERS_BUTTON_INPUT_BUTTON_INPUT_H_
#define USER_DRIVERS_BUTTON_INPUT_BUTTON_INPUT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "stm32f1xx_hal.h"

/** @brief Cấu hình phần cứng và debounce của một nút nhấn. */
typedef struct
{
    GPIO_TypeDef *port;               /**< GPIO port đã được CubeMX cấu hình làm EXTI input. */
    uint16_t pin;                     /**< Một GPIO pin duy nhất. */
    GPIO_PinState pressed_level;      /**< Mức GPIO xác nhận nút đang được nhấn. */
    uint32_t debounce_time_ms;        /**< Thời gian tín hiệu phải ổn định trước khi xác nhận. */
} ButtonInput_Config_t;

/**
 * @brief Context của một nút nhấn.
 *
 * ISR chỉ ghi edge_pending; các trạng thái còn lại thuộc main context.
 */
typedef struct
{
    ButtonInput_Config_t config;      /**< Bản sao cấu hình của instance. */
    volatile bool edge_pending;       /**< Cờ cạnh EXTI đang chờ main xử lý. */
    bool debounce_active;             /**< true trong cửa sổ debounce. */
    bool pressed_event_pending;       /**< Sự kiện nhấn đã xác nhận và chưa được lấy. */
    bool is_initialized;              /**< true sau khi khởi tạo thành công. */
    uint32_t debounce_start_tick_ms;  /**< Tick bắt đầu cửa sổ debounce hiện tại. */
} ButtonInput_t;

/**
 * @brief Khởi tạo một instance nút nhấn.
 * @param button Instance do caller cấp phát tĩnh.
 * @param config Cấu hình chân, mức nhấn và debounce.
 * @return true nếu cấu hình hợp lệ; false nếu đối số không hợp lệ.
 */
bool ButtonInput_Initialize(ButtonInput_t *button,
                            const ButtonInput_Config_t *config);

/**
 * @brief Tiếp nhận callback EXTI và đặt cờ cho main context.
 * @param button Instance cần nhận sự kiện.
 * @param gpio_pin Pin do HAL_GPIO_EXTI_Callback() cung cấp.
 * @note Hàm chạy trong interrupt context và có thời gian thực thi hằng số.
 */
void ButtonInput_HandleExtiInterrupt(ButtonInput_t *button, uint16_t gpio_pin);

/**
 * @brief Tiến state machine debounce trong main context.
 * @param button Instance cần xử lý.
 * @param current_tick_ms HAL tick hiện tại.
 */
void ButtonInput_Service(ButtonInput_t *button, uint32_t current_tick_ms);

/**
 * @brief Lấy và xóa một sự kiện nhấn đã qua debounce.
 * @param button Instance cần đọc.
 * @return true đúng một lần cho mỗi sự kiện nhấn đã xác nhận.
 */
bool ButtonInput_TakePressedEvent(ButtonInput_t *button);

#ifdef __cplusplus
}
#endif

#endif /* USER_DRIVERS_BUTTON_INPUT_BUTTON_INPUT_H_ */
