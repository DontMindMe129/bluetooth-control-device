/**
 * @file app_warning_controller.h
 * @brief Điều phối các nguồn warning và phân xử mask năm ngõ ra ở tầng App.
 *
 * Module thuần phần mềm: không truy cập HAL, GPIO, UART hoặc OLED. Caller cung
 * cấp snapshot môi trường, chuyển động và trạng thái monitoring; controller giữ
 * policy manual, history chuyển tiếp và state machine pattern warning.
 */

#ifndef USER_APP_APP_WARNING_CONTROLLER_H_
#define USER_APP_APP_WARNING_CONTROLLER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "environment_feedback.h"
#include "motion_monitor.h"
#include "warning_feedback.h"
#include "warning_history.h"

/** @brief Dữ liệu đầu vào bất biến của một vòng điều phối warning. */
typedef struct
{
    const EnvironmentMonitor_Status_t *environment; /**< Snapshot môi trường mới nhất. */
    const MotionMonitor_Status_t *motion; /**< Snapshot chuyển động mới nhất. */
    uint32_t current_tick_ms; /**< HAL tick dùng cho history và pattern không blocking. */
    uint16_t monitoring_session_id; /**< ID phiên đang chạy; bị bỏ qua khi monitoring tắt. */
    uint8_t user_output_mask; /**< Trạng thái năm output do người dùng chọn trước khi phân xử. */
    bool has_new_environment_sample; /**< Có mẫu DHT11 hợp lệ mới trong vòng hiện tại. */
    bool manual_warning_requested; /**< Có sự kiện manual warning mới trong vòng hiện tại. */
    bool monitoring_is_active; /**< Cho phép nguồn warning tự động và gắn session hiện tại. */
} AppWarningController_Input_t;

/** @brief Snapshot tập trung để App và debugger quan sát toàn bộ luồng warning. */
typedef struct
{
    bool is_initialized; /**< Controller và ba service thành phần đã khởi tạo. */
    EnvironmentFeedback_Status_t environment_feedback; /**< Policy manual/môi trường hiện tại. */
    WarningFeedback_Status_t output_pattern; /**< Nguồn và pha pattern năm LED hiện tại. */
    WarningHistory_Status_t history; /**< Trạng thái ring buffer các chuyển tiếp warning. */
    uint8_t effective_output_mask; /**< Mask phải xuất xuống GPIO sau khi phân xử. */
} AppWarningController_Status_t;

/** @brief Context tĩnh do application sở hữu; không dùng cấp phát động. */
typedef struct
{
    EnvironmentFeedback_t environment_feedback; /**< Policy manual và môi trường do controller sở hữu. */
    WarningFeedback_t output_pattern; /**< State machine pattern năm LED do controller sở hữu. */
    WarningHistory_t history; /**< Ring buffer warning do controller sở hữu. */
    AppWarningController_Status_t status; /**< Snapshot công khai được đồng bộ sau mỗi lần service. */
} AppWarningController_t;

/**
 * @brief Khởi tạo controller ở trạng thái không có warning.
 * @param controller Context do application cấp phát tĩnh.
 * @param current_tick_ms Tick làm mốc ban đầu cho state machine pattern.
 * @return true khi toàn bộ service thành phần khởi tạo thành công.
 */
bool AppWarningController_Initialize(AppWarningController_t *controller,
                                    uint32_t current_tick_ms);

/**
 * @brief Cập nhật nguồn warning, history, pattern và mask output đúng một lần.
 * @param controller Context đã khởi tạo.
 * @param input Snapshot đầu vào của vòng superloop hiện tại.
 * @param output_new_record Nếu khác NULL, nhận record khi source mask thay đổi.
 * @return true khi một record warning mới vừa được tạo.
 * @note Hàm không blocking và không trực tiếp điều khiển phần cứng.
 */
bool AppWarningController_Service(
    AppWarningController_t *controller,
    const AppWarningController_Input_t *input,
    WarningHistory_Record_t *output_new_record);

/**
 * @brief Lấy ring buffer chỉ-đọc để ghép với tầng hiển thị history.
 * @param controller Context đã khởi tạo.
 * @return Con trỏ có hiệu lực suốt vòng đời controller, hoặc NULL nếu chưa init.
 */
const WarningHistory_t *AppWarningController_GetHistory(
    const AppWarningController_t *controller);

/**
 * @brief Sao chép snapshot tập trung cho application hoặc debugger.
 * @param controller Context nguồn.
 * @param output_status Vùng nhớ nhận snapshot; NULL sẽ được bỏ qua.
 */
void AppWarningController_GetStatus(
    const AppWarningController_t *controller,
    AppWarningController_Status_t *output_status);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_APP_WARNING_CONTROLLER_H_ */
