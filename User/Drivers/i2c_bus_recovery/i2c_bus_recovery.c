/** @file i2c_bus_recovery.c @brief Hiện thực bus-clear I2C bằng GPIO open-drain. */
#include "i2c_bus_recovery.h"

#include <stddef.h>

#define I2C_BUS_RECOVERY_STEP_TIME_MS (1UL)
#define I2C_BUS_RECOVERY_MAX_CLOCK_PULSES (9U)

static bool recovery_step_elapsed(const I2cBusRecovery_t *recovery,
                                  uint32_t current_tick_ms)
{
    return ((uint32_t)(current_tick_ms - recovery->state_start_tick_ms) >=
            I2C_BUS_RECOVERY_STEP_TIME_MS);
}

static void recovery_enter(I2cBusRecovery_t *recovery,
                           I2cBusRecovery_State_t state,
                           uint32_t tick_ms)
{
    recovery->state = state;
    recovery->state_start_tick_ms = tick_ms;
}

static bool recovery_lines_are_high(const I2cBusRecovery_t *recovery)
{
    return (HAL_GPIO_ReadPin(recovery->config.scl_port,
                             recovery->config.scl_pin) == GPIO_PIN_SET) &&
           (HAL_GPIO_ReadPin(recovery->config.sda_port,
                             recovery->config.sda_pin) == GPIO_PIN_SET);
}

bool I2cBusRecovery_Initialize(I2cBusRecovery_t *recovery,
                              const I2cBusRecovery_Config_t *config)
{
    if ((recovery == NULL) || (config == NULL) ||
        (config->hal_i2c == NULL) || (config->scl_port == NULL) ||
        (config->sda_port == NULL) || (config->scl_pin == 0U) ||
        (config->sda_pin == 0U))
    {
        return false;
    }
    *recovery = (I2cBusRecovery_t){0};
    recovery->config = *config;
    recovery->state = I2C_BUS_RECOVERY_IDLE;
    return true;
}

bool I2cBusRecovery_Start(I2cBusRecovery_t *recovery,
                          uint32_t current_tick_ms)
{
    GPIO_InitTypeDef gpio = {0};

    if ((recovery == NULL) || I2cBusRecovery_IsRunning(recovery) ||
        (HAL_I2C_DeInit(recovery->config.hal_i2c) != HAL_OK))
    {
        return false;
    }

    HAL_GPIO_WritePin(recovery->config.scl_port, recovery->config.scl_pin,
                      GPIO_PIN_SET);
    HAL_GPIO_WritePin(recovery->config.sda_port, recovery->config.sda_pin,
                      GPIO_PIN_SET);
    gpio.Pin = recovery->config.scl_pin;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(recovery->config.scl_port, &gpio);
    gpio.Pin = recovery->config.sda_pin;
    HAL_GPIO_Init(recovery->config.sda_port, &gpio);

    recovery->clock_pulse_count = 0U;
    recovery->lines_were_released = false;
    recovery_enter(recovery, I2C_BUS_RECOVERY_RELEASING_LINES,
                   current_tick_ms);
    return true;
}

void I2cBusRecovery_Service(I2cBusRecovery_t *recovery,
                            uint32_t current_tick_ms)
{
    if ((recovery == NULL) || !I2cBusRecovery_IsRunning(recovery) ||
        !recovery_step_elapsed(recovery, current_tick_ms))
    {
        return;
    }

    switch (recovery->state)
    {
        case I2C_BUS_RECOVERY_RELEASING_LINES:
            if (recovery_lines_are_high(recovery))
            {
                recovery->lines_were_released = true;
                HAL_GPIO_WritePin(recovery->config.scl_port,
                                  recovery->config.scl_pin, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(recovery->config.sda_port,
                                  recovery->config.sda_pin, GPIO_PIN_RESET);
                recovery_enter(recovery, I2C_BUS_RECOVERY_STOP_LOW,
                               current_tick_ms);
            }
            else
            {
                HAL_GPIO_WritePin(recovery->config.scl_port,
                                  recovery->config.scl_pin, GPIO_PIN_RESET);
                recovery_enter(recovery, I2C_BUS_RECOVERY_CLOCK_LOW,
                               current_tick_ms);
            }
            break;

        case I2C_BUS_RECOVERY_CLOCK_LOW:
            HAL_GPIO_WritePin(recovery->config.scl_port,
                              recovery->config.scl_pin, GPIO_PIN_SET);
            recovery_enter(recovery, I2C_BUS_RECOVERY_CLOCK_HIGH,
                           current_tick_ms);
            break;

        case I2C_BUS_RECOVERY_CLOCK_HIGH:
            recovery->clock_pulse_count++;
            if ((HAL_GPIO_ReadPin(recovery->config.sda_port,
                                  recovery->config.sda_pin) == GPIO_PIN_SET) ||
                (recovery->clock_pulse_count >=
                 I2C_BUS_RECOVERY_MAX_CLOCK_PULSES))
            {
                HAL_GPIO_WritePin(recovery->config.scl_port,
                                  recovery->config.scl_pin, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(recovery->config.sda_port,
                                  recovery->config.sda_pin, GPIO_PIN_RESET);
                recovery_enter(recovery, I2C_BUS_RECOVERY_STOP_LOW,
                               current_tick_ms);
            }
            else
            {
                HAL_GPIO_WritePin(recovery->config.scl_port,
                                  recovery->config.scl_pin, GPIO_PIN_RESET);
                recovery_enter(recovery, I2C_BUS_RECOVERY_CLOCK_LOW,
                               current_tick_ms);
            }
            break;

        case I2C_BUS_RECOVERY_STOP_LOW:
            HAL_GPIO_WritePin(recovery->config.scl_port,
                              recovery->config.scl_pin, GPIO_PIN_SET);
            recovery_enter(recovery, I2C_BUS_RECOVERY_STOP_CLOCK_HIGH,
                           current_tick_ms);
            break;

        case I2C_BUS_RECOVERY_STOP_CLOCK_HIGH:
            HAL_GPIO_WritePin(recovery->config.sda_port,
                              recovery->config.sda_pin, GPIO_PIN_SET);
            recovery_enter(recovery, I2C_BUS_RECOVERY_STOP_DATA_HIGH,
                           current_tick_ms);
            break;

        case I2C_BUS_RECOVERY_STOP_DATA_HIGH:
            recovery->lines_were_released = recovery_lines_are_high(recovery);
            if (HAL_I2C_Init(recovery->config.hal_i2c) != HAL_OK)
            {
                recovery->state = I2C_BUS_RECOVERY_FAILED;
            }
            else
            {
                recovery->state = recovery->lines_were_released
                    ? I2C_BUS_RECOVERY_SUCCEEDED
                    : I2C_BUS_RECOVERY_FAILED;
            }
            break;

        default:
            break;
    }
}

bool I2cBusRecovery_IsRunning(const I2cBusRecovery_t *recovery)
{
    return (recovery != NULL) &&
           (recovery->state >= I2C_BUS_RECOVERY_RELEASING_LINES) &&
           (recovery->state <= I2C_BUS_RECOVERY_STOP_DATA_HIGH);
}
