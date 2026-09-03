/**
 * @file app.c
 * @brief Ghép cấu hình board, driver phần cứng và service cấp ứng dụng.
 */

#include "app.h"

#include <stddef.h>

#include "app_config.h"
#include "board_config.h"
#include "button_input.h"
#include "gpio_output.h"
#include "mono_graphics.h"
#include "system_heartbeat.h"

/** @brief Context duy nhất của application, được cấp phát tĩnh. */
typedef struct
{
    ButtonInput_t ui_ok_button;
    ButtonInput_t ui_left_button;
    ButtonInput_t ui_right_button;
    ButtonInput_t ui_up_button;
    ButtonInput_t ui_down_button;
    GpioOutput_t digital_outputs[OUTPUT_CONTROL_COUNT];
    GpioOutput_t heartbeat_led;
    SystemHeartbeat_t heartbeat;
    I2cBus_t shared_i2c_bus;
    I2cBusRecovery_t shared_i2c_bus_recovery;
    Adxl345_t adxl345;
    Ssd1306_t oled;
    MonoGraphics_Canvas_t oled_canvas;
    EnvironmentDisplay_t environment_display;
    MotionDisplay_t motion_display;
    OutputControl_t output_control;
    OutputDisplay_t output_display;
    WarningFeedback_t warning_feedback;
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
    uint32_t oled_state_start_tick_ms;
    App_Status_t status;
    bool oled_driver_initialized;
    bool is_initialized;
} App_InternalContext_t;

static App_InternalContext_t s_app;

static void app_request_current_page_redraw(void);

/** @brief true khi App đang giữ độc quyền I2C1 để chờ hoặc thực hiện bus-clear. */
static bool app_shared_i2c_recovery_is_active(void)
{
    return (s_app.status.oled_state == APP_OLED_BUS_RECOVERY_WAIT) ||
           (s_app.status.oled_state == APP_OLED_BUS_RECOVERING);
}

