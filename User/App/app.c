/**
 * @file app.c
 * @brief Ghép cấu hình board, driver phần cứng và service cấp ứng dụng.
 */

#include "app.h"

#include <stddef.h>

#include "app_config.h"
#include "board_config.h"
#include "gpio_output.h"
#include "mono_graphics.h"
#include "system_heartbeat.h"

/** @brief Context duy nhất của application, được cấp phát tĩnh. */
typedef struct
{
    AppUiController_t ui_controller;
    GpioOutput_t digital_outputs[OUTPUT_CONTROL_COUNT];
    GpioOutput_t heartbeat_led;
    SystemHeartbeat_t heartbeat;
    AppSharedI2cManager_t shared_i2c_manager;
    AppAdxlManager_t adxl_manager;
    AppOledManager_t oled_manager;
    EnvironmentHistory_t environment_history;
    AppDisplayController_t display_controller;
    OutputControl_t output_control;
    MonitoringSession_t monitoring_session;
    AppWarningController_t warning_controller;
    MotionMonitor_t motion_monitor;
    PwmOutput_t servo_pwm;
    UartStream_t pc_serial;
    UartLog_t pc_log;
    CommandConsole_t command_console;
    AppCommands_t app_commands;

    DHT11_Data_t new_dht11_data;
    uint8_t pc_serial_rx_buffer[APP_PC_SERIAL_RX_BUFFER_CAPACITY];
    uint8_t pc_serial_tx_buffer[APP_PC_SERIAL_TX_BUFFER_CAPACITY];
    char pc_log_format_buffer[APP_UART_LOG_FORMAT_BUFFER_CAPACITY];
    char command_line_buffer[APP_COMMAND_LINE_BUFFER_CAPACITY];
    char *command_argument_vector[APP_COMMAND_MAX_ARGUMENTS];
    uint32_t servo_pwm_led_test_step_start_tick_ms;
    App_Status_t status;
    bool is_initialized;
} App_InternalContext_t;

static App_InternalContext_t s_app;

/** @brief Kiểm tra elapsed time an toàn khi HAL tick tràn số. */
static bool app_time_has_elapsed(uint32_t current_tick_ms,
                                 uint32_t start_tick_ms,
                                 uint32_t duration_ms)
{
    return ((uint32_t)(current_tick_ms - start_tick_ms) >= duration_ms);
}

/** @brief Chuyển source mask warning thành chuỗi ngắn dùng cho UART log. */
static const char *app_warning_source_text(uint8_t source_mask)
{
    static const char *const source_text[8] =
    {
        "NONE",
        "MANUAL",
        "ENV",
        "MANUAL|ENV",
        "SHAKING",
        "MANUAL|SHAKING",
        "ENV|SHAKING",
        "MANUAL|ENV|SHAKING"
    };

    return source_text[source_mask & 0x07U];
}

/** @brief Chuyển loại event warning thành động từ ngắn dùng cho UART log. */
static const char *app_warning_event_text(WarningHistory_Event_t event)
{
    switch (event)
    {
        case WARNING_HISTORY_EVENT_STARTED:
            return "START";
        case WARNING_HISTORY_EVENT_SOURCE_CHANGED:
            return "CHANGE";
        case WARNING_HISTORY_EVENT_CLEARED:
        default:
            return "CLEAR";
    }
}

/** @brief Đổi một bước test LED sang pulse width dựa trên chu kỳ PWM thực tế. */
static uint32_t app_servo_pwm_led_test_get_pulse_us(
    App_ServoPwmLedTestStep_t step)
{
    static const uint8_t duty_percent[APP_SERVO_PWM_LED_TEST_STEP_COUNT] =
    {
        0U,
        100U,
        25U,
        50U,
        75U
    };
    uint32_t period_us = s_app.servo_pwm.status.period_us;

    if ((step >= APP_SERVO_PWM_LED_TEST_STEP_COUNT) || (period_us == 0UL))
    {
        return 0UL;
    }

    if (step == APP_SERVO_PWM_LED_TEST_100_PERCENT)
    {
        /* CCR tối đa hợp lệ bằng ARR, tương ứng period_us - 1 us ở timer này. */
        return period_us - 1UL;
    }

    return (period_us * duty_percent[step]) / 100UL;
}

/** @brief Tiến test LED qua 0%, gần 100%, 25%, 50% và 75%, không blocking. */
static void app_service_servo_pwm_led_test(uint32_t current_tick_ms)
{
    App_ServoPwmLedTestStep_t next_step;
    PwmOutput_Result_t result;

    if (!s_app.status.servo_pwm_led_test_enabled ||
        !app_time_has_elapsed(current_tick_ms,
                              s_app.servo_pwm_led_test_step_start_tick_ms,
                              APP_SERVO_PWM_LED_TEST_STEP_TIME_MS))
    {
        return;
    }

    next_step = (App_ServoPwmLedTestStep_t)(
        s_app.status.servo_pwm_led_test_step + 1U);
    if (next_step >= APP_SERVO_PWM_LED_TEST_STEP_COUNT)
    {
        next_step = APP_SERVO_PWM_LED_TEST_OFF;
    }

    result = PwmOutput_SetPulseWidthUs(
        &s_app.servo_pwm,
        app_servo_pwm_led_test_get_pulse_us(next_step));
    s_app.status.servo_pwm_led_test_last_result = result;
    s_app.servo_pwm_led_test_step_start_tick_ms = current_tick_ms;

    if (result == PWM_OUTPUT_RESULT_OK)
    {
        s_app.status.servo_pwm_led_test_step = next_step;
    }
    else
    {
        /* Dừng test tại lỗi đầu tiên để tránh retry liên tục trong superloop. */
        s_app.status.servo_pwm_led_test_enabled = false;
    }
}

