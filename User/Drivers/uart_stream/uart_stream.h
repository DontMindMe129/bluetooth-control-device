/**
 * @file uart_stream.h
 * @brief Driver UART song công không blocking sử dụng ngắt và ring buffer tĩnh.
 *
 * Driver chỉ vận chuyển byte, không nhận biết command, ký tự kết thúc hay timeout.
 * Caller sở hữu toàn bộ vùng nhớ RX/TX và phải giữ chúng tồn tại trong suốt vòng
 * đời của driver. Không API nào chờ phần cứng truyền hoặc nhận hoàn tất.
 */

#ifndef USER_DRIVERS_UART_STREAM_UART_STREAM_H_
#define USER_DRIVERS_UART_STREAM_UART_STREAM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "stm32f1xx_hal.h"

/** @brief Kết quả của thao tác khởi tạo, đọc hoặc xếp hàng truyền UART. */
typedef enum
{
    UART_STREAM_RESULT_OK = 0,          /**< Thao tác hoàn thành hoặc được chấp nhận. */
    UART_STREAM_RESULT_INVALID_ARGUMENT,/**< Con trỏ, buffer hoặc độ dài không hợp lệ. */
    UART_STREAM_RESULT_INVALID_CONFIG,  /**< UART handle/instance/mode không phù hợp. */
    UART_STREAM_RESULT_NOT_INITIALIZED, /**< Driver chưa khởi tạo thành công. */
    UART_STREAM_RESULT_RX_START_FAILED, /**< HAL từ chối arm lần nhận byte đầu tiên. */
    UART_STREAM_RESULT_RX_RESTART_FAILED, /**< HAL từ chối arm lại RX sau callback/lỗi. */
    UART_STREAM_RESULT_RX_OVERFLOW,     /**< RX ring đầy nên byte mới nhất đã bị bỏ. */
    UART_STREAM_RESULT_TX_QUEUE_FULL,   /**< TX ring buffer không đủ chỗ cho toàn bộ dữ liệu. */
    UART_STREAM_RESULT_TX_START_FAILED, /**< HAL từ chối bắt đầu đoạn truyền kế tiếp. */
    UART_STREAM_RESULT_UART_ERROR       /**< HAL đã báo lỗi phần cứng UART. */
} UartStream_Result_t;

/** @brief Cấu hình và vùng nhớ tĩnh của một UART stream. */
typedef struct
{
    UART_HandleTypeDef *uart;       /**< HAL UART handle đã được CubeMX khởi tạo. */
    USART_TypeDef *uart_instance;   /**< Instance mong đợi để phát hiện ghép sai handle. */
    uint8_t *rx_buffer;             /**< Ring buffer nhận do caller sở hữu. */
    uint16_t rx_buffer_capacity;    /**< Số byte thực tế có thể chứa trong RX ring. */
    uint8_t *tx_buffer;             /**< Ring buffer truyền do caller sở hữu. */
    uint16_t tx_buffer_capacity;    /**< Số byte thực tế có thể chứa trong TX ring. */
} UartStream_Config_t;

/** @brief Snapshot công khai phục vụ application và debugger. */
typedef struct
{
    bool is_initialized;            /**< Driver đã arm RX byte đầu và sẵn sàng hoạt động. */
    bool rx_is_armed;               /**< HAL đang sở hữu vùng staging nhận một byte. */
    bool tx_is_active;              /**< HAL đang truyền một đoạn liên tục của TX ring. */
    bool has_error;                 /**< Đã ghi nhận lỗi chưa được reset bởi khởi tạo lại. */
    uint16_t rx_queued_byte_count;  /**< Số byte đang chờ tầng trên đọc. */
    uint16_t tx_queued_byte_count;  /**< Số byte đã xếp hàng nhưng chưa truyền xong. */
    uint32_t received_byte_count;   /**< Tổng số byte phần cứng đã giao cho callback RX. */
    uint32_t transmitted_byte_count;/**< Tổng số byte đã được callback TX xác nhận hoàn tất. */
    uint32_t rx_overflow_count;     /**< Số byte mới bị bỏ vì RX ring đã đầy. */
    uint32_t tx_queue_full_count;   /**< Số yêu cầu bị từ chối vì TX ring thiếu chỗ. */
    uint32_t uart_error_count;      /**< Số lần HAL_UART_ErrorCallback được chuyển tiếp. */
    uint32_t last_hal_error;        /**< Bitmask lỗi gần nhất từ HAL_UART_GetError(). */
    HAL_StatusTypeDef last_hal_status; /**< Kết quả HAL gần nhất khi arm RX/start TX. */
    UartStream_Result_t last_result; /**< Kết quả driver gần nhất. */
} UartStream_Status_t;

