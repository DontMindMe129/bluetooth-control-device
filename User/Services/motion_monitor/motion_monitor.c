/**
 * @file motion_monitor.c
 * @brief Hiện thực phân loại tư thế/chuyển động bằng số nguyên, không blocking.
 */

#include "motion_monitor.h"

#include <limits.h>
#include <stddef.h>

/** @brief Kiểm tra elapsed time an toàn khi tick uint32_t tràn số. */
static bool motion_monitor_time_has_elapsed(uint32_t current_tick_ms,
                                            uint32_t start_tick_ms,
                                            uint32_t duration_ms)
{
    return ((uint32_t)(current_tick_ms - start_tick_ms) >= duration_ms);
}

/** @brief Trị tuyệt đối int32 trả về uint32, kể cả INT32_MIN. */
static uint32_t motion_monitor_absolute_i32(int32_t value)
{
    return (value < 0) ? (uint32_t)(-(int64_t)value) : (uint32_t)value;
}

/** @brief Trị tuyệt đối int64 bão hòa về UINT32_MAX để so sánh ngưỡng. */
static uint32_t motion_monitor_absolute_i64_saturated(int64_t value)
{
    uint64_t magnitude = (value < 0) ? (uint64_t)(-value) : (uint64_t)value;

    return (magnitude > UINT32_MAX) ? UINT32_MAX : (uint32_t)magnitude;
}

/** @brief Căn bậc hai nguyên floor(sqrt(value)); tối đa 32 vòng lặp hữu hạn. */
static uint32_t motion_monitor_integer_sqrt_u64(uint64_t value)
{
    uint64_t remainder = value;
    uint64_t result = 0U;
    uint64_t bit = (uint64_t)1U << 62U;

    while (bit > remainder)
    {
        bit >>= 2U;
    }

    while (bit != 0U)
    {
        if (remainder >= (result + bit))
        {
            remainder -= result + bit;
            result = (result >> 1U) + bit;
        }
        else
        {
            result >>= 1U;
        }
        bit >>= 2U;
    }

    return (result > UINT32_MAX) ? UINT32_MAX : (uint32_t)result;
}

/** @brief Tính độ lớn vector ba trục bằng toán số nguyên 64-bit. */
static int32_t motion_monitor_calculate_total_mg(
    const MotionMonitor_Sample_t *sample)
{
    uint64_t x = motion_monitor_absolute_i32(sample->x_mg);
    uint64_t y = motion_monitor_absolute_i32(sample->y_mg);
    uint64_t z = motion_monitor_absolute_i32(sample->z_mg);
    uint32_t magnitude = motion_monitor_integer_sqrt_u64(
        (x * x) + (y * y) + (z * z));

    return (magnitude > INT32_MAX) ? INT32_MAX : (int32_t)magnitude;
}

/** @brief Cập nhật một trục của bộ lọc trọng lực IIR bằng trung gian int64. */
static int32_t motion_monitor_filter_gravity_axis(int32_t gravity_mg,
                                                  int32_t sample_mg,
                                                  uint16_t divisor)
{
    int64_t delta = (int64_t)sample_mg - (int64_t)gravity_mg;

    return (int32_t)((int64_t)gravity_mg +
                     (delta / (int64_t)divisor));
}

