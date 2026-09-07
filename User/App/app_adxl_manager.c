/**
 * @file app_adxl_manager.c
 * @brief Policy tự phục hồi ADXL345 tách biệt với phục hồi shared I2C bus.
 */

#include "app_adxl_manager.h"

#include <stddef.h>

static bool app_adxl_time_has_elapsed(uint32_t current_tick_ms,
                                       uint32_t start_tick_ms,
                                       uint32_t duration_ms)
{
    return ((uint32_t)(current_tick_ms - start_tick_ms) >= duration_ms);
}

static void app_adxl_enter_state(AppAdxlManager_t *manager,
                                 AppAdxlManager_State_t state,
                                 uint32_t current_tick_ms)
{
    if (manager->status.state != state)
    {
        manager->status.state = state;
        manager->state_start_tick_ms = current_tick_ms;
    }
    manager->status.is_online = (state == APP_ADXL_MANAGER_ACTIVE);
}

static void app_adxl_increment_saturated_u8(uint8_t *value,
                                             uint32_t increment)
{
    if (increment >= (uint32_t)(UINT8_MAX - *value))
    {
        *value = UINT8_MAX;
    }
    else
    {
        *value = (uint8_t)(*value + (uint8_t)increment);
    }
}

static void app_adxl_request_bus_recovery(AppAdxlManager_t *manager,
                                           uint32_t current_tick_ms)
{
    manager->status.bus_recovery_request_pending = true;
    manager->status.last_restart_reason = APP_ADXL_MANAGER_RESTART_SHARED_BUS;
    app_adxl_enter_state(manager,
                         APP_ADXL_MANAGER_WAITING_BUS_RECOVERY,
                         current_tick_ms);
}

static bool app_adxl_start_driver(AppAdxlManager_t *manager,
                                  uint32_t current_tick_ms)
{
    const Adxl345_Config_t driver_config =
    {
        .i2c_bus = manager->config.i2c_bus,
        .address_7bit = manager->config.address_7bit,
        .data_ready_pin = manager->config.data_ready_pin,
        .transfer_timeout_ms = manager->config.transfer_timeout_ms,
        .initialize_retry_delay_ms =
            manager->config.driver_initialize_retry_delay_ms,
        .stale_timeout_ms = manager->config.driver_stale_timeout_ms,
        .initialize_max_attempts =
            manager->config.driver_initialize_max_attempts
    };

    manager->status.driver_initialize_result =
        Adxl345_Initialize(&manager->sensor,
                           &driver_config,
                           current_tick_ms);
    Adxl345_GetStatus(&manager->sensor, &manager->status.driver);
    manager->previous_successful_sample_count =
        manager->status.driver.successful_sample_count;
    manager->previous_failed_sample_count =
        manager->status.driver.failed_sample_count;
    manager->status.consecutive_read_error_count = 0U;
    manager->status.good_sample_count = 0U;
    manager->status.bus_recovery_request_pending = false;

    if (manager->status.driver_initialize_result !=
        ADXL345_INITIALIZE_ACCEPTED)
    {
        app_adxl_enter_state(manager,
                             APP_ADXL_MANAGER_ERROR,
                             current_tick_ms);
        return false;
    }
    app_adxl_enter_state(manager,
                         APP_ADXL_MANAGER_INITIALIZING,
                         current_tick_ms);
    return true;
}

static void app_adxl_schedule_restart(
    AppAdxlManager_t *manager,
    AppAdxlManager_RestartReason_t reason,
    uint32_t current_tick_ms)
{
    manager->status.last_restart_reason = reason;
    manager->status.good_sample_count = 0U;
    manager->status.consecutive_read_error_count = 0U;
    app_adxl_enter_state(manager,
                         APP_ADXL_MANAGER_RESTART_WAIT,
                         current_tick_ms);
}

