/**
 * @file gpio_output.h
 * @brief Driver tổng quát điều khiển một GPIO output theo mức active đã cấu hình.
 */

#ifndef USER_DRIVERS_GPIO_OUTPUT_GPIO_OUTPUT_H_
#define USER_DRIVERS_GPIO_OUTPUT_GPIO_OUTPUT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "stm32f1xx_hal.h"

/** @brief Cấu hình phần cứng của một GPIO output. */
typedef struct
{
    GPIO_TypeDef *port;             /**< GPIO port đã được CubeMX khởi tạo làm output. */
    uint16_t pin;                   /**< Một GPIO pin duy nhất. */
    GPIO_PinState active_level;     /**< Mức logic tương ứng với trạng thái active. */
} GpioOutput_Config_t;

/**
 * @brief Context của một GPIO output.
 *
 * Caller sở hữu vùng nhớ này và không nên sửa trực tiếp các trường sau khi khởi tạo.
 */
typedef struct
{
    GpioOutput_Config_t config; /**< Bản sao cấu hình dùng trong suốt vòng đời instance. */
    bool is_initialized;        /**< true sau khi cấu hình hợp lệ được chấp nhận. */
    bool is_active;             /**< Trạng thái logic gần nhất driver đã ghi. */
} GpioOutput_t;

/**
 * @brief Khởi tạo instance và đưa output về trạng thái inactive.
 * @param output Instance do caller cấp phát tĩnh.
 * @param config Cấu hình port, pin và active level.
 * @return true nếu cấu hình hợp lệ; false nếu có con trỏ NULL hoặc pin không hợp lệ.
 */
bool GpioOutput_Initialize(GpioOutput_t *output,
                           const GpioOutput_Config_t *config);

/**
 * @brief Đặt output sang trạng thái active hoặc inactive.
 * @param output Instance đã khởi tạo.
 * @param active true để active, false để inactive.
 */
void GpioOutput_SetActive(GpioOutput_t *output, bool active);

/**
 * @brief Kiểm tra trạng thái logic gần nhất driver đã ghi.
 * @param output Instance cần đọc.
 * @return true khi output đang active; false khi inactive hoặc instance không hợp lệ.
 */
bool GpioOutput_IsActive(const GpioOutput_t *output);

#ifdef __cplusplus
}
#endif

#endif /* USER_DRIVERS_GPIO_OUTPUT_GPIO_OUTPUT_H_ */
