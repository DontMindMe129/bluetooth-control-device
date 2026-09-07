/**
 * @file app_commands.c
 * @brief Bảng command và handler UART cấp ứng dụng.
 */

#include "app_commands.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "app_config.h"

typedef AppCommands_Result_t (*AppCommand_Handler_t)(
    AppCommands_t *commands,
    uint8_t argument_count,
    const char *const arguments[],
    uint32_t current_tick_ms);

/** @brief Một mục menu gắn tên/usage/mô tả với handler application. */
typedef struct
{
    const char *name;
    const char *usage;
    const char *description;
    AppCommand_Handler_t handler;
} AppCommand_Descriptor_t;

static AppCommands_Result_t app_commands_handle_help(
    AppCommands_t *commands,
    uint8_t argument_count,
    const char *const arguments[],
    uint32_t current_tick_ms);
static AppCommands_Result_t app_commands_handle_echo(
    AppCommands_t *commands,
    uint8_t argument_count,
    const char *const arguments[],
    uint32_t current_tick_ms);
static AppCommands_Result_t app_commands_handle_monitor(
    AppCommands_t *commands,
    uint8_t argument_count,
    const char *const arguments[],
    uint32_t current_tick_ms);

/** @brief Menu command duy nhất của application; help được sinh trực tiếp từ bảng này. */
static const AppCommand_Descriptor_t s_command_menu[] =
{
    {
        .name = "help",
        .usage = "help",
        .description = "Show this command menu",
        .handler = app_commands_handle_help
    },
    {
        .name = "echo",
        .usage = "echo [arg ...]",
        .description = "Print all received arguments",
        .handler = app_commands_handle_echo
    },
    {
        .name = "monitor",
        .usage = "monitor <start|stop|status>",
        .description = "Control the monitoring session",
        .handler = app_commands_handle_monitor
    }
};

#define APP_COMMAND_MENU_COUNT \
    ((uint8_t)(sizeof(s_command_menu) / sizeof(s_command_menu[0])))

/** @brief Tăng bộ đếm nhưng không cho phép wrap về 0. */
static void app_commands_increment_saturated(uint32_t *counter)
{
    if (*counter < UINT32_MAX)
    {
        (*counter)++;
    }
}

/** @brief Ghi log và chuẩn hóa lỗi logger thành kết quả app command. */
static bool app_commands_log_succeeded(AppCommands_t *commands,
                                       UartLog_Result_t log_result)
{
    commands->status.last_log_result = log_result;
    if (log_result == UART_LOG_RESULT_OK)
    {
        return true;
    }

    app_commands_increment_saturated(&commands->status.log_failure_count);
    commands->status.last_result = APP_COMMANDS_RESULT_LOG_FAILED;
    return false;
}

static AppCommands_Result_t app_commands_handle_help(
    AppCommands_t *commands,
    uint8_t argument_count,
    const char *const arguments[],
    uint32_t current_tick_ms)
{
    uint8_t index;

    (void)arguments;
    (void)current_tick_ms;
    if (argument_count != 1U)
    {
        app_commands_increment_saturated(
            &commands->status.invalid_argument_count);
        commands->status.last_result =
            APP_COMMANDS_RESULT_INVALID_ARGUMENT_COUNT;
        (void)app_commands_log_succeeded(
            commands,
            UartLog_Printf(commands->logger, "ERR usage: help\r\n"));
        return commands->status.last_result;
    }

    if (!app_commands_log_succeeded(
            commands,
            UartLog_Printf(commands->logger, "Commands:\r\n")))
    {
        return commands->status.last_result;
    }

    for (index = 0U; index < APP_COMMAND_MENU_COUNT; index++)
    {
        if (!app_commands_log_succeeded(
                commands,
                UartLog_Printf(commands->logger,
                               "  %s - %s\r\n",
                               s_command_menu[index].usage,
                               s_command_menu[index].description)))
        {
            return commands->status.last_result;
        }
    }

    commands->status.last_result = APP_COMMANDS_RESULT_OK;
    return commands->status.last_result;
}

