/*
 * uds_session.h
 * UDS Session State Machine — Module Header
 *
 * Wave 2 Task 7 — Session State Machine
 *
 * Implements DiagnosticSessionControl session transition rules
 * (Figure 7) and session parameter management per ISO 14229-1.
 */

#pragma once

#include "uds/uds_core.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== *
 * Session Context Structure                                                *
 * ======================================================================== */

/** @brief Session context holding current diagnostic session state */
typedef struct {
    uint8_t          current_session; /**< @brief Current active session ID (UDS_DEFAULT_SESSION, etc.) */
    uds_std_return_t params;          /**< @brief Timing parameters for the current session */
    bool             events_paused;   /**< @brief Events paused flag (set when entering non-default session) */
    bool             security_locked; /**< @brief Security access locked flag */
} uds_session_context_t;

/* ======================================================================== *
 * Function Declarations                                                   *
 * ======================================================================== */

/**
 * @brief Initialize session context to defaultSession.
 *
 * Sets current_session to UDS_DEFAULT_SESSION, loads defaultSession timing
 * parameters, clears events_paused, and sets security_locked = true.
 *
 * @param[out] ctx  Session context to initialize (must not be NULL)
 */
void uds_session_init(uds_session_context_t *ctx);

/**
 * @brief Attempt a diagnostic session switch.
 *
 * Implements Figure 7 session transition rules:
 *   - defaultSession → any non-default : allow, pause events
 *   - non-default → defaultSession     : allow, resume events, re-lock security
 *   - non-default → non-default        : allow, stop events, re-lock security
 *   - unsupported session              : NRC 0x12 (subFunctionNotSupported)
 *
 * On success the context is updated with the new session and its timing
 * parameters.  On failure the context is left unchanged and @p nrc is set
 * to the appropriate negative response code.
 *
 * @param[in,out] ctx          Session context to modify
 * @param[in]     new_session  Requested diagnostic session type
 * @param[out]    nrc          Set to NRC on failure (unchanged on success)
 * @return UDS_OK on successful switch, UDS_ERR_PARSE on failure
 */
uds_status_t uds_session_switch(uds_session_context_t *ctx,
                                uint8_t new_session,
                                uds_nrc_t *nrc);

/**
 * @brief Get pointer to the current session's timing parameters.
 *
 * @param[in] ctx  Session context
 * @return Pointer to the uds_std_return_t in the context,
 *         or NULL if ctx is NULL
 */
const uds_std_return_t* uds_session_get_params(const uds_session_context_t *ctx);

/**
 * @brief Check whether a session type is supported by this implementation.
 *
 * Supported sessions: 0x01 (default), 0x02 (programming), 0x03 (extended).
 *
 * @param[in] session  Session type identifier to check
 * @return true if the session is supported, false otherwise
 */
bool uds_session_is_supported(uint8_t session);

#ifdef __cplusplus
}
#endif
