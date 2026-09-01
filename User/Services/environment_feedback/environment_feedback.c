/**
 * @file environment_feedback.c
 * @brief Hiện thực policy pattern môi trường không blocking và không phụ thuộc HAL.
 */

#include "environment_feedback.h"

#include <stddef.h>

/** @brief Context nội bộ chỉ được truy cập trong main context. */
typedef struct
{
    EnvironmentFeedback_Status_t status;
} EnvironmentFeedback_InternalContext_t;

static EnvironmentFeedback_InternalContext_t s_environment_feedback;

/** @brief Chọn pattern từ manual warning, độ mới dữ liệu và điều kiện môi trường. */
static EnvironmentFeedback_Pattern_t environment_feedback_select_pattern(
    const EnvironmentMonitor_Status_t *environment_status)
{
    if (s_environment_feedback.status.manual_warning_active)
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

void EnvironmentFeedback_Initialize(void)
{
    s_environment_feedback = (EnvironmentFeedback_InternalContext_t){0};
    s_environment_feedback.status.pattern = ENVIRONMENT_FEEDBACK_PATTERN_OFF;
}

void EnvironmentFeedback_Service(
    const EnvironmentMonitor_Status_t *environment_status,
    bool has_new_environment_sample,
    bool manual_warning_requested)
{
    EnvironmentFeedback_Pattern_t selected_pattern;

    if (has_new_environment_sample)
    {
        s_environment_feedback.status.manual_warning_active = false;
    }

    if (manual_warning_requested)
    {
        s_environment_feedback.status.manual_warning_active = true;
    }

    selected_pattern =
        environment_feedback_select_pattern(environment_status);
    s_environment_feedback.status.pattern = selected_pattern;
    s_environment_feedback.status.automatic_warning_active =
        (environment_status != NULL) &&
        (environment_status->data_state == ENVIRONMENT_DATA_FRESH) &&
        (environment_status->condition ==
         ENVIRONMENT_CONDITION_WARM_AND_HUMID);
    s_environment_feedback.status.warning_active =
        s_environment_feedback.status.manual_warning_active ||
        s_environment_feedback.status.automatic_warning_active;
}

void EnvironmentFeedback_GetStatus(
    EnvironmentFeedback_Status_t *output_status)
{
    if (output_status != NULL)
    {
        *output_status = s_environment_feedback.status;
    }
}