/**
 * @brief Xử lý Up/Down/OK ở trang Outputs; nhiều nút đồng thời được bỏ qua.
 * @return true khi lựa chọn hoặc trạng thái output đã thay đổi.
 */
static bool app_handle_output_page_input(bool up_pressed,
                                         bool down_pressed,
                                         bool ok_pressed)
{
    uint8_t pressed_count = (uint8_t)(up_pressed ? 1U : 0U) +
                            (uint8_t)(down_pressed ? 1U : 0U) +
                            (uint8_t)(ok_pressed ? 1U : 0U);

    if ((s_app.status.ui.current_page != APP_DISPLAY_PAGE_OUTPUTS) ||
        (pressed_count != 1U))
    {
        return false;
    }

    if (up_pressed)
    {
        OutputControl_SelectPrevious(&s_app.output_control);
    }
    else if (down_pressed)
    {
        OutputControl_SelectNext(&s_app.output_control);
    }
    else
    {
        OutputControl_ToggleSelected(&s_app.output_control);
    }
    return true;
}

/** @brief Đồng bộ trạng thái logic của service xuống năm GPIO output. */
static void app_apply_digital_outputs(uint8_t effective_output_mask)
{
    uint8_t output_index;

    for (output_index = 0U;
         output_index < OUTPUT_CONTROL_COUNT;
         output_index++)
    {
        bool should_be_on =
            (effective_output_mask & (uint8_t)(1U << output_index)) != 0U;
        if (GpioOutput_IsActive(&s_app.digital_outputs[output_index]) !=
            should_be_on)
        {
            GpioOutput_SetActive(&s_app.digital_outputs[output_index],
                                 should_be_on);
        }
    }
}

/** @brief Cấp snapshot hệ thống hiện tại cho toàn bộ năm page OLED. */
static void app_update_display_controller(bool monitoring_is_active)
{
    const AppDisplayController_Input_t input =
    {
        .environment = &s_app.status.environment,
        .environment_feedback =
            &s_app.status.warning.environment_feedback,
        .adxl345 = &s_app.status.adxl345.driver,
        .motion = &s_app.status.motion_monitor,
        .outputs = &s_app.status.output_control,
        .monitoring_is_active = monitoring_is_active
    };

    AppDisplayController_Update(&s_app.display_controller, &input);
    AppDisplayController_GetStatus(&s_app.display_controller,
                                   &s_app.status.display);
}

/** @brief Adapter nối manager OLED với page hiện tại của display controller. */
static AppOledManager_RenderResult_t app_render_oled_frame(
    void *context,
    const MonoGraphics_Canvas_t *canvas,
    AppOledManager_ClearCanvasFunction_t clear_canvas,
    uint32_t current_tick_ms)
{
    App_InternalContext_t *app = (App_InternalContext_t *)context;
    AppDisplayController_RenderResult_t result;

    if (app == NULL)
    {
        return APP_OLED_MANAGER_RENDER_FAILED;
    }

    result = AppDisplayController_RenderIfDue(
        &app->display_controller,
        app->status.ui.current_page,
        canvas,
        clear_canvas,
        current_tick_ms);
    if (result == APP_DISPLAY_CONTROLLER_RENDER_DRAWN)
    {
        return APP_OLED_MANAGER_RENDER_DRAWN;
    }
    if (result == APP_DISPLAY_CONTROLLER_RENDER_FAILED)
    {
        return APP_OLED_MANAGER_RENDER_FAILED;
    }
    return APP_OLED_MANAGER_RENDER_IDLE;
}

/** @brief Log các notification one-shot mà manager phát khi đổi trạng thái. */
static void app_process_oled_notifications(void)
{
    AppOledManager_Notification_t notification;
    const char *text;

    while (AppOledManager_TakeNotification(&s_app.oled_manager,
                                            &notification))
    {
        text = NULL;
        switch (notification)
        {
            case APP_OLED_MANAGER_NOTIFICATION_ONLINE:
                text = "online";
                break;
            case APP_OLED_MANAGER_NOTIFICATION_RETRY_PENDING:
                text = "retry pending";
                break;
            case APP_OLED_MANAGER_NOTIFICATION_BUS_RECOVERY_PENDING:
                text = "bus recovery pending";
                break;
            case APP_OLED_MANAGER_NOTIFICATION_OFFLINE:
                text = "offline";
                break;
            case APP_OLED_MANAGER_NOTIFICATION_PROBE_COULD_NOT_START:
                text = "probe could not start";
                break;
            case APP_OLED_MANAGER_NOTIFICATION_PROBE_BUS_ERROR:
                text = "probe bus error";
                break;
            case APP_OLED_MANAGER_NOTIFICATION_NONE:
            default:
                break;
        }
        if (text != NULL)
        {
            (void)UartLog_Printf(&s_app.pc_log, "OLED: %s\r\n", text);
        }
    }
}

