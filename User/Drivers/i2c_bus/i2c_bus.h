/**
 * @file i2c_bus.h
 * @brief Driver tổng quát quản lý giao dịch I2C master không blocking bằng ngắt.
 *
 * Driver chỉ quản lý bus, địa chỉ thiết bị và vùng byte cần truyền/nhận. Nội dung các
 * byte và ý nghĩa của chúng thuộc về device driver như SSD1306, EEPROM hoặc cảm biến.
 */

#ifndef USER_DRIVERS_I2C_BUS_I2C_BUS_H_
#define USER_DRIVERS_I2C_BUS_I2C_BUS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "stm32f1xx_hal.h"

/** @brief Địa chỉ 7-bit đầu tiên không thuộc vùng dành riêng được bộ quét kiểm tra. */
#define I2C_BUS_SCAN_FIRST_ADDRESS_7BIT    0x08U

/** @brief Địa chỉ 7-bit cuối cùng không thuộc vùng dành riêng được bộ quét kiểm tra. */
#define I2C_BUS_SCAN_LAST_ADDRESS_7BIT     0x77U

/** @brief Kết quả trả về ngay khi yêu cầu bắt đầu một thao tác trên bus. */
typedef enum
{
    I2C_BUS_START_ACCEPTED = 0,    /**< Yêu cầu đã được HAL chấp nhận và đang chạy. */
    I2C_BUS_START_INVALID_ARGUMENT,/**< Con trỏ, địa chỉ, độ dài hoặc timeout không hợp lệ. */
    I2C_BUS_START_NOT_INITIALIZED, /**< Context chưa được khởi tạo. */
    I2C_BUS_START_BUSY,            /**< Bus đang thực hiện một thao tác khác. */
    I2C_BUS_START_RESULT_PENDING,  /**< Caller chưa lấy kết quả của thao tác trước. */
    I2C_BUS_START_RECOVERY_REQUIRED, /**< Lỗi trước đó khiến bus cần được khởi tạo lại. */
    I2C_BUS_START_HAL_REJECTED     /**< HAL không thể bắt đầu thao tác. */
} I2cBus_StartResult_t;

/** @brief Loại thao tác mà driver đang chạy hoặc vừa hoàn thành. */
typedef enum
{
    I2C_BUS_OPERATION_NONE = 0, /**< Không có thao tác. */
    I2C_BUS_OPERATION_TRANSMIT, /**< Truyền một buffer tới một thiết bị. */
    I2C_BUS_OPERATION_MEMORY_READ, /**< Gửi địa chỉ vùng nhớ/thanh ghi rồi đọc dữ liệu. */
    I2C_BUS_OPERATION_PROBE,    /**< Kiểm tra một địa chỉ 7-bit có phản hồi ACK hay không. */
    I2C_BUS_OPERATION_SCAN      /**< Dò các địa chỉ 7-bit từ 0x08 đến 0x77. */
} I2cBus_Operation_t;

/** @brief Độ rộng địa chỉ vùng nhớ/thanh ghi của thiết bị I2C. */
typedef enum
{
    I2C_BUS_MEMORY_ADDRESS_8BIT = 0, /**< Địa chỉ vùng nhớ/thanh ghi dài 8 bit. */
    I2C_BUS_MEMORY_ADDRESS_16BIT     /**< Địa chỉ vùng nhớ/thanh ghi dài 16 bit. */
} I2cBus_MemoryAddressSize_t;

/** @brief Kết quả cuối cùng của một thao tác bất đồng bộ. */
typedef enum
{
    I2C_BUS_RESULT_NONE = 0,   /**< Chưa có kết quả mới. */
    I2C_BUS_RESULT_SUCCESS,    /**< Thao tác hoàn thành thành công. */
    I2C_BUS_RESULT_NACK,       /**< Thiết bị không phản hồi ACK tại địa chỉ được yêu cầu. */
    I2C_BUS_RESULT_TIMEOUT,    /**< Thao tác vượt quá timeout và đã được yêu cầu hủy. */
    I2C_BUS_RESULT_HAL_ERROR,  /**< HAL báo lỗi khác NACK thông thường trong lúc scan. */
    I2C_BUS_RESULT_ABORT_FAILED /**< Không thể hoàn thành quá trình hủy sau timeout. */
} I2cBus_Result_t;

