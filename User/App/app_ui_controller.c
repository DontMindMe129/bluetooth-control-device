/** @file app_ui_controller.c @brief Hiện thực điều phối nút và page OLED. */

#include "app_ui_controller.h"

#include <stddef.h>

/** @brief Đổi page theo đúng một yêu cầu Left hoặc Right. */
static bool app_ui_controller_navigate(AppUiController_t *controller,
                                       bool left_requested,
                                       bool right_requested)
{
    if ((controller == NULL) || (left_requested == right_requested))
    {
        return false;
    }

    if (left_requested)
    {
        if (controller->status.current_page == APP_DISPLAY_PAGE_ENVIRONMENT)
        {
            controller->status.current_page =
                (App_DisplayPage_t)(APP_DISPLAY_PAGE_COUNT - 1);
        }
        else
        {
            controller->status.current_page =
                (App_DisplayPage_t)(controller->status.current_page - 1);
        }
    }
    else
    {
        controller->status.current_page =
            (App_DisplayPage_t)(controller->status.current_page + 1);
        if (controller->status.current_page >= APP_DISPLAY_PAGE_COUNT)
        {
            controller->status.current_page = APP_DISPLAY_PAGE_ENVIRONMENT;
        }
    }
    return true;
}

bool AppUiController_Initialize(AppUiController_t *controller,
                                const AppUiController_Config_t *config)
{
    if ((controller == NULL) || (config == NULL) ||
        (config->initial_page >= APP_DISPLAY_PAGE_COUNT))
    {
        return false;
    }

    *controller = (AppUiController_t){0};
    controller->status.current_page = config->initial_page;
    controller->status.ok_button_initialized =
        ButtonInput_Initialize(&controller->ok_button, &config->ok_button);
    controller->status.left_button_initialized =
        ButtonInput_Initialize(&controller->left_button, &config->left_button);
    controller->status.right_button_initialized =
        ButtonInput_Initialize(&controller->right_button,
                               &config->right_button);
    controller->status.up_button_initialized =
        ButtonInput_Initialize(&controller->up_button, &config->up_button);
    controller->status.down_button_initialized =
        ButtonInput_Initialize(&controller->down_button, &config->down_button);
    controller->status.ok_gesture_initialized =
        ButtonGesture_Initialize(&controller->ok_gesture,
                                 &config->ok_gesture);
    controller->status.is_initialized = true;
    return controller->status.ok_button_initialized &&
        controller->status.left_button_initialized &&
        controller->status.right_button_initialized &&
        controller->status.up_button_initialized &&
        controller->status.down_button_initialized &&
        controller->status.ok_gesture_initialized;
}

void AppUiController_Service(AppUiController_t *controller,
                             uint32_t current_tick_ms,
                             AppUiController_Events_t *events)
{
    bool ok_pressed_event;
    bool left_requested;
    bool right_requested;

    if (events != NULL)
    {
        *events = (AppUiController_Events_t){0};
    }
    if ((controller == NULL) || (events == NULL) ||
        !controller->status.is_initialized)
    {
        return;
    }

    ButtonInput_Service(&controller->ok_button, current_tick_ms);
    ButtonInput_Service(&controller->left_button, current_tick_ms);
    ButtonInput_Service(&controller->right_button, current_tick_ms);
    ButtonInput_Service(&controller->up_button, current_tick_ms);
    ButtonInput_Service(&controller->down_button, current_tick_ms);

    ok_pressed_event = ButtonInput_TakePressedEvent(&controller->ok_button);
    left_requested = ButtonInput_TakePressedEvent(&controller->left_button);
    right_requested = ButtonInput_TakePressedEvent(&controller->right_button);
    events->up_requested =
        ButtonInput_TakePressedEvent(&controller->up_button);
    events->down_requested =
        ButtonInput_TakePressedEvent(&controller->down_button);

    ButtonGesture_Service(&controller->ok_gesture,
                          current_tick_ms,
                          ok_pressed_event,
                          ButtonInput_IsPressed(&controller->ok_button));
    events->ok_short_requested =
        ButtonGesture_TakeShortPress(&controller->ok_gesture);
    events->ok_long_requested =
        ButtonGesture_TakeLongPress(&controller->ok_gesture);
    events->page_changed = app_ui_controller_navigate(controller,
                                                       left_requested,
                                                       right_requested);
}

void AppUiController_HandleExtiInterrupt(AppUiController_t *controller,
                                         uint16_t gpio_pin)
{
    if ((controller == NULL) || !controller->status.is_initialized)
    {
        return;
    }

    ButtonInput_HandleExtiInterrupt(&controller->ok_button, gpio_pin);
    ButtonInput_HandleExtiInterrupt(&controller->left_button, gpio_pin);
    ButtonInput_HandleExtiInterrupt(&controller->right_button, gpio_pin);
    ButtonInput_HandleExtiInterrupt(&controller->up_button, gpio_pin);
    ButtonInput_HandleExtiInterrupt(&controller->down_button, gpio_pin);
}

void AppUiController_GetStatus(const AppUiController_t *controller,
                               AppUiController_Status_t *output_status)
{
    if ((controller != NULL) && (output_status != NULL))
    {
        *output_status = controller->status;
    }
}
