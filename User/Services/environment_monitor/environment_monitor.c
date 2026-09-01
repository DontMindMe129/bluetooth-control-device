/**
 * @file environment_monitor.c
 * @brief Hiện thực monitor thuần phần mềm cho dữ liệu môi trường từ DHT11.
 */

#include "environment_monitor.h"

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Context nội bộ của monitor; chỉ được truy cập trong file này.
 */
typedef struct
{
    EnvironmentMonitor_Status_t status;
    bool has_received_valid_data;
} EnvironmentMonitor_InternalContext_t;

static EnvironmentMonitor_InternalContext_t s_environment_monitor;

static bool environment_monitor_has_elapsed(uint32_t now,
                                            uint32_t start,
                                            uint32_t duration);
static EnvironmentMonitor_Condition_t environment_monitor_classify(
    const DHT11_Data_t *data);

/**
 * @brief Kiểm tra thời gian trôi qua bằng phép trừ an toàn khi HAL tick tràn.
 */
static bool environment_monitor_has_elapsed(uint32_t now,
                                            uint32_t start,
                                            uint32_t duration)
{
    return ((uint32_t)(now - start) >= duration);
}

/**
 * @brief Phân loại mẫu theo hai ngưỡng nguyên dễ kích hoạt khi kiểm thử.
 */
static EnvironmentMonitor_Condition_t environment_monitor_classify(
    const DHT11_Data_t *data)
{
    bool is_warm = (data->temperature_integer >=
                    ENVIRONMENT_MONITOR_WARM_TEMPERATURE_C);
    bool is_humid = (data->humidity_integer >=
                     ENVIRONMENT_MONITOR_HUMIDITY_PERCENT);

    if (is_warm && is_humid)
    {
        return ENVIRONMENT_CONDITION_WARM_AND_HUMID;
    }

    if (is_warm)
    {
        return ENVIRONMENT_CONDITION_WARM;
    }

    if (is_humid)
    {
        return ENVIRONMENT_CONDITION_HUMID;
    }

    return ENVIRONMENT_CONDITION_NORMAL;
}

void EnvironmentMonitor_Initialize(void)
{
    s_environment_monitor = (EnvironmentMonitor_InternalContext_t){0};
    s_environment_monitor.status.data_state = ENVIRONMENT_DATA_NO_DATA;
    s_environment_monitor.status.condition = ENVIRONMENT_CONDITION_UNKNOWN;
    s_environment_monitor.status.last_sensor_error = DHT11_ERROR_NONE;
}

void EnvironmentMonitor_Service(uint32_t current_tick_ms,
                                const DHT11_Data_t *new_data,
                                const DHT11_Status_t *sensor_status)
{
    if (sensor_status == NULL)
    {
        return;
    }

    s_environment_monitor.status.last_sensor_error = sensor_status->last_error;
    s_environment_monitor.status.consecutive_sensor_errors =
        sensor_status->consecutive_error_count;

    if (new_data != NULL)
    {
        s_environment_monitor.status.latest_data = *new_data;
        s_environment_monitor.status.last_update_tick_ms =
            sensor_status->last_success_tick_ms;
        s_environment_monitor.status.condition =
            environment_monitor_classify(new_data);
        s_environment_monitor.has_received_valid_data = true;
    }

    if (!s_environment_monitor.has_received_valid_data)
    {
        s_environment_monitor.status.data_state = ENVIRONMENT_DATA_NO_DATA;
        s_environment_monitor.status.condition = ENVIRONMENT_CONDITION_UNKNOWN;
        return;
    }

    if (environment_monitor_has_elapsed(
            current_tick_ms,
            s_environment_monitor.status.last_update_tick_ms,
            ENVIRONMENT_MONITOR_STALE_TIMEOUT_MS))
    {
        s_environment_monitor.status.data_state = ENVIRONMENT_DATA_STALE;
    }
    else
    {
        s_environment_monitor.status.data_state = ENVIRONMENT_DATA_FRESH;
    }
}

void EnvironmentMonitor_GetStatus(EnvironmentMonitor_Status_t *output_status)
{
    if (output_status == NULL)
    {
        return;
    }

    *output_status = s_environment_monitor.status;
}
