/**
 * @file app_oled_manager.c
 * @brief State machine vòng đời OLED và chính sách retry/offline không blocking.
 */

#include "app_oled_manager.h"

#include <stddef.h>

/** @brief Kiểm tra elapsed time an toàn khi tick 32-bit tràn số. */
static bool app_oled_manager_time_has_elapsed(uint32_t current_tick_ms,
                                               uint32_t start_tick_ms,
                                               uint32_t duration_ms)
{
    return ((uint32_t)(current_tick_ms - start_tick_ms) >= duration_ms);
}

/** @brief Ghi một notification vào bitmask one-shot. */
static void app_oled_manager_notify(
    AppOledManager_t *manager,
    AppOledManager_Notification_t notification)
{
    if ((manager != NULL) &&
        (notification > APP_OLED_MANAGER_NOTIFICATION_NONE) &&
        (notification <= APP_OLED_MANAGER_NOTIFICATION_PROBE_BUS_ERROR))
    {
        manager->pending_notification_mask |=
            (uint16_t)(1UL << (uint8_t)notification);
        manager->status.last_notification = notification;
    }
}

/** @brief Chuyển state và cập nhật mốc thời gian chỉ khi state thực sự đổi. */
static void app_oled_manager_enter_state(AppOledManager_t *manager,
                                         AppOledManager_State_t state,
                                         uint32_t current_tick_ms)
{
    if ((manager != NULL) && (manager->status.state != state))
    {
        manager->status.state = state;
        manager->state_start_tick_ms = current_tick_ms;
    }
}

/** @brief Dừng tại lỗi cấu hình/phần mềm không có chính sách tự phục hồi. */
static void app_oled_manager_set_terminal_error(
    AppOledManager_t *manager,
    AppOledManager_Error_t error,
    uint32_t current_tick_ms)
{
    manager->status.error = error;
    manager->status.driver_initialized = false;
    app_oled_manager_enter_state(manager,
                                 APP_OLED_MANAGER_ERROR,
                                 current_tick_ms);
}

/** @brief Adapter primitive pixel tổng quát sang framebuffer SSD1306. */
static bool app_oled_manager_set_canvas_pixel(
    void *context,
    uint16_t x,
    uint16_t y,
    MonoGraphics_PixelOperation_t operation)
{
    Ssd1306_PixelOperation_t display_operation;

    if ((context == NULL) ||
        (x >= SSD1306_WIDTH_PIXELS) ||
        (y >= SSD1306_HEIGHT_PIXELS))
    {
        return false;
    }

    if (operation == MONO_GRAPHICS_PIXEL_OFF)
    {
        display_operation = SSD1306_PIXEL_OFF;
    }
    else if (operation == MONO_GRAPHICS_PIXEL_ON)
    {
        display_operation = SSD1306_PIXEL_ON;
    }
    else if (operation == MONO_GRAPHICS_PIXEL_TOGGLE)
    {
        display_operation = SSD1306_PIXEL_TOGGLE;
    }
    else
    {
        return false;
    }

    return Ssd1306_SetPixel((Ssd1306_t *)context,
                            (uint8_t)x,
                            (uint8_t)y,
                            display_operation);
}

/** @brief Primitive xóa framebuffer được chuyển cho callback render. */
static bool app_oled_manager_clear_canvas(void *context)
{
    return (context != NULL) && Ssd1306_Clear((Ssd1306_t *)context);
}

/** @brief Bắt đầu lại chuỗi init SSD1306 với cấu hình đã lưu. */
static bool app_oled_manager_start_display(AppOledManager_t *manager,
                                           uint32_t current_tick_ms)
{
    const Ssd1306_Config_t display_config =
    {
        .i2c_bus = manager->config.i2c_bus,
        .address_7bit = manager->config.address_7bit,
        .transfer_timeout_ms = manager->config.transfer_timeout_ms,
        .contrast = manager->config.contrast,
        .orientation = manager->config.orientation
    };

    manager->status.driver_initialize_result =
        Ssd1306_Initialize(&manager->display,
                           &display_config,
                           current_tick_ms);
    if (manager->status.driver_initialize_result !=
        SSD1306_INITIALIZE_ACCEPTED)
    {
        manager->status.driver_initialized = false;
        return false;
    }

    manager->status.driver_initialized = true;
    app_oled_manager_enter_state(manager,
                                 APP_OLED_MANAGER_INITIALIZING,
                                 current_tick_ms);
    return true;
}