bool AppAdxlManager_Initialize(AppAdxlManager_t *manager,
                               const AppAdxlManager_Config_t *config,
                               uint32_t current_tick_ms)
{
    I2cBus_Status_t bus_status = {0};

    if (manager == NULL)
    {
        return false;
    }
    *manager = (AppAdxlManager_t){0};
    manager->status.state = APP_ADXL_MANAGER_NOT_STARTED;
    manager->status.driver_initialize_result =
        ADXL345_INITIALIZE_INVALID_CONFIG;

    if ((config == NULL) || (config->i2c_bus == NULL) ||
        (config->transfer_timeout_ms == 0UL) ||
        (config->driver_initialize_retry_delay_ms == 0UL) ||
        (config->driver_stale_timeout_ms == 0UL) ||
        (config->driver_initialize_max_attempts == 0U) ||
        (config->persistent_stale_restart_ms <=
         config->driver_stale_timeout_ms) ||
        (config->device_restart_delay_ms == 0UL) ||
        (config->offline_probe_period_ms == 0UL) ||
        (config->consecutive_read_error_limit == 0U) ||
        (config->recovery_good_sample_count == 0U))
    {
        manager->status.state = APP_ADXL_MANAGER_ERROR;
        return false;
    }
    I2cBus_GetStatus(config->i2c_bus, &bus_status);
    if (!bus_status.is_initialized)
    {
        manager->status.state = APP_ADXL_MANAGER_ERROR;
        return false;
    }

    manager->config = *config;
    manager->status.is_initialized = true;
    return app_adxl_start_driver(manager, current_tick_ms);
}

void AppAdxlManager_Service(AppAdxlManager_t *manager,
                            uint32_t current_tick_ms)
{
    I2cBus_Status_t bus_status = {0};
    uint32_t successful_delta;
    uint32_t failed_delta;
    uint32_t freshness_start_tick_ms;

    if ((manager == NULL) || !manager->status.is_initialized)
    {
        return;
    }

    if (manager->status.state == APP_ADXL_MANAGER_RESTART_WAIT)
    {
        if (app_adxl_time_has_elapsed(
                current_tick_ms,
                manager->state_start_tick_ms,
                manager->config.device_restart_delay_ms))
        {
            if (manager->status.device_restart_count < UINT32_MAX)
            {
                manager->status.device_restart_count++;
            }
            (void)app_adxl_start_driver(manager, current_tick_ms);
        }
        return;
    }

    if (manager->status.state == APP_ADXL_MANAGER_OFFLINE_WAIT)
    {
        if (app_adxl_time_has_elapsed(
                current_tick_ms,
                manager->state_start_tick_ms,
                manager->config.offline_probe_period_ms))
        {
            if (manager->status.device_restart_count < UINT32_MAX)
            {
                manager->status.device_restart_count++;
            }
            (void)app_adxl_start_driver(manager, current_tick_ms);
        }
        return;
    }

    if ((manager->status.state == APP_ADXL_MANAGER_WAITING_BUS_RECOVERY) ||
        (manager->status.state == APP_ADXL_MANAGER_ERROR))
    {
        return;
    }

    I2cBus_GetStatus(manager->config.i2c_bus, &bus_status);
    if (bus_status.recovery_is_required)
    {
        app_adxl_request_bus_recovery(manager, current_tick_ms);
        return;
    }

    Adxl345_Service(&manager->sensor, current_tick_ms);
    Adxl345_GetStatus(&manager->sensor, &manager->status.driver);

    if ((manager->status.driver.last_error == ADXL345_ERROR_I2C_ABORT) ||
        manager->config.i2c_bus->status.recovery_is_required)
    {
        app_adxl_request_bus_recovery(manager, current_tick_ms);
        return;
    }

    successful_delta = manager->status.driver.successful_sample_count -
                       manager->previous_successful_sample_count;
    failed_delta = manager->status.driver.failed_sample_count -
                   manager->previous_failed_sample_count;
    manager->previous_successful_sample_count =
        manager->status.driver.successful_sample_count;
    manager->previous_failed_sample_count =
        manager->status.driver.failed_sample_count;

    if (successful_delta > 0UL)
    {
        manager->status.consecutive_read_error_count = 0U;
        app_adxl_increment_saturated_u8(&manager->status.good_sample_count,
                                        successful_delta);
    }
    else if (failed_delta > 0UL)
    {
        manager->status.good_sample_count = 0U;
        app_adxl_increment_saturated_u8(
            &manager->status.consecutive_read_error_count,
            failed_delta);
    }

    if (manager->status.state == APP_ADXL_MANAGER_INITIALIZING)
    {
        if (manager->status.driver.state == ADXL345_STATE_ERROR)
        {
            app_adxl_enter_state(manager,
                                 APP_ADXL_MANAGER_OFFLINE_WAIT,
                                 current_tick_ms);
            return;
        }
        if (manager->status.driver.is_ready)
        {
            manager->status.good_sample_count = 0U;
            app_adxl_enter_state(manager,
                                 APP_ADXL_MANAGER_CONFIRMING,
                                 current_tick_ms);
        }
        return;
    }

    if ((manager->status.state == APP_ADXL_MANAGER_CONFIRMING) &&
        (manager->status.good_sample_count >=
         manager->config.recovery_good_sample_count))
    {
        app_adxl_enter_state(manager,
                             APP_ADXL_MANAGER_ACTIVE,
                             current_tick_ms);
    }

    if (manager->status.consecutive_read_error_count >=
        manager->config.consecutive_read_error_limit)
    {
        app_adxl_schedule_restart(manager,
                                  APP_ADXL_MANAGER_RESTART_READ_ERRORS,
                                  current_tick_ms);
        return;
    }

    freshness_start_tick_ms = manager->status.driver.has_sample
        ? manager->status.driver.latest_sample.sample_tick_ms
        : manager->state_start_tick_ms;
    /* Không thay context driver khi buffer nhận vẫn đang được HAL mượn. */
    if (!manager->status.driver.transaction_is_pending &&
        app_adxl_time_has_elapsed(
            current_tick_ms,
            freshness_start_tick_ms,
            manager->config.persistent_stale_restart_ms))
    {
        app_adxl_schedule_restart(manager,
                                  APP_ADXL_MANAGER_RESTART_STALE,
                                  current_tick_ms);
    }
}

