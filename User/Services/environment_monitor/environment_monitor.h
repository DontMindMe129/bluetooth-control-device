/**
 * @file environment_monitor.h
 * @brief Giao diện giám sát đơn giản dữ liệu nhiệt độ và độ ẩm từ DHT11.
 *
 * Module chạy hoàn toàn trong main context, không truy cập HAL hoặc phần cứng.
 * Dữ liệu đo vẫn giữ nguyên bốn byte của DHT11; module chỉ phân loại điều kiện
 * môi trường, theo dõi độ mới của dữ liệu và sao chép trạng thái lỗi cảm biến.
 */

#ifndef USER_SERVICES_ENVIRONMENT_MONITOR_ENVIRONMENT_MONITOR_H_
#define USER_SERVICES_ENVIRONMENT_MONITOR_ENVIRONMENT_MONITOR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "dht11.h"

/** @brief Ngưỡng nhiệt độ nguyên dùng để kích hoạt trạng thái WARM. */
#define ENVIRONMENT_MONITOR_WARM_TEMPERATURE_C       (28U)

/** @brief Ngưỡng độ ẩm nguyên dùng để kích hoạt trạng thái HUMID. */
#define ENVIRONMENT_MONITOR_HUMIDITY_PERCENT         (65U)

/** @brief Thời gian không có mẫu hợp lệ mới trước khi dữ liệu bị xem là STALE. */
#define ENVIRONMENT_MONITOR_STALE_TIMEOUT_MS         (6000UL)

/**
 * @brief Độ mới của dữ liệu môi trường đang được monitor lưu giữ.
 */
typedef enum
{
    ENVIRONMENT_DATA_NO_DATA = 0, /**< Chưa từng nhận được mẫu DHT11 hợp lệ. */
    ENVIRONMENT_DATA_FRESH,       /**< Mẫu gần nhất chưa vượt quá stale timeout. */
    ENVIRONMENT_DATA_STALE        /**< Mẫu gần nhất đã vượt quá stale timeout. */
} EnvironmentMonitor_DataState_t;

/**
 * @brief Kết luận đơn giản được suy ra từ nhiệt độ và độ ẩm gần nhất.
 */
typedef enum
{
    ENVIRONMENT_CONDITION_UNKNOWN = 0, /**< Không thể phân loại vì chưa có dữ liệu. */
    ENVIRONMENT_CONDITION_NORMAL,      /**< Không đạt ngưỡng WARM hoặc HUMID. */
    ENVIRONMENT_CONDITION_WARM,        /**< Chỉ nhiệt độ đạt ngưỡng WARM. */
    ENVIRONMENT_CONDITION_HUMID,       /**< Chỉ độ ẩm đạt ngưỡng HUMID. */
    ENVIRONMENT_CONDITION_WARM_AND_HUMID /**< Cả nhiệt độ và độ ẩm đều đạt ngưỡng. */
} EnvironmentMonitor_Condition_t;

/**
 * @brief Snapshot đầy đủ mà tầng ứng dụng có thể đọc từ monitor.
 *
 * @note latest_data chỉ có ý nghĩa khi data_state khác ENVIRONMENT_DATA_NO_DATA.
 *       Khi dữ liệu chuyển sang STALE, mẫu hợp lệ cuối cùng vẫn được giữ lại.
 */
typedef struct
{
    DHT11_Data_t latest_data;                    /**< Bốn byte dữ liệu DHT11 hợp lệ gần nhất. */
    EnvironmentMonitor_DataState_t data_state;  /**< Độ mới hiện tại của latest_data. */
    EnvironmentMonitor_Condition_t condition;   /**< Kết luận từ các ngưỡng kiểm thử. */
    DHT11_Error_t last_sensor_error;             /**< Lỗi gần nhất được báo bởi driver DHT11. */
    uint16_t consecutive_sensor_errors;          /**< Số giao dịch DHT11 lỗi liên tiếp. */
    uint32_t last_update_tick_ms;                /**< HAL tick của mẫu hợp lệ gần nhất. */
} EnvironmentMonitor_Status_t;

/**
 * @brief Đưa monitor về trạng thái chưa có dữ liệu.
 *
 * @note Gọi một lần trước vòng lặp chính.
 */
void EnvironmentMonitor_Initialize(void);

/**
 * @brief Cập nhật dữ liệu, lỗi cảm biến và trạng thái stale của monitor.
 *
 * Hàm phải được gọi thường xuyên trong superloop sau DHT11_Service(). Khi lần
 * lặp hiện tại không có mẫu mới, truyền NULL vào new_data; monitor vẫn cập nhật
 * lỗi cảm biến và kiểm tra stale timeout.
 *
 * @param current_tick_ms HAL tick hiện tại.
 * @param new_data Con trỏ tới mẫu DHT11 mới, hoặc NULL nếu không có mẫu mới.
 * @param sensor_status Snapshot trạng thái hiện tại của driver DHT11.
 *
 * @note Hàm không lưu các con trỏ đầu vào sau khi trả về.
 */
void EnvironmentMonitor_Service(uint32_t current_tick_ms,
                                const DHT11_Data_t *new_data,
                                const DHT11_Status_t *sensor_status);

/**
 * @brief Sao chép snapshot hiện tại của monitor cho tầng ứng dụng.
 *
 * @param output_status Vùng nhớ của caller dùng để nhận snapshot.
 */
void EnvironmentMonitor_GetStatus(EnvironmentMonitor_Status_t *output_status);

#ifdef __cplusplus
}
#endif

#endif /* USER_SERVICES_ENVIRONMENT_MONITOR_ENVIRONMENT_MONITOR_H_ */