/** @brief Đưa OLED về offline và bắt đầu bộ định thời probe. */
static void app_oled_manager_mark_offline(AppOledManager_t *manager,
                                          AppOledManager_Error_t error,
                                          uint32_t current_tick_ms)
{
    manager->status.error = error;
    manager->status.driver_initialized = false;
    manager->status.fast_attempt_count = 0U;
    manager->status.bus_recovery_request_pending = false;
    app_oled_manager_enter_state(manager,
                                 APP_OLED_MANAGER_OFFLINE_WAIT,
                                 current_tick_ms);
    app_oled_manager_notify(manager,
                            APP_OLED_MANAGER_NOTIFICATION_OFFLINE);
}

/** @brief Phân loại lỗi giao tiếp thành retry nhanh, bus recovery hoặc offline. */
static void app_oled_manager_handle_transport_failure(
    AppOledManager_t *manager,
    AppOledManager_Error_t error,
    uint32_t current_tick_ms)
{
    I2cBus_Status_t bus_status = {0};
    I2cBus_Result_t result = manager->status.driver.last_i2c_result;
    bool bus_recovery_required;

    I2cBus_GetStatus(manager->config.i2c_bus, &bus_status);
    bus_recovery_required = bus_status.recovery_is_required ||
                            (result == I2C_BUS_RESULT_ABORT_FAILED);
    manager->status.error = error;
    manager->status.driver_initialized = false;

    if (manager->status.fast_attempt_count == 0U)
    {
        /* Giao dịch ACTIVE vừa lỗi chính là lần thử đầu tiên của đợt lỗi. */
        manager->status.fast_attempt_count = 1U;
    }

    if (result == I2C_BUS_RESULT_NACK)
    {
        if (manager->status.consecutive_nack_count < UINT8_MAX)
        {
            manager->status.consecutive_nack_count++;
        }
        if (manager->status.consecutive_nack_count >=
            manager->config.nack_offline_threshold)
        {
            app_oled_manager_mark_offline(manager, error, current_tick_ms);
            return;
        }
    }
    else
    {
        manager->status.consecutive_nack_count = 0U;
    }

    if (bus_recovery_required ||
        (manager->status.fast_attempt_count >=
         manager->config.fast_attempt_limit))
    {
        manager->status.bus_recovery_request_pending = true;
        app_oled_manager_enter_state(
            manager,
            APP_OLED_MANAGER_WAITING_BUS_RECOVERY,
            current_tick_ms);
        app_oled_manager_notify(
            manager,
            APP_OLED_MANAGER_NOTIFICATION_BUS_RECOVERY_PENDING);
        return;
    }

    manager->status.fast_attempt_count++;
    app_oled_manager_enter_state(manager,
                                 APP_OLED_MANAGER_RETRY_WAIT,
                                 current_tick_ms);
    app_oled_manager_notify(manager,
                            APP_OLED_MANAGER_NOTIFICATION_RETRY_PENDING);
}

