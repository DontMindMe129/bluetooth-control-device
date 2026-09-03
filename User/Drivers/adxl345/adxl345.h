/**
 * @file adxl345.h
 * @brief Driver ADXL345 không blocking qua I2C interrupt và chân DATA_READY.
 *
 * Driver sở hữu trình tự nhận dạng, cấu hình và đọc ba trục. Thuật toán suy ra
 * tư thế hoặc chuyển động không thuộc module này.
 */

#ifndef USER_DRIVERS_ADXL345_ADXL345_H_
#define USER_DRIVERS_ADXL345_ADXL345_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "i2c_bus.h"

/** @brief Trạng thái state machine công khai để quan sát bằng debugger. */
typedef enum
{
    ADXL345_STATE_UNINITIALIZED = 0, /**< Context chưa được khởi tạo. */
    ADXL345_STATE_VERIFYING_DEVICE,  /**< Đang đọc DEVID tại địa chỉ 7-bit đã cấu hình. */
    ADXL345_STATE_CONFIGURING_RATE,  /**< Đang cấu hình output data rate 100 Hz. */
    ADXL345_STATE_CONFIGURING_FORMAT,/**< Đang cấu hình full-resolution, dải ±2 g. */
    ADXL345_STATE_CONFIGURING_INT_MAP, /**< Đang ánh xạ DATA_READY sang INT1. */
    ADXL345_STATE_ENABLING_MEASUREMENT, /**< Đang chuyển cảm biến sang measurement mode. */
    ADXL345_STATE_ENABLING_DATA_READY,  /**< Đang bật interrupt DATA_READY. */
    ADXL345_STATE_RETRY_WAIT,        /**< Đang chờ trước lần khởi tạo kế tiếp. */
    ADXL345_STATE_READY,             /**< Đã sẵn sàng và đang chờ DATA_READY. */
    ADXL345_STATE_READING_SAMPLE,    /**< Đang đọc liền sáu byte X/Y/Z. */
    ADXL345_STATE_ERROR              /**< Khởi tạo thất bại sau số lần cho phép. */
} Adxl345_State_t;

/** @brief Nguyên nhân lỗi gần nhất của driver. */
typedef enum
{
    ADXL345_ERROR_NONE = 0,          /**< Không có lỗi đang được ghi nhận. */
    ADXL345_ERROR_INVALID_CONFIG,    /**< Cấu hình driver không hợp lệ. */
    ADXL345_ERROR_DEVICE_NOT_FOUND,  /**< Không có ADXL345 phản hồi tại địa chỉ 7-bit đã cấu hình. */
    ADXL345_ERROR_INVALID_DEVICE_ID, /**< Thiết bị phản hồi nhưng DEVID không bằng 0xE5. */
    ADXL345_ERROR_I2C_START,         /**< I2C bus từ chối bắt đầu giao dịch. */
    ADXL345_ERROR_I2C_TRANSFER,      /**< HAL báo lỗi trong giao dịch I2C. */
    ADXL345_ERROR_I2C_TIMEOUT,       /**< Giao dịch I2C vượt timeout đã duyệt. */
    ADXL345_ERROR_I2C_ABORT,         /**< Không thể hoàn thành quá trình abort I2C. */
    ADXL345_ERROR_DATA_STALE,        /**< Không nhận được mẫu hợp lệ trong thời gian cho phép. */
    ADXL345_ERROR_UNEXPECTED_RESULT  /**< Loại kết quả bus không khớp state đang chờ. */
} Adxl345_Error_t;

/** @brief Kết quả trả về ngay khi yêu cầu khởi tạo driver. */
typedef enum
{
    ADXL345_INITIALIZE_ACCEPTED = 0, /**< State machine khởi tạo đã bắt đầu. */
    ADXL345_INITIALIZE_INVALID_CONFIG /**< Cấu hình không hợp lệ. */
} Adxl345_InitializeResult_t;

/** @brief Một mẫu gia tốc đã ghép byte và đổi sang đơn vị mg. */
typedef struct
{
    int16_t raw_x;             /**< Giá trị two's-complement gốc của trục X. */
    int16_t raw_y;             /**< Giá trị two's-complement gốc của trục Y. */
    int16_t raw_z;             /**< Giá trị two's-complement gốc của trục Z. */
    int32_t x_mg;              /**< Gia tốc trục X, xấp xỉ theo mg. */
    int32_t y_mg;              /**< Gia tốc trục Y, xấp xỉ theo mg. */
    int32_t z_mg;              /**< Gia tốc trục Z, xấp xỉ theo mg. */
    uint32_t sample_tick_ms;   /**< HAL tick khi driver hoàn thành đọc mẫu. */
} Adxl345_Sample_t;

