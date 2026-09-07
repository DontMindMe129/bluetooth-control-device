/** @file environment_history.c @brief Hiện thực ring buffer history môi trường. */

#include "environment_history.h"

#include <limits.h>
#include <stddef.h>

_Static_assert(DHT11_ERROR_INTERNAL_STATE <= UINT8_MAX,
               "DHT11 error code no longer fits history storage");
_Static_assert(ENVIRONMENT_DATA_STALE <= UINT8_MAX,
               "Environment data state no longer fits history storage");
_Static_assert(ENVIRONMENT_TEMPERATURE_HIGH <= UINT8_MAX,
               "Temperature level no longer fits history storage");
_Static_assert(ENVIRONMENT_CONDITION_WARM_AND_HUMID <= UINT8_MAX,
               "Environment condition no longer fits history storage");
_Static_assert(sizeof(EnvironmentHistory_Record_t) == 16U,
               "Environment history record must remain 16 bytes");

bool EnvironmentHistory_Initialize(EnvironmentHistory_t *history)
{
    if (history == NULL)
    {
        return false;
    }

    *history = (EnvironmentHistory_t){0};
    history->status.is_initialized = true;
    return true;
}

bool EnvironmentHistory_Append(
                               EnvironmentHistory_t *history,
                               const DHT11_MeasurementResult_t *measurement_result,
                               uint16_t session_id,
                               const EnvironmentMonitor_Status_t *environment)
{
    EnvironmentHistory_Record_t *record;

    if ((history == NULL) || !history->status.is_initialized ||
        (measurement_result == NULL) || (environment == NULL) ||
        (session_id == 0U))
    {
        return false;
    }

    record = &history->records[history->status.write_index];
    *record = (EnvironmentHistory_Record_t){0};
    record->completed_tick_ms = measurement_result->completed_tick_ms;
    record->session_id = session_id;
    record->measurement_error = (uint8_t)measurement_result->error;
    record->cached_data_state = (uint8_t)environment->data_state;
    record->consecutive_error_count = environment->consecutive_sensor_errors;

    if (measurement_result->error == DHT11_ERROR_NONE)
    {
        record->measured_data = environment->latest_data;
        record->temperature_level = (uint8_t)environment->temperature_level;
        record->condition = (uint8_t)environment->condition;
    }

    history->status.write_index++;
    if (history->status.write_index >=
        ENVIRONMENT_HISTORY_CAPACITY)
    {
        history->status.write_index = 0U;
    }
    if (history->status.count < ENVIRONMENT_HISTORY_CAPACITY)
    {
        history->status.count++;
    }
    if (history->status.total_record_count < UINT32_MAX)
    {
        history->status.total_record_count++;
    }
    return true;
}

uint16_t EnvironmentHistory_GetCount(const EnvironmentHistory_t *history)
{
    return ((history != NULL) && history->status.is_initialized)
        ? history->status.count
        : 0U;
}

bool EnvironmentHistory_GetRecord(const EnvironmentHistory_t *history,
                                  uint16_t index,
                                  EnvironmentHistory_Record_t *output_record)
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
        (history->status.write_index +
         ENVIRONMENT_HISTORY_CAPACITY -
         history->status.count) % ENVIRONMENT_HISTORY_CAPACITY);
    physical_index = (uint16_t)(
        (oldest_index + index) % ENVIRONMENT_HISTORY_CAPACITY);
    *output_record = history->records[physical_index];
    return true;
}

void EnvironmentHistory_Clear(EnvironmentHistory_t *history)
{
    if ((history != NULL) && history->status.is_initialized)
    {
        history->status.write_index = 0U;
        history->status.count = 0U;
    }
}

void EnvironmentHistory_GetStatus(const EnvironmentHistory_t *history,
                                  EnvironmentHistory_Status_t *output_status)
{
    if ((history != NULL) && (output_status != NULL))
    {
        *output_status = history->status;
    }
}
