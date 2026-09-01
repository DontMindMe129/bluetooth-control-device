/**
 * @file warning_feedback.h
 * @brief Tổng hợp nhiều nguồn warning và tạo pattern logic cho năm ngõ ra LED.
 *
 * Service thuần phần mềm, không truy cập GPIO hoặc HAL. Application cung cấp
 * source bitmask và HAL tick, sau đó tự ánh xạ output_mask xuống phần cứng.
 */

#ifndef USER_SERVICES_WARNING_FEEDBACK_WARNING_FEEDBACK_H_
#define USER_SERVICES_WARNING_FEEDBACK_WARNING_FEEDBACK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/** @brief Thời gian mỗi pha luân phiên 10101/01010. */
#define WARNING_FEEDBACK_ALTERNATE_PHASE_MS (150UL)

/** @brief Thời gian tắt cả năm LED ở cuối frame cảnh báo. */
#define WARNING_FEEDBACK_OFF_PHASE_MS       (400UL)

/** @brief Mask bật OUT1, OUT3 và OUT5. */
#define WARNING_FEEDBACK_ODD_OUTPUT_MASK    (0x15U)

/** @brief Mask bật OUT2 và OUT4. */
#define WARNING_FEEDBACK_EVEN_OUTPUT_MASK   (0x0AU)

/** @brief Các bit nguồn warning có thể hoạt động đồng thời. */
typedef enum
{
    WARNING_FEEDBACK_SOURCE_NONE = 0U,          /**< Không có warning. */
    WARNING_FEEDBACK_SOURCE_MANUAL = (1U << 0), /**< Nút manual warning. */
    WARNING_FEEDBACK_SOURCE_ENVIRONMENT = (1U << 1), /**< Môi trường warm-and-humid. */
    WARNING_FEEDBACK_SOURCE_SHAKING = (1U << 2) /**< Motion monitor xác nhận shaking. */
} WarningFeedback_Source_t;

/** @brief Pha pattern hiện tại để quan sát bằng debugger. */
typedef enum
{
    WARNING_FEEDBACK_PHASE_INACTIVE = 0, /**< Không có nguồn warning. */
    WARNING_FEEDBACK_PHASE_ODD_1,        /**< OUT1/3/5, lần thứ nhất. */
    WARNING_FEEDBACK_PHASE_EVEN_1,       /**< OUT2/4, lần thứ nhất. */
    WARNING_FEEDBACK_PHASE_ODD_2,        /**< OUT1/3/5, lần thứ hai. */
    WARNING_FEEDBACK_PHASE_EVEN_2,       /**< OUT2/4, lần thứ hai. */
    WARNING_FEEDBACK_PHASE_ALL_OFF       /**< Tắt cả năm LED cuối frame. */
} WarningFeedback_Phase_t;

/** @brief Snapshot công khai phục vụ application và debugger. */
typedef struct
{
    bool is_initialized;          /**< Context đã được khởi tạo. */
    bool warning_active;          /**< Có ít nhất một nguồn warning đang hoạt động. */
    uint8_t active_source_mask;   /**< Tổ hợp bit WarningFeedback_Source_t hiện tại. */
    uint8_t output_mask;          /**< Mask năm LED của pha hiện tại. */
    WarningFeedback_Phase_t phase; /**< Pha pattern hiện tại. */
    uint32_t pattern_start_tick_ms; /**< Tick bắt đầu đợt warning liên tục. */
    uint32_t activation_count;    /**< Số lần chuyển từ không warning sang warning. */
} WarningFeedback_Status_t;

/** @brief Context tĩnh do application sở hữu. */
typedef struct
{
    WarningFeedback_Status_t status; /**< Trạng thái hiện tại của service. */
} WarningFeedback_t;

/** @brief Khởi tạo service ở trạng thái không warning. */
bool WarningFeedback_Initialize(WarningFeedback_t *feedback,
                                uint32_t current_tick_ms);

/**
 * @brief Tổng hợp source mask và cập nhật pattern theo thời gian, không blocking.
 *
 * Timer chỉ khởi động lại khi source mask chuyển từ 0 sang khác 0. Thay đổi giữa
 * các tổ hợp nguồn khác 0 không làm pattern bị giật hoặc bắt đầu lại.
 */
void WarningFeedback_Service(WarningFeedback_t *feedback,
                             uint32_t current_tick_ms,
                             uint8_t active_source_mask);

/** @brief Sao chép snapshot cho application hoặc debugger. */
void WarningFeedback_GetStatus(const WarningFeedback_t *feedback,
                               WarningFeedback_Status_t *output_status);

#ifdef __cplusplus
}
#endif

#endif /* USER_SERVICES_WARNING_FEEDBACK_WARNING_FEEDBACK_H_ */
