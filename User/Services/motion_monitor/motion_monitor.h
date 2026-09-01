/**
 * @file motion_monitor.h
 * @brief Thuật toán suy ra tư thế và mức chuyển động từ mẫu gia tốc ba trục.
 *
 * Module không truy cập I2C, GPIO hay driver ADXL345. Application chuyển mẫu mg
 * vào service và cung cấp trạng thái fresh của nguồn dữ liệu.
 */

#ifndef USER_SERVICES_MOTION_MONITOR_MOTION_MONITOR_H_
#define USER_SERVICES_MOTION_MONITOR_MOTION_MONITOR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/** @brief Kết quả khởi tạo motion monitor. */
typedef enum
{
    MOTION_MONITOR_RESULT_OK = 0,          /**< Cấu hình hợp lệ và monitor sẵn sàng. */
    MOTION_MONITOR_RESULT_INVALID_ARGUMENT /**< Context hoặc ngưỡng cấu hình không hợp lệ. */
} MotionMonitor_Result_t;

/** @brief Hướng của trục cảm biến đang gần song song nhất với trọng lực. */
typedef enum
{
    MOTION_MONITOR_ORIENTATION_UNKNOWN = 0, /**< Không trục nào đủ chiếm ưu thế. */
    MOTION_MONITOR_ORIENTATION_X_POSITIVE,  /**< Trọng lực đo được chủ yếu theo +X. */
    MOTION_MONITOR_ORIENTATION_X_NEGATIVE,  /**< Trọng lực đo được chủ yếu theo -X. */
    MOTION_MONITOR_ORIENTATION_Y_POSITIVE,  /**< Trọng lực đo được chủ yếu theo +Y. */
    MOTION_MONITOR_ORIENTATION_Y_NEGATIVE,  /**< Trọng lực đo được chủ yếu theo -Y. */
    MOTION_MONITOR_ORIENTATION_Z_POSITIVE,  /**< Trọng lực đo được chủ yếu theo +Z. */
    MOTION_MONITOR_ORIENTATION_Z_NEGATIVE   /**< Trọng lực đo được chủ yếu theo -Z. */
} MotionMonitor_Orientation_t;

/** @brief Phân loại chuyển động sau khi trừ vector trọng lực đã lọc. */
typedef enum
{
    MOTION_MONITOR_STATE_UNKNOWN = 0, /**< Chưa có mẫu đầu tiên. */
    MOTION_MONITOR_STATE_STILL,       /**< Tín hiệu động thấp đủ thời gian xác nhận. */
    MOTION_MONITOR_STATE_MOVING,      /**< Thiết bị đang có chuyển động vừa. */
    MOTION_MONITOR_STATE_SHAKING      /**< Thiết bị đang rung/lắc mạnh. */
} MotionMonitor_State_t;

/** @brief Một mẫu đầu vào độc lập với loại cảm biến cụ thể. */
typedef struct
{
    int32_t x_mg;            /**< Gia tốc trục X theo mg. */
    int32_t y_mg;            /**< Gia tốc trục Y theo mg. */
    int32_t z_mg;            /**< Gia tốc trục Z theo mg. */
    uint32_t sample_tick_ms; /**< Tick lúc nguồn hoàn thành mẫu. */
} MotionMonitor_Sample_t;

/** @brief Các ngưỡng hành vi do application sở hữu. */
typedef struct
{
    uint16_t gravity_filter_divisor;       /**< Hệ số IIR: gravity += delta/divisor. */
    uint32_t orientation_minimum_mg;       /**< Độ lớn tối thiểu của trục chiếm ưu thế. */
    uint32_t orientation_margin_mg;        /**< Khoảng cách tối thiểu với trục lớn thứ hai. */
    uint32_t still_threshold_mg;           /**< Ngưỡng động tối đa để bắt đầu xác nhận STILL. */
    uint32_t shaking_enter_threshold_mg;   /**< Ngưỡng chuyển ngay sang SHAKING. */
    uint32_t shaking_exit_threshold_mg;    /**< Phải xuống dưới ngưỡng này để thoát SHAKING. */
    uint32_t still_confirmation_ms;        /**< Thời gian yên liên tục trước khi báo STILL. */
    uint32_t shaking_exit_confirmation_ms; /**< Thời gian dịu liên tục trước khi thoát SHAKING. */
} MotionMonitor_Config_t;

