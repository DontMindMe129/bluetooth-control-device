/** @file monitoring_session.c @brief Hiện thực quản lý phiên giám sát. */

#include "monitoring_session.h"

#include <limits.h>
#include <stddef.h>

bool MonitoringSession_Initialize(MonitoringSession_t *session,
                                  uint32_t current_tick_ms)
{
    if (session == NULL)
    {
        return false;
    }

    *session = (MonitoringSession_t){0};
    session->status.is_initialized = true;
    session->status.state = MONITORING_SESSION_INACTIVE;
    session->status.last_change_source = MONITORING_SESSION_SOURCE_NONE;
    session->status.last_change_tick_ms = current_tick_ms;
    return true;
}

MonitoringSession_ChangeResult_t MonitoringSession_SetActive(
    MonitoringSession_t *session,
    bool active,
    MonitoringSession_Source_t source,
    uint32_t current_tick_ms)
{
    MonitoringSession_State_t requested_state = active
        ? MONITORING_SESSION_ACTIVE
        : MONITORING_SESSION_INACTIVE;

    if ((session == NULL) || !session->status.is_initialized ||
        (source == MONITORING_SESSION_SOURCE_NONE))
    {
        return MONITORING_SESSION_CHANGE_INVALID;
    }
    if (session->status.state == requested_state)
    {
        return MONITORING_SESSION_CHANGE_UNCHANGED;
    }

    session->status.state = requested_state;
    session->status.last_change_source = source;
    session->status.last_change_tick_ms = current_tick_ms;
    if (session->status.state_change_count < UINT32_MAX)
    {
        session->status.state_change_count++;
    }
    if ((requested_state == MONITORING_SESSION_ACTIVE) &&
        (session->status.session_id < UINT16_MAX))
    {
        session->status.session_id++;
    }
    return MONITORING_SESSION_CHANGE_APPLIED;
}

MonitoringSession_ChangeResult_t MonitoringSession_Toggle(
    MonitoringSession_t *session,
    MonitoringSession_Source_t source,
    uint32_t current_tick_ms)
{
    if ((session == NULL) || !session->status.is_initialized)
    {
        return MONITORING_SESSION_CHANGE_INVALID;
    }
    return MonitoringSession_SetActive(
        session,
        session->status.state != MONITORING_SESSION_ACTIVE,
        source,
        current_tick_ms);
}

bool MonitoringSession_IsActive(const MonitoringSession_t *session)
{
    return (session != NULL) && session->status.is_initialized &&
           (session->status.state == MONITORING_SESSION_ACTIVE);
}

void MonitoringSession_GetStatus(const MonitoringSession_t *session,
                                 MonitoringSession_Status_t *output_status)
{
    if ((session != NULL) && (output_status != NULL))
    {
        *output_status = session->status;
    }
}
