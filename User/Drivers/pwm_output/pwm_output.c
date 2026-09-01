/**
 * @file pwm_output.c
 * @brief Hiện thực driver PWM theo microsecond, không thay đổi timer base.
 */

#include "pwm_output.h"

#include <limits.h>
#include <stddef.h>

#define PWM_OUTPUT_CCMR_SELECTION_MASK (0x03UL)
#define PWM_OUTPUT_CCMR_MODE_MASK      (0x70UL)
#define PWM_OUTPUT_CCMR_PWM1_MODE      (0x60UL)

/** @brief Xác nhận channel thuộc tập channel output chuẩn của general timer. */
static bool pwm_output_channel_is_valid(uint32_t channel)
{
    return (channel == TIM_CHANNEL_1) ||
           (channel == TIM_CHANNEL_2) ||
           (channel == TIM_CHANNEL_3) ||
           (channel == TIM_CHANNEL_4);
}

/** @brief Đọc clock đầu vào timer, có xử lý hệ số nhân hai khi APB prescaler khác 1. */
static uint32_t pwm_output_get_timer_input_clock_hz(
    TIM_TypeDef *timer_instance)
{
    uint32_t timer_clock_hz;

    if (timer_instance == TIM1)
    {
        timer_clock_hz = HAL_RCC_GetPCLK2Freq();
        if ((RCC->CFGR & RCC_CFGR_PPRE2) != RCC_CFGR_PPRE2_DIV1)
        {
            timer_clock_hz *= 2UL;
        }
        return timer_clock_hz;
    }

    if ((timer_instance == TIM2) ||
        (timer_instance == TIM3)
#if defined(TIM4)
        || (timer_instance == TIM4)
#endif
       )
    {
        timer_clock_hz = HAL_RCC_GetPCLK1Freq();
        if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1)
        {
            timer_clock_hz *= 2UL;
        }
        return timer_clock_hz;
    }

    return 0UL;
}

/** @brief Đọc field CCxS/OCxM của channel về vị trí bit chuẩn của channel 1. */
static uint32_t pwm_output_get_normalized_ccmr(
    const TIM_TypeDef *timer_instance,
    uint32_t channel)
{
    if (channel == TIM_CHANNEL_1)
    {
        return timer_instance->CCMR1;
    }
    if (channel == TIM_CHANNEL_2)
    {
        return (timer_instance->CCMR1 >> 8U);
    }
    if (channel == TIM_CHANNEL_3)
    {
        return timer_instance->CCMR2;
    }

    return (timer_instance->CCMR2 >> 8U);
}

/** @brief Đọc bit polarity CCxP tương ứng với channel. */
static bool pwm_output_channel_is_active_high(
    const TIM_TypeDef *timer_instance,
    uint32_t channel)
{
    uint32_t polarity_mask;

    if (channel == TIM_CHANNEL_1)
    {
        polarity_mask = TIM_CCER_CC1P;
    }
    else if (channel == TIM_CHANNEL_2)
    {
        polarity_mask = TIM_CCER_CC2P;
    }
    else if (channel == TIM_CHANNEL_3)
    {
        polarity_mask = TIM_CCER_CC3P;
    }
    else
    {
        polarity_mask = TIM_CCER_CC4P;
    }

    return ((timer_instance->CCER & polarity_mask) == 0UL);
}

/** @brief Xác nhận channel đang ở output PWM mode 1 active-high. */
static bool pwm_output_channel_configuration_is_valid(
    const TIM_TypeDef *timer_instance,
    uint32_t channel)
{
    uint32_t normalized_ccmr =
        pwm_output_get_normalized_ccmr(timer_instance, channel);

    return
        ((normalized_ccmr & PWM_OUTPUT_CCMR_SELECTION_MASK) == 0UL) &&
        ((normalized_ccmr & PWM_OUTPUT_CCMR_MODE_MASK) ==
         PWM_OUTPUT_CCMR_PWM1_MODE) &&
        pwm_output_channel_is_active_high(timer_instance, channel);
}

