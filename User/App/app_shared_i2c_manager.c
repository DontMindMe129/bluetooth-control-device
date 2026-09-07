/**
 * @file app_shared_i2c_manager.c
 * @brief Arbitration và phục hồi vật lý cho I2C bus dùng chung.
 */

#include "app_shared_i2c_manager.h"

#include <stddef.h>

#define APP_SHARED_I2C_VALID_SOURCE_MASK \
    ((uint8_t)APP_SHARED_I2C_RECOVERY_SOURCE_OLED | \
     (uint8_t)APP_SHARED_I2C_RECOVERY_SOURCE_ADXL345)

static bool app_shared_i2c_time_has_elapsed(uint32_t current_tick_ms,
                                             uint32_t start_tick_ms,
                                             uint32_t duration_ms)
{
    return ((uint32_t)(current_tick_ms - start_tick_ms) >= duration_ms);
}

static void app_shared_i2c_notify(
    AppSharedI2cManager_t *manager,
    AppSharedI2cManager_Notification_t notification)
{
    if ((notification > APP_SHARED_I2C_NOTIFICATION_NONE) &&
        (notification <= APP_SHARED_I2C_NOTIFICATION_RECOVERY_FAILED))
    {
        manager->pending_notification_mask |=
            (uint16_t)(1UL << (uint8_t)notification);
        manager->status.last_notification = notification;
    }
}

static void app_shared_i2c_enter_state(
    AppSharedI2cManager_t *manager,
    AppSharedI2cManager_State_t state,
    uint32_t current_tick_ms)
{
    if (manager->status.state != state)
    {
        manager->status.state = state;
        manager->state_start_tick_ms = current_tick_ms;
    }
}

static bool app_shared_i2c_reinitialize_bus(
    AppSharedI2cManager_t *manager)
{
    manager->status.bus_is_initialized =
        I2cBus_Initialize(&manager->bus, manager->config.hal_i2c);
    if (manager->status.bus_is_initialized)
    {
        I2cBus_GetStatus(&manager->bus, &manager->status.bus);
    }
    return manager->status.bus_is_initialized;
}

static void app_shared_i2c_finish_recovery(
    AppSharedI2cManager_t *manager,
    AppSharedI2cManager_RecoveryResult_t result,
    uint32_t current_tick_ms)
{
    manager->status.last_recovery_result = result;
    manager->status.recovery_result_pending = true;
    manager->status.completed_source_mask =
        manager->status.requested_source_mask;
    manager->status.requested_source_mask = 0U;
    manager->status.error =
        (result == APP_SHARED_I2C_RECOVERY_SUCCEEDED)
            ? APP_SHARED_I2C_ERROR_NONE
            : APP_SHARED_I2C_ERROR_RECOVERY_FAILED;
    app_shared_i2c_enter_state(manager,
                               manager->status.bus_is_initialized
                                   ? APP_SHARED_I2C_READY
                                   : APP_SHARED_I2C_ERROR,
                               current_tick_ms);
    app_shared_i2c_notify(
        manager,
        (result == APP_SHARED_I2C_RECOVERY_SUCCEEDED)
            ? APP_SHARED_I2C_NOTIFICATION_RECOVERY_SUCCEEDED
            : APP_SHARED_I2C_NOTIFICATION_RECOVERY_FAILED);
}

bool AppSharedI2cManager_Initialize(
    AppSharedI2cManager_t *manager,
    const AppSharedI2cManager_Config_t *config)
{
    I2cBusRecovery_Config_t recovery_config;

    if (manager == NULL)
    {
        return false;
    }
    *manager = (AppSharedI2cManager_t){0};
    manager->status.state = APP_SHARED_I2C_NOT_STARTED;

    if ((config == NULL) || (config->hal_i2c == NULL) ||
        (config->hal_i2c->Instance == NULL) ||
        (config->scl_port == NULL) || (config->scl_pin == 0U) ||
        (config->sda_port == NULL) || (config->sda_pin == 0U) ||
        (config->recovery_delay_ms == 0UL) ||
        (config->recovery_attempt_limit == 0U))
    {
        manager->status.error = APP_SHARED_I2C_ERROR_INVALID_CONFIG;
        manager->status.state = APP_SHARED_I2C_ERROR;
        return false;
    }

    manager->config = *config;
    recovery_config = (I2cBusRecovery_Config_t)
    {
        .hal_i2c = config->hal_i2c,
        .scl_port = config->scl_port,
        .scl_pin = config->scl_pin,
        .sda_port = config->sda_port,
        .sda_pin = config->sda_pin
    };

    if (!I2cBusRecovery_Initialize(&manager->physical_recovery,
                                   &recovery_config) ||
        !app_shared_i2c_reinitialize_bus(manager))
    {
        manager->status.error = APP_SHARED_I2C_ERROR_BUS_INIT;
        manager->status.state = APP_SHARED_I2C_ERROR;
        return false;
    }

    manager->status.is_initialized = true;
    manager->status.error = APP_SHARED_I2C_ERROR_NONE;
    manager->status.physical_recovery_state =
        manager->physical_recovery.state;
    manager->status.state = APP_SHARED_I2C_READY;
    return true;
}