/** @brief Tìm trục trọng lực lớn nhất và kiểm tra điều kiện chiếm ưu thế. */
static MotionMonitor_Orientation_t motion_monitor_classify_orientation(
    const MotionMonitor_t *monitor)
{
    const uint32_t axis_magnitude[3] =
    {
        motion_monitor_absolute_i32(monitor->gravity_x_mg),
        motion_monitor_absolute_i32(monitor->gravity_y_mg),
        motion_monitor_absolute_i32(monitor->gravity_z_mg)
    };
    uint8_t dominant_axis = 0U;
    uint8_t index;
    uint32_t second_largest = 0U;

    for (index = 1U; index < 3U; index++)
    {
        if (axis_magnitude[index] > axis_magnitude[dominant_axis])
        {
            dominant_axis = index;
        }
    }

    for (index = 0U; index < 3U; index++)
    {
        if ((index != dominant_axis) &&
            (axis_magnitude[index] > second_largest))
        {
            second_largest = axis_magnitude[index];
        }
    }

    if ((axis_magnitude[dominant_axis] <
         monitor->config.orientation_minimum_mg) ||
        ((axis_magnitude[dominant_axis] - second_largest) <
         monitor->config.orientation_margin_mg))
    {
        return MOTION_MONITOR_ORIENTATION_UNKNOWN;
    }

    if (dominant_axis == 0U)
    {
        return (monitor->gravity_x_mg >= 0) ?
            MOTION_MONITOR_ORIENTATION_X_POSITIVE :
            MOTION_MONITOR_ORIENTATION_X_NEGATIVE;
    }
    if (dominant_axis == 1U)
    {
        return (monitor->gravity_y_mg >= 0) ?
            MOTION_MONITOR_ORIENTATION_Y_POSITIVE :
            MOTION_MONITOR_ORIENTATION_Y_NEGATIVE;
    }
    return (monitor->gravity_z_mg >= 0) ?
        MOTION_MONITOR_ORIENTATION_Z_POSITIVE :
        MOTION_MONITOR_ORIENTATION_Z_NEGATIVE;
}

/** @brief Lấy độ lệch động lớn nhất trên ba trục so với gravity trước lọc. */
static uint32_t motion_monitor_calculate_motion_level(
    const MotionMonitor_t *monitor,
    const MotionMonitor_Sample_t *sample)
{
    uint32_t x = motion_monitor_absolute_i64_saturated(
        (int64_t)sample->x_mg - monitor->gravity_x_mg);
    uint32_t y = motion_monitor_absolute_i64_saturated(
        (int64_t)sample->y_mg - monitor->gravity_y_mg);
    uint32_t z = motion_monitor_absolute_i64_saturated(
        (int64_t)sample->z_mg - monitor->gravity_z_mg);
    uint32_t maximum = (x > y) ? x : y;

    return (z > maximum) ? z : maximum;
}

/** @brief Cập nhật state chuyển động với hysteresis và thời gian xác nhận. */
static void motion_monitor_update_motion_state(MotionMonitor_t *monitor,
                                               uint32_t current_tick_ms,
                                               uint32_t motion_level_mg)
{
    if (motion_level_mg >= monitor->config.shaking_enter_threshold_mg)
    {
        monitor->status.motion_state = MOTION_MONITOR_STATE_SHAKING;
        monitor->still_timer_is_active = false;
        monitor->shaking_exit_timer_is_active = false;
        return;
    }

    if (motion_level_mg <= monitor->config.still_threshold_mg)
    {
        if (!monitor->still_timer_is_active)
        {
            monitor->still_timer_is_active = true;
            monitor->still_start_tick_ms = current_tick_ms;
        }
    }
    else
    {
        monitor->still_timer_is_active = false;
    }

    if (monitor->status.motion_state == MOTION_MONITOR_STATE_SHAKING)
    {
        if (motion_level_mg <= monitor->config.shaking_exit_threshold_mg)
        {
            if (!monitor->shaking_exit_timer_is_active)
            {
                monitor->shaking_exit_timer_is_active = true;
                monitor->shaking_exit_start_tick_ms = current_tick_ms;
            }
            else if (motion_monitor_time_has_elapsed(
                         current_tick_ms,
                         monitor->shaking_exit_start_tick_ms,
                         monitor->config.shaking_exit_confirmation_ms))
            {
                monitor->status.motion_state = MOTION_MONITOR_STATE_MOVING;
                monitor->shaking_exit_timer_is_active = false;
            }
        }
        else
        {
            monitor->shaking_exit_timer_is_active = false;
        }
    }
    else if (motion_level_mg > monitor->config.still_threshold_mg)
    {
        monitor->status.motion_state = MOTION_MONITOR_STATE_MOVING;
    }

    if (monitor->still_timer_is_active &&
        motion_monitor_time_has_elapsed(
            current_tick_ms,
            monitor->still_start_tick_ms,
            monitor->config.still_confirmation_ms))
    {
        monitor->status.motion_state = MOTION_MONITOR_STATE_STILL;
        monitor->shaking_exit_timer_is_active = false;
    }
}

