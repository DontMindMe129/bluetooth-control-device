/**
 * @file app_display_controller.c
 * @brief Hiện thực điều phối model và render năm page OLED.
 */

#include "app_display_controller.h"

#include <stddef.h>

/** @brief Đồng bộ trạng thái năm display service vào snapshot tập trung. */
static void app_display_controller_sync_status(
    AppDisplayController_t *controller)
{
    EnvironmentDisplay_GetStatus(&controller->environment,
                                 &controller->status.environment);
    EnvironmentHistoryDisplay_GetStatus(
        &controller->environment_history,
        &controller->status.environment_history);
    MotionDisplay_GetStatus(&controller->motion,
                            &controller->status.motion);
    WarningHistoryDisplay_GetStatus(&controller->warning_history,
                                    &controller->status.warning_history);
    OutputDisplay_GetStatus(&controller->outputs,
                            &controller->status.outputs);
}

bool AppDisplayController_Initialize(
    AppDisplayController_t *controller,
    const EnvironmentHistory_t *environment_history,
    const WarningHistory_t *warning_history)
{
    EnvironmentHistory_Status_t environment_history_status = {0};
    WarningHistory_Status_t warning_history_status = {0};
    bool environment_initialized;
    bool environment_history_initialized;
    bool motion_initialized;
    bool warning_history_initialized;
    bool outputs_initialized;

    if (controller == NULL)
    {
        return false;
    }

    *controller = (AppDisplayController_t){0};
    if ((environment_history == NULL) || (warning_history == NULL))
    {
        return false;
    }

    EnvironmentHistory_GetStatus(environment_history,
                                 &environment_history_status);
    WarningHistory_GetStatus(warning_history, &warning_history_status);
    if (!environment_history_status.is_initialized ||
        !warning_history_status.is_initialized)
    {
        return false;
    }

    environment_initialized =
        EnvironmentDisplay_Initialize(&controller->environment);
    environment_history_initialized = EnvironmentHistoryDisplay_Initialize(
        &controller->environment_history, environment_history);
    motion_initialized = MotionDisplay_Initialize(&controller->motion);
    warning_history_initialized = WarningHistoryDisplay_Initialize(
        &controller->warning_history, warning_history);
    outputs_initialized = OutputDisplay_Initialize(&controller->outputs);

    controller->status.is_initialized = environment_initialized &&
        environment_history_initialized && motion_initialized &&
        warning_history_initialized && outputs_initialized;
    controller->status.last_render_result =
        APP_DISPLAY_CONTROLLER_RENDER_IDLE;
    app_display_controller_sync_status(controller);
    return controller->status.is_initialized;
}

void AppDisplayController_Update(
    AppDisplayController_t *controller,
    const AppDisplayController_Input_t *input)
{
    if ((controller == NULL) || (input == NULL) ||
        !controller->status.is_initialized ||
        (input->environment == NULL) ||
        (input->environment_feedback == NULL) ||
        (input->adxl345 == NULL) || (input->motion == NULL) ||
        (input->outputs == NULL))
    {
        return;
    }

    EnvironmentDisplay_Update(&controller->environment,
                              input->environment,
                              input->environment_feedback,
                              input->monitoring_is_active);
    EnvironmentHistoryDisplay_Update(&controller->environment_history,
                                     input->environment,
                                     input->monitoring_is_active);
    MotionDisplay_Update(&controller->motion,
                         input->adxl345,
                         input->motion);
    WarningHistoryDisplay_Update(&controller->warning_history,
                                 input->monitoring_is_active);
    OutputDisplay_Update(&controller->outputs, input->outputs);
    app_display_controller_sync_status(controller);
}

void AppDisplayController_RequestRedraw(AppDisplayController_t *controller,
                                        App_DisplayPage_t page)
{
    if ((controller == NULL) || !controller->status.is_initialized)
    {
        return;
    }

    switch (page)
    {
        case APP_DISPLAY_PAGE_ENVIRONMENT:
            EnvironmentDisplay_RequestRedraw(&controller->environment);
            break;
        case APP_DISPLAY_PAGE_HISTORY:
            EnvironmentHistoryDisplay_RequestRedraw(
                &controller->environment_history);
            break;
        case APP_DISPLAY_PAGE_MOTION:
            MotionDisplay_RequestRedraw(&controller->motion);
            break;
        case APP_DISPLAY_PAGE_WARNING_HISTORY:
            WarningHistoryDisplay_RequestRedraw(
                &controller->warning_history);
            break;
        case APP_DISPLAY_PAGE_OUTPUTS:
            OutputDisplay_RequestRedraw(&controller->outputs);
            break;
        case APP_DISPLAY_PAGE_COUNT:
        default:
            break;
    }
    app_display_controller_sync_status(controller);
}

void AppDisplayController_EnterPage(AppDisplayController_t *controller,
                                    App_DisplayPage_t page)
{
    if ((controller == NULL) || !controller->status.is_initialized)
    {
        return;
    }

    if (page == APP_DISPLAY_PAGE_HISTORY)
    {
        EnvironmentHistoryDisplay_EnterPage(
            &controller->environment_history);
    }
    else if (page == APP_DISPLAY_PAGE_WARNING_HISTORY)
    {
        WarningHistoryDisplay_EnterPage(&controller->warning_history);
    }
    AppDisplayController_RequestRedraw(controller, page);
}

