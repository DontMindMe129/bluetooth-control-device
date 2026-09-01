/**
 * @file gpio_output.c
 * @brief Hiện thực driver GPIO output có hỗ trợ active-high và active-low.
 */

#include "gpio_output.h"

#include <stddef.h>

/** @brief Kiểm tra bitmask có chứa đúng một GPIO pin hay không. */
static bool gpio_output_is_single_pin(uint16_t pin)
{
    return ((pin != 0U) && ((pin & (uint16_t)(pin - 1U)) == 0U));
}

bool GpioOutput_Initialize(GpioOutput_t *output,
                           const GpioOutput_Config_t *config)
{
    if ((output == NULL) ||
        (config == NULL) ||
        (config->port == NULL) ||
        !gpio_output_is_single_pin(config->pin) ||
        ((config->active_level != GPIO_PIN_RESET) &&
         (config->active_level != GPIO_PIN_SET)))
    {
        return false;
    }

    *output = (GpioOutput_t){0};
    output->config = *config;
    output->is_initialized = true;
    GpioOutput_SetActive(output, false);
    return true;
}

void GpioOutput_SetActive(GpioOutput_t *output, bool active)
{
    GPIO_PinState output_level;

    if ((output == NULL) || !output->is_initialized)
    {
        return;
    }

    if (active)
    {
        output_level = output->config.active_level;
    }
    else
    {
        output_level = (output->config.active_level == GPIO_PIN_SET)
                           ? GPIO_PIN_RESET
                           : GPIO_PIN_SET;
    }

    HAL_GPIO_WritePin(output->config.port, output->config.pin, output_level);
    output->is_active = active;
}

bool GpioOutput_IsActive(const GpioOutput_t *output)
{
    return ((output != NULL) && output->is_initialized && output->is_active);
}