/** @brief Sự kiện ngắn được callback HAL ghi lại để main context xử lý. */
typedef enum
{
    I2C_BUS_INTERRUPT_EVENT_NONE = 0, /**< Chưa có sự kiện. */
    I2C_BUS_INTERRUPT_EVENT_TRANSMIT_COMPLETE, /**< Truyền đã hoàn thành. */
    I2C_BUS_INTERRUPT_EVENT_MEMORY_READ_COMPLETE, /**< Đọc vùng nhớ/thanh ghi đã hoàn thành. */
    I2C_BUS_INTERRUPT_EVENT_ERROR, /**< HAL đã kết thúc giao dịch vì lỗi. */
    I2C_BUS_INTERRUPT_EVENT_ABORT_COMPLETE /**< HAL đã hoàn thành yêu cầu hủy. */
} I2cBus_InterruptEvent_t;

/** @brief Snapshot trạng thái công khai phục vụ tầng ứng dụng và debugger. */
typedef struct
{
    bool is_initialized;              /**< true sau khi context nhận một HAL I2C handle hợp lệ. */
    bool is_busy;                     /**< true khi transmit, memory read, scan hoặc abort đang diễn ra. */
    bool recovery_is_required;        /**< true khi trạng thái HAL không còn đủ tin cậy để dùng tiếp. */
    I2cBus_Operation_t active_operation; /**< Thao tác hiện đang chạy. */
    I2cBus_Operation_t completed_operation; /**< Loại thao tác tạo ra kết quả gần nhất. */
    I2cBus_Result_t last_result;      /**< Kết quả cuối cùng đã được main context xử lý. */
    uint32_t last_hal_error;          /**< Bitmask HAL_I2C_ERROR_* gần nhất. */
    uint8_t active_address_7bit;      /**< Địa chỉ thiết bị của giao dịch hoặc probe hiện tại. */
    uint16_t active_memory_address;   /**< Địa chỉ vùng nhớ/thanh ghi của thao tác hiện tại. */
    uint16_t active_data_length;      /**< Số byte của giao dịch truyền hoặc nhận hiện tại. */
    uint8_t scan_found_count;         /**< Tổng số địa chỉ đã phản hồi ACK. */
    uint8_t scan_stored_count;        /**< Số địa chỉ đã ghi được vào buffer của caller. */
    bool scan_result_overflow;        /**< true nếu số thiết bị vượt sức chứa buffer kết quả. */
} I2cBus_Status_t;

/**
 * @brief Context của một peripheral I2C vật lý.
 *
 * Caller cấp phát tĩnh context này. Không sửa trực tiếp các trường sau khi khởi tạo;
 * hãy sử dụng API và các hàm chuyển tiếp callback của driver.
 */
typedef struct
{
    I2C_HandleTypeDef *hal_i2c;       /**< HAL handle do CubeMX khởi tạo. */
    I2cBus_Status_t status;           /**< Trạng thái quan sát được của bus. */

    const uint8_t *active_transmit_data; /**< Buffer truyền được mượn đến khi có kết quả. */
    uint8_t *active_receive_data;     /**< Buffer nhận được mượn đến khi có kết quả. */
    uint32_t operation_start_tick_ms; /**< Tick bắt đầu giao dịch hoặc quá trình hủy. */
    uint32_t operation_timeout_ms;    /**< Timeout hữu hạn do caller cung cấp. */

    uint8_t *scan_address_buffer;     /**< Buffer nhận danh sách địa chỉ do caller sở hữu. */
    uint8_t scan_address_capacity;    /**< Số địa chỉ tối đa buffer có thể chứa. */
    uint8_t scan_probe_dummy_byte;    /**< Địa chỉ RAM hợp lệ cho giao dịch probe dài 0 byte. */

    I2cBus_Operation_t operation_before_abort; /**< Thao tác bị hủy do timeout. */
    volatile I2cBus_InterruptEvent_t interrupt_event; /**< Sự kiện do ISR ghi, service đọc. */
    volatile uint32_t interrupt_hal_error; /**< Mã lỗi được chụp trong error callback. */
    bool abort_is_in_progress;        /**< true trong lúc chờ HAL hoàn thành abort. */
    bool result_is_new;               /**< true khi có kết quả chưa được caller lấy. */
} I2cBus_t;