bool AppOledManager_Initialize(AppOledManager_t *manager,
                               const AppOledManager_Config_t *config,
                               uint32_t current_tick_ms)
{
    I2cBus_Status_t bus_status = {0};

    if (manager == NULL)
    {
        return false;
    }

    *manager = (AppOledManager_t){0};
    manager->status.state = APP_OLED_MANAGER_NOT_STARTED;
    manager->status.driver_initialize_result =
        SSD1306_INITIALIZE_INVALID_ARGUMENT;

    if ((config == NULL) ||
        (config->probe_timeout_ms == 0UL) ||
        (config->transfer_timeout_ms == 0UL) ||
        (config->retry_delay_ms == 0UL) ||
        (config->offline_probe_period_ms == 0UL) ||
        (config->fast_attempt_limit == 0U) ||
        (config->nack_offline_threshold == 0U) ||
        (config->render_frame == NULL) ||
        ((config->address_7bit != SSD1306_ADDRESS_LOW_7BIT) &&
         (config->address_7bit != SSD1306_ADDRESS_HIGH_7BIT)) ||
        ((config->orientation != SSD1306_ORIENTATION_0_DEGREES) &&
         (config->orientation != SSD1306_ORIENTATION_180_DEGREES)))
    {
        app_oled_manager_set_terminal_error(
            manager,
            APP_OLED_MANAGER_ERROR_DISPLAY_INIT,
            current_tick_ms);
        return false;
    }

    manager->config = *config;
    manager->status.address_7bit = config->address_7bit;
    manager->status.canvas_initialized = MonoGraphics_InitializeCanvas(
        &manager->canvas,
        SSD1306_WIDTH_PIXELS,
        SSD1306_HEIGHT_PIXELS,
        &manager->display,
        app_oled_manager_set_canvas_pixel);
    manager->status.is_initialized = manager->status.canvas_initialized;
    if (!manager->status.canvas_initialized)
    {
        app_oled_manager_set_terminal_error(manager,
                                             APP_OLED_MANAGER_ERROR_DRAW,
                                             current_tick_ms);
        return false;
    }

    if (config->i2c_bus == NULL)
    {
        app_oled_manager_set_terminal_error(
            manager,
            APP_OLED_MANAGER_ERROR_INVALID_I2C,
            current_tick_ms);
        return false;
    }

    I2cBus_GetStatus(config->i2c_bus, &bus_status);
    if (!bus_status.is_initialized)
    {
        app_oled_manager_set_terminal_error(manager,
                                             APP_OLED_MANAGER_ERROR_BUS_INIT,
                                             current_tick_ms);
        return false;
    }

    if (!app_oled_manager_start_display(manager, current_tick_ms))
    {
        app_oled_manager_set_terminal_error(
            manager,
            APP_OLED_MANAGER_ERROR_DISPLAY_INIT,
            current_tick_ms);
        return false;
    }
    return true;
}

