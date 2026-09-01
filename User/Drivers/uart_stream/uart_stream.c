/**
 * @file uart_stream.c
 * @brief Hiện thực UART stream song công bằng HAL interrupt và ring buffer tĩnh.
 */

#include "uart_stream.h"

#include <limits.h>
#include <stddef.h>

/** @brief Khóa ngắt ngắn để main cập nhật dữ liệu dùng chung với callback ISR. */
static uint32_t uart_stream_enter_critical(void)
{
    uint32_t previous_primask = __get_PRIMASK();

    __disable_irq();
    return previous_primask;
}

/** @brief Khôi phục trạng thái mask ngắt trước critical section. */
static void uart_stream_exit_critical(uint32_t previous_primask)
{
    if (previous_primask == 0UL)
    {
        __enable_irq();
    }
}

/** @brief Tăng bộ đếm thống kê nhưng không cho phép wrap về 0. */
static void uart_stream_increment_saturated(volatile uint32_t *counter)
{
    if (*counter < UINT32_MAX)
    {
        (*counter)++;
    }
}

/** @brief Cộng thống kê có saturation tại UINT32_MAX. */
static void uart_stream_add_saturated(volatile uint32_t *counter,
                                      uint16_t amount)
{
    if (*counter <= (UINT32_MAX - (uint32_t)amount))
    {
        *counter += amount;
    }
    else
    {
        *counter = UINT32_MAX;
    }
}

/** @brief Xác nhận callback thuộc đúng UART instance mà driver sở hữu. */
static bool uart_stream_matches_uart(const UartStream_t *stream,
                                     const UART_HandleTypeDef *uart)
{
    return (stream != NULL) &&
           stream->status.is_initialized &&
           (uart != NULL) &&
           (stream->config.uart == uart) &&
           (uart->Instance == stream->config.uart_instance);
}

/**
 * @brief Bắt đầu truyền đoạn liên tục tại tx_tail; caller bảo đảm TX đang rảnh và queue không rỗng.
 */
static bool uart_stream_start_next_transmit(UartStream_t *stream)
{
    uint16_t contiguous_length;
    HAL_StatusTypeDef hal_status;

    contiguous_length = (uint16_t)(
        stream->config.tx_buffer_capacity - stream->tx_tail);
    if (contiguous_length > stream->tx_count)
    {
        contiguous_length = stream->tx_count;
    }

    stream->tx_active_length = contiguous_length;
    stream->status.tx_is_active = true;
    hal_status = HAL_UART_Transmit_IT(
        stream->config.uart,
        &stream->config.tx_buffer[stream->tx_tail],
        contiguous_length);
    stream->status.last_hal_status = hal_status;

    if (hal_status != HAL_OK)
    {
        stream->tx_active_length = 0U;
        stream->status.tx_is_active = false;
        stream->status.has_error = true;
        stream->status.last_result = UART_STREAM_RESULT_TX_START_FAILED;
        return false;
    }

    return true;
}

/** @brief Arm HAL nhận đúng một byte tiếp theo vào staging byte. */
static bool uart_stream_arm_receive(UartStream_t *stream,
                                    UartStream_Result_t failure_result)
{
    HAL_StatusTypeDef hal_status = HAL_UART_Receive_IT(
        stream->config.uart,
        &stream->rx_staging_byte,
        1U);

    stream->status.last_hal_status = hal_status;
    stream->status.rx_is_armed = (hal_status == HAL_OK);
    if (hal_status != HAL_OK)
    {
        stream->status.has_error = true;
        stream->status.last_result = failure_result;
        return false;
    }

    return true;
}

