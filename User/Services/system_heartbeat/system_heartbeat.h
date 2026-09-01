/**
 * @file system_heartbeat.h
 * @brief Bộ tạo trạng thái heartbeat tuần hoàn, không phụ thuộc GPIO hoặc HAL.
 */

#ifndef USER_SERVICES_SYSTEM_HEARTBEAT_SYSTEM_HEARTBEAT_H_
#define USER_SERVICES_SYSTEM_HEARTBEAT_SYSTEM_HEARTBEAT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/** @brief Cấu hình thời gian của heartbeat. */
typedef struct
{
    uint32_t on_time_ms;  /**< Thời gian trạng thái heartbeat active. */
    uint32_t off_time_ms; /**< Thời gian trạng thái heartbeat inactive. */
} SystemHeartbeat_Config_t;

/** @brief Context của một bộ tạo heartbeat. */
typedef struct
{
    SystemHeartbeat_Config_t config; /**< Bản sao cấu hình thời gian. */
    uint32_t cycle_start_tick_ms;     /**< Mốc bắt đầu chu kỳ. */
    bool is_initialized;              /**< true sau khi cấu hình hợp lệ. */
    bool output_should_be_active;     /**< Đầu ra logic tại lần Service gần nhất. */
} SystemHeartbeat_t;

/**
 * @brief Khởi tạo heartbeat ở trạng thái inactive.
 * @param heartbeat Instance do caller cấp phát tĩnh.
 * @param config Thời gian on/off; tổng chu kỳ phải khác 0 và không tràn uint32_t.
 * @param current_tick_ms HAL tick hiện tại dùng làm mốc chu kỳ.
 * @return true nếu cấu hình hợp lệ; false nếu đối số không hợp lệ.
 */
bool SystemHeartbeat_Initialize(SystemHeartbeat_t *heartbeat,
                                const SystemHeartbeat_Config_t *config,
                                uint32_t current_tick_ms);

/**
 * @brief Cập nhật đầu ra heartbeat theo thời gian hiện tại.
 * @param heartbeat Instance đã khởi tạo.
 * @param current_tick_ms HAL tick hiện tại.
 */
void SystemHeartbeat_Service(SystemHeartbeat_t *heartbeat,
                             uint32_t current_tick_ms);

/**
 * @brief Đọc trạng thái logic heartbeat hiện tại.
 * @return true khi output cần active; false khi inactive hoặc instance không hợp lệ.
 */
bool SystemHeartbeat_ShouldBeActive(const SystemHeartbeat_t *heartbeat);

#ifdef __cplusplus
}
#endif

#endif /* USER_SERVICES_SYSTEM_HEARTBEAT_SYSTEM_HEARTBEAT_H_ */
