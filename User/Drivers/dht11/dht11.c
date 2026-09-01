/**
 * @file dht11.c
 * @brief Hiện thực state machine và bộ thu input capture cho DHT11.
 */

#include "dht11.h"

#include <limits.h>

#define DHT11_READ_PERIOD_MS             (2000UL)
#define DHT11_START_LOW_TIME_MS          (20UL)
#define DHT11_FRAME_TIMEOUT_MS           (10UL)

#define DHT11_ACK_MIN_US                 (120UL)
#define DHT11_ACK_MAX_US                 (220UL)
#define DHT11_BIT_MIN_US                 (60UL)
#define DHT11_BIT_THRESHOLD_US           (100UL)
#define DHT11_BIT_MAX_US                 (150UL)

#define DHT11_RECEIVED_FRAME_BYTE_COUNT  (5U)
#define DHT11_FRAME_BIT_COUNT            (40U)
/** @brief Cơ sở đổi giữa giây và microsecond; không phải clock bị hard-code của timer. */
#define DHT11_MICROSECOND_BASE_HZ        (1000000UL)

/**
 * @brief Context nội bộ của một cảm biến DHT11 duy nhất.
 */
typedef struct
{
    DHT11_Config_t config;

    uint32_t capture_interrupt;
    uint32_t capture_flag;
    uint32_t overcapture_flag;
    HAL_TIM_ActiveChannel active_channel;

    volatile DHT11_State_t state;
    volatile DHT11_Error_t last_error;
    volatile uint32_t previous_capture;
    volatile uint8_t received_frame[DHT11_RECEIVED_FRAME_BYTE_COUNT];
    volatile uint8_t received_bit_count;

    DHT11_Data_t latest_valid_data;
    uint32_t timer_period_counts;
    uint32_t ticks_per_microsecond;
    uint32_t last_attempt_tick_ms;
    uint32_t capture_start_tick_ms;
    uint32_t last_success_tick_ms;
    uint16_t consecutive_error_count;
    bool has_latest_valid_data;
    bool has_unread_data;
} DHT11_InternalContext_t;

static DHT11_InternalContext_t s_dht11_context;

static uint32_t dht11_enter_critical(void);
static void dht11_exit_critical(uint32_t previous_primask);
static bool dht11_has_elapsed(uint32_t now, uint32_t start, uint32_t duration);
static bool dht11_is_single_pin(uint16_t pin);
static bool dht11_configure_channel_metadata(void);
static bool dht11_is_capture_channel_configured(void);
static void dht11_configure_data_pin_as_input(void);
static void dht11_configure_data_pin_as_open_drain_output(void);
static void dht11_clear_capture_flags(void);
static void dht11_disable_capture_interrupt(void);
static void dht11_increment_error_count(void);
static void dht11_finish_transaction_with_error(DHT11_Error_t error);
static void dht11_set_error_from_interrupt(DHT11_Error_t error);
static uint32_t dht11_get_timer_input_clock_hz(void);
static bool dht11_configure_timing_conversion(void);
static uint32_t dht11_microseconds_to_ticks(uint32_t microseconds);
static uint32_t dht11_capture_delta(uint32_t previous, uint32_t current);
static void dht11_begin_transaction(uint32_t current_tick_ms);
static void dht11_release_bus_and_arm_capture(uint32_t current_tick_ms);
static void dht11_check_capture_timeout(uint32_t current_tick_ms);
static void dht11_process_completed_frame(uint32_t current_tick_ms);
static void dht11_copy_latest_data(DHT11_Data_t *output_data);

/**
 * @brief Lưu PRIMASK và tạm khóa ngắt toàn cục cho một critical section ngắn.
 *
 * @return Giá trị PRIMASK trước khi khóa ngắt.
 */
static uint32_t dht11_enter_critical(void)
{
    uint32_t previous_primask = __get_PRIMASK();
    __disable_irq();
    return previous_primask;
}

/**
 * @brief Khôi phục trạng thái ngắt toàn cục trước critical section.
 *
 * @param previous_primask Giá trị do dht11_enter_critical() trả về.
 */
static void dht11_exit_critical(uint32_t previous_primask)
{
    if ((previous_primask & 1UL) == 0UL)
    {
        __enable_irq();
    }
}

/**
 * @brief Kiểm tra thời gian đã trôi qua bằng phép trừ an toàn khi HAL tick tràn.
 */
static bool dht11_has_elapsed(uint32_t now, uint32_t start, uint32_t duration)
{
    return ((uint32_t)(now - start) >= duration);
}

