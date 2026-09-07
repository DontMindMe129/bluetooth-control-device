/**
 * @file monitoring_session.h
 * @brief Quản lý phiên giám sát do người dùng bật hoặc tắt.
 *
 * Service thuần phần mềm này không đọc cảm biến và không điều khiển output.
 * Application dùng trạng thái của nó để cho phép ghi history và warning tự động.
 */

#ifndef USER_SERVICES_MONITORING_SESSION_MONITORING_SESSION_H_
#define USER_SERVICES_MONITORING_SESSION_MONITORING_SESSION_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/** @brief Trạng thái vận hành của phiên giám sát. */
typedef enum
{
    MONITORING_SESSION_INACTIVE = 0, /**< Vẫn đọc cảm biến nhưng không ghi history/cảnh báo tự động. */
    MONITORING_SESSION_ACTIVE        /**< Ghi history và cho phép cảnh báo tự động. */
} MonitoringSession_State_t;

/** @brief Nguồn yêu cầu thay đổi trạng thái gần nhất. */
typedef enum
{
    MONITORING_SESSION_SOURCE_NONE = 0,
    MONITORING_SESSION_SOURCE_LOCAL_BUTTON,
    MONITORING_SESSION_SOURCE_REMOTE_COMMAND
} MonitoringSession_Source_t;

/** @brief Kết quả yêu cầu thay đổi trạng thái. */
typedef enum
{
    MONITORING_SESSION_CHANGE_INVALID = 0,
    MONITORING_SESSION_CHANGE_UNCHANGED,
    MONITORING_SESSION_CHANGE_APPLIED
} MonitoringSession_ChangeResult_t;

/** @brief Snapshot công khai để App và debugger quan sát phiên hiện tại. */
typedef struct
{
    bool is_initialized; /**< Service đã được khởi tạo. */
    MonitoringSession_State_t state; /**< ACTIVE hoặc INACTIVE hiện tại. */
    MonitoringSession_Source_t last_change_source; /**< Nguồn của thay đổi gần nhất. */
    uint16_t session_id; /**< Tăng bão hòa mỗi lần bắt đầu phiên mới. */
    uint32_t last_change_tick_ms; /**< Tick của lần đổi trạng thái gần nhất. */
    uint32_t state_change_count; /**< Tổng số lần trạng thái thực sự thay đổi. */
} MonitoringSession_Status_t;

/** @brief Context tĩnh do application sở hữu. */
typedef struct
{
    MonitoringSession_Status_t status;
} MonitoringSession_t;

/** @brief Khởi tạo ở trạng thái INACTIVE. */
bool MonitoringSession_Initialize(MonitoringSession_t *session,
                                  uint32_t current_tick_ms);

/** @brief Đặt trạng thái mong muốn; session_id chỉ tăng khi chuyển sang ACTIVE. */
MonitoringSession_ChangeResult_t MonitoringSession_SetActive(
    MonitoringSession_t *session,
    bool active,
    MonitoringSession_Source_t source,
    uint32_t current_tick_ms);

/** @brief Đảo ACTIVE/INACTIVE và ghi nhận nguồn yêu cầu. */
MonitoringSession_ChangeResult_t MonitoringSession_Toggle(
    MonitoringSession_t *session,
    MonitoringSession_Source_t source,
    uint32_t current_tick_ms);

/** @brief Trả về true khi history và warning tự động được phép hoạt động. */
bool MonitoringSession_IsActive(const MonitoringSession_t *session);

/** @brief Sao chép trạng thái session cho application hoặc debugger. */
void MonitoringSession_GetStatus(const MonitoringSession_t *session,
                                 MonitoringSession_Status_t *output_status);

#ifdef __cplusplus
}
#endif

#endif
