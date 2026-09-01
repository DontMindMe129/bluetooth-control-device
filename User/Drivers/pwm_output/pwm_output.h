/**
 * @file pwm_output.h
 * @brief Driver PWM không blocking điều khiển độ rộng xung theo microsecond.
 *
 * CubeMX sở hữu cấu hình clock, prescaler, ARR, PWM mode, polarity và GPIO
 * alternate-function. Driver chỉ xác nhận cấu hình cần thiết, khởi động đúng
 * một channel và thay đổi CCR của channel đó. Driver không dừng hoặc cấu hình
 * lại timer nên có thể dùng timer đang được channel khác chia sẻ.
 */

#ifndef USER_DRIVERS_PWM_OUTPUT_PWM_OUTPUT_H_
#define USER_DRIVERS_PWM_OUTPUT_PWM_OUTPUT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "stm32f1xx_hal.h"

/** @brief Cơ sở thời gian dùng để đổi microsecond sang counter count. */
#define PWM_OUTPUT_MICROSECOND_BASE_HZ (1000000UL)

/** @brief Kết quả của thao tác khởi tạo hoặc thay đổi độ rộng xung. */
typedef enum
{
    PWM_OUTPUT_RESULT_OK = 0,          /**< Thao tác hoàn thành thành công. */
    PWM_OUTPUT_RESULT_INVALID_ARGUMENT,/**< Context hoặc cấu hình chứa con trỏ NULL. */
    PWM_OUTPUT_RESULT_INVALID_CONFIG,  /**< Timer, channel, clock hoặc PWM mode không phù hợp. */
    PWM_OUTPUT_RESULT_START_FAILED,    /**< HAL không thể khởi động PWM channel. */
    PWM_OUTPUT_RESULT_NOT_INITIALIZED, /**< Context chưa được khởi tạo thành công. */
    PWM_OUTPUT_RESULT_PULSE_OUT_OF_RANGE /**< Độ rộng xung vượt khả năng ARR hiện tại. */
} PwmOutput_Result_t;

/** @brief Cấu hình phần cứng của một ngõ ra PWM. */
typedef struct
{
    TIM_HandleTypeDef *timer;    /**< HAL timer handle đã được CubeMX khởi tạo. */
    TIM_TypeDef *timer_instance; /**< Instance mong đợi để phát hiện ghép sai handle. */
    uint32_t timer_channel;      /**< Một trong TIM_CHANNEL_1 đến TIM_CHANNEL_4. */
} PwmOutput_Config_t;

/** @brief Snapshot công khai phục vụ tầng ứng dụng và debugger. */
typedef struct
{
    bool is_initialized;          /**< Driver đã khởi tạo và start PWM thành công. */
    bool is_running;              /**< Channel PWM đang được giữ ở trạng thái chạy. */
    uint32_t counter_clock_hz;    /**< Clock counter sau prescaler. */
    uint32_t ticks_per_microsecond; /**< Số counter count trong một microsecond. */
    uint32_t period_counts;       /**< Chu kỳ timer bằng ARR + 1. */
    uint32_t period_us;           /**< Chu kỳ timer quy đổi sang microsecond. */
    uint32_t pulse_width_us;      /**< Độ rộng xung được yêu cầu gần nhất. */
    uint32_t compare_counts;      /**< Giá trị đang được ghi vào CCR của channel. */
    HAL_StatusTypeDef last_hal_status; /**< Kết quả HAL gần nhất khi start channel. */
    PwmOutput_Result_t last_result; /**< Kết quả API gần nhất. */
} PwmOutput_Status_t;

/** @brief Context đầy đủ do caller cấp phát tĩnh cho một ngõ PWM. */
typedef struct
{
    PwmOutput_Config_t config; /**< Bản sao cấu hình dùng trong suốt vòng đời instance. */
    PwmOutput_Status_t status; /**< Trạng thái công khai có thể sao chép để debug. */
} PwmOutput_t;

/**
 * @brief Xác nhận cấu hình, đặt CCR bằng 0 và khởi động PWM channel.
 *
 * Driver yêu cầu counter chạy lên, PWM mode 1, polarity active-high và clock
 * counter là bội số nguyên của 1 MHz. Hàm không thay đổi PSC, ARR hoặc channel
 * khác. Sau khi thành công, output chạy với pulse 0 us cho đến khi caller đặt
 * giá trị mới.
 *
 * @param output Context do caller cấp phát tĩnh.
 * @param config Timer handle, instance và channel đã được CubeMX cấu hình.
 * @return Kết quả kiểm tra cấu hình hoặc HAL_TIM_PWM_Start().
 */
PwmOutput_Result_t PwmOutput_Initialize(
    PwmOutput_t *output,
    const PwmOutput_Config_t *config);

/**
 * @brief Đặt độ rộng mức active của xung PWM theo microsecond.
 *
 * Giá trị 0 làm output luôn inactive nhưng vẫn giữ timer/channel chạy để không
 * ảnh hưởng tài nguyên dùng chung. Driver không cung cấp API stop timer.
 *
 * @param output Context đã khởi tạo thành công.
 * @param pulse_width_us Độ rộng xung từ 0 đến giới hạn do ARR cho phép.
 * @return PWM_OUTPUT_RESULT_OK nếu CCR đã được cập nhật.
 */
PwmOutput_Result_t PwmOutput_SetPulseWidthUs(
    PwmOutput_t *output,
    uint32_t pulse_width_us);

/**
 * @brief Sao chép snapshot hiện tại cho application hoặc debugger.
 * @param output Context cần đọc.
 * @param output_status Vùng nhớ nhận snapshot; NULL sẽ được bỏ qua.
 */
void PwmOutput_GetStatus(const PwmOutput_t *output,
                         PwmOutput_Status_t *output_status);

#ifdef __cplusplus
}
#endif

#endif /* USER_DRIVERS_PWM_OUTPUT_PWM_OUTPUT_H_ */
