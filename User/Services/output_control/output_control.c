/**
 * @file output_control.c
 * @brief Hiện thực state machine lựa chọn và bật/tắt năm ngõ ra số.
 */

#include "output_control.h"

#include <limits.h>
#include <stddef.h>

/** @brief Tăng bộ đếm thay đổi nhưng không cho phép tràn về 0. */
static void output_control_count_change(OutputControl_t *control)
{
    if (control->status.change_count < UINT32_MAX)
    {
        control->status.change_count++;
    }
}

bool OutputControl_Initialize(OutputControl_t *control)
{
    if (control == NULL)
    {
        return false;
    }

    *control = (OutputControl_t){0};
    control->status.is_initialized = true;
    return true;
}

void OutputControl_SelectPrevious(OutputControl_t *control)
{
    if ((control == NULL) || !control->status.is_initialized)
    {
        return;
    }

    if (control->status.selected_output == 0U)
    {
        control->status.selected_output = OUTPUT_CONTROL_COUNT - 1U;
    }
    else
    {
        control->status.selected_output--;
    }
    output_control_count_change(control);
}

void OutputControl_SelectNext(OutputControl_t *control)
{
    if ((control == NULL) || !control->status.is_initialized)
    {
        return;
    }

    control->status.selected_output++;
    if (control->status.selected_output >= OUTPUT_CONTROL_COUNT)
    {
        control->status.selected_output = 0U;
    }
    output_control_count_change(control);
}

void OutputControl_ToggleSelected(OutputControl_t *control)
{
    uint8_t selected_mask;

    if ((control == NULL) || !control->status.is_initialized)
    {
        return;
    }

    selected_mask = (uint8_t)(1U << control->status.selected_output);
    control->status.output_on_mask ^= selected_mask;
    output_control_count_change(control);
}

bool OutputControl_IsOutputOn(const OutputControl_t *control,
                              uint8_t output_index)
{
    if ((control == NULL) ||
        !control->status.is_initialized ||
        (output_index >= OUTPUT_CONTROL_COUNT))
    {
        return false;
    }

    return ((control->status.output_on_mask &
             (uint8_t)(1U << output_index)) != 0U);
}

void OutputControl_GetStatus(const OutputControl_t *control,
                             OutputControl_Status_t *output_status)
{
    if ((control != NULL) && (output_status != NULL))
    {
        *output_status = control->status;
    }
}