MotionMonitor_Result_t MotionMonitor_Initialize(
    MotionMonitor_t *monitor,
    const MotionMonitor_Config_t *config)
{
    if (monitor == NULL)
    {
        return MOTION_MONITOR_RESULT_INVALID_ARGUMENT;
    }

    *monitor = (MotionMonitor_t){0};
    monitor->status.orientation = MOTION_MONITOR_ORIENTATION_UNKNOWN;
    monitor->status.motion_state = MOTION_MONITOR_STATE_UNKNOWN;

    if ((config == NULL) ||
        (config->gravity_filter_divisor < 2U) ||
        (config->orientation_minimum_mg == 0U) ||
        (config->still_threshold_mg == 0U) ||
        (config->shaking_enter_threshold_mg <=
         config->still_threshold_mg) ||
        (config->shaking_exit_threshold_mg <=
         config->still_threshold_mg) ||
        (config->shaking_exit_threshold_mg >=
         config->shaking_enter_threshold_mg) ||
        (config->still_confirmation_ms == 0UL) ||
        (config->shaking_exit_confirmation_ms == 0UL))
    {
        return MOTION_MONITOR_RESULT_INVALID_ARGUMENT;
    }

    monitor->config = *config;
    monitor->is_initialized = true;
    return MOTION_MONITOR_RESULT_OK;
}

void MotionMonitor_Service(MotionMonitor_t *monitor,
                           uint32_t current_tick_ms,
                           const MotionMonitor_Sample_t *new_sample,
                           bool source_data_is_fresh)
{
    MotionMonitor_Orientation_t previous_orientation;
    MotionMonitor_State_t previous_motion_state;

    if ((monitor == NULL) || !monitor->is_initialized)
    {
        return;
    }

    monitor->status.orientation_changed = false;
    monitor->status.motion_state_changed = false;
    monitor->status.data_is_fresh =
        monitor->status.has_data && source_data_is_fresh;

    if (new_sample == NULL)
    {
        return;
    }

    previous_orientation = monitor->status.orientation;
    previous_motion_state = monitor->status.motion_state;
    monitor->status.total_acceleration_mg =
        motion_monitor_calculate_total_mg(new_sample);
    monitor->status.last_update_tick_ms = new_sample->sample_tick_ms;

    if (!monitor->status.has_data)
    {
        monitor->gravity_x_mg = new_sample->x_mg;
        monitor->gravity_y_mg = new_sample->y_mg;
        monitor->gravity_z_mg = new_sample->z_mg;
        monitor->status.motion_state = MOTION_MONITOR_STATE_STILL;
        monitor->status.has_data = true;
    }
    else
    {
        const uint32_t motion_level_mg =
            motion_monitor_calculate_motion_level(monitor, new_sample);
        monitor->gravity_x_mg = motion_monitor_filter_gravity_axis(
            monitor->gravity_x_mg,
            new_sample->x_mg,
            monitor->config.gravity_filter_divisor);
        monitor->gravity_y_mg = motion_monitor_filter_gravity_axis(
            monitor->gravity_y_mg,
            new_sample->y_mg,
            monitor->config.gravity_filter_divisor);
        monitor->gravity_z_mg = motion_monitor_filter_gravity_axis(
            monitor->gravity_z_mg,
            new_sample->z_mg,
            monitor->config.gravity_filter_divisor);
        motion_monitor_update_motion_state(monitor,
                                           current_tick_ms,
                                           motion_level_mg);
    }

    monitor->status.orientation =
        motion_monitor_classify_orientation(monitor);
    monitor->status.data_is_fresh = source_data_is_fresh;
    monitor->status.orientation_changed =
        (monitor->status.orientation != previous_orientation);
    monitor->status.motion_state_changed =
        (monitor->status.motion_state != previous_motion_state);
    if (monitor->status.processed_sample_count < UINT32_MAX)
    {
        monitor->status.processed_sample_count++;
    }
}

void MotionMonitor_GetStatus(const MotionMonitor_t *monitor,
                             MotionMonitor_Status_t *output_status)
{
    if ((monitor != NULL) && (output_status != NULL))
    {
        *output_status = monitor->status;
    }
}
