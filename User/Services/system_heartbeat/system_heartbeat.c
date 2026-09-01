/**
 * @file system_heartbeat.c
 * @brief Hiện thực bộ tạo heartbeat tuần hoàn không blocking.
 */

#include "system_heartbeat.h"

#include <stddef.h>
#include <stdint.h>

bool SystemHeartbeat_Initialize(SystemHeartbeat_t *heartbeat,
                                const SystemHeartbeat_Config_t *config,
                                uint32_t current_tick_ms)
{
    if ((heartbeat == NULL) ||
        (config == NULL) ||
        ((config->on_time_ms == 0UL) && (config->off_time_ms == 0UL)) ||
        (config->on_time_ms > (UINT32_MAX - config->off_time_ms)))
    {
        return false;
    }

    *heartbeat = (SystemHeartbeat_t){0};
    heartbeat->config = *config;
    heartbeat->cycle_start_tick_ms = current_tick_ms;
    heartbeat->is_initialized = true;
    return true;
}

void SystemHeartbeat_Service(SystemHeartbeat_t *heartbeat,
                             uint32_t current_tick_ms)
{
    uint32_t period_ms;
    uint32_t phase_ms;

    if ((heartbeat == NULL) || !heartbeat->is_initialized)
    {
        return;
    }

    period_ms = heartbeat->config.on_time_ms + heartbeat->config.off_time_ms;
    phase_ms = (uint32_t)(current_tick_ms - heartbeat->cycle_start_tick_ms) %
               period_ms;
    heartbeat->output_should_be_active =
        (phase_ms < heartbeat->config.on_time_ms);
}

bool SystemHeartbeat_ShouldBeActive(const SystemHeartbeat_t *heartbeat)
{
    return ((heartbeat != NULL) &&
            heartbeat->is_initialized &&
            heartbeat->output_should_be_active);
}