void AppSharedI2cManager_Service(AppSharedI2cManager_t *manager,
                                 uint32_t current_tick_ms)
{
    if ((manager == NULL) || !manager->status.is_initialized)
    {
        return;
    }

    if ((manager->status.state == APP_SHARED_I2C_READY) ||
        (manager->status.state == APP_SHARED_I2C_RECOVERY_WAIT))
    {
        I2cBus_Service(&manager->bus, current_tick_ms);
        I2cBus_GetStatus(&manager->bus, &manager->status.bus);
    }

    if (manager->status.state == APP_SHARED_I2C_RECOVERY_WAIT)
    {
        if (!app_shared_i2c_time_has_elapsed(
                current_tick_ms,
                manager->state_start_tick_ms,
                manager->config.recovery_delay_ms))
        {
            return;
        }

        if (manager->status.recovery_attempt_count >=
            manager->config.recovery_attempt_limit)
        {
            app_shared_i2c_finish_recovery(
                manager,
                APP_SHARED_I2C_RECOVERY_FAILED,
                current_tick_ms);
            return;
        }

        manager->status.recovery_attempt_count++;
        if (I2cBusRecovery_Start(&manager->physical_recovery,
                                 current_tick_ms))
        {
            manager->status.physical_recovery_state =
                manager->physical_recovery.state;
            app_shared_i2c_enter_state(manager,
                                       APP_SHARED_I2C_RECOVERING,
                                       current_tick_ms);
            app_shared_i2c_notify(
                manager,
                APP_SHARED_I2C_NOTIFICATION_RECOVERY_STARTED);
        }
        else if (manager->status.recovery_attempt_count <
                 manager->config.recovery_attempt_limit)
        {
            manager->state_start_tick_ms = current_tick_ms;
            app_shared_i2c_notify(
                manager,
                APP_SHARED_I2C_NOTIFICATION_RECOVERY_RETRY_PENDING);
        }
        else
        {
            (void)HAL_I2C_Init(manager->config.hal_i2c);
            (void)app_shared_i2c_reinitialize_bus(manager);
            app_shared_i2c_finish_recovery(
                manager,
                APP_SHARED_I2C_RECOVERY_FAILED,
                current_tick_ms);
        }
        return;
    }

    if (manager->status.state != APP_SHARED_I2C_RECOVERING)
    {
        return;
    }

    I2cBusRecovery_Service(&manager->physical_recovery, current_tick_ms);
    manager->status.physical_recovery_state =
        manager->physical_recovery.state;

    if (manager->physical_recovery.state == I2C_BUS_RECOVERY_SUCCEEDED)
    {
        if (app_shared_i2c_reinitialize_bus(manager))
        {
            app_shared_i2c_finish_recovery(
                manager,
                APP_SHARED_I2C_RECOVERY_SUCCEEDED,
                current_tick_ms);
        }
        else
        {
            app_shared_i2c_finish_recovery(
                manager,
                APP_SHARED_I2C_RECOVERY_FAILED,
                current_tick_ms);
        }
    }
    else if (manager->physical_recovery.state == I2C_BUS_RECOVERY_FAILED)
    {
        (void)HAL_I2C_Init(manager->config.hal_i2c);
        (void)app_shared_i2c_reinitialize_bus(manager);
        if (manager->status.recovery_attempt_count <
            manager->config.recovery_attempt_limit)
        {
            app_shared_i2c_enter_state(manager,
                                       APP_SHARED_I2C_RECOVERY_WAIT,
                                       current_tick_ms);
            app_shared_i2c_notify(
                manager,
                APP_SHARED_I2C_NOTIFICATION_RECOVERY_RETRY_PENDING);
        }
        else
        {
            app_shared_i2c_finish_recovery(
                manager,
                APP_SHARED_I2C_RECOVERY_FAILED,
                current_tick_ms);
        }
    }
}