/** @brief Context đầy đủ của một UART stream; caller cấp phát tĩnh. */
typedef struct
{
    UartStream_Config_t config;     /**< Bản sao cấu hình và buffer thuộc caller. */
    volatile UartStream_Status_t status; /**< Trạng thái dùng chung, chỉ đọc nhất quán qua GetStatus. */
    volatile uint16_t rx_head;      /**< Vị trí ISR sẽ ghi byte RX kế tiếp. */
    volatile uint16_t rx_tail;      /**< Vị trí main sẽ đọc byte RX kế tiếp. */
    volatile uint16_t rx_count;     /**< Số byte hiện có trong RX ring. */
    volatile uint16_t tx_head;      /**< Vị trí main sẽ ghi byte TX kế tiếp. */
    volatile uint16_t tx_tail;      /**< Vị trí ISR sẽ giải phóng sau khi truyền xong. */
    volatile uint16_t tx_count;     /**< Số byte chưa được callback TX xác nhận. */
    volatile uint16_t tx_active_length; /**< Độ dài đoạn liên tục HAL đang truyền. */
    uint8_t rx_staging_byte;        /**< Byte duy nhất HAL đang nhận bằng interrupt. */
} UartStream_t;

/**
 * @brief Khởi tạo ring buffer và arm lần nhận một byte đầu tiên bằng interrupt.
 * @param stream Context do caller cấp phát tĩnh.
 * @param config UART handle/instance cùng hai vùng nhớ ring buffer tĩnh.
 * @return Kết quả kiểm tra cấu hình hoặc HAL_UART_Receive_IT().
 */
UartStream_Result_t UartStream_Initialize(
    UartStream_t *stream,
    const UartStream_Config_t *config);

/**
 * @brief Xếp toàn bộ dữ liệu vào TX ring và bắt đầu truyền bằng interrupt nếu đang rảnh.
 * @param stream Context đã khởi tạo.
 * @param data Dữ liệu caller muốn truyền; được sao chép vào ring trước khi trả về.
 * @param data_length Số byte phải xếp hàng nguyên vẹn.
 * @return OK khi nhận toàn bộ; TX_QUEUE_FULL khi không đủ chỗ, không nhận một phần.
 * @note Gọi từ main context; hàm không chờ byte được truyền ra chân TX.
 */
UartStream_Result_t UartStream_Write(
    UartStream_t *stream,
    const uint8_t *data,
    uint16_t data_length);

/**
 * @brief Lấy tối đa output_capacity byte đang chờ trong RX ring.
 * @param stream Context đã khởi tạo.
 * @param output Buffer nhận dữ liệu của caller.
 * @param output_capacity Số byte tối đa được phép ghi vào output.
 * @return Số byte thực tế đã lấy; 0 khi không có dữ liệu hoặc tham số sai.
 * @note Gọi từ main context.
 */
uint16_t UartStream_Read(UartStream_t *stream,
                         uint8_t *output,
                         uint16_t output_capacity);

/** @brief Trả về số byte RX đang chờ mà không lấy chúng khỏi ring buffer. */
uint16_t UartStream_GetReceivedByteCount(const UartStream_t *stream);

/** @brief Sao chép snapshot trạng thái nhất quán để quan sát hoặc debug. */
void UartStream_GetStatus(const UartStream_t *stream,
                          UartStream_Status_t *output_status);

/** @brief Chuyển tiếp HAL_UART_RxCpltCallback và arm ngay byte RX kế tiếp. */
void UartStream_HandleReceiveCompleteInterrupt(
    UartStream_t *stream,
    UART_HandleTypeDef *uart);

/** @brief Chuyển tiếp HAL_UART_TxCpltCallback và phát đoạn TX kế tiếp nếu có. */
void UartStream_HandleTransmitCompleteInterrupt(
    UartStream_t *stream,
    UART_HandleTypeDef *uart);

/**
 * @brief Chuyển tiếp HAL_UART_ErrorCallback và thử arm lại RX đúng một lần nếu HAL đã abort RX.
 */
void UartStream_HandleErrorInterrupt(UartStream_t *stream,
                                     UART_HandleTypeDef *uart);

#ifdef __cplusplus
}
#endif

#endif /* USER_DRIVERS_UART_STREAM_UART_STREAM_H_ */
