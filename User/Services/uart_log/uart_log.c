/**
 * @file uart_log.c
 * @brief Hiện thực logger format hữu hạn, không chờ UART truyền hoàn tất.
 */

#include "uart_log.h"

#include <limits.h>
#include <stddef.h>
#include <stdio.h>

/** @brief Tăng bộ đếm nhưng không cho phép wrap về 0. */
static void uart_log_increment_saturated(uint32_t *counter)
{
    if (*counter < UINT32_MAX)
    {
        (*counter)++;
    }
}

UartLog_Result_t UartLog_Initialize(UartLog_t *logger,
                                   const UartLog_Config_t *config)
{
    UartStream_Status_t stream_status = {0};

    if (logger == NULL)
    {
        return UART_LOG_RESULT_INVALID_ARGUMENT;
    }

    *logger = (UartLog_t){0};
    logger->status.last_result = UART_LOG_RESULT_INVALID_ARGUMENT;
    logger->status.last_stream_result = UART_STREAM_RESULT_NOT_INITIALIZED;

    if ((config == NULL) ||
        (config->stream == NULL) ||
        (config->format_buffer == NULL) ||
        (config->format_buffer_capacity < 2U))
    {
        return logger->status.last_result;
    }

    UartStream_GetStatus(config->stream, &stream_status);
    if (!stream_status.is_initialized)
    {
        logger->status.last_result = UART_LOG_RESULT_NOT_INITIALIZED;
        return logger->status.last_result;
    }

    logger->config = *config;
    logger->status.is_initialized = true;
    logger->status.last_result = UART_LOG_RESULT_OK;
    return logger->status.last_result;
}

UartLog_Result_t UartLog_Printf(UartLog_t *logger,
                               const char *format,
                               ...)
{
    UartLog_Result_t result;
    va_list arguments;

    va_start(arguments, format);
    result = UartLog_VPrintf(logger, format, arguments);
    va_end(arguments);
    return result;
}

UartLog_Result_t UartLog_VPrintf(UartLog_t *logger,
                                const char *format,
                                va_list arguments)
{
    int formatted_length;
    UartStream_Result_t stream_result;

    if ((logger == NULL) || (format == NULL))
    {
        return UART_LOG_RESULT_INVALID_ARGUMENT;
    }

    if (!logger->status.is_initialized)
    {
        logger->status.last_result = UART_LOG_RESULT_NOT_INITIALIZED;
        return logger->status.last_result;
    }

    formatted_length = vsnprintf(logger->config.format_buffer,
                                 logger->config.format_buffer_capacity,
                                 format,
                                 arguments);
    if (formatted_length < 0)
    {
        uart_log_increment_saturated(&logger->status.format_error_count);
        logger->status.last_result = UART_LOG_RESULT_FORMAT_ERROR;
        return logger->status.last_result;
    }

    if ((uint32_t)formatted_length >=
        (uint32_t)logger->config.format_buffer_capacity)
    {
        uart_log_increment_saturated(&logger->status.too_long_message_count);
        logger->status.last_result = UART_LOG_RESULT_TOO_LONG;
        return logger->status.last_result;
    }

    logger->status.last_message_length = (uint16_t)formatted_length;
    if (formatted_length == 0)
    {
        logger->status.last_result = UART_LOG_RESULT_OK;
        return logger->status.last_result;
    }

    stream_result = UartStream_Write(
        logger->config.stream,
        (const uint8_t *)logger->config.format_buffer,
        (uint16_t)formatted_length);
    logger->status.last_stream_result = stream_result;

    if (stream_result == UART_STREAM_RESULT_OK)
    {
        uart_log_increment_saturated(&logger->status.accepted_message_count);
        logger->status.last_result = UART_LOG_RESULT_OK;
    }
    else
    {
        uart_log_increment_saturated(
            &logger->status.tx_rejected_message_count);
        logger->status.last_result =
            (stream_result == UART_STREAM_RESULT_TX_QUEUE_FULL) ?
            UART_LOG_RESULT_TX_QUEUE_FULL : UART_LOG_RESULT_STREAM_ERROR;
    }

    return logger->status.last_result;
}

void UartLog_GetStatus(const UartLog_t *logger,
                       UartLog_Status_t *output_status)
{
    if ((logger != NULL) && (output_status != NULL))
    {
        *output_status = logger->status;
    }
}
