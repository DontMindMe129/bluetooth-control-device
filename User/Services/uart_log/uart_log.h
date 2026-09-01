/**
 * @file uart_log.h
 * @brief Logger kiểu printf, định dạng trong buffer tĩnh rồi gửi qua UartStream.
 */

#ifndef USER_SERVICES_UART_LOG_UART_LOG_H_
#define USER_SERVICES_UART_LOG_UART_LOG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>

#include "uart_stream.h"

/** @brief Kết quả khởi tạo hoặc ghi một bản tin log. */
typedef enum
{
    UART_LOG_RESULT_OK = 0,          /**< Bản tin đã được nhận trọn vẹn vào TX ring. */
    UART_LOG_RESULT_INVALID_ARGUMENT,/**< Con trỏ hoặc kích thước cấu hình không hợp lệ. */
    UART_LOG_RESULT_NOT_INITIALIZED, /**< Logger chưa được khởi tạo thành công. */
    UART_LOG_RESULT_FORMAT_ERROR,    /**< vsnprintf không thể định dạng bản tin. */
    UART_LOG_RESULT_TOO_LONG,        /**< Bản tin vượt buffer; toàn bộ bản tin bị hủy. */
    UART_LOG_RESULT_TX_QUEUE_FULL,   /**< TX ring không đủ chỗ; toàn bộ bản tin bị hủy. */
    UART_LOG_RESULT_STREAM_ERROR     /**< UartStream từ chối bản tin vì lỗi khác. */
} UartLog_Result_t;

/** @brief Cấu hình vùng nhớ tĩnh và UART stream đích. */
typedef struct
{
    UartStream_t *stream;         /**< Stream đã khởi tạo dùng để phát log. */
    char *format_buffer;          /**< Buffer tạm do caller sở hữu. */
    uint16_t format_buffer_capacity; /**< Sức chứa gồm cả ký tự kết thúc chuỗi. */
} UartLog_Config_t;

/** @brief Snapshot phục vụ theo dõi logger trong debugger. */
typedef struct
{
    bool is_initialized;               /**< Cấu hình logger đã hợp lệ. */
    uint32_t accepted_message_count;    /**< Số bản tin đã đưa trọn vẹn vào TX ring. */
    uint32_t format_error_count;        /**< Số lần vsnprintf trả về lỗi. */
    uint32_t too_long_message_count;    /**< Số bản tin bị hủy vì quá dài. */
    uint32_t tx_rejected_message_count; /**< Số bản tin bị stream từ chối. */
    uint16_t last_message_length;       /**< Độ dài bản tin hợp lệ gần nhất. */
    UartStream_Result_t last_stream_result; /**< Kết quả UartStream gần nhất. */
    UartLog_Result_t last_result;       /**< Kết quả logger gần nhất. */
} UartLog_Status_t;

/** @brief Context logger; caller cấp phát tĩnh và chỉ sử dụng từ main context. */
typedef struct
{
    UartLog_Config_t config; /**< Bản sao cấu hình. */
    UartLog_Status_t status; /**< Trạng thái và thống kê. */
} UartLog_t;

/** @brief Khởi tạo logger với buffer format và stream do caller sở hữu. */
UartLog_Result_t UartLog_Initialize(UartLog_t *logger,
                                   const UartLog_Config_t *config);

/**
 * @brief Định dạng và xếp trọn bản tin vào TX ring, tương tự printf.
 * @note Chỉ gọi từ main context. Không hỗ trợ gửi một phần khi thiếu chỗ.
 */
UartLog_Result_t UartLog_Printf(UartLog_t *logger,
                               const char *format,
                               ...)
#if defined(__GNUC__)
                               __attribute__((format(printf, 2, 3)))
#endif
                               ;

/** @brief Phiên bản nhận va_list để các API khác có thể bọc logger. */
UartLog_Result_t UartLog_VPrintf(UartLog_t *logger,
                                const char *format,
                                va_list arguments);

/** @brief Sao chép snapshot trạng thái hiện tại của logger. */
void UartLog_GetStatus(const UartLog_t *logger,
                       UartLog_Status_t *output_status);

#ifdef __cplusplus
}
#endif

#endif /* USER_SERVICES_UART_LOG_UART_LOG_H_ */
