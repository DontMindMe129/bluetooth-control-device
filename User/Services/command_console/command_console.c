/**
 * @file command_console.c
 * @brief Parser dòng lệnh ASCII hữu hạn dùng '\n' làm ký tự kết thúc.
 */

#include "command_console.h"

#include <limits.h>
#include <stddef.h>

/** @brief Tăng bộ đếm nhưng không cho phép wrap về 0. */
static void command_console_increment_saturated(uint32_t *counter)
{
    if (*counter < UINT32_MAX)
    {
        (*counter)++;
    }
}

/** @brief So sánh elapsed time an toàn khi HAL tick tràn số. */
static bool command_console_time_has_elapsed(uint32_t current_tick_ms,
                                             uint32_t start_tick_ms,
                                             uint32_t duration_ms)
{
    return ((uint32_t)(current_tick_ms - start_tick_ms) >= duration_ms);
}

/** @brief Xóa dòng đang ghép nhưng không thay đổi các bộ đếm thống kê. */
static void command_console_reset_line(CommandConsole_t *console)
{
    console->line_length = 0U;
    console->discard_until_line_feed = false;
    console->discard_due_to_rx_overflow = false;
    console->status.has_partial_line = false;
    console->status.is_discarding_overlong_line = false;
    console->status.current_line_length = 0U;
    console->status.current_argument_count = 0U;
}

/** @brief Tạo event và dừng parser cho tới khi application lấy event. */
static void command_console_publish_event(
    CommandConsole_t *console,
    CommandConsole_EventType_t event_type,
    uint8_t argument_count)
{
    console->pending_event.type = event_type;
    console->pending_event.argument_count = argument_count;
    console->pending_event.arguments =
        (event_type == COMMAND_CONSOLE_EVENT_COMMAND_READY) ?
        (const char *const *)console->config.argument_vector : NULL;
    console->status.has_pending_event = true;
    console->status.current_argument_count = argument_count;
    console->status.last_event_type = event_type;
}

/** @brief Tách buffer tại chỗ theo space/tab; trả false nếu argv không đủ chỗ. */
static bool command_console_tokenize(CommandConsole_t *console,
                                     uint8_t *output_argument_count)
{
    char *cursor = console->config.line_buffer;
    uint8_t argument_count = 0U;

    while (*cursor != '\0')
    {
        while ((*cursor == ' ') || (*cursor == '\t'))
        {
            cursor++;
        }

        if (*cursor == '\0')
        {
            break;
        }

        if (argument_count >= console->config.argument_capacity)
        {
            *output_argument_count = 0U;
            return false;
        }

        console->config.argument_vector[argument_count] = cursor;
        argument_count++;

        while ((*cursor != '\0') &&
               (*cursor != ' ') &&
               (*cursor != '\t'))
        {
            cursor++;
        }

        if (*cursor != '\0')
        {
            *cursor = '\0';
            cursor++;
        }
    }

    *output_argument_count = argument_count;
    return true;
}

/** @brief Kết thúc dòng hiện tại khi nhận LF và tạo event phù hợp. */
static void command_console_finish_line(CommandConsole_t *console)
{
    uint8_t argument_count = 0U;

    if (console->discard_until_line_feed)
    {
        if (console->discard_due_to_rx_overflow)
        {
            command_console_increment_saturated(
                &console->status.rx_overflow_line_count);
            command_console_reset_line(console);
            command_console_publish_event(console,
                                          COMMAND_CONSOLE_EVENT_RX_OVERFLOW,
                                          0U);
            return;
        }

        command_console_increment_saturated(
            &console->status.too_long_line_count);
        command_console_reset_line(console);
        command_console_publish_event(console,
                                      COMMAND_CONSOLE_EVENT_LINE_TOO_LONG,
                                      0U);
        return;
    }

    console->config.line_buffer[console->line_length] = '\0';
    if (!command_console_tokenize(console, &argument_count))
    {
        command_console_increment_saturated(
            &console->status.too_many_arguments_count);
        command_console_reset_line(console);
        command_console_publish_event(
            console,
            COMMAND_CONSOLE_EVENT_TOO_MANY_ARGUMENTS,
            0U);
        return;
    }

    if (argument_count == 0U)
    {
        command_console_increment_saturated(&console->status.empty_line_count);
        command_console_reset_line(console);
        return;
    }

    command_console_increment_saturated(
        &console->status.completed_command_count);
    console->line_length = 0U;
    console->discard_until_line_feed = false;
    console->status.has_partial_line = false;
    console->status.is_discarding_overlong_line = false;
    console->status.current_line_length = 0U;
    command_console_publish_event(console,
                                  COMMAND_CONSOLE_EVENT_COMMAND_READY,
                                  argument_count);
}