/**
 * @brief Khởi tạo một context quản lý một peripheral I2C đã được CubeMX khởi tạo.
 * @param bus Context do caller cấp phát tĩnh.
 * @param hal_i2c HAL handle tương ứng với peripheral cần quản lý.
 * @return true khi hai con trỏ hợp lệ; false nếu không thể khởi tạo.
 */
bool I2cBus_Initialize(I2cBus_t *bus, I2C_HandleTypeDef *hal_i2c);

/**
 * @brief Bắt đầu truyền một buffer bằng interrupt và trả về ngay.
 * @param bus Context bus đã khởi tạo.
 * @param device_address_7bit Địa chỉ thiết bị dạng 7-bit, chưa dịch trái.
 * @param data Buffer byte do caller sở hữu và phải giữ nguyên đến khi có kết quả.
 * @param data_length Số byte cần truyền; phải lớn hơn 0.
 * @param current_tick_ms HAL tick tại thời điểm bắt đầu.
 * @param timeout_ms Thời gian tối đa cho phép giao dịch chạy; phải lớn hơn 0.
 * @return Trạng thái chấp nhận yêu cầu; kết quả cuối cùng được lấy bằng I2cBus_TakeResult().
 * @note Caller phải lấy kết quả trước khi bắt đầu một thao tác mới.
 */
I2cBus_StartResult_t I2cBus_StartTransmit(I2cBus_t *bus,
                                          uint8_t device_address_7bit,
                                          const uint8_t *data,
                                          uint16_t data_length,
                                          uint32_t current_tick_ms,
                                          uint32_t timeout_ms);

/**
 * @brief Bắt đầu đọc bất đồng bộ một vùng nhớ hoặc thanh ghi bằng interrupt.
 * @param bus Context bus đã khởi tạo.
 * @param device_address_7bit Địa chỉ thiết bị dạng 7-bit, chưa dịch trái.
 * @param memory_address Địa chỉ vùng nhớ/thanh ghi bắt đầu cần đọc.
 * @param memory_address_size Độ rộng 8 bit hoặc 16 bit của địa chỉ vùng nhớ/thanh ghi.
 * @param receive_data Buffer do caller sở hữu để nhận dữ liệu; phải còn tồn tại đến khi có kết quả.
 * @param data_length Số byte cần đọc; phải lớn hơn 0.
 * @param current_tick_ms HAL tick tại thời điểm bắt đầu.
 * @param timeout_ms Thời gian tối đa cho phép giao dịch chạy; phải lớn hơn 0.
 * @return Trạng thái chấp nhận yêu cầu; kết quả cuối cùng lấy bằng I2cBus_TakeResult().
 * @note Driver thực hiện chuỗi gửi địa chỉ vùng nhớ/thanh ghi rồi đọc dữ liệu bằng HAL interrupt.
 * @note Caller phải lấy kết quả trước khi bắt đầu một thao tác mới trên cùng context.
 */
I2cBus_StartResult_t I2cBus_StartMemoryRead(
    I2cBus_t *bus,
    uint8_t device_address_7bit,
    uint16_t memory_address,
    I2cBus_MemoryAddressSize_t memory_address_size,
    uint8_t *receive_data,
    uint16_t data_length,
    uint32_t current_tick_ms,
    uint32_t timeout_ms);

/**
 * @brief Bắt đầu kiểm tra một địa chỉ I2C bằng interrupt và trả về ngay.
 * @param bus Context bus đã khởi tạo.
 * @param device_address_7bit Địa chỉ thiết bị dạng 7-bit, chưa dịch trái.
 * @param current_tick_ms HAL tick tại thời điểm bắt đầu.
 * @param timeout_ms Thời gian tối đa cho phép probe chạy; phải lớn hơn 0.
 * @return Trạng thái chấp nhận; kết quả cuối là SUCCESS, NACK hoặc lỗi bus.
 */
I2cBus_StartResult_t I2cBus_StartProbe(I2cBus_t *bus,
                                       uint8_t device_address_7bit,
                                       uint32_t current_tick_ms,
                                       uint32_t timeout_ms);

