/** @file warning_history.c @brief Hiện thực ring buffer event warning. */

#include "warning_history.h"

#include <limits.h>
#include <stddef.h>

bool WarningHistory_Initialize(WarningHistory_t *history)
{
    if (history == NULL)
    {
        return false;
    }

    *history = (WarningHistory_t){0};
    history->status.is_initialized = true;
    return true;
}

bool WarningHistory_Service(WarningHistory_t *history,
                            uint32_t current_tick_ms,
                            uint8_t source_mask,
                            uint16_t session_id,
                            WarningHistory_Record_t *output_record)
{
    WarningHistory_Record_t record;
    uint8_t previous_source_mask;

    if ((history == NULL) || !history->status.is_initialized)
    {
        return false;
    }

    previous_source_mask = history->status.current_source_mask;
    if (source_mask == previous_source_mask)
    {
        return false;
    }

    record.tick_ms = current_tick_ms;
    record.source_mask = source_mask;
    if (previous_source_mask == 0U)
    {
        record.event = WARNING_HISTORY_EVENT_STARTED;
        record.session_id = session_id;
        history->current_warning_session_id = session_id;
    }
    else if (source_mask == 0U)
    {
        record.event = WARNING_HISTORY_EVENT_CLEARED;
        record.session_id = history->current_warning_session_id;
        history->current_warning_session_id = 0U;
    }
    else
    {
        record.event = WARNING_HISTORY_EVENT_SOURCE_CHANGED;
        record.session_id = session_id;
        history->current_warning_session_id = session_id;
    }

    history->records[history->status.write_index] = record;
    history->status.write_index++;
    if (history->status.write_index >= WARNING_HISTORY_CAPACITY)
    {
        history->status.write_index = 0U;
    }

    if (history->status.count < WARNING_HISTORY_CAPACITY)
    {
        history->status.count++;
    }
    else if (history->status.overwritten_event_count < UINT32_MAX)
    {
        history->status.overwritten_event_count++;
    }
    if (history->status.total_event_count < UINT32_MAX)
    {
        history->status.total_event_count++;
    }
    history->status.current_source_mask = source_mask;

    if (output_record != NULL)
    {
        *output_record = record;
    }
    return true;
}

uint16_t WarningHistory_GetCount(const WarningHistory_t *history)
{
    return ((history != NULL) && history->status.is_initialized)
        ? history->status.count
        : 0U;
}

bool WarningHistory_GetRecord(const WarningHistory_t *history,
                              uint16_t index,
                              WarningHistory_Record_t *output_record)
{
    uint16_t oldest_index;
    uint16_t physical_index;

    if ((history == NULL) || !history->status.is_initialized ||
        (output_record == NULL) ||
        (index >= history->status.count))
    {
        return false;
    }

    oldest_index = (uint16_t)(
        (history->status.write_index + WARNING_HISTORY_CAPACITY -
         history->status.count) % WARNING_HISTORY_CAPACITY);
    physical_index = (uint16_t)(
        (oldest_index + index) % WARNING_HISTORY_CAPACITY);
    *output_record = history->records[physical_index];
    return true;
}

void WarningHistory_Clear(WarningHistory_t *history,
                          uint8_t current_source_mask,
                          uint16_t current_session_id)
{
    if ((history == NULL) || !history->status.is_initialized)
    {
        return;
    }

    history->status.count = 0U;
    history->status.write_index = 0U;
    history->status.current_source_mask = current_source_mask;
    history->current_warning_session_id =
        current_source_mask == 0U ? 0U : current_session_id;
}

void WarningHistory_GetStatus(const WarningHistory_t *history,
                              WarningHistory_Status_t *output_status)
{
    if ((history != NULL) && (output_status != NULL))
    {
        *output_status = history->status;
    }
}