bool AppSharedI2cManager_RequestRecovery(
    AppSharedI2cManager_t *manager,
    uint8_t source_mask,
    uint32_t current_tick_ms)
{
    source_mask &= APP_SHARED_I2C_VALID_SOURCE_MASK;
    if ((manager == NULL) || !manager->status.is_initialized ||
        (source_mask == 0U) ||
        (manager->status.state == APP_SHARED_I2C_ERROR))
    {
        return false;
    }

    manager->status.requested_source_mask |= source_mask;
    if ((manager->status.state == APP_SHARED_I2C_RECOVERY_WAIT) ||
        (manager->status.state == APP_SHARED_I2C_RECOVERING))
    {
        return true;
    }

    manager->status.recovery_attempt_count = 0U;
    manager->status.recovery_result_pending = false;
    manager->status.completed_source_mask = 0U;
    app_shared_i2c_enter_state(manager,
                               APP_SHARED_I2C_RECOVERY_WAIT,
                               current_tick_ms);
    app_shared_i2c_notify(
        manager,
        APP_SHARED_I2C_NOTIFICATION_RECOVERY_PENDING);
    return true;
}

bool AppSharedI2cManager_TakeRecoveryResult(
    AppSharedI2cManager_t *manager,
    AppSharedI2cManager_RecoveryResult_t *result,
    uint8_t *source_mask)
{
    if ((manager == NULL) || (result == NULL) ||
        !manager->status.recovery_result_pending)
    {
        return false;
    }
    *result = manager->status.last_recovery_result;
    if (source_mask != NULL)
    {
        *source_mask = manager->status.completed_source_mask;
    }
    manager->status.recovery_result_pending = false;
    return true;
}

bool AppSharedI2cManager_TakeNotification(
    AppSharedI2cManager_t *manager,
    AppSharedI2cManager_Notification_t *notification)
{
    uint8_t candidate;

    if ((manager == NULL) || (notification == NULL))
    {
        return false;
    }
    for (candidate = (uint8_t)APP_SHARED_I2C_NOTIFICATION_RECOVERY_PENDING;
         candidate <=
             (uint8_t)APP_SHARED_I2C_NOTIFICATION_RECOVERY_FAILED;
         candidate++)
    {
        uint16_t bit = (uint16_t)(1UL << candidate);
        if ((manager->pending_notification_mask & bit) != 0U)
        {
            manager->pending_notification_mask &= (uint16_t)~bit;
            *notification =
                (AppSharedI2cManager_Notification_t)candidate;
            return true;
        }
    }
    *notification = APP_SHARED_I2C_NOTIFICATION_NONE;
    return false;
}

bool AppSharedI2cManager_IsRecoveryActive(
    const AppSharedI2cManager_t *manager)
{
    return (manager != NULL) &&
           ((manager->status.state == APP_SHARED_I2C_RECOVERY_WAIT) ||
            (manager->status.state == APP_SHARED_I2C_RECOVERING));
}

I2cBus_t *AppSharedI2cManager_GetBus(AppSharedI2cManager_t *manager)
{
    if ((manager == NULL) || !manager->status.is_initialized ||
        !manager->status.bus_is_initialized)
    {
        return NULL;
    }
    return &manager->bus;
}

void AppSharedI2cManager_GetStatus(
    const AppSharedI2cManager_t *manager,
    AppSharedI2cManager_Status_t *output_status)
{
    if ((manager != NULL) && (output_status != NULL))
    {
        *output_status = manager->status;
    }
}

void AppSharedI2cManager_HandleMasterTransmitCompleteInterrupt(
    AppSharedI2cManager_t *manager,
    I2C_HandleTypeDef *hal_i2c)
{
    if (manager != NULL)
    {
        I2cBus_HandleMasterTransmitCompleteInterrupt(&manager->bus,
                                                     hal_i2c);
    }
}

void AppSharedI2cManager_HandleMemoryReadCompleteInterrupt(
    AppSharedI2cManager_t *manager,
    I2C_HandleTypeDef *hal_i2c)
{
    if (manager != NULL)
    {
        I2cBus_HandleMemoryReadCompleteInterrupt(&manager->bus, hal_i2c);
    }
}

void AppSharedI2cManager_HandleErrorInterrupt(
    AppSharedI2cManager_t *manager,
    I2C_HandleTypeDef *hal_i2c)
{
    if (manager != NULL)
    {
        I2cBus_HandleErrorInterrupt(&manager->bus, hal_i2c);
    }
}

void AppSharedI2cManager_HandleAbortCompleteInterrupt(
    AppSharedI2cManager_t *manager,
    I2C_HandleTypeDef *hal_i2c)
{
    if (manager != NULL)
    {
        I2cBus_HandleAbortCompleteInterrupt(&manager->bus, hal_i2c);
    }
}