bool AppDisplayController_HandleHistoryInput(
    AppDisplayController_t *controller,
    App_DisplayPage_t page,
    bool up_requested,
    bool down_requested,
    bool ok_requested)
{
    bool changed = false;

    if ((controller == NULL) || !controller->status.is_initialized)
    {
        return false;
    }

    if (page == APP_DISPLAY_PAGE_HISTORY)
    {
        changed = EnvironmentHistoryDisplay_HandleInput(
            &controller->environment_history,
            up_requested,
            down_requested,
            ok_requested);
    }
    else if (page == APP_DISPLAY_PAGE_WARNING_HISTORY)
    {
        changed = WarningHistoryDisplay_HandleInput(
            &controller->warning_history,
            up_requested,
            down_requested,
            ok_requested);
    }
    app_display_controller_sync_status(controller);
    return changed;
}

AppDisplayController_RenderResult_t AppDisplayController_RenderIfDue(
    AppDisplayController_t *controller,
    App_DisplayPage_t page,
    const MonoGraphics_Canvas_t *canvas,
    AppDisplayController_ClearCanvasFunction_t clear_canvas,
    uint32_t current_tick_ms)
{
    AppDisplayController_RenderResult_t result =
        APP_DISPLAY_CONTROLLER_RENDER_IDLE;

    if (controller == NULL)
    {
        return APP_DISPLAY_CONTROLLER_RENDER_FAILED;
    }
    if (!controller->status.is_initialized || (canvas == NULL) ||
        (clear_canvas == NULL))
    {
        controller->status.last_render_result =
            APP_DISPLAY_CONTROLLER_RENDER_FAILED;
        return controller->status.last_render_result;
    }

    switch (page)
    {
        case APP_DISPLAY_PAGE_ENVIRONMENT:
        {
            EnvironmentDisplay_RenderResult_t page_result =
                EnvironmentDisplay_RenderIfDue(
                    &controller->environment,
                    canvas,
                    clear_canvas,
                    current_tick_ms);
            if (page_result == ENVIRONMENT_DISPLAY_RENDER_DRAWN)
                result = APP_DISPLAY_CONTROLLER_RENDER_DRAWN;
            else if (page_result == ENVIRONMENT_DISPLAY_RENDER_FAILED)
                result = APP_DISPLAY_CONTROLLER_RENDER_FAILED;
            break;
        }
        case APP_DISPLAY_PAGE_HISTORY:
        {
            EnvironmentHistoryDisplay_RenderResult_t page_result =
                EnvironmentHistoryDisplay_RenderIfDue(
                    &controller->environment_history,
                    canvas,
                    clear_canvas,
                    current_tick_ms);
            if (page_result == ENVIRONMENT_HISTORY_DISPLAY_RENDER_DRAWN)
                result = APP_DISPLAY_CONTROLLER_RENDER_DRAWN;
            else if (page_result == ENVIRONMENT_HISTORY_DISPLAY_RENDER_FAILED)
                result = APP_DISPLAY_CONTROLLER_RENDER_FAILED;
            break;
        }
        case APP_DISPLAY_PAGE_MOTION:
        {
            MotionDisplay_RenderResult_t page_result =
                MotionDisplay_RenderIfDue(&controller->motion,
                                          canvas,
                                          clear_canvas,
                                          current_tick_ms);
            if (page_result == MOTION_DISPLAY_RENDER_DRAWN)
                result = APP_DISPLAY_CONTROLLER_RENDER_DRAWN;
            else if (page_result == MOTION_DISPLAY_RENDER_FAILED)
                result = APP_DISPLAY_CONTROLLER_RENDER_FAILED;
            break;
        }
        case APP_DISPLAY_PAGE_WARNING_HISTORY:
        {
            WarningHistoryDisplay_RenderResult_t page_result =
                WarningHistoryDisplay_RenderIfDue(
                    &controller->warning_history,
                    canvas,
                    clear_canvas,
                    current_tick_ms);
            if (page_result == WARNING_HISTORY_DISPLAY_RENDER_DRAWN)
                result = APP_DISPLAY_CONTROLLER_RENDER_DRAWN;
            else if (page_result == WARNING_HISTORY_DISPLAY_RENDER_FAILED)
                result = APP_DISPLAY_CONTROLLER_RENDER_FAILED;
            break;
        }
        case APP_DISPLAY_PAGE_OUTPUTS:
        {
            OutputDisplay_RenderResult_t page_result =
                OutputDisplay_RenderIfDue(&controller->outputs,
                                          canvas,
                                          clear_canvas,
                                          current_tick_ms);
            if (page_result == OUTPUT_DISPLAY_RENDER_DRAWN)
                result = APP_DISPLAY_CONTROLLER_RENDER_DRAWN;
            else if (page_result == OUTPUT_DISPLAY_RENDER_FAILED)
                result = APP_DISPLAY_CONTROLLER_RENDER_FAILED;
            break;
        }
        case APP_DISPLAY_PAGE_COUNT:
        default:
            result = APP_DISPLAY_CONTROLLER_RENDER_FAILED;
            break;
    }

    controller->status.last_render_result = result;
    app_display_controller_sync_status(controller);
    return result;
}

void AppDisplayController_GetStatus(
    const AppDisplayController_t *controller,
    AppDisplayController_Status_t *output_status)
{
    if ((controller != NULL) && (output_status != NULL))
    {
        *output_status = controller->status;
    }
}