UartStream_Result_t UartStream_Initialize(
    UartStream_t *stream,
    const UartStream_Config_t *config)
{
    if (stream == NULL)
    {
        return UART_STREAM_RESULT_INVALID_ARGUMENT;
    }

    *stream = (UartStream_t){0};
    stream->status.last_hal_status = HAL_OK;
    stream->status.last_result = UART_STREAM_RESULT_INVALID_ARGUMENT;

    if ((config == NULL) ||
        (config->uart == NULL) ||
        (config->uart_instance == NULL) ||
        (config->rx_buffer == NULL) ||
        (config->tx_buffer == NULL) ||
        (config->rx_buffer_capacity == 0U) ||
        (config->tx_buffer_capacity == 0U))
    {
        return stream->status.last_result;
    }

    stream->config = *config;
    if ((config->uart->Instance != config->uart_instance) ||
        ((config->uart->Init.Mode & UART_MODE_TX_RX) != UART_MODE_TX_RX))
    {
        stream->status.last_result = UART_STREAM_RESULT_INVALID_CONFIG;
        return stream->status.last_result;
    }

    /* Cho phép callback nhận diện context nếu byte đến ngay khi HAL vừa arm RX. */
    stream->status.is_initialized = true;
    if (!uart_stream_arm_receive(stream,
                                 UART_STREAM_RESULT_RX_START_FAILED))
    {
        stream->status.is_initialized = false;
        return stream->status.last_result;
    }

    stream->status.last_result = UART_STREAM_RESULT_OK;
    return stream->status.last_result;
}

UartStream_Result_t UartStream_Write(
    UartStream_t *stream,
    const uint8_t *data,
    uint16_t data_length)
{
    uint32_t previous_primask;
    uint16_t index;
    uint16_t free_space;
    uint16_t write_head;

    if ((stream == NULL) || (data == NULL) || (data_length == 0U))
    {
        return UART_STREAM_RESULT_INVALID_ARGUMENT;
    }

    if (!stream->status.is_initialized)
    {
        stream->status.last_result = UART_STREAM_RESULT_NOT_INITIALIZED;
        return stream->status.last_result;
    }

    previous_primask = uart_stream_enter_critical();
    free_space = (uint16_t)(
        stream->config.tx_buffer_capacity - stream->tx_count);
    if (data_length > free_space)
    {
        uart_stream_increment_saturated(
            &stream->status.tx_queue_full_count);
        stream->status.last_result = UART_STREAM_RESULT_TX_QUEUE_FULL;
        uart_stream_exit_critical(previous_primask);
        return stream->status.last_result;
    }
    write_head = stream->tx_head;
    uart_stream_exit_critical(previous_primask);

    /*
     * ISR không đọc phần chưa được cộng vào tx_count. Vì API chỉ gọi từ main,
     * có thể sao chép ngoài critical section để không làm trễ các ngắt timing.
     */
    for (index = 0U; index < data_length; index++)
    {
        stream->config.tx_buffer[write_head] = data[index];
        write_head++;
        if (write_head >= stream->config.tx_buffer_capacity)
        {
            write_head = 0U;
        }
    }

    previous_primask = uart_stream_enter_critical();
    stream->tx_head = write_head;
    stream->tx_count = (uint16_t)(stream->tx_count + data_length);

    if (!stream->status.tx_is_active &&
        !uart_stream_start_next_transmit(stream))
    {
        uart_stream_exit_critical(previous_primask);
        return stream->status.last_result;
    }

    stream->status.last_result = UART_STREAM_RESULT_OK;
    uart_stream_exit_critical(previous_primask);
    return stream->status.last_result;
}

uint16_t UartStream_Read(UartStream_t *stream,
                         uint8_t *output,
                         uint16_t output_capacity)
{
    uint32_t previous_primask;
    uint16_t read_count;
    uint16_t index;
    uint16_t read_tail;

    if ((stream == NULL) ||
        (output == NULL) ||
        (output_capacity == 0U) ||
        !stream->status.is_initialized)
    {
        return 0U;
    }

    previous_primask = uart_stream_enter_critical();
    read_count = stream->rx_count;
    if (read_count > output_capacity)
    {
        read_count = output_capacity;
    }
    read_tail = stream->rx_tail;
    uart_stream_exit_critical(previous_primask);

    /* ISR chỉ ghi tại rx_head nên không thay đổi các byte đã snapshot ở rx_tail. */
    for (index = 0U; index < read_count; index++)
    {
        output[index] = stream->config.rx_buffer[read_tail];
        read_tail++;
        if (read_tail >= stream->config.rx_buffer_capacity)
        {
            read_tail = 0U;
        }
    }

    previous_primask = uart_stream_enter_critical();
    stream->rx_tail = read_tail;
    stream->rx_count = (uint16_t)(stream->rx_count - read_count);
    uart_stream_exit_critical(previous_primask);
    return read_count;
}