/** @brief Log notification của shared-I2C manager đúng một lần khi state đổi. */
static void app_process_shared_i2c_notifications(void)
{
    AppSharedI2cManager_Notification_t notification;
    const char *text;

    while (AppSharedI2cManager_TakeNotification(
               &s_app.shared_i2c_manager,
               &notification))
    {
        text = NULL;
        switch (notification)
        {
            case APP_SHARED_I2C_NOTIFICATION_RECOVERY_PENDING:
                text = "recovery pending";
                break;
            case APP_SHARED_I2C_NOTIFICATION_RECOVERY_STARTED:
                text = "recovery started";
                break;
            case APP_SHARED_I2C_NOTIFICATION_RECOVERY_RETRY_PENDING:
                text = "recovery retry pending";
                break;
            case APP_SHARED_I2C_NOTIFICATION_RECOVERY_SUCCEEDED:
                text = "shared bus recovered";
                break;
            case APP_SHARED_I2C_NOTIFICATION_RECOVERY_FAILED:
                text = "recovery failed";
                break;
            case APP_SHARED_I2C_NOTIFICATION_NONE:
            default:
                break;
        }
        if (text != NULL)
        {
            (void)UartLog_Printf(&s_app.pc_log, "I2C1: %s\r\n", text);
        }
    }
}

/** @brief Báo kết quả bus recovery tới cả hai device vì context bus đã được tạo lại. */
static void app_forward_shared_i2c_recovery_result(uint32_t current_tick_ms)
{
    AppSharedI2cManager_RecoveryResult_t result;
    bool succeeded;

    if (!AppSharedI2cManager_TakeRecoveryResult(
            &s_app.shared_i2c_manager,
            &result,
            NULL))
    {
        return;
    }

    succeeded = (result == APP_SHARED_I2C_RECOVERY_SUCCEEDED);
    AppOledManager_ReportBusRecoveryResult(
        &s_app.oled_manager,
        succeeded ? APP_OLED_MANAGER_BUS_RECOVERY_SUCCEEDED
                  : APP_OLED_MANAGER_BUS_RECOVERY_FAILED,
        current_tick_ms);
    AppAdxlManager_ReportBusRecoveryResult(&s_app.adxl_manager,
                                            succeeded,
                                            current_tick_ms);
}

/** @brief Thu yêu cầu bus recovery từ hai device manager và chuyển tới owner của bus. */
static void app_collect_shared_i2c_recovery_requests(
    uint32_t current_tick_ms)
{
    if (AppAdxlManager_TakeBusRecoveryRequest(&s_app.adxl_manager) &&
        !AppSharedI2cManager_RequestRecovery(
            &s_app.shared_i2c_manager,
            (uint8_t)APP_SHARED_I2C_RECOVERY_SOURCE_ADXL345,
            current_tick_ms))
    {
        AppAdxlManager_ReportBusRecoveryResult(&s_app.adxl_manager,
                                                false,
                                                current_tick_ms);
    }

    if (AppOledManager_TakeBusRecoveryRequest(&s_app.oled_manager) &&
        !AppSharedI2cManager_RequestRecovery(
            &s_app.shared_i2c_manager,
            (uint8_t)APP_SHARED_I2C_RECOVERY_SOURCE_OLED,
            current_tick_ms))
    {
        AppOledManager_ReportBusRecoveryResult(
            &s_app.oled_manager,
            APP_OLED_MANAGER_BUS_RECOVERY_FAILED,
            current_tick_ms);
    }
}

/** @brief Tiến OLED manager, xử lý yêu cầu liên-module rồi đồng bộ snapshot debug. */
static void app_service_oled(uint32_t current_tick_ms)
{
    if (!AppSharedI2cManager_IsRecoveryActive(
            &s_app.shared_i2c_manager))
    {
        AppOledManager_Service(&s_app.oled_manager, current_tick_ms);
    }
    app_collect_shared_i2c_recovery_requests(current_tick_ms);

    if (AppOledManager_TakeRedrawRequest(&s_app.oled_manager))
    {
        AppDisplayController_RequestRedraw(
            &s_app.display_controller,
            s_app.status.ui.current_page);
    }
    app_process_oled_notifications();
    AppOledManager_GetStatus(&s_app.oled_manager, &s_app.status.oled);
    AppDisplayController_GetStatus(&s_app.display_controller,
                                   &s_app.status.display);
}

