/**
 * @file warning_feedback.c
 * @brief Hiện thực bộ tổng hợp warning và pattern năm LED không blocking.
 */

#include "warning_feedback.h"

#include <limits.h>
#include <stddef.h>

#define WARNING_FEEDBACK_VALID_SOURCE_MASK \
    ((uint8_t)(WARNING_FEEDBACK_SOURCE_MANUAL | \
               WARNING_FEEDBACK_SOURCE_ENVIRONMENT | \
               WARNING_FEEDBACK_SOURCE_SHAKING))

/** @brief Chọn phase và output mask từ elapsed time trong frame một giây. */
static void warning_feedback_update_pattern(WarningFeedback_t *feedback,
                                            uint32_t current_tick_ms)
{
    const uint32_t phase_time_ms = WARNING_FEEDBACK_ALTERNATE_PHASE_MS;
    const uint32_t frame_time_ms =
        (4UL * WARNING_FEEDBACK_ALTERNATE_PHASE_MS) +
        WARNING_FEEDBACK_OFF_PHASE_MS;
    uint32_t elapsed_in_frame_ms =
        (uint32_t)(current_tick_ms -
                   feedback->status.pattern_start_tick_ms) %
        frame_time_ms;

    if (elapsed_in_frame_ms < phase_time_ms)
    {
        feedback->status.phase = WARNING_FEEDBACK_PHASE_ODD_1;
        feedback->status.output_mask = WARNING_FEEDBACK_ODD_OUTPUT_MASK;
    }
    else if (elapsed_in_frame_ms < (2UL * phase_time_ms))
    {
        feedback->status.phase = WARNING_FEEDBACK_PHASE_EVEN_1;
        feedback->status.output_mask = WARNING_FEEDBACK_EVEN_OUTPUT_MASK;
    }
    else if (elapsed_in_frame_ms < (3UL * phase_time_ms))
    {
        feedback->status.phase = WARNING_FEEDBACK_PHASE_ODD_2;
        feedback->status.output_mask = WARNING_FEEDBACK_ODD_OUTPUT_MASK;
    }
    else if (elapsed_in_frame_ms < (4UL * phase_time_ms))
    {
        feedback->status.phase = WARNING_FEEDBACK_PHASE_EVEN_2;
        feedback->status.output_mask = WARNING_FEEDBACK_EVEN_OUTPUT_MASK;
    }
    else
    {
        feedback->status.phase = WARNING_FEEDBACK_PHASE_ALL_OFF;
        feedback->status.output_mask = 0U;
    }
}

bool WarningFeedback_Initialize(WarningFeedback_t *feedback,
                                uint32_t current_tick_ms)
{
    if (feedback == NULL)
    {
        return false;
    }

    *feedback = (WarningFeedback_t){0};
    feedback->status.is_initialized = true;
    feedback->status.phase = WARNING_FEEDBACK_PHASE_INACTIVE;
    feedback->status.pattern_start_tick_ms = current_tick_ms;
    return true;
}

void WarningFeedback_Service(WarningFeedback_t *feedback,
                             uint32_t current_tick_ms,
                             uint8_t active_source_mask)
{
    bool was_active;

    if ((feedback == NULL) || !feedback->status.is_initialized)
    {
        return;
    }

    active_source_mask &= WARNING_FEEDBACK_VALID_SOURCE_MASK;
    was_active = feedback->status.warning_active;
    feedback->status.active_source_mask = active_source_mask;
    feedback->status.warning_active = (active_source_mask != 0U);

    if (!feedback->status.warning_active)
    {
        feedback->status.phase = WARNING_FEEDBACK_PHASE_INACTIVE;
        feedback->status.output_mask = 0U;
        return;
    }

    if (!was_active)
    {
        feedback->status.pattern_start_tick_ms = current_tick_ms;
        if (feedback->status.activation_count < UINT32_MAX)
        {
            feedback->status.activation_count++;
        }
    }

    warning_feedback_update_pattern(feedback, current_tick_ms);
}

void WarningFeedback_GetStatus(const WarningFeedback_t *feedback,
                               WarningFeedback_Status_t *output_status)
{
    if ((feedback != NULL) && (output_status != NULL))
    {
        *output_status = feedback->status;
    }
}