CommandConsole_Result_t CommandConsole_Initialize(
    CommandConsole_t *console,
    const CommandConsole_Config_t *config)
{
    UartStream_Status_t stream_status = {0};

    if (console == NULL)
    {
        return COMMAND_CONSOLE_RESULT_INVALID_ARGUMENT;
    }

    *console = (CommandConsole_t){0};
    console->status.initialize_result =
        COMMAND_CONSOLE_RESULT_INVALID_ARGUMENT;

    if ((config == NULL) ||
        (config->stream == NULL) ||
        (config->line_buffer == NULL) ||
        (config->line_buffer_capacity < 2U) ||
        (config->argument_vector == NULL) ||
        (config->argument_capacity == 0U) ||
        (config->max_bytes_per_service == 0U) ||
        (config->incomplete_timeout_ms == 0UL))
    {
        return console->status.initialize_result;
    }

    UartStream_GetStatus(config->stream, &stream_status);
    if (!stream_status.is_initialized)
    {
        console->status.initialize_result =
            COMMAND_CONSOLE_RESULT_STREAM_NOT_READY;
        return console->status.initialize_result;
    }

    console->config = *config;
    console->status.is_initialized = true;
    console->status.initialize_result = COMMAND_CONSOLE_RESULT_OK;
    return console->status.initialize_result;
}

void CommandConsole_Service(CommandConsole_t *console,
                            uint32_t current_tick_ms)
{
    UartStream_Status_t stream_status = {0};
    uint16_t processed_this_service = 0U;
    uint8_t received_byte;

    if ((console == NULL) ||
        !console->status.is_initialized ||
        console->status.has_pending_event)
    {
        return;
    }

    UartStream_GetStatus(console->config.stream, &stream_status);
    if (stream_status.rx_overflow_count !=
        console->observed_rx_overflow_count)
    {
        console->observed_rx_overflow_count =
            stream_status.rx_overflow_count;
        console->discard_until_line_feed = true;
        console->discard_due_to_rx_overflow = true;
        console->status.has_partial_line = true;
        console->status.is_discarding_overlong_line = false;
        console->status.last_input_tick_ms = current_tick_ms;
    }

    while ((processed_this_service < console->config.max_bytes_per_service) &&
           (UartStream_Read(console->config.stream, &received_byte, 1U) == 1U))
    {
        processed_this_service++;
        command_console_increment_saturated(
            &console->status.processed_byte_count);

        if (received_byte == '\n')
        {
            command_console_finish_line(console);
            if (console->status.has_pending_event)
            {
                return;
            }
            continue;
        }

        /* CR bị bỏ qua để cùng hỗ trợ cả LF và CRLF. */
        if (received_byte == '\r')
        {
            if (console->status.has_partial_line)
            {
                console->status.last_input_tick_ms = current_tick_ms;
            }
            continue;
        }

        console->status.last_input_tick_ms = current_tick_ms;
        console->status.has_partial_line = true;

        if (console->discard_until_line_feed)
        {
            continue;
        }

        if (console->line_length >=
            (uint16_t)(console->config.line_buffer_capacity - 1U))
        {
            console->discard_until_line_feed = true;
            console->status.is_discarding_overlong_line = true;
            continue;
        }

        console->config.line_buffer[console->line_length] =
            (char)received_byte;
        console->line_length++;
        console->status.current_line_length = console->line_length;
    }

    /* Chỉ xét timeout khi RX ring không còn byte được xử lý trong vòng này. */
    if ((processed_this_service == 0U) &&
        console->status.has_partial_line &&
        command_console_time_has_elapsed(
            current_tick_ms,
            console->status.last_input_tick_ms,
            console->config.incomplete_timeout_ms))
    {
        if (console->discard_until_line_feed)
        {
            command_console_finish_line(console);
            return;
        }

        command_console_increment_saturated(
            &console->status.input_timeout_count);
        command_console_reset_line(console);
        command_console_publish_event(console,
                                      COMMAND_CONSOLE_EVENT_INPUT_TIMEOUT,
                                      0U);
    }
}

bool CommandConsole_TakeEvent(CommandConsole_t *console,
                              CommandConsole_Event_t *output_event)
{
    if ((console == NULL) ||
        (output_event == NULL) ||
        !console->status.has_pending_event)
    {
        return false;
    }

    *output_event = console->pending_event;
    console->pending_event = (CommandConsole_Event_t){0};
    console->status.has_pending_event = false;
    return true;
}

void CommandConsole_GetStatus(const CommandConsole_t *console,
                              CommandConsole_Status_t *output_status)
{
    if ((console != NULL) && (output_status != NULL))
    {
        *output_status = console->status;
    }
}
