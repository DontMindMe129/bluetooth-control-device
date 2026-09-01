/**
 * @file command_console.h
 * @brief Ghép byte UART thành dòng lệnh và tách nhiều đối số theo argc/argv.
 */

#ifndef USER_SERVICES_COMMAND_CONSOLE_COMMAND_CONSOLE_H_
#define USER_SERVICES_COMMAND_CONSOLE_COMMAND_CONSOLE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "uart_stream.h"

/** @brief Kết quả khởi tạo command console. */
typedef enum
{
    COMMAND_CONSOLE_RESULT_OK = 0,          /**< Console đã sẵn sàng. */
    COMMAND_CONSOLE_RESULT_INVALID_ARGUMENT,/**< Con trỏ hoặc giới hạn không hợp lệ. */
    COMMAND_CONSOLE_RESULT_STREAM_NOT_READY /**< UART stream chưa khởi tạo. */
} CommandConsole_Result_t;

/** @brief Loại event được chuyển từ parser sang application. */
typedef enum
{
    COMMAND_CONSOLE_EVENT_NONE = 0,        /**< Không có event chờ xử lý. */
    COMMAND_CONSOLE_EVENT_COMMAND_READY,   /**< Một dòng hợp lệ đã được tách thành argc/argv. */
    COMMAND_CONSOLE_EVENT_LINE_TOO_LONG,   /**< Dòng vượt giới hạn và đã bị hủy toàn bộ. */
    COMMAND_CONSOLE_EVENT_TOO_MANY_ARGUMENTS, /**< Số thành phần vượt mảng argv. */
    COMMAND_CONSOLE_EVENT_RX_OVERFLOW,     /**< RX ring đã mất byte nên dòng liên quan bị hủy. */
    COMMAND_CONSOLE_EVENT_INPUT_TIMEOUT    /**< Dòng nhập dở đã bị hủy vì timeout. */
} CommandConsole_EventType_t;

/** @brief Cấu hình bộ nhớ và giới hạn xử lý của console. */
typedef struct
{
    UartStream_t *stream;             /**< RX stream cung cấp byte đầu vào. */
    char *line_buffer;                /**< Buffer ghép dòng do caller sở hữu. */
    uint16_t line_buffer_capacity;    /**< Sức chứa gồm ký tự '\0'. */
    char **argument_vector;           /**< Mảng con trỏ argv do caller sở hữu. */
    uint8_t argument_capacity;        /**< Số thành phần argc tối đa. */
    uint16_t max_bytes_per_service;   /**< Số byte RX tối đa xử lý trong một vòng service. */
    uint32_t incomplete_timeout_ms;   /**< Thời gian hủy dòng không nhận thêm byte. */
} CommandConsole_Config_t;

/** @brief Event command; các con trỏ chỉ hợp lệ tới lần Service tiếp theo. */
typedef struct
{
    CommandConsole_EventType_t type; /**< Loại event. */
    uint8_t argument_count;          /**< argc, bằng 0 với event lỗi. */
    const char *const *arguments;    /**< argv, chỉ dùng khi COMMAND_READY. */
} CommandConsole_Event_t;

/** @brief Snapshot trạng thái parser phục vụ debugger. */
typedef struct
{
    bool is_initialized;                 /**< Console đã được cấu hình thành công. */
    bool has_partial_line;               /**< Đang giữ ít nhất một ký tự của dòng dở. */
    bool is_discarding_overlong_line;    /**< Đang bỏ byte cho tới LF vì dòng quá dài. */
    bool has_pending_event;              /**< App chưa lấy event gần nhất. */
    uint16_t current_line_length;         /**< Số ký tự đang giữ, không tính '\0'. */
    uint8_t current_argument_count;       /**< argc của command đang chờ xử lý. */
    uint32_t processed_byte_count;        /**< Tổng số byte đã lấy khỏi RX ring. */
    uint32_t completed_command_count;     /**< Số command hợp lệ đã tạo. */
    uint32_t empty_line_count;            /**< Số dòng rỗng đã bỏ qua. */
    uint32_t too_long_line_count;         /**< Số dòng bị hủy vì quá dài. */
    uint32_t too_many_arguments_count;    /**< Số dòng bị hủy vì quá nhiều thành phần. */
    uint32_t rx_overflow_line_count;       /**< Số dòng bị hủy vì RX ring đã mất byte. */
    uint32_t input_timeout_count;         /**< Số dòng nhập dở bị timeout. */
    uint32_t last_input_tick_ms;          /**< Tick xử lý byte gần nhất của dòng dở. */
    CommandConsole_EventType_t last_event_type; /**< Event gần nhất đã tạo. */
    CommandConsole_Result_t initialize_result;  /**< Kết quả khởi tạo. */
} CommandConsole_Status_t;

/** @brief Context command console; toàn bộ API Service/TakeEvent chạy ở main. */
typedef struct
{
    CommandConsole_Config_t config; /**< Bản sao cấu hình. */
    CommandConsole_Status_t status; /**< Trạng thái parser. */
    CommandConsole_Event_t pending_event; /**< Event chờ application lấy. */
    uint16_t line_length;           /**< Độ dài dòng nội bộ hiện tại. */
    bool discard_until_line_feed;   /**< Bỏ phần còn lại của dòng không còn đáng tin cậy. */
    bool discard_due_to_rx_overflow;/**< Phân biệt mất byte RX với dòng quá dài. */
    uint32_t observed_rx_overflow_count; /**< Bộ đếm overflow stream đã quan sát. */
} CommandConsole_t;

/** @brief Khởi tạo parser với buffer dòng và argv được cấp phát tĩnh. */
CommandConsole_Result_t CommandConsole_Initialize(
    CommandConsole_t *console,
    const CommandConsole_Config_t *config);

/**
 * @brief Lấy và xử lý hữu hạn byte RX, sau đó kiểm tra timeout dòng nhập dở.
 * @note Hàm dừng sớm khi tạo một event để buffer không bị ghi đè trước khi app xử lý.
 */
void CommandConsole_Service(CommandConsole_t *console,
                            uint32_t current_tick_ms);

/**
 * @brief Lấy event đang chờ; argv phải được dùng trước lần Service kế tiếp.
 * @return true nếu đã lấy được một event.
 */
bool CommandConsole_TakeEvent(CommandConsole_t *console,
                              CommandConsole_Event_t *output_event);

/** @brief Sao chép snapshot trạng thái hiện tại của console. */
void CommandConsole_GetStatus(const CommandConsole_t *console,
                              CommandConsole_Status_t *output_status);

#ifdef __cplusplus
}
#endif

#endif /* USER_SERVICES_COMMAND_CONSOLE_COMMAND_CONSOLE_H_ */