uint16_t UartStream_GetReceivedByteCount(const UartStream_t *stream)
{
    uint32_t previous_primask;
    uint16_t received_count;

    if ((stream == NULL) || !stream->status.is_initialized)
    {
        return 0U;
    }

    previous_primask = uart_stream_enter_critical();
    received_count = stream->rx_count;
    uart_stream_exit_critical(previous_primask);
    return received_count;
}

void UartStream_GetStatus(const UartStream_t *stream,
                          UartStream_Status_t *output_status)
{
    uint32_t previous_primask;

    if ((stream == NULL) || (output_status == NULL))
    {
        return;
    }

    previous_primask = uart_stream_enter_critical();
    *output_status = stream->status;
    output_status->rx_queued_byte_count = stream->rx_count;
    output_status->tx_queued_byte_count = stream->tx_count;
    uart_stream_exit_critical(previous_primask);
}

void UartStream_HandleReceiveCompleteInterrupt(
    UartStream_t *stream,
    UART_HandleTypeDef *uart)
{
    bool overflowed = false;

    if (!uart_stream_matches_uart(stream, uart))
    {
        return;
    }

    stream->status.rx_is_armed = false;
    uart_stream_increment_saturated(&stream->status.received_byte_count);

    if (stream->rx_count < stream->config.rx_buffer_capacity)
    {
        stream->config.rx_buffer[stream->rx_head] = stream->rx_staging_byte;
        stream->rx_head++;
        if (stream->rx_head >= stream->config.rx_buffer_capacity)
        {
            stream->rx_head = 0U;
        }
        stream->rx_count++;
    }
    else
    {
        uart_stream_increment_saturated(&stream->status.rx_overflow_count);
        stream->status.has_error = true;
        stream->status.last_result = UART_STREAM_RESULT_RX_OVERFLOW;
        overflowed = true;
    }

    if (uart_stream_arm_receive(stream,
                                UART_STREAM_RESULT_RX_RESTART_FAILED) &&
        !overflowed)
    {
        stream->status.last_result = UART_STREAM_RESULT_OK;
    }
}

void UartStream_HandleTransmitCompleteInterrupt(
    UartStream_t *stream,
    UART_HandleTypeDef *uart)
{
    uint16_t completed_length;

    if (!uart_stream_matches_uart(stream, uart) ||
        !stream->status.tx_is_active ||
        (stream->tx_active_length == 0U))
    {
        return;
    }

    completed_length = stream->tx_active_length;
    stream->tx_tail = (uint16_t)(stream->tx_tail + completed_length);
    if (stream->tx_tail >= stream->config.tx_buffer_capacity)
    {
        stream->tx_tail = 0U;
    }
    stream->tx_count = (uint16_t)(stream->tx_count - completed_length);
    stream->tx_active_length = 0U;
    stream->status.tx_is_active = false;
    uart_stream_add_saturated(&stream->status.transmitted_byte_count,
                              completed_length);

    if (stream->tx_count > 0U)
    {
        if (uart_stream_start_next_transmit(stream))
        {
            stream->status.last_result = UART_STREAM_RESULT_OK;
        }
    }
    else
    {
        stream->status.last_result = UART_STREAM_RESULT_OK;
    }
}

void UartStream_HandleErrorInterrupt(UartStream_t *stream,
                                     UART_HandleTypeDef *uart)
{
    if (!uart_stream_matches_uart(stream, uart))
    {
        return;
    }

    stream->status.last_hal_error = HAL_UART_GetError(uart);
    stream->status.has_error = true;
    stream->status.last_result = UART_STREAM_RESULT_UART_ERROR;
    uart_stream_increment_saturated(&stream->status.uart_error_count);

    /*
     * FE/NE/PE trong HAL hiện tại là recoverable và RX vẫn chạy. ORE kết thúc
     * RX, đưa RxState về READY; chỉ trường hợp đó mới arm lại đúng một lần.
     */
    if (uart->RxState == HAL_UART_STATE_READY)
    {
        stream->status.rx_is_armed = false;
        (void)uart_stream_arm_receive(
            stream,
            UART_STREAM_RESULT_RX_RESTART_FAILED);
    }
}