bool AppAdxlManager_TakeNewSample(AppAdxlManager_t *manager,
                                  Adxl345_Sample_t *output_sample)
{
    return (manager != NULL) &&
           Adxl345_TakeNewSample(&manager->sensor, output_sample);
}

void AppAdxlManager_HandleDataReadyInterrupt(AppAdxlManager_t *manager,
                                             uint16_t gpio_pin)
{
    if (manager != NULL)
    {
        Adxl345_HandleDataReadyInterrupt(&manager->sensor, gpio_pin);
    }
}

bool AppAdxlManager_TakeBusRecoveryRequest(AppAdxlManager_t *manager)
{
    bool request_pending;

    if (manager == NULL)
    {
        return false;
    }
    request_pending = manager->status.bus_recovery_request_pending;
    manager->status.bus_recovery_request_pending = false;
    return request_pending;
}

void AppAdxlManager_ReportBusRecoveryResult(AppAdxlManager_t *manager,
                                             bool recovery_succeeded,
                                             uint32_t current_tick_ms)
{
    if ((manager == NULL) || !manager->status.is_initialized ||
        (manager->status.state == APP_ADXL_MANAGER_ERROR))
    {
        return;
    }

    manager->status.bus_recovery_request_pending = false;
    manager->status.last_restart_reason = APP_ADXL_MANAGER_RESTART_SHARED_BUS;
    if (recovery_succeeded)
    {
        if (manager->status.device_restart_count < UINT32_MAX)
        {
            manager->status.device_restart_count++;
        }
        (void)app_adxl_start_driver(manager, current_tick_ms);
    }
    else
    {
        app_adxl_enter_state(manager,
                             APP_ADXL_MANAGER_OFFLINE_WAIT,
                             current_tick_ms);
    }
}

void AppAdxlManager_GetStatus(const AppAdxlManager_t *manager,
                              AppAdxlManager_Status_t *output_status)
{
    if ((manager != NULL) && (output_status != NULL))
    {
        *output_status = manager->status;
    }
}