void App_Initialize(TIM_HandleTypeDef *dht11_timer,
                    TIM_HandleTypeDef *servo_timer,
                    UART_HandleTypeDef *pc_serial_uart,
                    I2C_HandleTypeDef *shared_i2c,
                    uint32_t current_tick_ms)
{
    const DHT11_Config_t dht11_config =
    {
        .timer = dht11_timer,
        .timer_instance = BOARD_DHT11_TIMER_INSTANCE,
        .timer_channel = BOARD_DHT11_TIMER_CHANNEL,
        .data_port = BOARD_DHT11_DATA_PORT,
        .data_pin = BOARD_DHT11_DATA_PIN
    };
    const PwmOutput_Config_t servo_pwm_config =
    {
        .timer = servo_timer,
        .timer_instance = BOARD_SERVO_TIMER_INSTANCE,
        .timer_channel = BOARD_SERVO_TIMER_CHANNEL
    };
    const GpioOutput_Config_t digital_output_configs[OUTPUT_CONTROL_COUNT] =
    {
        {
            .port = BOARD_DIGITAL_OUTPUT_1_PORT,
            .pin = BOARD_DIGITAL_OUTPUT_1_PIN,
            .active_level = BOARD_DIGITAL_OUTPUT_1_ACTIVE_LEVEL
        },
        {
            .port = BOARD_DIGITAL_OUTPUT_2_PORT,
            .pin = BOARD_DIGITAL_OUTPUT_2_PIN,
            .active_level = BOARD_DIGITAL_OUTPUT_2_ACTIVE_LEVEL
        },
        {
            .port = BOARD_DIGITAL_OUTPUT_3_PORT,
            .pin = BOARD_DIGITAL_OUTPUT_3_PIN,
            .active_level = BOARD_DIGITAL_OUTPUT_3_ACTIVE_LEVEL
        },
        {
            .port = BOARD_DIGITAL_OUTPUT_4_PORT,
            .pin = BOARD_DIGITAL_OUTPUT_4_PIN,
            .active_level = BOARD_DIGITAL_OUTPUT_4_ACTIVE_LEVEL
        },
        {
            .port = BOARD_DIGITAL_OUTPUT_5_PORT,
            .pin = BOARD_DIGITAL_OUTPUT_5_PIN,
            .active_level = BOARD_DIGITAL_OUTPUT_5_ACTIVE_LEVEL
        }
    };
    const GpioOutput_Config_t heartbeat_led_config =
    {
        .port = BOARD_HEARTBEAT_LED_PORT,
        .pin = BOARD_HEARTBEAT_LED_PIN,
        .active_level = BOARD_HEARTBEAT_LED_ACTIVE_LEVEL
    };
    const AppUiController_Config_t ui_controller_config =
    {
        .ok_button =
        {
            .port = BOARD_UI_OK_BUTTON_PORT,
            .pin = BOARD_UI_OK_BUTTON_PIN,
            .pressed_level = BOARD_UI_OK_BUTTON_PRESSED_LEVEL,
            .debounce_time_ms = APP_UI_BUTTON_DEBOUNCE_MS
        },
        .left_button =
        {
            .port = BOARD_UI_LEFT_BUTTON_PORT,
            .pin = BOARD_UI_LEFT_BUTTON_PIN,
            .pressed_level = BOARD_UI_LEFT_BUTTON_PRESSED_LEVEL,
            .debounce_time_ms = APP_UI_BUTTON_DEBOUNCE_MS
        },
        .right_button =
        {
            .port = BOARD_UI_RIGHT_BUTTON_PORT,
            .pin = BOARD_UI_RIGHT_BUTTON_PIN,
            .pressed_level = BOARD_UI_RIGHT_BUTTON_PRESSED_LEVEL,
            .debounce_time_ms = APP_UI_BUTTON_DEBOUNCE_MS
        },
        .up_button =
        {
            .port = BOARD_UI_UP_BUTTON_PORT,
            .pin = BOARD_UI_UP_BUTTON_PIN,
            .pressed_level = BOARD_UI_UP_BUTTON_PRESSED_LEVEL,
            .debounce_time_ms = APP_UI_BUTTON_DEBOUNCE_MS
        },
        .down_button =
        {
            .port = BOARD_UI_DOWN_BUTTON_PORT,
            .pin = BOARD_UI_DOWN_BUTTON_PIN,
            .pressed_level = BOARD_UI_DOWN_BUTTON_PRESSED_LEVEL,
            .debounce_time_ms = APP_UI_BUTTON_DEBOUNCE_MS
        },
        .ok_gesture =
        {
            .long_press_time_ms = APP_MONITORING_TOGGLE_HOLD_MS,
            .release_debounce_time_ms = APP_UI_BUTTON_DEBOUNCE_MS
        },
        .initial_page = APP_DISPLAY_PAGE_ENVIRONMENT
    };
    const SystemHeartbeat_Config_t heartbeat_config =
    {
        .on_time_ms = APP_HEARTBEAT_ON_TIME_MS,
        .off_time_ms = APP_HEARTBEAT_OFF_TIME_MS
    };
    const UartStream_Config_t pc_serial_config =
    {
        .uart = pc_serial_uart,
        .uart_instance = BOARD_PC_SERIAL_UART_INSTANCE,
        .rx_buffer = s_app.pc_serial_rx_buffer,
        .rx_buffer_capacity = APP_PC_SERIAL_RX_BUFFER_CAPACITY,
        .tx_buffer = s_app.pc_serial_tx_buffer,
        .tx_buffer_capacity = APP_PC_SERIAL_TX_BUFFER_CAPACITY
    };
    const UartLog_Config_t pc_log_config =
    {
        .stream = &s_app.pc_serial,
        .format_buffer = s_app.pc_log_format_buffer,
        .format_buffer_capacity = APP_UART_LOG_FORMAT_BUFFER_CAPACITY
    };
    const CommandConsole_Config_t command_console_config =
    {
        .stream = &s_app.pc_serial,
        .line_buffer = s_app.command_line_buffer,
        .line_buffer_capacity = APP_COMMAND_LINE_BUFFER_CAPACITY,
        .argument_vector = s_app.command_argument_vector,
        .argument_capacity = APP_COMMAND_MAX_ARGUMENTS,
        .max_bytes_per_service = APP_COMMAND_MAX_BYTES_PER_SERVICE,
        .incomplete_timeout_ms = APP_COMMAND_INCOMPLETE_TIMEOUT_MS
    };
    const MotionMonitor_Config_t motion_monitor_config =
    {
        .gravity_filter_divisor = APP_MOTION_GRAVITY_FILTER_DIVISOR,
        .orientation_minimum_mg = APP_MOTION_ORIENTATION_MINIMUM_MG,
        .orientation_margin_mg = APP_MOTION_ORIENTATION_MARGIN_MG,
        .still_threshold_mg = APP_MOTION_STILL_THRESHOLD_MG,
        .shaking_enter_threshold_mg =
            APP_MOTION_SHAKING_ENTER_THRESHOLD_MG,
        .shaking_exit_threshold_mg =
            APP_MOTION_SHAKING_EXIT_THRESHOLD_MG,
        .still_confirmation_ms = APP_MOTION_STILL_CONFIRMATION_MS,
        .shaking_exit_confirmation_ms =
            APP_MOTION_SHAKING_EXIT_CONFIRMATION_MS
    };
    AppSharedI2cManager_Config_t shared_i2c_manager_config;
    AppAdxlManager_Config_t adxl_manager_config;
    AppOledManager_Config_t oled_manager_config;
    I2cBus_t *shared_i2c_bus;
    uint8_t output_index;

    s_app = (App_InternalContext_t){0};

    EnvironmentMonitor_Initialize();
    (void)EnvironmentHistory_Initialize(&s_app.environment_history);
    (void)AppWarningController_Initialize(&s_app.warning_controller,
                                         current_tick_ms);
    (void)AppDisplayController_Initialize(
        &s_app.display_controller,
        &s_app.environment_history,
        AppWarningController_GetHistory(&s_app.warning_controller));
    AppDisplayController_GetStatus(&s_app.display_controller,
                                   &s_app.status.display);
    s_app.status.monitoring_session_initialized =
        MonitoringSession_Initialize(&s_app.monitoring_session,
                                     current_tick_ms);
    (void)AppUiController_Initialize(&s_app.ui_controller,
                                     &ui_controller_config);
    AppUiController_GetStatus(&s_app.ui_controller, &s_app.status.ui);
    s_app.status.motion_monitor_initialize_result =
        MotionMonitor_Initialize(&s_app.motion_monitor,
                                 &motion_monitor_config);
    s_app.status.pc_serial_initialize_result =
        UartStream_Initialize(&s_app.pc_serial, &pc_serial_config);
    UartStream_GetStatus(&s_app.pc_serial, &s_app.status.pc_serial);
    s_app.status.pc_log_initialize_result =
        UartLog_Initialize(&s_app.pc_log, &pc_log_config);
    s_app.status.command_console_initialize_result =
        CommandConsole_Initialize(&s_app.command_console,
                                  &command_console_config);
    s_app.status.app_commands_initialize_result =
        AppCommands_Initialize(&s_app.app_commands,
                               &s_app.pc_log,
                               &s_app.monitoring_session);
    if (s_app.status.app_commands_initialize_result == APP_COMMANDS_RESULT_OK)
    {
        (void)UartLog_Printf(&s_app.pc_log,
                             "Console ready. Type help.\r\n");
    }
    UartLog_GetStatus(&s_app.pc_log, &s_app.status.pc_log);
    CommandConsole_GetStatus(&s_app.command_console,
                             &s_app.status.command_console);
    AppCommands_GetStatus(&s_app.app_commands,
                          &s_app.status.app_commands);
    s_app.status.output_control_initialized =
        OutputControl_Initialize(&s_app.output_control);

    for (output_index = 0U;
         output_index < OUTPUT_CONTROL_COUNT;
         output_index++)
    {
        if (GpioOutput_Initialize(&s_app.digital_outputs[output_index],
                                  &digital_output_configs[output_index]))
        {
            s_app.status.digital_output_initialized_mask |=
                (uint8_t)(1U << output_index);
        }
    }
    s_app.status.heartbeat_led_initialized =
        GpioOutput_Initialize(&s_app.heartbeat_led,
                              &heartbeat_led_config);
    s_app.status.heartbeat_generator_initialized =
        SystemHeartbeat_Initialize(&s_app.heartbeat,
                                   &heartbeat_config,
                                   current_tick_ms);
    s_app.status.dht11_initialize_result =
        DHT11_Initialize(&dht11_config, current_tick_ms);
    s_app.status.servo_pwm_initialize_result =
        PwmOutput_Initialize(&s_app.servo_pwm, &servo_pwm_config);
    s_app.status.servo_pwm_led_test_enabled =
        (APP_SERVO_PWM_LED_TEST_ENABLED != 0U) &&
        (s_app.status.servo_pwm_initialize_result == PWM_OUTPUT_RESULT_OK);
    s_app.status.servo_pwm_led_test_step = APP_SERVO_PWM_LED_TEST_OFF;
    s_app.status.servo_pwm_led_test_last_result =
        s_app.status.servo_pwm_initialize_result;
    s_app.servo_pwm_led_test_step_start_tick_ms = current_tick_ms;
    PwmOutput_GetStatus(&s_app.servo_pwm, &s_app.status.servo_pwm);

    shared_i2c_manager_config = (AppSharedI2cManager_Config_t)
    {
        .hal_i2c = ((shared_i2c != NULL) &&
                    (shared_i2c->Instance == BOARD_SHARED_I2C_INSTANCE))
                       ? shared_i2c
                       : NULL,
        .scl_port = BOARD_SHARED_I2C_SCL_PORT,
        .scl_pin = BOARD_SHARED_I2C_SCL_PIN,
        .sda_port = BOARD_SHARED_I2C_SDA_PORT,
        .sda_pin = BOARD_SHARED_I2C_SDA_PIN,
        .recovery_delay_ms = APP_SHARED_I2C_BUS_RECOVERY_DELAY_MS,
        .recovery_attempt_limit =
            APP_SHARED_I2C_BUS_RECOVERY_ATTEMPT_LIMIT
    };
    (void)AppSharedI2cManager_Initialize(&s_app.shared_i2c_manager,
                                         &shared_i2c_manager_config);
    shared_i2c_bus = AppSharedI2cManager_GetBus(
        &s_app.shared_i2c_manager);
    AppSharedI2cManager_GetStatus(&s_app.shared_i2c_manager,
                                  &s_app.status.shared_i2c);

    oled_manager_config = (AppOledManager_Config_t)
    {
        .i2c_bus = shared_i2c_bus,
        .address_7bit = BOARD_OLED_I2C_ADDRESS_7BIT,
        .probe_timeout_ms = APP_OLED_PROBE_TIMEOUT_MS,
        .transfer_timeout_ms = APP_OLED_TRANSFER_TIMEOUT_MS,
        .retry_delay_ms = APP_OLED_RETRY_DELAY_MS,
        .offline_probe_period_ms = APP_OLED_OFFLINE_PROBE_PERIOD_MS,
        .fast_attempt_limit = APP_OLED_FAST_ATTEMPT_LIMIT,
        .nack_offline_threshold = APP_OLED_NACK_OFFLINE_THRESHOLD,
        .contrast = APP_OLED_INITIAL_CONTRAST,
        .orientation = SSD1306_ORIENTATION_0_DEGREES,
        .render_frame = app_render_oled_frame,
        .render_context = &s_app
    };
    (void)AppOledManager_Initialize(&s_app.oled_manager,
                                    &oled_manager_config,
                                    current_tick_ms);
    AppOledManager_GetStatus(&s_app.oled_manager, &s_app.status.oled);

    adxl_manager_config = (AppAdxlManager_Config_t)
    {
        .i2c_bus = shared_i2c_bus,
        .address_7bit = BOARD_ADXL345_I2C_ADDRESS_7BIT,
        .data_ready_pin = BOARD_ADXL345_INTERRUPT_PIN,
        .transfer_timeout_ms = APP_ADXL345_TRANSFER_TIMEOUT_MS,
        .driver_initialize_retry_delay_ms =
            APP_ADXL345_INIT_RETRY_DELAY_MS,
        .driver_stale_timeout_ms = APP_ADXL345_STALE_TIMEOUT_MS,
        .driver_initialize_max_attempts = APP_ADXL345_INIT_MAX_ATTEMPTS,
        .persistent_stale_restart_ms =
            APP_ADXL345_PERSISTENT_STALE_RESTART_MS,
        .device_restart_delay_ms = APP_ADXL345_DEVICE_RESTART_DELAY_MS,
        .offline_probe_period_ms = APP_ADXL345_OFFLINE_PROBE_PERIOD_MS,
        .consecutive_read_error_limit = APP_ADXL345_READ_ERROR_LIMIT,
        .recovery_good_sample_count =
            APP_ADXL345_RECOVERY_GOOD_SAMPLE_COUNT
    };
    (void)AppAdxlManager_Initialize(&s_app.adxl_manager,
                                    &adxl_manager_config,
                                    current_tick_ms);
    AppAdxlManager_GetStatus(&s_app.adxl_manager,
                             &s_app.status.adxl345);

    DHT11_GetStatus(&s_app.status.dht11_status);
    EnvironmentMonitor_GetStatus(&s_app.status.environment);
    EnvironmentHistory_GetStatus(&s_app.environment_history,
                                 &s_app.status.environment_history);
    AppWarningController_GetStatus(&s_app.warning_controller,
                                   &s_app.status.warning);
    MonitoringSession_GetStatus(&s_app.monitoring_session,
                                &s_app.status.monitoring_session);
    MotionMonitor_GetStatus(&s_app.motion_monitor,
                            &s_app.status.motion_monitor);
    OutputControl_GetStatus(&s_app.output_control,
                            &s_app.status.output_control);
    app_update_display_controller(MonitoringSession_IsActive(
        &s_app.monitoring_session));
    s_app.is_initialized = true;
}

