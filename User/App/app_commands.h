/**
 * @file app_commands.h
 * @brief Menu và handler các command thuộc riêng ứng dụng hiện tại.
 */

#ifndef USER_APP_APP_COMMANDS_H_
#define USER_APP_APP_COMMANDS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "command_console.h"
#include "uart_log.h"

/** @brief Kết quả khởi tạo hoặc xử lý một event console. */
typedef enum
{
    APP_COMMANDS_RESULT_OK = 0,          /**< Event đã được xử lý và phản hồi đầy đủ. */
    APP_COMMANDS_RESULT_INVALID_ARGUMENT,/**< Context, logger hoặc event không hợp lệ. */
    APP_COMMANDS_RESULT_NOT_INITIALIZED, /**< Module chưa được khởi tạo. */
    APP_COMMANDS_RESULT_UNKNOWN_COMMAND, /**< Tên command không có trong menu. */
    APP_COMMANDS_RESULT_INVALID_ARGUMENT_COUNT, /**< argc không phù hợp với command. */
    APP_COMMANDS_RESULT_LOG_FAILED      /**< Không thể xếp đầy đủ phản hồi vào UART TX. */
} AppCommands_Result_t;

/** @brief Snapshot thống kê command dành cho debugger. */
typedef struct
{
    bool is_initialized;               /**< Module đã ghép với logger hợp lệ. */
    uint32_t received_event_count;      /**< Tổng event console đã nhận. */
    uint32_t executed_command_count;    /**< Số command đã tìm thấy và chạy handler. */
    uint32_t unknown_command_count;     /**< Số tên command không tồn tại. */
    uint32_t invalid_argument_count;    /**< Số command có argc không hợp lệ. */
    uint32_t rejected_input_count;      /**< Số dòng lỗi/timeout do console báo. */
    uint32_t log_failure_count;         /**< Số phản hồi không thể xếp trọn vào TX. */
    UartLog_Result_t last_log_result;   /**< Kết quả logger gần nhất. */
    AppCommands_Result_t last_result;   /**< Kết quả xử lý event gần nhất. */
} AppCommands_Status_t;

/** @brief Context app command; menu cụ thể được định nghĩa trong app_commands.c. */
typedef struct
{
    UartLog_t *logger;             /**< Logger dùng chung cho phản hồi command. */
    AppCommands_Status_t status;   /**< Trạng thái và thống kê. */
} AppCommands_t;

/** @brief Ghép module command của application với UART logger. */
AppCommands_Result_t AppCommands_Initialize(AppCommands_t *commands,
                                            UartLog_t *logger);

/** @brief Xử lý một event từ CommandConsole và phát phản hồi tương ứng. */
AppCommands_Result_t AppCommands_HandleConsoleEvent(
    AppCommands_t *commands,
    const CommandConsole_Event_t *event);

/** @brief Sao chép snapshot trạng thái command application. */
void AppCommands_GetStatus(const AppCommands_t *commands,
                           AppCommands_Status_t *output_status);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_APP_COMMANDS_H_ */