void AppOledManager_Service(AppOledManager_t *manager,
                            uint32_t current_tick_ms)
{
    I2cBus_Operation_t completed_operation;
    I2cBus_Result_t bus_result;
    Ssd1306_Result_t refresh_result;
    Ssd1306_RefreshRequestResult_t refresh_request;
    AppOledManager_RenderResult_t render_result;

    if ((manager == NULL) || !manager->status.is_initialized)
    {
        return;
    }

    if (manager->status.driver_initialized)
    {
        Ssd1306_Service(&manager->display, current_tick_ms);
        Ssd1306_GetStatus(&manager->display, &manager->status.driver);
    }

    switch (manager->status.state)
    {
        case APP_OLED_MANAGER_INITIALIZING:
            if (manager->status.driver.is_ready)
            {
                manager->status.error = APP_OLED_MANAGER_ERROR_NONE;
                manager->status.fast_attempt_count = 0U;
                manager->status.consecutive_nack_count = 0U;
                manager->status.redraw_request_pending = true;
                app_oled_manager_enter_state(manager,
                                             APP_OLED_MANAGER_ACTIVE,
                                             current_tick_ms);
                app_oled_manager_notify(
                    manager,
                    APP_OLED_MANAGER_NOTIFICATION_ONLINE);
            }
            else if (manager->status.driver.has_error)
            {
                app_oled_manager_handle_transport_failure(
                    manager,
                    APP_OLED_MANAGER_ERROR_DISPLAY_INIT,
                    current_tick_ms);
            }
            break;

        case APP_OLED_MANAGER_ACTIVE:
            if (Ssd1306_TakeRefreshResult(&manager->display,
                                          &refresh_result) &&
                (refresh_result != SSD1306_RESULT_SUCCESS))
            {
                manager->status.redraw_request_pending = true;
                app_oled_manager_handle_transport_failure(
                    manager,
                    APP_OLED_MANAGER_ERROR_REFRESH_RESULT,
                    current_tick_ms);
                break;
            }

            Ssd1306_GetStatus(&manager->display, &manager->status.driver);
            if (manager->status.driver.refresh_is_pending ||
                manager->status.driver.refresh_is_running ||
                manager->status.driver.framebuffer_is_locked)
            {
                break;
            }

            render_result = manager->config.render_frame(
                manager->config.render_context,
                &manager->canvas,
                app_oled_manager_clear_canvas,
                current_tick_ms);
            if (render_result == APP_OLED_MANAGER_RENDER_FAILED)
            {
                app_oled_manager_set_terminal_error(
                    manager,
                    APP_OLED_MANAGER_ERROR_DRAW,
                    current_tick_ms);
                break;
            }
            if (render_result != APP_OLED_MANAGER_RENDER_DRAWN)
            {
                break;
            }

            refresh_request = Ssd1306_RequestRefresh(&manager->display);
            if ((refresh_request != SSD1306_REFRESH_ACCEPTED) &&
                (refresh_request != SSD1306_REFRESH_NO_CHANGES))
            {
                manager->status.redraw_request_pending = true;
                app_oled_manager_set_terminal_error(
                    manager,
                    APP_OLED_MANAGER_ERROR_REFRESH_REQUEST,
                    current_tick_ms);
            }
            break;

        case APP_OLED_MANAGER_RETRY_WAIT:
            if (app_oled_manager_time_has_elapsed(
                    current_tick_ms,
                    manager->state_start_tick_ms,
                    manager->config.retry_delay_ms) &&
                !app_oled_manager_start_display(manager, current_tick_ms))
            {
                app_oled_manager_set_terminal_error(
                    manager,
                    APP_OLED_MANAGER_ERROR_DISPLAY_INIT,
                    current_tick_ms);
            }
            break;

        case APP_OLED_MANAGER_OFFLINE_WAIT:
            if (app_oled_manager_time_has_elapsed(
                    current_tick_ms,
                    manager->state_start_tick_ms,
                    manager->config.offline_probe_period_ms))
            {
                I2cBus_StartResult_t probe_start_result =
                    I2cBus_StartProbe(manager->config.i2c_bus,
                                      manager->config.address_7bit,
                                      current_tick_ms,
                                      manager->config.probe_timeout_ms);

                if (probe_start_result == I2C_BUS_START_ACCEPTED)
                {
                    app_oled_manager_enter_state(
                        manager,
                        APP_OLED_MANAGER_OFFLINE_PROBING,
                        current_tick_ms);
                }
                else if ((probe_start_result != I2C_BUS_START_BUSY) &&
                         (probe_start_result !=
                          I2C_BUS_START_RESULT_PENDING))
                {
                    manager->status.bus_recovery_request_pending = true;
                    app_oled_manager_enter_state(
                        manager,
                        APP_OLED_MANAGER_WAITING_BUS_RECOVERY,
                        current_tick_ms);
                    app_oled_manager_notify(
                        manager,
                        APP_OLED_MANAGER_NOTIFICATION_PROBE_COULD_NOT_START);
                }
            }
            break;

        case APP_OLED_MANAGER_OFFLINE_PROBING:
            if (!I2cBus_TakeResult(manager->config.i2c_bus,
                                   &completed_operation,
                                   &bus_result))
            {
                break;
            }
            if ((completed_operation == I2C_BUS_OPERATION_PROBE) &&
                (bus_result == I2C_BUS_RESULT_SUCCESS))
            {
                manager->status.fast_attempt_count = 1U;
                manager->status.consecutive_nack_count = 0U;
                if (!app_oled_manager_start_display(manager,
                                                     current_tick_ms))
                {
                    app_oled_manager_set_terminal_error(
                        manager,
                        APP_OLED_MANAGER_ERROR_DISPLAY_INIT,
                        current_tick_ms);
                }
            }
            else if (bus_result == I2C_BUS_RESULT_NACK)
            {
                app_oled_manager_enter_state(
                    manager,
                    APP_OLED_MANAGER_OFFLINE_WAIT,
                    current_tick_ms);
            }
            else
            {
                manager->status.bus_recovery_request_pending = true;
                app_oled_manager_enter_state(
                    manager,
                    APP_OLED_MANAGER_WAITING_BUS_RECOVERY,
                    current_tick_ms);
                app_oled_manager_notify(
                    manager,
                    APP_OLED_MANAGER_NOTIFICATION_PROBE_BUS_ERROR);
            }
            break;

        case APP_OLED_MANAGER_WAITING_BUS_RECOVERY:
        case APP_OLED_MANAGER_NOT_STARTED:
        case APP_OLED_MANAGER_ERROR:
        default:
            break;
    }

    if (manager->status.driver_initialized)
    {
        Ssd1306_GetStatus(&manager->display, &manager->status.driver);
    }
}