void App_Service(uint32_t current_tick_ms)
{
    const DHT11_Data_t *new_environment_sample = NULL;
    const DHT11_MeasurementResult_t *new_dht11_result = NULL;
    const MotionMonitor_Sample_t *new_motion_sample = NULL;
    DHT11_MeasurementResult_t dht11_result;
    Adxl345_Sample_t new_adxl345_sample;
    MotionMonitor_Sample_t motion_sample;
    CommandConsole_Event_t command_event;
    AppUiController_Events_t ui_events;
    bool has_new_environment_sample;
    bool has_new_dht11_result;
    bool manual_warning_requested;
    bool monitoring_is_active;
    MonitoringSession_State_t monitoring_state_before_command;
    MonitoringSession_ChangeResult_t monitoring_change_result;
    WarningHistory_Record_t new_warning_record;
    AppWarningController_Input_t warning_input;

    if (!s_app.is_initialized)
    {
        return;
    }

    CommandConsole_Service(&s_app.command_console, current_tick_ms);
    monitoring_state_before_command = s_app.status.monitoring_session.state;
    if (CommandConsole_TakeEvent(&s_app.command_console, &command_event))
    {
        (void)AppCommands_HandleConsoleEvent(&s_app.app_commands,
                                             &command_event,
                                             current_tick_ms);
    }
    MonitoringSession_GetStatus(&s_app.monitoring_session,
                                &s_app.status.monitoring_session);
    if (monitoring_state_before_command !=
        s_app.status.monitoring_session.state)
    {
        AppDisplayController_RequestRedraw(
            &s_app.display_controller,
            s_app.status.ui.current_page);
    }
    monitoring_is_active = MonitoringSession_IsActive(
        &s_app.monitoring_session);

    DHT11_Service(current_tick_ms);

    AppSharedI2cManager_Service(&s_app.shared_i2c_manager,
                                current_tick_ms);
    app_forward_shared_i2c_recovery_result(current_tick_ms);
    app_process_shared_i2c_notifications();
    AppSharedI2cManager_GetStatus(&s_app.shared_i2c_manager,
                                  &s_app.status.shared_i2c);

    if (!AppSharedI2cManager_IsRecoveryActive(
            &s_app.shared_i2c_manager))
    {
        AppAdxlManager_Service(&s_app.adxl_manager, current_tick_ms);
        AppAdxlManager_GetStatus(&s_app.adxl_manager,
                                 &s_app.status.adxl345);
        if (AppAdxlManager_TakeNewSample(&s_app.adxl_manager,
                                         &new_adxl345_sample))
        {
            motion_sample.x_mg = new_adxl345_sample.x_mg;
            motion_sample.y_mg = new_adxl345_sample.y_mg;
            motion_sample.z_mg = new_adxl345_sample.z_mg;
            motion_sample.sample_tick_ms = new_adxl345_sample.sample_tick_ms;
            new_motion_sample = &motion_sample;
        }
    }
    app_collect_shared_i2c_recovery_requests(current_tick_ms);

    MotionMonitor_Service(&s_app.motion_monitor,
                          current_tick_ms,
                          new_motion_sample,
                          s_app.status.adxl345.driver.sample_is_fresh);
    MotionMonitor_GetStatus(&s_app.motion_monitor,
                            &s_app.status.motion_monitor);

    has_new_environment_sample = DHT11_TakeNewData(&s_app.new_dht11_data);
    if (has_new_environment_sample)
    {
        new_environment_sample = &s_app.new_dht11_data;
    }

    has_new_dht11_result = DHT11_TakeMeasurementResult(&dht11_result);
    if (has_new_dht11_result)
    {
        new_dht11_result = &dht11_result;
    }

    DHT11_GetStatus(&s_app.status.dht11_status);
    EnvironmentMonitor_Service(current_tick_ms,
                               new_environment_sample,
                               &s_app.status.dht11_status);
    EnvironmentMonitor_GetStatus(&s_app.status.environment);
    if ((new_dht11_result != NULL) && monitoring_is_active)
    {
        (void)EnvironmentHistory_Append(
            &s_app.environment_history,
            new_dht11_result,
            s_app.status.monitoring_session.session_id,
            &s_app.status.environment);
    }
    EnvironmentHistory_GetStatus(&s_app.environment_history,
                                 &s_app.status.environment_history);
    app_update_display_controller(monitoring_is_active);
    AppUiController_Service(&s_app.ui_controller,
                            current_tick_ms,
                            &ui_events);
    AppUiController_GetStatus(&s_app.ui_controller, &s_app.status.ui);

    if (ui_events.ok_long_requested)
    {
        monitoring_change_result = MonitoringSession_Toggle(
            &s_app.monitoring_session,
            MONITORING_SESSION_SOURCE_LOCAL_BUTTON,
            current_tick_ms);
        MonitoringSession_GetStatus(&s_app.monitoring_session,
                                    &s_app.status.monitoring_session);
        monitoring_is_active = MonitoringSession_IsActive(
            &s_app.monitoring_session);
        if (monitoring_change_result == MONITORING_SESSION_CHANGE_APPLIED)
        {
            (void)UartLog_Printf(
                &s_app.pc_log,
                "MONITOR %s session=%u source=local\r\n",
                monitoring_is_active ? "ACTIVE" : "INACTIVE",
                (unsigned int)s_app.status.monitoring_session.session_id);
            AppDisplayController_RequestRedraw(
                &s_app.display_controller,
                s_app.status.ui.current_page);
        }
    }

    if (ui_events.page_changed)
    {
        AppDisplayController_EnterPage(&s_app.display_controller,
                                       s_app.status.ui.current_page);
    }
    manual_warning_requested = false;
    if (!ui_events.page_changed)
    {
        if ((s_app.status.ui.current_page == APP_DISPLAY_PAGE_WARNING_HISTORY) ||
            (s_app.status.ui.current_page == APP_DISPLAY_PAGE_HISTORY))
        {
            (void)AppDisplayController_HandleHistoryInput(
                &s_app.display_controller,
                s_app.status.ui.current_page,
                ui_events.up_requested,
                ui_events.down_requested,
                ui_events.ok_short_requested);
        }
        else if (s_app.status.ui.current_page == APP_DISPLAY_PAGE_OUTPUTS)
        {
            (void)app_handle_output_page_input(ui_events.up_requested,
                                               ui_events.down_requested,
                                               ui_events.ok_short_requested);
        }
        else if (ui_events.ok_short_requested &&
                 !ui_events.up_requested &&
                 !ui_events.down_requested)
        {
            manual_warning_requested = true;
        }
    }

    OutputControl_GetStatus(&s_app.output_control,
                            &s_app.status.output_control);

    warning_input = (AppWarningController_Input_t)
    {
        .environment = &s_app.status.environment,
        .motion = &s_app.status.motion_monitor,
        .current_tick_ms = current_tick_ms,
        .monitoring_session_id =
            s_app.status.monitoring_session.session_id,
        .user_output_mask = s_app.status.output_control.output_on_mask,
        .has_new_environment_sample = has_new_environment_sample,
        .manual_warning_requested = manual_warning_requested,
        .monitoring_is_active = monitoring_is_active
    };
    if (AppWarningController_Service(&s_app.warning_controller,
                                     &warning_input,
                                     &new_warning_record))
    {
        (void)UartLog_Printf(
            &s_app.pc_log,
            "WARNING %s source=%s session=%u\r\n",
            app_warning_event_text(new_warning_record.event),
            app_warning_source_text(new_warning_record.source_mask),
            (unsigned int)new_warning_record.session_id);
    }
    AppWarningController_GetStatus(&s_app.warning_controller,
                                   &s_app.status.warning);
    app_apply_digital_outputs(s_app.status.warning.effective_output_mask);
    app_update_display_controller(monitoring_is_active);

    app_service_oled(current_tick_ms);
    AppSharedI2cManager_GetStatus(&s_app.shared_i2c_manager,
                                  &s_app.status.shared_i2c);

    SystemHeartbeat_Service(&s_app.heartbeat, current_tick_ms);
    s_app.status.heartbeat_led_is_active =
        SystemHeartbeat_ShouldBeActive(&s_app.heartbeat);

    GpioOutput_SetActive(&s_app.heartbeat_led,
                         s_app.status.heartbeat_led_is_active);
    app_service_servo_pwm_led_test(current_tick_ms);
    PwmOutput_GetStatus(&s_app.servo_pwm, &s_app.status.servo_pwm);
    UartStream_GetStatus(&s_app.pc_serial, &s_app.status.pc_serial);
    UartLog_GetStatus(&s_app.pc_log, &s_app.status.pc_log);
    CommandConsole_GetStatus(&s_app.command_console,
                             &s_app.status.command_console);
    AppCommands_GetStatus(&s_app.app_commands,
                          &s_app.status.app_commands);
}

