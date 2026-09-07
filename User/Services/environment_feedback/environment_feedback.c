/**
 * @file environment_feedback.c
 * @brief Hiện thực policy pattern môi trường không blocking và không phụ thuộc HAL.
 */

#include "environment_feedback.h"

#include <stddef.h>

/** @brief Chọn pattern từ manual warning, độ mới dữ liệu và điều kiện môi trường. */
static EnvironmentFeedback_Pattern_t environment_feedback_select_pattern(
    const EnvironmentFeedback_t *feedback,
    const EnvironmentMonitor_Status_t *environment_status)
{
    if (feedback->status.manual_warning_active)
    {
        return ENVIRONMENT_FEEDBACK_PATTERN_WARM_AND_HUMID;
    }

    if ((environment_status == NULL) ||
        (environment_status->data_state != ENVIRONMENT_DATA_FRESH))
    {
        return ENVIRONMENT_FEEDBACK_PATTERN_OFF;
    }

    switch (environment_status->condition)
    {
        case ENVIRONMENT_CONDITION_NORMAL:
            return ENVIRONMENT_FEEDBACK_PATTERN_NORMAL;

        case ENVIRONMENT_CONDITION_WARM:
            return ENVIRONMENT_FEEDBACK_PATTERN_WARM;

        case ENVIRONMENT_CONDITION_HUMID:
            return ENVIRONMENT_FEEDBACK_PATTERN_HUMID;

        case ENVIRONMENT_CONDITION_WARM_AND_HUMID:
            return ENVIRONMENT_FEEDBACK_PATTERN_WARM_AND_HUMID;

        case ENVIRONMENT_CONDITION_UNKNOWN:
        default:
            return ENVIRONMENT_FEEDBACK_PATTERN_OFF;
    }
}

bool EnvironmentFeedback_Initialize(EnvironmentFeedback_t *feedback)
{
    if (feedback == NULL)
    {
        return false;
    }

    *feedback = (EnvironmentFeedback_t){0};
    feedback->status.pattern = ENVIRONMENT_FEEDBACK_PATTERN_OFF;
    return true;
}

void EnvironmentFeedback_Service(
    EnvironmentFeedback_t *feedback,
    const EnvironmentMonitor_Status_t *environment_status,
    bool has_new_environment_sample,
    bool manual_warning_requested,
    bool automatic_warning_enabled)
{
    EnvironmentFeedback_Pattern_t selected_pattern;

    if (feedback == NULL)
    {
        return;
    }

    if (has_new_environment_sample)
    {
        feedback->status.manual_warning_active = false;
    }

    if (manual_warning_requested)
    {
        feedback->status.manual_warning_active = true;
    }

    selected_pattern =
        environment_feedback_select_pattern(feedback, environment_status);
    feedback->status.pattern = selected_pattern;
    feedback->status.automatic_warning_active =
        automatic_warning_enabled &&
        (environment_status != NULL) &&
        (environment_status->data_state == ENVIRONMENT_DATA_FRESH) &&
        (environment_status->condition ==
         ENVIRONMENT_CONDITION_WARM_AND_HUMID);
    feedback->status.warning_active =
        feedback->status.manual_warning_active ||
        feedback->status.automatic_warning_active;
}

void EnvironmentFeedback_GetStatus(
    const EnvironmentFeedback_t *feedback,
    EnvironmentFeedback_Status_t *output_status)
{
    if ((feedback != NULL) && (output_status != NULL))
    {
        *output_status = feedback->status;
    }
}
