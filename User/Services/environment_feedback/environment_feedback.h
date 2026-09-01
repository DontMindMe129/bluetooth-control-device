/**
 * @file environment_feedback.h
 * @brief Policy thuần phần mềm chuyển trạng thái môi trường thành pattern phản hồi.
 *
 * Service không truy cập GPIO hoặc EXTI. Caller cung cấp snapshot monitor và sự
 * kiện manual; service chỉ kết luận nguồn cảnh báo môi trường/manual có hoạt động.
 */

#ifndef USER_SERVICES_ENVIRONMENT_FEEDBACK_ENVIRONMENT_FEEDBACK_H_
#define USER_SERVICES_ENVIRONMENT_FEEDBACK_ENVIRONMENT_FEEDBACK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "environment_monitor.h"

/** @brief Phân loại phản hồi logic từ trạng thái môi trường hoặc manual override. */
typedef enum
{
    ENVIRONMENT_FEEDBACK_PATTERN_OFF = 0,          /**< Không có dữ liệu fresh để phân loại. */
    ENVIRONMENT_FEEDBACK_PATTERN_NORMAL,           /**< Môi trường bình thường. */
    ENVIRONMENT_FEEDBACK_PATTERN_WARM,             /**< Chỉ vượt điều kiện nhiệt độ. */
    ENVIRONMENT_FEEDBACK_PATTERN_HUMID,            /**< Chỉ vượt điều kiện độ ẩm. */
    ENVIRONMENT_FEEDBACK_PATTERN_WARM_AND_HUMID    /**< Cảnh báo môi trường hoặc manual. */
} EnvironmentFeedback_Pattern_t;

/** @brief Snapshot đầu ra và trạng thái nội bộ có ý nghĩa với application. */
typedef struct
{
    EnvironmentFeedback_Pattern_t pattern; /**< Phân loại phản hồi môi trường hiện tại. */
    bool manual_warning_active;             /**< Cảnh báo manual đang được giữ. */
    bool automatic_warning_active;          /**< Dữ liệu fresh đang là warm-and-humid. */
    bool warning_active;                    /**< Manual hoặc automatic warning đang hoạt động. */
} EnvironmentFeedback_Status_t;

/**
 * @brief Khởi tạo policy phản hồi ở trạng thái tắt.
 */
void EnvironmentFeedback_Initialize(void);

/**
 * @brief Cập nhật manual override và kết luận cảnh báo môi trường hiện tại.
 *
 * Một mẫu môi trường hợp lệ mới sẽ xóa cảnh báo manual cũ. Nếu đồng thời có sự
 * kiện manual mới, sự kiện mới được giữ lại đến mẫu hợp lệ tiếp theo. Khi dữ liệu
 * không fresh, warning chỉ còn active nếu manual warning đang được giữ.
 *
 * @param environment_status Snapshot mới nhất từ environment monitor.
 * @param has_new_environment_sample true khi vừa nhận một mẫu cảm biến hợp lệ mới.
 * @param manual_warning_requested true khi nút manual vừa được xác nhận qua debounce.
 */
void EnvironmentFeedback_Service(
    const EnvironmentMonitor_Status_t *environment_status,
    bool has_new_environment_sample,
    bool manual_warning_requested);

/**
 * @brief Sao chép snapshot hiện tại cho application.
 * @param output_status Vùng nhớ nhận snapshot; NULL sẽ được bỏ qua.
 */
void EnvironmentFeedback_GetStatus(
    EnvironmentFeedback_Status_t *output_status);

#ifdef __cplusplus
}
#endif

#endif /* USER_SERVICES_ENVIRONMENT_FEEDBACK_ENVIRONMENT_FEEDBACK_H_ */