/** @brief Kiểm tra elapsed time an toàn khi HAL tick tràn số. */
static bool app_time_has_elapsed(uint32_t current_tick_ms,
                                 uint32_t start_tick_ms,
                                 uint32_t duration_ms)
{
    return ((uint32_t)(current_tick_ms - start_tick_ms) >= duration_ms);
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

/** @brief Dừng luồng OLED tại lỗi đầu tiên; không tự retry. */
static void app_set_oled_error(App_OledError_t error)
{
    s_app.status.oled_error = error;
    s_app.status.oled_state = APP_OLED_ERROR;
}

/** @brief Chuyển trạng thái OLED và chỉ phát log UART tại đúng thời điểm chuyển. */
static void app_enter_oled_state(App_OledState_t state,
                                 uint32_t current_tick_ms,
                                 const char *log_text)
{
    if (s_app.status.oled_state == state)
    {
        return;
    }
    s_app.status.oled_state = state;
    s_app.oled_state_start_tick_ms = current_tick_ms;
    if (log_text != NULL)
    {
        (void)UartLog_Printf(&s_app.pc_log, "OLED: %s\r\n", log_text);
    }
}

/** @brief Khởi động lại chuỗi init SSD1306 tại địa chỉ đã chọn. */
static bool app_start_oled_initialization(uint32_t current_tick_ms)
{
    const Ssd1306_Config_t config =
    {
        .i2c_bus = &s_app.shared_i2c_bus,
        .address_7bit = s_app.status.oled_selected_address_7bit,
        .transfer_timeout_ms = APP_OLED_TRANSFER_TIMEOUT_MS,
        .contrast = APP_OLED_INITIAL_CONTRAST,
        .orientation = SSD1306_ORIENTATION_0_DEGREES
    };

    s_app.status.oled_initialize_result =
        Ssd1306_Initialize(&s_app.oled, &config, current_tick_ms);
    if (s_app.status.oled_initialize_result != SSD1306_INITIALIZE_ACCEPTED)
    {
        return false;
    }
    s_app.oled_driver_initialized = true;
    app_enter_oled_state(APP_OLED_INITIALIZING, current_tick_ms, NULL);
    return true;
}

/** @brief Khởi động lại state machine ADXL345 sau khi context I2C1 được tạo lại. */
static void app_restart_adxl345(uint32_t current_tick_ms)
{
    const Adxl345_Config_t config =
    {
        .i2c_bus = &s_app.shared_i2c_bus,
        .address_7bit = BOARD_ADXL345_I2C_ADDRESS_7BIT,
        .data_ready_pin = BOARD_ADXL345_INTERRUPT_PIN,
        .transfer_timeout_ms = APP_ADXL345_TRANSFER_TIMEOUT_MS,
        .initialize_retry_delay_ms = APP_ADXL345_INIT_RETRY_DELAY_MS,
        .stale_timeout_ms = APP_ADXL345_STALE_TIMEOUT_MS,
        .initialize_max_attempts = APP_ADXL345_INIT_MAX_ATTEMPTS
    };

    s_app.status.adxl345_initialize_result =
        Adxl345_Initialize(&s_app.adxl345, &config, current_tick_ms);
}

/** @brief Đưa OLED về offline và bắt đầu bộ định thời probe 2 giây. */
static void app_mark_oled_offline(App_OledError_t error,
                                  uint32_t current_tick_ms)
{
    s_app.status.oled_error = error;
    s_app.oled_driver_initialized = false;
    s_app.status.oled_fast_attempt_count = 0U;
    s_app.status.shared_i2c_bus_recovery_attempt_count = 0U;
    app_enter_oled_state(APP_OLED_OFFLINE_WAIT, current_tick_ms, "offline");
}

/** @brief Phân loại lỗi giao tiếp để retry thiết bị, bus-clear hoặc offline. */
static void app_handle_oled_transport_failure(App_OledError_t error,
                                              uint32_t current_tick_ms)
{
    I2cBus_Status_t bus_status;
    I2cBus_Result_t result = s_app.status.oled.last_i2c_result;
    bool bus_recovery_required;

    I2cBus_GetStatus(&s_app.shared_i2c_bus, &bus_status);
    bus_recovery_required = bus_status.recovery_is_required ||
                            (result == I2C_BUS_RESULT_ABORT_FAILED);
    s_app.status.oled_error = error;
    s_app.oled_driver_initialized = false;
    if (s_app.status.oled_fast_attempt_count == 0U)
    {
        /* Giao dịch ACTIVE vừa thất bại chính là lần thử đầu tiên của đợt lỗi. */
        s_app.status.oled_fast_attempt_count = 1U;
    }

    if (result == I2C_BUS_RESULT_NACK)
    {
        s_app.status.oled_consecutive_nack_count++;
        if (s_app.status.oled_consecutive_nack_count >=
            APP_OLED_NACK_OFFLINE_THRESHOLD)
        {
            app_mark_oled_offline(error, current_tick_ms);
            return;
        }
    }
    else
    {
        s_app.status.oled_consecutive_nack_count = 0U;
    }

    if (bus_recovery_required ||
        (s_app.status.oled_fast_attempt_count >=
         APP_OLED_FAST_ATTEMPT_LIMIT))
    {
        app_enter_oled_state(APP_OLED_BUS_RECOVERY_WAIT,
                             current_tick_ms, "bus recovery pending");
        return;
    }

    s_app.status.oled_fast_attempt_count++;
    app_enter_oled_state(APP_OLED_RETRY_WAIT, current_tick_ms, "retry pending");
}

/** @brief Adapter chuyển operation canvas tổng quát sang primitive pixel của SSD1306. */
static bool app_set_oled_canvas_pixel(
    void *context,
    uint16_t x,
    uint16_t y,
    MonoGraphics_PixelOperation_t operation)
{
    Ssd1306_PixelOperation_t ssd1306_operation;

    if ((context == NULL) ||
        (x >= SSD1306_WIDTH_PIXELS) ||
        (y >= SSD1306_HEIGHT_PIXELS))
    {
        return false;
    }

    if (operation == MONO_GRAPHICS_PIXEL_OFF)
    {
        ssd1306_operation = SSD1306_PIXEL_OFF;
    }
    else if (operation == MONO_GRAPHICS_PIXEL_ON)
    {
        ssd1306_operation = SSD1306_PIXEL_ON;
    }
    else if (operation == MONO_GRAPHICS_PIXEL_TOGGLE)
    {
        ssd1306_operation = SSD1306_PIXEL_TOGGLE;
    }
    else
    {
        return false;
    }

    return Ssd1306_SetPixel((Ssd1306_t *)context,
                            (uint8_t)x,
                            (uint8_t)y,
                            ssd1306_operation);
}

/** @brief Adapter xóa nhanh canvas bằng primitive framebuffer của SSD1306. */
static bool app_clear_oled_canvas(void *context)
{
    return (context != NULL) && Ssd1306_Clear((Ssd1306_t *)context);
}

/** @brief Kết quả render chung để App không phụ thuộc enum riêng của từng page. */
typedef enum
{
    APP_PAGE_RENDER_IDLE = 0,
    APP_PAGE_RENDER_DRAWN,
    APP_PAGE_RENDER_FAILED
} App_PageRenderResult_t;

/** @brief Đánh dấu page đang chọn cần được vẽ lại. */
static void app_request_current_page_redraw(void)
{
    switch (s_app.status.current_display_page)
    {
        case APP_DISPLAY_PAGE_MOTION:
            MotionDisplay_RequestRedraw(&s_app.motion_display);
            break;

        case APP_DISPLAY_PAGE_OUTPUTS:
            OutputDisplay_RequestRedraw(&s_app.output_display);
            break;

        case APP_DISPLAY_PAGE_ENVIRONMENT:
        default:
            EnvironmentDisplay_RequestRedraw(&s_app.environment_display);
            break;
    }
}

/** @brief Vẽ đúng page đang chọn nếu service tương ứng cho phép. */
static App_PageRenderResult_t app_render_current_page(uint32_t current_tick_ms)
{
    if (s_app.status.current_display_page == APP_DISPLAY_PAGE_MOTION)
    {
        MotionDisplay_RenderResult_t result = MotionDisplay_RenderIfDue(
            &s_app.motion_display,
            &s_app.oled_canvas,
            app_clear_oled_canvas,
            current_tick_ms);

        if (result == MOTION_DISPLAY_RENDER_DRAWN)
        {
            return APP_PAGE_RENDER_DRAWN;
        }
        if (result == MOTION_DISPLAY_RENDER_FAILED)
        {
            return APP_PAGE_RENDER_FAILED;
        }
        return APP_PAGE_RENDER_IDLE;
    }
    else if (s_app.status.current_display_page == APP_DISPLAY_PAGE_OUTPUTS)
    {
        OutputDisplay_RenderResult_t result = OutputDisplay_RenderIfDue(
            &s_app.output_display,
            &s_app.oled_canvas,
            app_clear_oled_canvas,
            current_tick_ms);

        if (result == OUTPUT_DISPLAY_RENDER_DRAWN)
        {
            return APP_PAGE_RENDER_DRAWN;
        }
        if (result == OUTPUT_DISPLAY_RENDER_FAILED)
        {
            return APP_PAGE_RENDER_FAILED;
        }
        return APP_PAGE_RENDER_IDLE;
    }
    else
    {
        EnvironmentDisplay_RenderResult_t result =
            EnvironmentDisplay_RenderIfDue(&s_app.environment_display,
                                           &s_app.oled_canvas,
                                           app_clear_oled_canvas,
                                           current_tick_ms);

        if (result == ENVIRONMENT_DISPLAY_RENDER_DRAWN)
        {
            return APP_PAGE_RENDER_DRAWN;
        }
        if (result == ENVIRONMENT_DISPLAY_RENDER_FAILED)
        {
            return APP_PAGE_RENDER_FAILED;
        }
        return APP_PAGE_RENDER_IDLE;
    }
}

/** @brief Xử lý hai sự kiện page; nhấn đồng thời được bỏ qua để tránh lựa chọn mơ hồ. */
static bool app_handle_display_navigation(bool left_pressed,
                                          bool right_pressed)
{
    if (left_pressed == right_pressed)
    {
        return false;
    }

    if (left_pressed)
    {
        if (s_app.status.current_display_page == APP_DISPLAY_PAGE_ENVIRONMENT)
        {
            s_app.status.current_display_page =
                (App_DisplayPage_t)(APP_DISPLAY_PAGE_COUNT - 1);
        }
        else
        {
            s_app.status.current_display_page =
                (App_DisplayPage_t)(s_app.status.current_display_page - 1);
        }
    }
    else
    {
        s_app.status.current_display_page =
            (App_DisplayPage_t)(s_app.status.current_display_page + 1);
        if (s_app.status.current_display_page >= APP_DISPLAY_PAGE_COUNT)
        {
            s_app.status.current_display_page = APP_DISPLAY_PAGE_ENVIRONMENT;
        }
    }

    app_request_current_page_redraw();
    return true;
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

    if ((s_app.status.current_display_page != APP_DISPLAY_PAGE_OUTPUTS) ||
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

/** @brief Tiến luồng init, phục hồi và refresh page đang chọn trên OLED. */
static void app_service_oled(uint32_t current_tick_ms)
{
    I2cBus_Operation_t completed_operation;
    I2cBus_Result_t bus_result;
    Ssd1306_Result_t refresh_result;
    App_PageRenderResult_t render_result;
    Ssd1306_RefreshRequestResult_t refresh_request;

    if (s_app.oled_driver_initialized)
    {
        Ssd1306_Service(&s_app.oled, current_tick_ms);
        Ssd1306_GetStatus(&s_app.oled, &s_app.status.oled);
    }

    switch (s_app.status.oled_state)
    {
        case APP_OLED_INITIALIZING:
            if (s_app.status.oled.is_ready)
            {
                s_app.status.oled_error = APP_OLED_ERROR_NONE;
                s_app.status.oled_fast_attempt_count = 0U;
                s_app.status.oled_consecutive_nack_count = 0U;
                s_app.status.shared_i2c_bus_recovery_attempt_count = 0U;
                app_request_current_page_redraw();
                app_enter_oled_state(APP_OLED_ACTIVE, current_tick_ms,
                                     "online");
            }
            else if (s_app.status.oled.has_error)
            {
                app_handle_oled_transport_failure(
                    APP_OLED_ERROR_DISPLAY_INIT, current_tick_ms);
            }
            break;

        case APP_OLED_ACTIVE:
            if (Ssd1306_TakeRefreshResult(&s_app.oled, &refresh_result) &&
                (refresh_result != SSD1306_RESULT_SUCCESS))
            {
                app_request_current_page_redraw();
                app_handle_oled_transport_failure(
                    APP_OLED_ERROR_REFRESH_RESULT, current_tick_ms);
                break;
            }

            Ssd1306_GetStatus(&s_app.oled, &s_app.status.oled);
            if (s_app.status.oled.refresh_is_pending ||
                s_app.status.oled.refresh_is_running ||
                s_app.status.oled.framebuffer_is_locked)
            {
                break;
            }

            render_result = app_render_current_page(current_tick_ms);

            if (render_result == APP_PAGE_RENDER_FAILED)
            {
                app_set_oled_error(APP_OLED_ERROR_DRAW);
                break;
            }

            if (render_result != APP_PAGE_RENDER_DRAWN)
            {
                break;
            }

            refresh_request = Ssd1306_RequestRefresh(&s_app.oled);
            if ((refresh_request != SSD1306_REFRESH_ACCEPTED) &&
                (refresh_request != SSD1306_REFRESH_NO_CHANGES))
            {
                app_request_current_page_redraw();
                app_set_oled_error(APP_OLED_ERROR_REFRESH_REQUEST);
            }
            break;

        case APP_OLED_RETRY_WAIT:
            if (app_time_has_elapsed(current_tick_ms,
                                     s_app.oled_state_start_tick_ms,
                                     APP_OLED_RETRY_DELAY_MS) &&
                !app_start_oled_initialization(current_tick_ms))
            {
                app_set_oled_error(APP_OLED_ERROR_DISPLAY_INIT);
            }
            break;

        case APP_OLED_BUS_RECOVERY_WAIT:
            if (!app_time_has_elapsed(current_tick_ms,
                                      s_app.oled_state_start_tick_ms,
                                      APP_SHARED_I2C_BUS_RECOVERY_DELAY_MS))
            {
                break;
            }
            if (s_app.status.shared_i2c_bus_recovery_attempt_count >=
                APP_SHARED_I2C_BUS_RECOVERY_ATTEMPT_LIMIT)
            {
                app_mark_oled_offline(s_app.status.oled_error,
                                      current_tick_ms);
                break;
            }
            s_app.status.shared_i2c_bus_recovery_attempt_count++;
            if (I2cBusRecovery_Start(&s_app.shared_i2c_bus_recovery,
                                     current_tick_ms))
            {
                app_enter_oled_state(APP_OLED_BUS_RECOVERING,
                                     current_tick_ms, "bus recovery started");
            }
            else
            {
                app_mark_oled_offline(s_app.status.oled_error,
                                      current_tick_ms);
            }
            break;

        case APP_OLED_BUS_RECOVERING:
            I2cBusRecovery_Service(&s_app.shared_i2c_bus_recovery,
                                   current_tick_ms);
            s_app.status.shared_i2c_bus_recovery_state =
                s_app.shared_i2c_bus_recovery.state;
            if (s_app.shared_i2c_bus_recovery.state ==
                I2C_BUS_RECOVERY_SUCCEEDED)
            {
                s_app.status.shared_i2c_bus_initialized =
                    I2cBus_Initialize(
                        &s_app.shared_i2c_bus,
                        s_app.shared_i2c_bus_recovery.config.hal_i2c);
                s_app.status.oled_fast_attempt_count = 1U;
                if (!s_app.status.shared_i2c_bus_initialized)
                {
                    app_mark_oled_offline(APP_OLED_ERROR_BUS_INIT,
                                          current_tick_ms);
                }
                else
                {
                    app_restart_adxl345(current_tick_ms);
                    if (!app_start_oled_initialization(current_tick_ms))
                    {
                        app_mark_oled_offline(APP_OLED_ERROR_DISPLAY_INIT,
                                              current_tick_ms);
                        break;
                    }
                    (void)UartLog_Printf(&s_app.pc_log,
                                         "I2C1: shared bus recovered\r\n");
                }
            }
            else if (s_app.shared_i2c_bus_recovery.state ==
                     I2C_BUS_RECOVERY_FAILED)
            {
                /* Tạo lại context sạch; lần recovery kế tiếp sẽ DeInit/Init HAL lần nữa. */
                (void)HAL_I2C_Init(
                    s_app.shared_i2c_bus_recovery.config.hal_i2c);
                s_app.status.shared_i2c_bus_initialized =
                    I2cBus_Initialize(
                        &s_app.shared_i2c_bus,
                        s_app.shared_i2c_bus_recovery.config.hal_i2c);
                if (s_app.status.shared_i2c_bus_recovery_attempt_count <
                    APP_SHARED_I2C_BUS_RECOVERY_ATTEMPT_LIMIT)
                {
                    app_enter_oled_state(APP_OLED_BUS_RECOVERY_WAIT,
                                         current_tick_ms,
                                         "bus recovery retry pending");
                }
                else
                {
                    app_mark_oled_offline(s_app.status.oled_error,
                                          current_tick_ms);
                }
            }
            break;

        case APP_OLED_OFFLINE_WAIT:
            if (app_time_has_elapsed(current_tick_ms,
                                     s_app.oled_state_start_tick_ms,
                                     APP_OLED_OFFLINE_PROBE_PERIOD_MS))
            {
                I2cBus_StartResult_t probe_start_result = I2cBus_StartProbe(
                    &s_app.shared_i2c_bus,
                    BOARD_OLED_I2C_ADDRESS_7BIT,
                    current_tick_ms,
                    APP_OLED_PROBE_TIMEOUT_MS);

                if (probe_start_result == I2C_BUS_START_ACCEPTED)
                {
                    app_enter_oled_state(APP_OLED_OFFLINE_PROBING,
                                         current_tick_ms, NULL);
                }
                else if ((probe_start_result != I2C_BUS_START_BUSY) &&
                         (probe_start_result !=
                          I2C_BUS_START_RESULT_PENDING))
                {
                    app_enter_oled_state(APP_OLED_BUS_RECOVERY_WAIT,
                                         current_tick_ms,
                                         "probe could not start");
                }
            }
            break;

        case APP_OLED_OFFLINE_PROBING:
            if (!I2cBus_TakeResult(&s_app.shared_i2c_bus,
                                   &completed_operation, &bus_result))
            {
                break;
            }
            if ((completed_operation == I2C_BUS_OPERATION_PROBE) &&
                (bus_result == I2C_BUS_RESULT_SUCCESS))
            {
                s_app.status.oled_fast_attempt_count = 1U;
                s_app.status.oled_consecutive_nack_count = 0U;
                if (!app_start_oled_initialization(current_tick_ms))
                {
                    app_set_oled_error(APP_OLED_ERROR_DISPLAY_INIT);
                }
            }
            else if (bus_result == I2C_BUS_RESULT_NACK)
            {
                app_enter_oled_state(APP_OLED_OFFLINE_WAIT,
                                     current_tick_ms, NULL);
            }
            else
            {
                app_enter_oled_state(APP_OLED_BUS_RECOVERY_WAIT,
                                     current_tick_ms,
                                     "probe bus error");
            }
            break;

        case APP_OLED_NOT_STARTED:
        case APP_OLED_ERROR:
        default:
            break;
    }

    if (s_app.oled_driver_initialized)
    {
        Ssd1306_GetStatus(&s_app.oled, &s_app.status.oled);
    }
    EnvironmentDisplay_GetStatus(&s_app.environment_display,
                                 &s_app.status.environment_display);
    MotionDisplay_GetStatus(&s_app.motion_display,
                            &s_app.status.motion_display);
    OutputDisplay_GetStatus(&s_app.output_display,
                            &s_app.status.output_display);
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
    const ButtonInput_Config_t ui_ok_button_config =
    {
        .port = BOARD_UI_OK_BUTTON_PORT,
        .pin = BOARD_UI_OK_BUTTON_PIN,
        .pressed_level = BOARD_UI_OK_BUTTON_PRESSED_LEVEL,
        .debounce_time_ms = APP_UI_BUTTON_DEBOUNCE_MS
    };
    const ButtonInput_Config_t ui_left_button_config =
    {
        .port = BOARD_UI_LEFT_BUTTON_PORT,
        .pin = BOARD_UI_LEFT_BUTTON_PIN,
        .pressed_level = BOARD_UI_LEFT_BUTTON_PRESSED_LEVEL,
        .debounce_time_ms = APP_UI_BUTTON_DEBOUNCE_MS
    };
    const ButtonInput_Config_t ui_right_button_config =
    {
        .port = BOARD_UI_RIGHT_BUTTON_PORT,
        .pin = BOARD_UI_RIGHT_BUTTON_PIN,
        .pressed_level = BOARD_UI_RIGHT_BUTTON_PRESSED_LEVEL,
        .debounce_time_ms = APP_UI_BUTTON_DEBOUNCE_MS
    };
    const ButtonInput_Config_t ui_up_button_config =
    {
        .port = BOARD_UI_UP_BUTTON_PORT,
        .pin = BOARD_UI_UP_BUTTON_PIN,
        .pressed_level = BOARD_UI_UP_BUTTON_PRESSED_LEVEL,
        .debounce_time_ms = APP_UI_BUTTON_DEBOUNCE_MS
    };
    const ButtonInput_Config_t ui_down_button_config =
    {
        .port = BOARD_UI_DOWN_BUTTON_PORT,
        .pin = BOARD_UI_DOWN_BUTTON_PIN,
        .pressed_level = BOARD_UI_DOWN_BUTTON_PRESSED_LEVEL,
        .debounce_time_ms = APP_UI_BUTTON_DEBOUNCE_MS
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
    uint8_t output_index;

    s_app = (App_InternalContext_t){0};
    s_app.status.current_display_page = APP_DISPLAY_PAGE_ENVIRONMENT;

    EnvironmentMonitor_Initialize();
    EnvironmentFeedback_Initialize();
    s_app.status.warning_feedback_initialized =
        WarningFeedback_Initialize(&s_app.warning_feedback, current_tick_ms);
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
        AppCommands_Initialize(&s_app.app_commands, &s_app.pc_log);
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
    s_app.status.environment_display_initialized =
        EnvironmentDisplay_Initialize(&s_app.environment_display);
    s_app.status.motion_display_initialized =
        MotionDisplay_Initialize(&s_app.motion_display);
    s_app.status.output_control_initialized =
        OutputControl_Initialize(&s_app.output_control);
    s_app.status.output_display_initialized =
        OutputDisplay_Initialize(&s_app.output_display);
    s_app.status.oled_canvas_initialized =
        MonoGraphics_InitializeCanvas(&s_app.oled_canvas,
                                      SSD1306_WIDTH_PIXELS,
                                      SSD1306_HEIGHT_PIXELS,
                                      &s_app.oled,
                                      app_set_oled_canvas_pixel);

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
    s_app.status.ui_ok_button_initialized =
        ButtonInput_Initialize(&s_app.ui_ok_button,
                               &ui_ok_button_config);
    s_app.status.ui_left_button_initialized =
        ButtonInput_Initialize(&s_app.ui_left_button,
                               &ui_left_button_config);
    s_app.status.ui_right_button_initialized =
        ButtonInput_Initialize(&s_app.ui_right_button,
                               &ui_right_button_config);
    s_app.status.ui_up_button_initialized =
        ButtonInput_Initialize(&s_app.ui_up_button,
                               &ui_up_button_config);
    s_app.status.ui_down_button_initialized =
        ButtonInput_Initialize(&s_app.ui_down_button,
                               &ui_down_button_config);
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

    s_app.status.oled_state = APP_OLED_NOT_STARTED;
    s_app.status.oled_selected_address_7bit =
        BOARD_OLED_I2C_ADDRESS_7BIT;

    if ((shared_i2c != NULL) &&
        (shared_i2c->Instance == BOARD_SHARED_I2C_INSTANCE))
    {
        const I2cBusRecovery_Config_t shared_recovery_config =
        {
            .hal_i2c = shared_i2c,
            .scl_port = BOARD_SHARED_I2C_SCL_PORT,
            .scl_pin = BOARD_SHARED_I2C_SCL_PIN,
            .sda_port = BOARD_SHARED_I2C_SDA_PORT,
            .sda_pin = BOARD_SHARED_I2C_SDA_PIN
        };
        bool recovery_initialized = I2cBusRecovery_Initialize(
            &s_app.shared_i2c_bus_recovery,
            &shared_recovery_config);

        s_app.status.shared_i2c_bus_initialized = recovery_initialized &&
            I2cBus_Initialize(&s_app.shared_i2c_bus, shared_i2c);
    }

    if (!s_app.status.environment_display_initialized ||
        !s_app.status.motion_display_initialized ||
        !s_app.status.output_control_initialized ||
        !s_app.status.output_display_initialized ||
        !s_app.status.oled_canvas_initialized)
    {
        app_set_oled_error(APP_OLED_ERROR_DRAW);
    }
    else if ((shared_i2c == NULL) ||
             (shared_i2c->Instance != BOARD_SHARED_I2C_INSTANCE))
    {
        app_set_oled_error(APP_OLED_ERROR_INVALID_I2C);
    }
    else if (!s_app.status.shared_i2c_bus_initialized)
    {
        app_set_oled_error(APP_OLED_ERROR_BUS_INIT);
    }
    else if (!app_start_oled_initialization(current_tick_ms))
    {
        app_set_oled_error(APP_OLED_ERROR_DISPLAY_INIT);
    }

    if (s_app.status.shared_i2c_bus_initialized)
    {
        app_restart_adxl345(current_tick_ms);
    }
    else
    {
        s_app.status.adxl345_initialize_result =
            ADXL345_INITIALIZE_INVALID_CONFIG;
    }

    DHT11_GetStatus(&s_app.status.dht11_status);
    EnvironmentMonitor_GetStatus(&s_app.status.environment);
    EnvironmentFeedback_GetStatus(&s_app.status.feedback);
    WarningFeedback_GetStatus(&s_app.warning_feedback,
                              &s_app.status.warning_feedback);
    EnvironmentDisplay_Update(&s_app.environment_display,
                              &s_app.status.environment,
                              &s_app.status.feedback);
    EnvironmentDisplay_GetStatus(&s_app.environment_display,
                                 &s_app.status.environment_display);
    Adxl345_GetStatus(&s_app.adxl345, &s_app.status.adxl345);
    MotionMonitor_GetStatus(&s_app.motion_monitor,
                            &s_app.status.motion_monitor);
    MotionDisplay_Update(&s_app.motion_display,
                         &s_app.status.adxl345,
                         &s_app.status.motion_monitor);
    MotionDisplay_GetStatus(&s_app.motion_display,
                            &s_app.status.motion_display);
    OutputControl_GetStatus(&s_app.output_control,
                            &s_app.status.output_control);
    OutputDisplay_Update(&s_app.output_display,
                         &s_app.status.output_control);
    OutputDisplay_GetStatus(&s_app.output_display,
                            &s_app.status.output_display);
    if (s_app.status.shared_i2c_bus_initialized)
    {
        I2cBus_GetStatus(&s_app.shared_i2c_bus,
                         &s_app.status.shared_i2c_bus);
    }
    s_app.is_initialized = true;
}

void App_Service(uint32_t current_tick_ms)
{
    const DHT11_Data_t *new_environment_sample = NULL;
    const MotionMonitor_Sample_t *new_motion_sample = NULL;
    Adxl345_Sample_t new_adxl345_sample;
    MotionMonitor_Sample_t motion_sample;
    CommandConsole_Event_t command_event;
    bool has_new_environment_sample;
    bool manual_warning_requested;
    bool ui_ok_requested;
    bool ui_left_requested;
    bool ui_right_requested;
    bool ui_up_requested;
    bool ui_down_requested;
    bool display_page_changed;
    uint8_t warning_source_mask;

    if (!s_app.is_initialized)
    {
        return;
    }

    CommandConsole_Service(&s_app.command_console, current_tick_ms);
    if (CommandConsole_TakeEvent(&s_app.command_console, &command_event))
    {
        (void)AppCommands_HandleConsoleEvent(&s_app.app_commands,
                                             &command_event);
    }

    DHT11_Service(current_tick_ms);

    if (s_app.status.shared_i2c_bus_initialized)
    {
        I2cBus_Service(&s_app.shared_i2c_bus, current_tick_ms);
        I2cBus_GetStatus(&s_app.shared_i2c_bus,
                         &s_app.status.shared_i2c_bus);
    }

    if (s_app.status.shared_i2c_bus_initialized &&
        !app_shared_i2c_recovery_is_active())
    {
        Adxl345_Service(&s_app.adxl345, current_tick_ms);
        Adxl345_GetStatus(&s_app.adxl345, &s_app.status.adxl345);
        if (Adxl345_TakeNewSample(&s_app.adxl345, &new_adxl345_sample))
        {
            motion_sample.x_mg = new_adxl345_sample.x_mg;
            motion_sample.y_mg = new_adxl345_sample.y_mg;
            motion_sample.z_mg = new_adxl345_sample.z_mg;
            motion_sample.sample_tick_ms = new_adxl345_sample.sample_tick_ms;
            new_motion_sample = &motion_sample;
        }
    }

    MotionMonitor_Service(&s_app.motion_monitor,
                          current_tick_ms,
                          new_motion_sample,
                          s_app.status.adxl345.sample_is_fresh);
    MotionMonitor_GetStatus(&s_app.motion_monitor,
                            &s_app.status.motion_monitor);

    has_new_environment_sample = DHT11_TakeNewData(&s_app.new_dht11_data);
    if (has_new_environment_sample)
    {
        new_environment_sample = &s_app.new_dht11_data;
    }

    DHT11_GetStatus(&s_app.status.dht11_status);
    EnvironmentMonitor_Service(current_tick_ms,
                               new_environment_sample,
                               &s_app.status.dht11_status);
    EnvironmentMonitor_GetStatus(&s_app.status.environment);

    ButtonInput_Service(&s_app.ui_ok_button, current_tick_ms);
    ButtonInput_Service(&s_app.ui_left_button, current_tick_ms);
    ButtonInput_Service(&s_app.ui_right_button, current_tick_ms);
    ButtonInput_Service(&s_app.ui_up_button, current_tick_ms);
    ButtonInput_Service(&s_app.ui_down_button, current_tick_ms);
    ui_ok_requested = ButtonInput_TakePressedEvent(&s_app.ui_ok_button);
    ui_left_requested = ButtonInput_TakePressedEvent(&s_app.ui_left_button);
    ui_right_requested = ButtonInput_TakePressedEvent(&s_app.ui_right_button);
    ui_up_requested = ButtonInput_TakePressedEvent(&s_app.ui_up_button);
    ui_down_requested = ButtonInput_TakePressedEvent(&s_app.ui_down_button);

    display_page_changed = app_handle_display_navigation(ui_left_requested,
                                                         ui_right_requested);
    manual_warning_requested = false;
    if (!display_page_changed)
    {
        if (s_app.status.current_display_page == APP_DISPLAY_PAGE_OUTPUTS)
        {
            (void)app_handle_output_page_input(ui_up_requested,
                                               ui_down_requested,
                                               ui_ok_requested);
        }
        else if (ui_ok_requested &&
                 !ui_up_requested &&
                 !ui_down_requested)
        {
            manual_warning_requested = true;
        }
    }

    OutputControl_GetStatus(&s_app.output_control,
                            &s_app.status.output_control);

    EnvironmentFeedback_Service(&s_app.status.environment,
                                has_new_environment_sample,
                                manual_warning_requested);
    EnvironmentFeedback_GetStatus(&s_app.status.feedback);
    warning_source_mask = WARNING_FEEDBACK_SOURCE_NONE;
    if (s_app.status.feedback.manual_warning_active)
    {
        warning_source_mask |= WARNING_FEEDBACK_SOURCE_MANUAL;
    }
    if (s_app.status.feedback.automatic_warning_active)
    {
        warning_source_mask |= WARNING_FEEDBACK_SOURCE_ENVIRONMENT;
    }
    if (s_app.status.motion_monitor.data_is_fresh &&
        (s_app.status.motion_monitor.motion_state ==
         MOTION_MONITOR_STATE_SHAKING))
    {
        warning_source_mask |= WARNING_FEEDBACK_SOURCE_SHAKING;
    }
    WarningFeedback_Service(&s_app.warning_feedback,
                            current_tick_ms,
                            warning_source_mask);
    WarningFeedback_GetStatus(&s_app.warning_feedback,
                              &s_app.status.warning_feedback);
    s_app.status.effective_output_mask =
        s_app.status.warning_feedback.warning_active
            ? s_app.status.warning_feedback.output_mask
            : s_app.status.output_control.output_on_mask;
    app_apply_digital_outputs(s_app.status.effective_output_mask);
    EnvironmentDisplay_Update(&s_app.environment_display,
                              &s_app.status.environment,
                              &s_app.status.feedback);
    EnvironmentDisplay_GetStatus(&s_app.environment_display,
                                 &s_app.status.environment_display);
    MotionDisplay_Update(&s_app.motion_display,
                         &s_app.status.adxl345,
                         &s_app.status.motion_monitor);
    MotionDisplay_GetStatus(&s_app.motion_display,
                            &s_app.status.motion_display);
    OutputDisplay_Update(&s_app.output_display,
                         &s_app.status.output_control);
    OutputDisplay_GetStatus(&s_app.output_display,
                            &s_app.status.output_display);

    if (s_app.status.shared_i2c_bus_initialized)
    {
        app_service_oled(current_tick_ms);
        I2cBus_GetStatus(&s_app.shared_i2c_bus,
                         &s_app.status.shared_i2c_bus);
    }

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
    ButtonInput_HandleExtiInterrupt(&s_app.ui_ok_button, gpio_pin);
    ButtonInput_HandleExtiInterrupt(&s_app.ui_left_button, gpio_pin);
    ButtonInput_HandleExtiInterrupt(&s_app.ui_right_button, gpio_pin);
    ButtonInput_HandleExtiInterrupt(&s_app.ui_up_button, gpio_pin);
    ButtonInput_HandleExtiInterrupt(&s_app.ui_down_button, gpio_pin);
    Adxl345_HandleDataReadyInterrupt(&s_app.adxl345, gpio_pin);
}

void App_HandleI2cMasterTransmitCompleteInterrupt(I2C_HandleTypeDef *i2c)
{
    I2cBus_HandleMasterTransmitCompleteInterrupt(&s_app.shared_i2c_bus, i2c);
}

void App_HandleI2cMemoryReadCompleteInterrupt(I2C_HandleTypeDef *i2c)
{
    I2cBus_HandleMemoryReadCompleteInterrupt(&s_app.shared_i2c_bus, i2c);
}

void App_HandleI2cErrorInterrupt(I2C_HandleTypeDef *i2c)
{
    I2cBus_HandleErrorInterrupt(&s_app.shared_i2c_bus, i2c);
}

void App_HandleI2cAbortCompleteInterrupt(I2C_HandleTypeDef *i2c)
{
    I2cBus_HandleAbortCompleteInterrupt(&s_app.shared_i2c_bus, i2c);
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