/** @brief Dữ liệu đã suy ra để App, display hoặc feedback sử dụng. */
typedef struct
{
    int32_t total_acceleration_mg; /**< Độ lớn sqrt(x^2+y^2+z^2) của mẫu mới nhất. */
    MotionMonitor_Orientation_t orientation; /**< Tư thế theo trục trọng lực chiếm ưu thế. */
    MotionMonitor_State_t motion_state;      /**< STILL, MOVING hoặc SHAKING. */
    uint32_t last_update_tick_ms; /**< Tick của mẫu cuối đã xử lý. */
    uint32_t processed_sample_count; /**< Tổng số mẫu đã xử lý, tăng bão hòa. */
    bool has_data;               /**< Đã xử lý thành công ít nhất một mẫu. */
    bool data_is_fresh;          /**< Nguồn xác nhận mẫu cuối vẫn còn fresh. */
    bool orientation_changed;    /**< Tư thế đổi trong đúng lần Service hiện tại. */
    bool motion_state_changed;   /**< Mức chuyển động đổi trong đúng lần Service hiện tại. */
} MotionMonitor_Status_t;

/** @brief Context thuật toán, được application cấp phát tĩnh. */
typedef struct
{
    MotionMonitor_Config_t config; /**< Bản sao cấu hình đã duyệt. */
    MotionMonitor_Status_t status; /**< Kết quả công khai. */
    int32_t gravity_x_mg;          /**< Thành phần trọng lực X đã lọc. */
    int32_t gravity_y_mg;          /**< Thành phần trọng lực Y đã lọc. */
    int32_t gravity_z_mg;          /**< Thành phần trọng lực Z đã lọc. */
    uint32_t still_start_tick_ms;  /**< Tick bắt đầu khoảng yên hiện tại. */
    uint32_t shaking_exit_start_tick_ms; /**< Tick bắt đầu khoảng dịu sau SHAKING. */
    bool still_timer_is_active;          /**< Đang xác nhận chuyển sang STILL. */
    bool shaking_exit_timer_is_active;   /**< Đang xác nhận thoát SHAKING. */
    bool is_initialized;                 /**< Context đã nhận cấu hình hợp lệ. */
} MotionMonitor_t;

/** @brief Khởi tạo monitor và xóa toàn bộ lịch sử lọc/trạng thái. */
MotionMonitor_Result_t MotionMonitor_Initialize(
    MotionMonitor_t *monitor,
    const MotionMonitor_Config_t *config);

/**
 * @brief Tiến thuật toán đúng một bước và tùy chọn xử lý một mẫu mới.
 * @param current_tick_ms Tick hiện tại dùng cho các khoảng xác nhận.
 * @param new_sample NULL nếu vòng này không có mẫu mới.
 * @param source_data_is_fresh Trạng thái fresh do nguồn cảm biến cung cấp.
 */
void MotionMonitor_Service(MotionMonitor_t *monitor,
                           uint32_t current_tick_ms,
                           const MotionMonitor_Sample_t *new_sample,
                           bool source_data_is_fresh);

/** @brief Sao chép snapshot kết quả hiện tại để quan sát hoặc sử dụng. */
void MotionMonitor_GetStatus(const MotionMonitor_t *monitor,
                             MotionMonitor_Status_t *output_status);

#ifdef __cplusplus
}
#endif

#endif /* USER_SERVICES_MOTION_MONITOR_MOTION_MONITOR_H_ */