/** @brief Kiểm tra bitmask có chứa đúng một GPIO pin hay không. */
static bool dht11_is_single_pin(uint16_t pin)
{
    return ((pin != 0U) && ((pin & (uint16_t)(pin - 1U)) == 0U));
}

/** @brief Ánh xạ timer channel sang cờ, nguồn interrupt và active channel của HAL. */
static bool dht11_configure_channel_metadata(void)
{
    switch (s_dht11_context.config.timer_channel)
    {
        case TIM_CHANNEL_1:
            s_dht11_context.capture_interrupt = TIM_IT_CC1;
            s_dht11_context.capture_flag = TIM_FLAG_CC1;
            s_dht11_context.overcapture_flag = TIM_FLAG_CC1OF;
            s_dht11_context.active_channel = HAL_TIM_ACTIVE_CHANNEL_1;
            return true;

        case TIM_CHANNEL_2:
            s_dht11_context.capture_interrupt = TIM_IT_CC2;
            s_dht11_context.capture_flag = TIM_FLAG_CC2;
            s_dht11_context.overcapture_flag = TIM_FLAG_CC2OF;
            s_dht11_context.active_channel = HAL_TIM_ACTIVE_CHANNEL_2;
            return true;

        case TIM_CHANNEL_3:
            s_dht11_context.capture_interrupt = TIM_IT_CC3;
            s_dht11_context.capture_flag = TIM_FLAG_CC3;
            s_dht11_context.overcapture_flag = TIM_FLAG_CC3OF;
            s_dht11_context.active_channel = HAL_TIM_ACTIVE_CHANNEL_3;
            return true;

        case TIM_CHANNEL_4:
            s_dht11_context.capture_interrupt = TIM_IT_CC4;
            s_dht11_context.capture_flag = TIM_FLAG_CC4;
            s_dht11_context.overcapture_flag = TIM_FLAG_CC4OF;
            s_dht11_context.active_channel = HAL_TIM_ACTIVE_CHANNEL_4;
            return true;

        default:
            return false;
    }
}

/** @brief Xác nhận channel đã được cấu hình input capture cạnh xuống. */
static bool dht11_is_capture_channel_configured(void)
{
    TIM_TypeDef *timer_instance = s_dht11_context.config.timer->Instance;

    switch (s_dht11_context.config.timer_channel)
    {
        case TIM_CHANNEL_1:
            return (((timer_instance->CCMR1 & TIM_CCMR1_CC1S) != 0UL) &&
                    ((timer_instance->CCER & TIM_CCER_CC1P) != 0UL));

        case TIM_CHANNEL_2:
            return (((timer_instance->CCMR1 & TIM_CCMR1_CC2S) != 0UL) &&
                    ((timer_instance->CCER & TIM_CCER_CC2P) != 0UL));

        case TIM_CHANNEL_3:
            return (((timer_instance->CCMR2 & TIM_CCMR2_CC3S) != 0UL) &&
                    ((timer_instance->CCER & TIM_CCER_CC3P) != 0UL));

        case TIM_CHANNEL_4:
            return (((timer_instance->CCMR2 & TIM_CCMR2_CC4S) != 0UL) &&
                    ((timer_instance->CCER & TIM_CCER_CC4P) != 0UL));

        default:
            return false;
    }
}

/**
 * @brief Chuyển chân DATA về input để MCU nhả single-wire bus.
 */
static void dht11_configure_data_pin_as_input(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    gpio_init.Pin = s_dht11_context.config.data_pin;
    gpio_init.Mode = GPIO_MODE_INPUT;
    gpio_init.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(s_dht11_context.config.data_port, &gpio_init);
}

/**
 * @brief Chuyển chân DATA thành output open-drain để phát start signal.
 */
static void dht11_configure_data_pin_as_open_drain_output(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    gpio_init.Pin = s_dht11_context.config.data_pin;
    gpio_init.Mode = GPIO_MODE_OUTPUT_OD;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(s_dht11_context.config.data_port, &gpio_init);
}

/**
 * @brief Xóa cờ capture và overcapture cũ của channel đã cấu hình.
 */
static void dht11_clear_capture_flags(void)
{
    __HAL_TIM_CLEAR_FLAG(s_dht11_context.config.timer,
                         s_dht11_context.capture_flag);
    __HAL_TIM_CLEAR_FLAG(s_dht11_context.config.timer,
                         s_dht11_context.overcapture_flag);
}