/**
 * @brief Bắt đầu dò bất đồng bộ các địa chỉ I2C 7-bit từ 0x08 đến 0x77.
 * @param bus Context bus đã khởi tạo.
 * @param found_addresses Buffer do caller cấp phát để nhận các địa chỉ phản hồi ACK.
 * @param found_address_capacity Số phần tử tối đa của found_addresses; phải lớn hơn 0.
 * @param current_tick_ms HAL tick tại thời điểm bắt đầu.
 * @param probe_timeout_ms Timeout áp dụng riêng cho mỗi địa chỉ được kiểm tra.
 * @return Trạng thái chấp nhận yêu cầu; kết quả cuối cùng được lấy bằng I2cBus_TakeResult().
 * @note Buffer kết quả phải còn tồn tại và không được sửa cho tới khi scan kết thúc.
 * @note Caller phải lấy kết quả trước khi bắt đầu một thao tác mới.
 */
I2cBus_StartResult_t I2cBus_StartScan(I2cBus_t *bus,
                                      uint8_t *found_addresses,
                                      uint8_t found_address_capacity,
                                      uint32_t current_tick_ms,
                                      uint32_t probe_timeout_ms);

/**
 * @brief Tiến state machine của giao dịch, timeout, abort hoặc scan đúng một bước.
 * @param bus Context bus đã khởi tạo.
 * @param current_tick_ms HAL tick hiện tại.
 * @note Gọi thường xuyên trong superloop; hàm không chờ giao dịch hoàn thành.
 */
void I2cBus_Service(I2cBus_t *bus, uint32_t current_tick_ms);

/**
 * @brief Lấy một lần kết quả mới nhất của thao tác bất đồng bộ đã hoàn thành.
 * @param bus Context bus đã khởi tạo.
 * @param completed_operation Nơi nhận loại thao tác đã hoàn thành; có thể là NULL.
 * @param result Nơi nhận kết quả; không được là NULL.
 * @return true khi vừa lấy được kết quả mới; false nếu chưa có kết quả mới.
 */
bool I2cBus_TakeResult(I2cBus_t *bus,
                       I2cBus_Operation_t *completed_operation,
                       I2cBus_Result_t *result);

/**
 * @brief Sao chép snapshot trạng thái hiện tại để quan sát hoặc debug.
 * @param bus Context bus cần đọc.
 * @param output_status Vùng nhớ nhận snapshot; NULL sẽ được bỏ qua.
 */
void I2cBus_GetStatus(const I2cBus_t *bus, I2cBus_Status_t *output_status);

/**
 * @brief Chuyển tiếp HAL callback hoàn thành master transmit tới đúng bus context.
 * @param bus Context có thể đang sở hữu HAL handle phát sinh callback.
 * @param hal_i2c HAL handle do callback cung cấp.
 * @note Gọi từ HAL_I2C_MasterTxCpltCallback(); chỉ ghi một sự kiện ngắn trong ISR.
 */
void I2cBus_HandleMasterTransmitCompleteInterrupt(I2cBus_t *bus,
                                                 I2C_HandleTypeDef *hal_i2c);

/**
 * @brief Chuyển tiếp HAL callback hoàn thành memory read tới đúng bus context.
 * @param bus Context có thể đang sở hữu HAL handle phát sinh callback.
 * @param hal_i2c HAL handle do callback cung cấp.
 * @note Gọi từ HAL_I2C_MemRxCpltCallback(); chỉ ghi một sự kiện ngắn trong ISR.
 */
void I2cBus_HandleMemoryReadCompleteInterrupt(I2cBus_t *bus,
                                             I2C_HandleTypeDef *hal_i2c);

/**
 * @brief Chuyển tiếp HAL error callback tới đúng bus context.
 * @param bus Context có thể đang sở hữu HAL handle phát sinh callback.
 * @param hal_i2c HAL handle do callback cung cấp.
 * @note Gọi từ HAL_I2C_ErrorCallback(); xử lý dài hơn được hoãn về I2cBus_Service().
 */
void I2cBus_HandleErrorInterrupt(I2cBus_t *bus,
                                 I2C_HandleTypeDef *hal_i2c);

/**
 * @brief Chuyển tiếp HAL abort-complete callback tới đúng bus context.
 * @param bus Context có thể đang sở hữu HAL handle phát sinh callback.
 * @param hal_i2c HAL handle do callback cung cấp.
 * @note Gọi từ HAL_I2C_AbortCpltCallback().
 */
void I2cBus_HandleAbortCompleteInterrupt(I2cBus_t *bus,
                                        I2C_HandleTypeDef *hal_i2c);

#ifdef __cplusplus
}
#endif

#endif /* USER_DRIVERS_I2C_BUS_I2C_BUS_H_ */