void App_HandleTimerInputCaptureInterrupt(TIM_HandleTypeDef *timer)
{
    DHT11_HandleInputCaptureInterrupt(timer);
}

void App_HandleGpioExtiInterrupt(uint16_t gpio_pin)
{
    AppUiController_HandleExtiInterrupt(&s_app.ui_controller, gpio_pin);
    AppAdxlManager_HandleDataReadyInterrupt(&s_app.adxl_manager, gpio_pin);
}

void App_HandleI2cMasterTransmitCompleteInterrupt(I2C_HandleTypeDef *i2c)
{
    AppSharedI2cManager_HandleMasterTransmitCompleteInterrupt(
        &s_app.shared_i2c_manager,
        i2c);
}

void App_HandleI2cMemoryReadCompleteInterrupt(I2C_HandleTypeDef *i2c)
{
    AppSharedI2cManager_HandleMemoryReadCompleteInterrupt(
        &s_app.shared_i2c_manager,
        i2c);
}

void App_HandleI2cErrorInterrupt(I2C_HandleTypeDef *i2c)
{
    AppSharedI2cManager_HandleErrorInterrupt(&s_app.shared_i2c_manager,
                                             i2c);
}

void App_HandleI2cAbortCompleteInterrupt(I2C_HandleTypeDef *i2c)
{
    AppSharedI2cManager_HandleAbortCompleteInterrupt(
        &s_app.shared_i2c_manager,
        i2c);
}

void App_HandleUartReceiveCompleteInterrupt(UART_HandleTypeDef *uart)
{
    UartStream_HandleReceiveCompleteInterrupt(&s_app.pc_serial, uart);
}

void App_HandleUartTransmitCompleteInterrupt(UART_HandleTypeDef *uart)
{
    UartStream_HandleTransmitCompleteInterrupt(&s_app.pc_serial, uart);
}

void App_HandleUartErrorInterrupt(UART_HandleTypeDef *uart)
{
    UartStream_HandleErrorInterrupt(&s_app.pc_serial, uart);
}

void App_GetStatus(App_Status_t *output_status)
{
    if (output_status != NULL)
    {
        *output_status = s_app.status;
    }
}