/** @brief Tính và lưu các thông số thời gian từ clock/PSC/ARR hiện tại. */
static bool pwm_output_configure_timing(PwmOutput_t *output)
{
    uint32_t timer_clock_hz =
        pwm_output_get_timer_input_clock_hz(output->config.timer_instance);
    uint32_t prescaler_divider;
    uint32_t counter_clock_hz;

    if ((timer_clock_hz == 0UL) ||
        (output->config.timer->Init.Prescaler == UINT32_MAX) ||
        (output->config.timer->Init.Period == UINT32_MAX))
    {
        return false;
    }

    prescaler_divider = output->config.timer->Init.Prescaler + 1UL;
    if ((prescaler_divider == 0UL) ||
        ((timer_clock_hz % prescaler_divider) != 0UL))
    {
        return false;
    }

    counter_clock_hz = timer_clock_hz / prescaler_divider;
    if ((counter_clock_hz < PWM_OUTPUT_MICROSECOND_BASE_HZ) ||
        ((counter_clock_hz % PWM_OUTPUT_MICROSECOND_BASE_HZ) != 0UL))
    {
        return false;
    }

    output->status.counter_clock_hz = counter_clock_hz;
    output->status.ticks_per_microsecond =
        counter_clock_hz / PWM_OUTPUT_MICROSECOND_BASE_HZ;
    output->status.period_counts = output->config.timer->Init.Period + 1UL;
    output->status.period_us =
        output->status.period_counts /
        output->status.ticks_per_microsecond;

    return (output->status.period_us > 0UL);
}

PwmOutput_Result_t PwmOutput_Initialize(
    PwmOutput_t *output,
    const PwmOutput_Config_t *config)
{
    HAL_StatusTypeDef hal_status;

    if (output == NULL)
    {
        return PWM_OUTPUT_RESULT_INVALID_ARGUMENT;
    }

    *output = (PwmOutput_t){0};
    output->status.last_hal_status = HAL_OK;
    output->status.last_result = PWM_OUTPUT_RESULT_INVALID_ARGUMENT;

    if ((config == NULL) ||
        (config->timer == NULL) ||
        (config->timer_instance == NULL))
    {
        return output->status.last_result;
    }

    output->config = *config;
    if ((config->timer->Instance != config->timer_instance) ||
        !pwm_output_channel_is_valid(config->timer_channel) ||
        (config->timer->Init.CounterMode != TIM_COUNTERMODE_UP) ||
        !pwm_output_channel_configuration_is_valid(config->timer_instance,
                                                   config->timer_channel) ||
        !pwm_output_configure_timing(output))
    {
        output->status.last_result = PWM_OUTPUT_RESULT_INVALID_CONFIG;
        return output->status.last_result;
    }

    __HAL_TIM_SET_COMPARE(config->timer, config->timer_channel, 0UL);
    hal_status = HAL_TIM_PWM_Start(config->timer, config->timer_channel);
    output->status.last_hal_status = hal_status;
    if (hal_status != HAL_OK)
    {
        output->status.last_result = PWM_OUTPUT_RESULT_START_FAILED;
        return output->status.last_result;
    }

    output->status.is_initialized = true;
    output->status.is_running = true;
    output->status.pulse_width_us = 0UL;
    output->status.compare_counts = 0UL;
    output->status.last_result = PWM_OUTPUT_RESULT_OK;
    return output->status.last_result;
}

PwmOutput_Result_t PwmOutput_SetPulseWidthUs(
    PwmOutput_t *output,
    uint32_t pulse_width_us)
{
    uint32_t maximum_pulse_width_us;
    uint32_t compare_counts;

    if (output == NULL)
    {
        return PWM_OUTPUT_RESULT_INVALID_ARGUMENT;
    }

    if (!output->status.is_initialized || !output->status.is_running)
    {
        output->status.last_result = PWM_OUTPUT_RESULT_NOT_INITIALIZED;
        return output->status.last_result;
    }

    maximum_pulse_width_us =
        output->config.timer->Init.Period /
        output->status.ticks_per_microsecond;
    if (pulse_width_us > maximum_pulse_width_us)
    {
        output->status.last_result = PWM_OUTPUT_RESULT_PULSE_OUT_OF_RANGE;
        return output->status.last_result;
    }

    compare_counts = pulse_width_us * output->status.ticks_per_microsecond;
    __HAL_TIM_SET_COMPARE(output->config.timer,
                          output->config.timer_channel,
                          compare_counts);

    output->status.pulse_width_us = pulse_width_us;
    output->status.compare_counts = compare_counts;
    output->status.last_result = PWM_OUTPUT_RESULT_OK;
    return output->status.last_result;
}

void PwmOutput_GetStatus(const PwmOutput_t *output,
                         PwmOutput_Status_t *output_status)
{
    if ((output != NULL) && (output_status != NULL))
    {
        *output_status = output->status;
    }
}