/** @brief Các tham số phần cứng và chính sách lỗi do application cung cấp. */
typedef struct
{
    I2cBus_t *i2c_bus;             /**< Context bus I2C vật lý dùng chung đã được khởi tạo. */
    uint8_t address_7bit;          /**< Địa chỉ I2C 7-bit cố định đã xác nhận của cảm biến. */
    uint16_t data_ready_pin;       /**< Pin EXTI nối với INT1 của ADXL345. */
    uint32_t transfer_timeout_ms;  /**< Timeout cho từng giao dịch I2C. */
    uint32_t initialize_retry_delay_ms; /**< Khoảng chờ giữa hai lần khởi tạo. */
    uint32_t stale_timeout_ms;     /**< Thời gian không có mẫu trước khi báo stale. */
    uint8_t initialize_max_attempts; /**< Tổng số lần khởi tạo tối đa. */
} Adxl345_Config_t;

/** @brief Snapshot trạng thái công khai cho App, OLED và debugger. */
typedef struct
{
    bool is_initialized;          /**< Context đã nhận cấu hình hợp lệ. */
    bool is_ready;                /**< Cảm biến đã nhận dạng và cấu hình xong. */
    bool has_sample;              /**< Đã đọc thành công ít nhất một mẫu. */
    bool sample_is_fresh;         /**< Mẫu gần nhất chưa vượt stale timeout. */
    bool has_error;               /**< Có lỗi chưa được một lần đọc thành công xóa. */
    bool transaction_is_pending;  /**< Driver đang sở hữu một giao dịch trên bus I2C dùng chung. */
    Adxl345_State_t state;        /**< State machine hiện tại. */
    Adxl345_Error_t last_error;   /**< Nguyên nhân lỗi gần nhất. */
    Adxl345_Sample_t latest_sample; /**< Mẫu hợp lệ gần nhất. */
    uint8_t selected_address_7bit; /**< Địa chỉ 7-bit đã cấu hình và đang được driver sử dụng. */
    uint8_t device_id;            /**< Byte DEVID đọc gần nhất. */
    uint8_t initialize_attempt_count; /**< Số lần khởi tạo đã bắt đầu. */
    uint32_t successful_sample_count; /**< Tổng mẫu thành công, tăng bão hòa. */
    uint32_t failed_sample_count; /**< Tổng lần đọc mẫu thất bại, tăng bão hòa. */
} Adxl345_Status_t;

/** @brief Context tĩnh của một cảm biến ADXL345. */
typedef struct
{
    Adxl345_Config_t config;       /**< Bản sao cấu hình thuộc sở hữu driver. */
    Adxl345_Status_t status;       /**< Trạng thái công khai. */
    uint8_t transfer_buffer[6];    /**< Buffer dùng chung cho cấu hình và đọc XYZ. */
    uint32_t state_start_tick_ms;  /**< Tick bắt đầu retry wait hoặc measurement. */
    volatile bool data_ready_pending; /**< Cờ ngắn do ISR ghi, main context lấy. */
    bool sample_is_new;            /**< Mẫu mới chưa được caller lấy. */
} Adxl345_t;

/**
 * @brief Bắt đầu nhận dạng và cấu hình ADXL345 theo state machine không blocking.
 * @param sensor Context do caller cấp phát tĩnh.
 * @param config Cấu hình bus, EXTI và timeout đã được duyệt.
 * @param current_tick_ms HAL tick hiện tại.
 */
Adxl345_InitializeResult_t Adxl345_Initialize(
    Adxl345_t *sensor,
    const Adxl345_Config_t *config,
    uint32_t current_tick_ms);

/**
 * @brief Tiến state machine khởi tạo, đọc mẫu và kiểm tra freshness đúng một bước.
 * @note Gọi thường xuyên trong superloop sau I2cBus_Service() của cùng bus.
 */
void Adxl345_Service(Adxl345_t *sensor, uint32_t current_tick_ms);

/**
 * @brief Ghi nhận cạnh DATA_READY từ callback EXTI.
 * @note Hàm chạy trong ISR, chỉ kiểm tra pin và đặt một cờ boolean.
 */
void Adxl345_HandleDataReadyInterrupt(Adxl345_t *sensor, uint16_t gpio_pin);

/** @brief Lấy đúng một lần mẫu mới nhất đã đọc thành công. */
bool Adxl345_TakeNewSample(Adxl345_t *sensor, Adxl345_Sample_t *output_sample);

/** @brief Sao chép snapshot trạng thái phục vụ App, OLED hoặc debugger. */
void Adxl345_GetStatus(const Adxl345_t *sensor, Adxl345_Status_t *output_status);

#ifdef __cplusplus
}
#endif

#endif /* USER_DRIVERS_ADXL345_ADXL345_H_ */