/**
 * @brief Tắt riêng interrupt của capture channel nhưng giữ timer tiếp tục chạy.
 */
static void dht11_disable_capture_interrupt(void)
{
    __HAL_TIM_DISABLE_IT(s_dht11_context.config.timer,
                         s_dht11_context.capture_interrupt);
}

/**
 * @brief Tăng bộ đếm lỗi liên tiếp và bão hòa tại UINT16_MAX.
 */
static void dht11_increment_error_count(void)
{
    if (s_dht11_context.consecutive_error_count < UINT16_MAX)
    {
        s_dht11_context.consecutive_error_count++;
    }
}

/**
 * @brief Kết thúc một giao dịch lỗi trong main context và trả module về chờ.
 */
static void dht11_finish_transaction_with_error(DHT11_Error_t error)
{
    uint32_t previous_primask = dht11_enter_critical();

    dht11_disable_capture_interrupt();
    dht11_clear_capture_flags();
    s_dht11_context.last_error = error;
    s_dht11_context.state = DHT11_STATE_WAIT_PERIOD;

    dht11_exit_critical(previous_primask);

    dht11_configure_data_pin_as_input();
    dht11_increment_error_count();
}

/**
 * @brief Báo lỗi từ ISR và giao phần phục hồi còn lại cho main context.
 */
static void dht11_set_error_from_interrupt(DHT11_Error_t error)
{
    dht11_disable_capture_interrupt();
    s_dht11_context.last_error = error;
    s_dht11_context.state = DHT11_STATE_ERROR;
}

/**
 * @brief Tính clock đầu vào timer từ bus APB tương ứng trên STM32F103.
 */