static AppCommands_Result_t app_commands_handle_echo(
    AppCommands_t *commands,
    uint8_t argument_count,
    const char *const arguments[],
    uint32_t current_tick_ms)
{
    uint8_t index;

    (void)current_tick_ms;

    if (!app_commands_log_succeeded(
            commands,
            UartLog_Printf(commands->logger, "echo:")))
    {
        return commands->status.last_result;
    }

    for (index = 1U; index < argument_count; index++)
    {
        if (!app_commands_log_succeeded(
                commands,
                UartLog_Printf(commands->logger, " %s", arguments[index])))
        {
            return commands->status.last_result;
        }
    }

    if (!app_commands_log_succeeded(
            commands,
            UartLog_Printf(commands->logger, "\r\n")))
    {
        return commands->status.last_result;
    }

    commands->status.last_result = APP_COMMANDS_RESULT_OK;
    return commands->status.last_result;
}

static AppCommands_Result_t app_commands_handle_monitor(
    AppCommands_t *commands,
    uint8_t argument_count,
    const char *const arguments[],
    uint32_t current_tick_ms)
{
    MonitoringSession_ChangeResult_t change_result =
        MONITORING_SESSION_CHANGE_UNCHANGED;
    MonitoringSession_Status_t session_status;

    if ((argument_count != 2U) ||
        ((strcmp(arguments[1], "start") != 0) &&
         (strcmp(arguments[1], "stop") != 0) &&
         (strcmp(arguments[1], "status") != 0)))
    {
        app_commands_increment_saturated(
            &commands->status.invalid_argument_count);
        commands->status.last_result =
            APP_COMMANDS_RESULT_INVALID_ARGUMENT_COUNT;
        (void)app_commands_log_succeeded(
            commands,
            UartLog_Printf(commands->logger,
                           "ERR usage: monitor <start|stop|status>\r\n"));
        return commands->status.last_result;
    }

    if (strcmp(arguments[1], "start") == 0)
    {
        change_result = MonitoringSession_SetActive(
            commands->monitoring_session,
            true,
            MONITORING_SESSION_SOURCE_REMOTE_COMMAND,
            current_tick_ms);
    }
    else if (strcmp(arguments[1], "stop") == 0)
    {
        change_result = MonitoringSession_SetActive(
            commands->monitoring_session,
            false,
            MONITORING_SESSION_SOURCE_REMOTE_COMMAND,
            current_tick_ms);
    }

    if (change_result == MONITORING_SESSION_CHANGE_INVALID)
    {
        commands->status.last_result = APP_COMMANDS_RESULT_NOT_INITIALIZED;
        return commands->status.last_result;
    }

    MonitoringSession_GetStatus(commands->monitoring_session,
                                &session_status);
    if (!app_commands_log_succeeded(
            commands,
            UartLog_Printf(commands->logger,
                           "MONITOR %s session=%u\r\n",
                           session_status.state == MONITORING_SESSION_ACTIVE
                               ? "ACTIVE"
                               : "INACTIVE",
                           (unsigned int)session_status.session_id)))
    {
        return commands->status.last_result;
    }

    commands->status.last_result = APP_COMMANDS_RESULT_OK;
    return commands->status.last_result;
}

AppCommands_Result_t AppCommands_Initialize(AppCommands_t *commands,
                                            UartLog_t *logger,
                                            MonitoringSession_t *monitoring_session)
{
    UartLog_Status_t logger_status = {0};

    if (commands == NULL)
    {
        return APP_COMMANDS_RESULT_INVALID_ARGUMENT;
    }

    *commands = (AppCommands_t){0};
    commands->status.last_result = APP_COMMANDS_RESULT_INVALID_ARGUMENT;
    commands->status.last_log_result = UART_LOG_RESULT_NOT_INITIALIZED;

    if ((logger == NULL) || (monitoring_session == NULL))
    {
        return commands->status.last_result;
    }

    UartLog_GetStatus(logger, &logger_status);
    if (!logger_status.is_initialized)
    {
        return commands->status.last_result;
    }

    commands->logger = logger;
    commands->monitoring_session = monitoring_session;
    commands->status.is_initialized = true;
    commands->status.last_result = APP_COMMANDS_RESULT_OK;
    return commands->status.last_result;
}

