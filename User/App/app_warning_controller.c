/**
 * @file app_warning_controller.c
 * @brief Hiện thực điều phối warning thuần phần mềm của tầng App.
 */

#include "app_warning_controller.h"

#include <stddef.h>

/** @brief Tổng hợp manual, môi trường và shaking thành source bitmask. */
static uint8_t app_warning_controller_build_source_mask(
    const AppWarningController_Input_t *input,
    const EnvironmentFeedback_Status_t *environment_feedback)
{
    uint8_t source_mask = WARNING_FEEDBACK_SOURCE_NONE;

    if (environment_feedback->manual_warning_active)
    {
        source_mask |= WARNING_FEEDBACK_SOURCE_MANUAL;
    }
    if (input->monitoring_is_active &&
        environment_feedback->automatic_warning_active)
    {
        source_mask |= WARNING_FEEDBACK_SOURCE_ENVIRONMENT;
    }
    if (input->monitoring_is_active &&
        (input->motion != NULL) &&
        input->motion->data_is_fresh &&
        (input->motion->motion_state == MOTION_MONITOR_STATE_SHAKING))
    {
        source_mask |= WARNING_FEEDBACK_SOURCE_SHAKING;
    }

    return source_mask;
}

bool AppWarningController_Initialize(AppWarningController_t *controller,
                                    uint32_t current_tick_ms)
{
    bool environment_initialized;
    bool pattern_initialized;
    bool history_initialized;

    if (controller == NULL)
    {
        return false;
    }

    *controller = (AppWarningController_t){0};
    environment_initialized = EnvironmentFeedback_Initialize(
        &controller->environment_feedback);
    pattern_initialized = WarningFeedback_Initialize(
        &controller->output_pattern, current_tick_ms);
    history_initialized = WarningHistory_Initialize(&controller->history);

    controller->status.is_initialized = environment_initialized &&
        pattern_initialized && history_initialized;
    EnvironmentFeedback_GetStatus(&controller->environment_feedback,
                                  &controller->status.environment_feedback);
    WarningFeedback_GetStatus(&controller->output_pattern,
                              &controller->status.output_pattern);
    WarningHistory_GetStatus(&controller->history,
                             &controller->status.history);
    return controller->status.is_initialized;
}

bool AppWarningController_Service(
    AppWarningController_t *controller,
    const AppWarningController_Input_t *input,
    WarningHistory_Record_t *output_new_record)
{
    uint8_t source_mask;
    bool history_record_created;

    if ((controller == NULL) || (input == NULL) ||
        !controller->status.is_initialized)
    {
        return false;
    }

    EnvironmentFeedback_Service(
        &controller->environment_feedback,
        input->environment,
        input->has_new_environment_sample,
        input->manual_warning_requested,
        input->monitoring_is_active);
    EnvironmentFeedback_GetStatus(
        &controller->environment_feedback,
        &controller->status.environment_feedback);

    source_mask = app_warning_controller_build_source_mask(
        input, &controller->status.environment_feedback);
    history_record_created = WarningHistory_Service(
        &controller->history,
        input->current_tick_ms,
        source_mask,
        input->monitoring_is_active ? input->monitoring_session_id : 0U,
        output_new_record);
    WarningHistory_GetStatus(&controller->history,
                             &controller->status.history);

    WarningFeedback_Service(&controller->output_pattern,
                            input->current_tick_ms,
                            source_mask);
    WarningFeedback_GetStatus(&controller->output_pattern,
                              &controller->status.output_pattern);
    controller->status.effective_output_mask =
        controller->status.output_pattern.warning_active
            ? controller->status.output_pattern.output_mask
            : input->user_output_mask;

    return history_record_created;
}

const WarningHistory_t *AppWarningController_GetHistory(
    const AppWarningController_t *controller)
{
    if ((controller == NULL) || !controller->status.is_initialized)
    {
        return NULL;
    }
    return &controller->history;
}

void AppWarningController_GetStatus(
    const AppWarningController_t *controller,
    AppWarningController_Status_t *output_status)
{
    if ((controller != NULL) && (output_status != NULL))
    {
        *output_status = controller->status;
    }
}