static uint32_t dht11_get_timer_input_clock_hz(void)
{
    uint32_t timer_clock_hz;
    TIM_TypeDef *timer_instance = s_dht11_context.config.timer->Instance;

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

/**
 * @brief Kiểm tra timer và chuẩn bị hệ số đổi microsecond sang counter tick.
 */
static bool dht11_configure_timing_conversion(void)
{
    uint32_t timer_clock_hz = dht11_get_timer_input_clock_hz();
    uint32_t prescaler_divider =
        s_dht11_context.config.timer->Init.Prescaler + 1UL;
    uint32_t counter_clock_hz;
    uint32_t maximum_interval_ticks;

    if ((prescaler_divider == 0UL) ||
        ((timer_clock_hz % prescaler_divider) != 0UL))
    {
        return false;
    }

    counter_clock_hz = timer_clock_hz / prescaler_divider;
    if ((counter_clock_hz < DHT11_MICROSECOND_BASE_HZ) ||
        ((counter_clock_hz % DHT11_MICROSECOND_BASE_HZ) != 0UL))
    {
        return false;
    }

    s_dht11_context.ticks_per_microsecond =
        counter_clock_hz / DHT11_MICROSECOND_BASE_HZ;
    s_dht11_context.timer_period_counts =
        s_dht11_context.config.timer->Init.Period + 1UL;
    maximum_interval_ticks = DHT11_ACK_MAX_US * s_dht11_context.ticks_per_microsecond;

    return (s_dht11_context.timer_period_counts > maximum_interval_ticks);
}

/**
 * @brief Đổi một khoảng microsecond đã xác nhận sang counter tick.
 */
static uint32_t dht11_microseconds_to_ticks(uint32_t microseconds)
{
    return (microseconds * s_dht11_context.ticks_per_microsecond);
}

/**
 * @brief Tính khoảng cách giữa hai capture, có xử lý một lần tràn ARR.
 */
static uint32_t dht11_capture_delta(uint32_t previous, uint32_t current)
{
    if (current >= previous)
    {
        return (current - previous);
    }

    return ((s_dht11_context.timer_period_counts - previous) + current);
}

/**
 * @brief Bắt đầu một chu kỳ đọc bằng cách kéo DATA xuống thấp.
 */
static void dht11_begin_transaction(uint32_t current_tick_ms)
{
    uint8_t index;

    s_dht11_context.last_attempt_tick_ms = current_tick_ms;

    if (HAL_GPIO_ReadPin(s_dht11_context.config.data_port,
                        s_dht11_context.config.data_pin) != GPIO_PIN_SET)
    {
        dht11_finish_transaction_with_error(DHT11_ERROR_BUS_STUCK_LOW);
        return;
    }

    dht11_disable_capture_interrupt();
    dht11_clear_capture_flags();

    for (index = 0U; index < DHT11_RECEIVED_FRAME_BYTE_COUNT; index++)
    {
        s_dht11_context.received_frame[index] = 0U;
    }
    s_dht11_context.received_bit_count = 0U;

    HAL_GPIO_WritePin(s_dht11_context.config.data_port,
                      s_dht11_context.config.data_pin,
                      GPIO_PIN_RESET);
    dht11_configure_data_pin_as_open_drain_output();

    if (HAL_GPIO_ReadPin(s_dht11_context.config.data_port,
                        s_dht11_context.config.data_pin) != GPIO_PIN_RESET)
    {
        dht11_finish_transaction_with_error(DHT11_ERROR_START_DRIVE);
        return;
    }

    s_dht11_context.state = DHT11_STATE_START_LOW;
}

/**
 * @brief Nhả bus sau start signal và bật nhận cạnh xuống ACK.
 */
static void dht11_release_bus_and_arm_capture(uint32_t current_tick_ms)
{
    uint32_t previous_primask = dht11_enter_critical();

    dht11_clear_capture_flags();
    s_dht11_context.capture_start_tick_ms = current_tick_ms;
    s_dht11_context.state = DHT11_STATE_WAIT_ACK_FIRST_EDGE;
    __HAL_TIM_ENABLE_IT(s_dht11_context.config.timer,
                        s_dht11_context.capture_interrupt);

    dht11_configure_data_pin_as_input();

    dht11_exit_critical(previous_primask);
}

/**
 * @brief Kiểm tra timeout và chuyển lỗi một cách nguyên tử với ISR capture.
 */
static void dht11_check_capture_timeout(uint32_t current_tick_ms)
{
    DHT11_State_t state;
    DHT11_Error_t error = DHT11_ERROR_NONE;
    uint32_t previous_primask;

    if (!dht11_has_elapsed(current_tick_ms,
                          s_dht11_context.capture_start_tick_ms,
                          DHT11_FRAME_TIMEOUT_MS))
    {
        return;
    }

    previous_primask = dht11_enter_critical();
    state = s_dht11_context.state;

    if ((state == DHT11_STATE_WAIT_ACK_FIRST_EDGE) ||
        (state == DHT11_STATE_WAIT_ACK_SECOND_EDGE))
    {
        error = DHT11_ERROR_ACK_TIMEOUT;
    }
    else if (state == DHT11_STATE_RECEIVING_BITS)
    {
        error = DHT11_ERROR_FRAME_TIMEOUT;
    }

    if (error != DHT11_ERROR_NONE)
    {
        dht11_disable_capture_interrupt();
        s_dht11_context.last_error = error;
        s_dht11_context.state = DHT11_STATE_ERROR;
    }

    dht11_exit_critical(previous_primask);
}

/**
 * @brief Kiểm tra checksum và xuất bản một khung đã nhận đủ 40 bit.
 */
static void dht11_process_completed_frame(uint32_t current_tick_ms)
{
    uint8_t calculated_checksum = (uint8_t)(s_dht11_context.received_frame[0] +
                                            s_dht11_context.received_frame[1] +
                                            s_dht11_context.received_frame[2] +
                                            s_dht11_context.received_frame[3]);

    if (calculated_checksum != s_dht11_context.received_frame[4])
    {
        dht11_finish_transaction_with_error(DHT11_ERROR_CHECKSUM);
        return;
    }

    s_dht11_context.latest_valid_data.humidity_integer = s_dht11_context.received_frame[0];
    s_dht11_context.latest_valid_data.humidity_decimal = s_dht11_context.received_frame[1];
    s_dht11_context.latest_valid_data.temperature_integer = s_dht11_context.received_frame[2];
    s_dht11_context.latest_valid_data.temperature_decimal = s_dht11_context.received_frame[3];

    s_dht11_context.last_success_tick_ms = current_tick_ms;
    s_dht11_context.consecutive_error_count = 0U;
    s_dht11_context.last_error = DHT11_ERROR_NONE;
    s_dht11_context.has_latest_valid_data = true;
    s_dht11_context.has_unread_data = true;
    s_dht11_context.state = DHT11_STATE_WAIT_PERIOD;
}

/**
 * @brief Sao chép bốn byte dữ liệu hợp lệ sang vùng nhớ người gọi.
 */
static void dht11_copy_latest_data(DHT11_Data_t *output_data)
{
    output_data->humidity_integer = s_dht11_context.latest_valid_data.humidity_integer;
    output_data->humidity_decimal = s_dht11_context.latest_valid_data.humidity_decimal;
    output_data->temperature_integer = s_dht11_context.latest_valid_data.temperature_integer;
    output_data->temperature_decimal = s_dht11_context.latest_valid_data.temperature_decimal;
}

DHT11_Result_t DHT11_Initialize(const DHT11_Config_t *config,
                                uint32_t current_tick_ms)
{
    HAL_StatusTypeDef hal_status;
    uint32_t previous_primask;

    s_dht11_context = (DHT11_InternalContext_t){0};
    s_dht11_context.state = DHT11_STATE_UNINITIALIZED;
    s_dht11_context.last_error = DHT11_ERROR_INVALID_CONFIG;

    if ((config == NULL) ||
        (config->timer == NULL) ||
        (config->timer_instance == NULL) ||
        (config->data_port == NULL) ||
        !dht11_is_single_pin(config->data_pin))
    {
        return DHT11_RESULT_INVALID_ARGUMENT;
    }

    s_dht11_context.config = *config;

    if ((config->timer->Instance != config->timer_instance) ||
        !dht11_configure_channel_metadata() ||
        !dht11_is_capture_channel_configured() ||
        !dht11_configure_timing_conversion())
    {
        return DHT11_RESULT_INVALID_CONFIG;
    }

    previous_primask = dht11_enter_critical();
    dht11_clear_capture_flags();
    hal_status = HAL_TIM_IC_Start_IT(config->timer, config->timer_channel);
    dht11_disable_capture_interrupt();
    dht11_clear_capture_flags();
    dht11_exit_critical(previous_primask);

    if (hal_status != HAL_OK)
    {
        s_dht11_context.last_error = DHT11_ERROR_TIMER_START;
        return DHT11_RESULT_TIMER_START_FAILED;
    }

    dht11_configure_data_pin_as_input();
    s_dht11_context.last_attempt_tick_ms = current_tick_ms;
    s_dht11_context.last_error = DHT11_ERROR_NONE;
    s_dht11_context.state = DHT11_STATE_WAIT_PERIOD;

    return DHT11_RESULT_OK;
}

void DHT11_Service(uint32_t current_tick_ms)
{
    DHT11_State_t state = s_dht11_context.state;

    switch (state)
    {
        case DHT11_STATE_UNINITIALIZED:
            break;

        case DHT11_STATE_WAIT_PERIOD:
            if (dht11_has_elapsed(current_tick_ms,
                                 s_dht11_context.last_attempt_tick_ms,
                                 DHT11_READ_PERIOD_MS))
            {
                dht11_begin_transaction(current_tick_ms);
            }
            break;

        case DHT11_STATE_START_LOW:
            if (dht11_has_elapsed(current_tick_ms,
                                 s_dht11_context.last_attempt_tick_ms,
                                 DHT11_START_LOW_TIME_MS))
            {
                dht11_release_bus_and_arm_capture(current_tick_ms);
            }
            break;

        case DHT11_STATE_WAIT_ACK_FIRST_EDGE:
        case DHT11_STATE_WAIT_ACK_SECOND_EDGE:
        case DHT11_STATE_RECEIVING_BITS:
            dht11_check_capture_timeout(current_tick_ms);
            break;

        case DHT11_STATE_FRAME_READY:
            dht11_process_completed_frame(current_tick_ms);
            break;

        case DHT11_STATE_ERROR:
            dht11_finish_transaction_with_error(s_dht11_context.last_error);
            break;

        default:
            dht11_finish_transaction_with_error(DHT11_ERROR_INTERNAL_STATE);
            break;
    }
}

void DHT11_HandleInputCaptureInterrupt(TIM_HandleTypeDef *timer)
{
    uint32_t current_capture;
    uint32_t delta_ticks;
    uint32_t ack_min_ticks;
    uint32_t ack_max_ticks;
    uint32_t bit_min_ticks;
    uint32_t bit_threshold_ticks;
    uint32_t bit_max_ticks;
    uint8_t byte_index;
    uint8_t bit_value;

    if ((timer == NULL) ||
        (timer != s_dht11_context.config.timer) ||
        (timer->Channel != s_dht11_context.active_channel))
    {
        return;
    }

    if (__HAL_TIM_GET_FLAG(timer, s_dht11_context.overcapture_flag))
    {
        __HAL_TIM_CLEAR_FLAG(timer, s_dht11_context.overcapture_flag);
        dht11_set_error_from_interrupt(DHT11_ERROR_CAPTURE_OVERRUN);
        return;
    }

    current_capture = HAL_TIM_ReadCapturedValue(
        timer,
        s_dht11_context.config.timer_channel);

    if (s_dht11_context.state == DHT11_STATE_WAIT_ACK_FIRST_EDGE)
    {
        s_dht11_context.previous_capture = current_capture;
        s_dht11_context.state = DHT11_STATE_WAIT_ACK_SECOND_EDGE;
        return;
    }

    delta_ticks = dht11_capture_delta(s_dht11_context.previous_capture, current_capture);
    s_dht11_context.previous_capture = current_capture;

    if (s_dht11_context.state == DHT11_STATE_WAIT_ACK_SECOND_EDGE)
    {
        ack_min_ticks = dht11_microseconds_to_ticks(DHT11_ACK_MIN_US);
        ack_max_ticks = dht11_microseconds_to_ticks(DHT11_ACK_MAX_US);

        if ((delta_ticks < ack_min_ticks) || (delta_ticks > ack_max_ticks))
        {
            dht11_set_error_from_interrupt(DHT11_ERROR_ACK_TIMING);
            return;
        }

        s_dht11_context.state = DHT11_STATE_RECEIVING_BITS;
        return;
    }

    if (s_dht11_context.state != DHT11_STATE_RECEIVING_BITS)
    {
        dht11_set_error_from_interrupt(DHT11_ERROR_INTERNAL_STATE);
        return;
    }

    bit_min_ticks = dht11_microseconds_to_ticks(DHT11_BIT_MIN_US);
    bit_threshold_ticks = dht11_microseconds_to_ticks(DHT11_BIT_THRESHOLD_US);
    bit_max_ticks = dht11_microseconds_to_ticks(DHT11_BIT_MAX_US);

    if ((delta_ticks < bit_min_ticks) || (delta_ticks > bit_max_ticks))
    {
        dht11_set_error_from_interrupt(DHT11_ERROR_BIT_TIMING);
        return;
    }

    if (s_dht11_context.received_bit_count >= DHT11_FRAME_BIT_COUNT)
    {
        dht11_set_error_from_interrupt(DHT11_ERROR_INTERNAL_STATE);
        return;
    }

    bit_value = (delta_ticks < bit_threshold_ticks) ? 0U : 1U;
    byte_index = s_dht11_context.received_bit_count / 8U;
    s_dht11_context.received_frame[byte_index] =
        (uint8_t)((s_dht11_context.received_frame[byte_index] << 1U) | bit_value);
    s_dht11_context.received_bit_count++;

    if (s_dht11_context.received_bit_count == DHT11_FRAME_BIT_COUNT)
    {
        dht11_disable_capture_interrupt();
        s_dht11_context.state = DHT11_STATE_FRAME_READY;
    }
}

bool DHT11_GetLatestData(DHT11_Data_t *output_data)
{
    uint32_t previous_primask;
    bool has_latest_valid_data;

    if (output_data == NULL)
    {
        return false;
    }

    previous_primask = dht11_enter_critical();
    has_latest_valid_data = s_dht11_context.has_latest_valid_data;
    if (has_latest_valid_data)
    {
        dht11_copy_latest_data(output_data);
    }
    dht11_exit_critical(previous_primask);

    return has_latest_valid_data;
}

bool DHT11_TakeNewData(DHT11_Data_t *output_data)
{
    uint32_t previous_primask;
    bool has_unread_data;

    if (output_data == NULL)
    {
        return false;
    }

    previous_primask = dht11_enter_critical();
    has_unread_data = s_dht11_context.has_latest_valid_data && s_dht11_context.has_unread_data;
    if (has_unread_data)
    {
        dht11_copy_latest_data(output_data);
        s_dht11_context.has_unread_data = false;
    }
    dht11_exit_critical(previous_primask);

    return has_unread_data;
}

void DHT11_GetStatus(DHT11_Status_t *output_status)
{
    uint32_t previous_primask;

    if (output_status == NULL)
    {
        return;
    }

    previous_primask = dht11_enter_critical();
    output_status->state = s_dht11_context.state;
    output_status->last_error = s_dht11_context.last_error;
    output_status->consecutive_error_count = s_dht11_context.consecutive_error_count;
    output_status->last_success_tick_ms = s_dht11_context.last_success_tick_ms;
    output_status->has_latest_valid_data = s_dht11_context.has_latest_valid_data;
    output_status->has_unread_data = s_dht11_context.has_unread_data;
    dht11_exit_critical(previous_primask);
}