AppCommands_Result_t AppCommands_HandleConsoleEvent(
    AppCommands_t *commands,
    const CommandConsole_Event_t *event,
    uint32_t current_tick_ms)
{
    uint8_t index;

    if ((commands == NULL) || (event == NULL))
    {
        return APP_COMMANDS_RESULT_INVALID_ARGUMENT;
    }

    if (!commands->status.is_initialized)
    {
        commands->status.last_result = APP_COMMANDS_RESULT_NOT_INITIALIZED;
        return commands->status.last_result;
    }

    app_commands_increment_saturated(&commands->status.received_event_count);

    if (event->type == COMMAND_CONSOLE_EVENT_LINE_TOO_LONG)
    {
        app_commands_increment_saturated(&commands->status.rejected_input_count);
        commands->status.last_result = APP_COMMANDS_RESULT_OK;
        (void)app_commands_log_succeeded(
            commands,
            UartLog_Printf(commands->logger,
                           "ERR command exceeds %u characters\r\n",
                           APP_COMMAND_MAX_LENGTH));
        return commands->status.last_result;
    }

    if (event->type == COMMAND_CONSOLE_EVENT_TOO_MANY_ARGUMENTS)
    {
        app_commands_increment_saturated(&commands->status.rejected_input_count);
        commands->status.last_result = APP_COMMANDS_RESULT_OK;
        (void)app_commands_log_succeeded(
            commands,
            UartLog_Printf(commands->logger,
                           "ERR command exceeds %u arguments\r\n",
                           APP_COMMAND_MAX_ARGUMENTS));
        return commands->status.last_result;
    }

    if (event->type == COMMAND_CONSOLE_EVENT_INPUT_TIMEOUT)
    {
        app_commands_increment_saturated(&commands->status.rejected_input_count);
        commands->status.last_result = APP_COMMANDS_RESULT_OK;
        (void)app_commands_log_succeeded(
            commands,
            UartLog_Printf(commands->logger,
                           "ERR incomplete command timed out\r\n"));
        return commands->status.last_result;
    }

    if (event->type == COMMAND_CONSOLE_EVENT_RX_OVERFLOW)
    {
        app_commands_increment_saturated(&commands->status.rejected_input_count);
        commands->status.last_result = APP_COMMANDS_RESULT_OK;
        (void)app_commands_log_succeeded(
            commands,
            UartLog_Printf(commands->logger,
                           "ERR command discarded after RX overflow\r\n"));
        return commands->status.last_result;
    }

    if ((event->type != COMMAND_CONSOLE_EVENT_COMMAND_READY) ||
        (event->argument_count == 0U) ||
        (event->arguments == NULL) ||
        (event->arguments[0] == NULL))
    {
        commands->status.last_result = APP_COMMANDS_RESULT_INVALID_ARGUMENT;
        return commands->status.last_result;
    }

    for (index = 0U; index < APP_COMMAND_MENU_COUNT; index++)
    {
        if (strcmp(event->arguments[0], s_command_menu[index].name) == 0)
        {
            app_commands_increment_saturated(
                &commands->status.executed_command_count);
            return s_command_menu[index].handler(commands,
                                                  event->argument_count,
                                                  event->arguments,
                                                  current_tick_ms);
        }
    }

    app_commands_increment_saturated(&commands->status.unknown_command_count);
    commands->status.last_result = APP_COMMANDS_RESULT_UNKNOWN_COMMAND;
    (void)app_commands_log_succeeded(
        commands,
        UartLog_Printf(commands->logger,
                       "ERR unknown command: %s\r\n",
                       event->arguments[0]));
    return commands->status.last_result;
}

void AppCommands_GetStatus(const AppCommands_t *commands,
                           AppCommands_Status_t *output_status)
{
    if ((commands != NULL) && (output_status != NULL))
    {
        *output_status = commands->status;
    }
}
