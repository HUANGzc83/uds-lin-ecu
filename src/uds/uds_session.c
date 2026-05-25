/*
 * uds_session.c
 * UDS Session State Machine — Implementation
 *
 * Wave 2 Task 7 — Session State Machine
 *
 * Manages diagnostic session transitions per ISO 14229-1 Figure 7 rules
 * and provides session parameter lookups (P2Server_max / P2*Server_max).
 */

#include "uds/uds_session.h"
#include <stddef.h>

/* ======================================================================== *
 * Session Parameter Table                                                   *
 * ======================================================================== *
 * Maps each supported session type to its timing parameters.
 *   P2Server_max     — maximum time the server waits before starting response
 *   P2StarServer_max — maximum time between consecutive responses
 *
 * Values per ISO 14229-1 Table A.1 (typical embedded implementation):
 *   defaultSession   (0x01): P2=50ms,    P2*=5000ms
 *   programmingSession (0x02): P2=5000ms, P2*=5000ms
 *   extendedSession  (0x03): P2=5000ms,  P2*=5000ms
 */
typedef struct {
    uint8_t  session;           /**< @brief Session type identifier */
    uint16_t p2_server_max;     /**< @brief P2Server_max in milliseconds */
    uint16_t p2_star_server_max; /**< @brief P2StarServer_max in milliseconds */
} session_param_entry_t;

static const session_param_entry_t session_params_table[] = {
    {UDS_DEFAULT_SESSION,      50,   5000},
    {UDS_PROGRAMMING_SESSION,  5000, 5000},
    {UDS_EXTENDED_SESSION,     5000, 5000},
};

#define SESSION_PARAMS_COUNT  (sizeof(session_params_table) / sizeof(session_params_table[0]))

/* ======================================================================== *
 * Helper: find session parameter entry by session type                     *
 * ======================================================================== */
static const session_param_entry_t* find_session_entry(uint8_t session)
{
    for (size_t i = 0; i < SESSION_PARAMS_COUNT; i++)
    {
        if (session_params_table[i].session == session)
        {
            return &session_params_table[i];
        }
    }
    return NULL;
}

/* ======================================================================== *
 *  uds_session_init                                                        *
 * ======================================================================== */
void uds_session_init(uds_session_context_t *ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    ctx->current_session = UDS_DEFAULT_SESSION;
    ctx->params.p2_server_max     = 50;
    ctx->params.p2_star_server_max = 5000;
    ctx->events_paused   = false;
    ctx->security_locked = true;
}

/* ======================================================================== *
 *  uds_session_switch                                                      *
 * ======================================================================== */
uds_status_t uds_session_switch(uds_session_context_t *ctx,
                                uint8_t new_session,
                                uds_nrc_t *nrc)
{
    if (ctx == NULL || nrc == NULL)
    {
        return UDS_ERR_PARSE;
    }

    /* --- Validate the requested session is supported --- */
    if (!uds_session_is_supported(new_session))
    {
        *nrc = NRC_SUB_FUNCTION_NOT_SUPPORTED;
        return UDS_ERR_PARSE;
    }

    uint8_t old_session = ctx->current_session;

    /* --- Same-session request: no side effects, just update params --- */
    if (old_session == new_session)
    {
        const session_param_entry_t *entry = find_session_entry(new_session);
        if (entry != NULL)
        {
            ctx->params.p2_server_max     = entry->p2_server_max;
            ctx->params.p2_star_server_max = entry->p2_star_server_max;
        }
        return UDS_OK;
    }

    /* --- Apply Figure 7 transition rules --- */
    if (old_session == UDS_DEFAULT_SESSION)
    {
        /* defaultSession → any non-default: allow, pause events */
        ctx->events_paused = true;
        /* security_locked unchanged */
    }
    else if (new_session == UDS_DEFAULT_SESSION)
    {
        /* non-default → defaultSession: allow, resume events, re-lock security */
        ctx->events_paused   = false;
        ctx->security_locked = true;
    }
    else
    {
        /* non-default → non-default: allow, stop events, re-lock security */
        ctx->events_paused   = true;
        ctx->security_locked = true;
    }

    /* --- Update session identifier and timing parameters --- */
    ctx->current_session = new_session;

    const session_param_entry_t *entry = find_session_entry(new_session);
    if (entry != NULL)
    {
        ctx->params.p2_server_max     = entry->p2_server_max;
        ctx->params.p2_star_server_max = entry->p2_star_server_max;
    }

    return UDS_OK;
}

/* ======================================================================== *
 *  uds_session_get_params                                                  *
 * ======================================================================== */
const uds_std_return_t* uds_session_get_params(const uds_session_context_t *ctx)
{
    if (ctx == NULL)
    {
        return NULL;
    }

    return &ctx->params;
}

/* ======================================================================== *
 *  uds_session_is_supported                                                *
 * ======================================================================== */
bool uds_session_is_supported(uint8_t session)
{
    return (find_session_entry(session) != NULL);
}