bool AppOledManager_TakeBusRecoveryRequest(AppOledManager_t *manager)
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

void AppOledManager_ReportBusRecoveryResult(
    AppOledManager_t *manager,
    AppOledManager_BusRecoveryResult_t result,
    uint32_t current_tick_ms)
{
    if ((manager == NULL) || !manager->status.is_initialized ||
        (manager->status.state == APP_OLED_MANAGER_ERROR))
    {
        return;
    }

    manager->status.bus_recovery_request_pending = false;
    if (result == APP_OLED_MANAGER_BUS_RECOVERY_SUCCEEDED)
    {
        manager->status.fast_attempt_count = 1U;
        if (!app_oled_manager_start_display(manager, current_tick_ms))
        {
            app_oled_manager_mark_offline(
                manager,
                APP_OLED_MANAGER_ERROR_DISPLAY_INIT,
                current_tick_ms);
        }
    }
    else
    {
        app_oled_manager_mark_offline(manager,
                                      manager->status.error,
                                      current_tick_ms);
    }
}

bool AppOledManager_TakeRedrawRequest(AppOledManager_t *manager)
{
    bool request_pending;

    if (manager == NULL)
    {
        return false;
    }
    request_pending = manager->status.redraw_request_pending;
    manager->status.redraw_request_pending = false;
    return request_pending;
}

bool AppOledManager_TakeNotification(
    AppOledManager_t *manager,
    AppOledManager_Notification_t *notification)
{
    uint8_t candidate;

    if ((manager == NULL) || (notification == NULL))
    {
        return false;
    }

    for (candidate = (uint8_t)APP_OLED_MANAGER_NOTIFICATION_ONLINE;
         candidate <=
             (uint8_t)APP_OLED_MANAGER_NOTIFICATION_PROBE_BUS_ERROR;
         candidate++)
    {
        uint16_t notification_bit = (uint16_t)(1UL << candidate);
        if ((manager->pending_notification_mask & notification_bit) != 0U)
        {
            manager->pending_notification_mask &=
                (uint16_t)~notification_bit;
            *notification = (AppOledManager_Notification_t)candidate;
            return true;
        }
    }
    *notification = APP_OLED_MANAGER_NOTIFICATION_NONE;
    return false;
}

bool AppOledManager_IsWaitingForBusRecovery(
    const AppOledManager_t *manager)
{
    return (manager != NULL) &&
           (manager->status.state ==
            APP_OLED_MANAGER_WAITING_BUS_RECOVERY);
}

void AppOledManager_GetStatus(const AppOledManager_t *manager,
                              AppOledManager_Status_t *output_status)
{
    if ((manager != NULL) && (output_status != NULL))
    {
        *output_status = manager->status;
    }
}
