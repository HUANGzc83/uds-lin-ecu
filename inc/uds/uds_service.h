/*
 * uds_service.h
 * Master Service Dispatch (UDS Request Router) — Module Header
 *
 * Central dispatch engine for all UDS service handlers.  Provides a
 * static service table with session-aware routing, handler registration,
 * and functional-addressing NRC suppression per ISO 14229-1 Tables 4-7.
 *
 * Each service handler follows the pattern:
 *   bool handler(const uds_request_t *req, uds_response_t *rsp, void *context);
 *
 * Return value: true = send response (rsp populated), false = suppress.
 *
 * Wave 3d Task 18 — Master Service Dispatch (UDS Request Router)
 */

#pragma once

#include "uds/uds_core.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== *
 * Session Availability Mask Constants                                      *
 * ======================================================================== *
 * Bitmask used when registering a service to indicate which diagnostic
 * sessions it is available in.  These correspond to the session type
 * identifiers defined in ISO 14229-1 Table 25.
 *
 *   Bit 0 (0x01) — defaultSession
 *   Bit 1 (0x02) — programmingSession
 *   Bit 2 (0x04) — extendedSession
 */

#define UDS_SESSION_DEFAULT      0x01u  /**< @brief Available in defaultSession */
#define UDS_SESSION_PROGRAMMING  0x02u  /**< @brief Available in programmingSession */
#define UDS_SESSION_EXTENDED     0x04u  /**< @brief Available in extendedSession */
#define UDS_SESSION_ALL          0x07u  /**< @brief Available in all sessions */
#define UDS_SESSION_NON_DEFAULT  0x06u  /**< @brief Available in non-default sessions (programming | extended) */

/* ======================================================================== *
 * Configuration                                                           *
 * ======================================================================== */

/** @brief Maximum number of service entries in the static dispatch table */
#ifndef UDS_SERVICE_TABLE_MAX
#define UDS_SERVICE_TABLE_MAX  32u
#endif

/**
 * @brief Default RCRRP pre-send delay in milliseconds.
 *
 * When a handler takes longer than this threshold, the dispatch engine
 * signals that an RCRRP (requestCorrectlyReceived-ResponsePending, NRC 0x78)
 * should be sent before the final response.  Set to 0 to disable (current
 * behaviour — RCRRP is sent immediately before every handler that needs it).
 *
 * May be overridden at compile time via -DUDS_RCRRP_DELAY_MS=<value>.
 */
#ifndef UDS_RCRRP_DELAY_MS
#define UDS_RCRRP_DELAY_MS  0u
#endif

/* ======================================================================== *
 * Service Handler Typedef                                                  *
 * ======================================================================== */

/**
 * @brief Service handler function signature.
 *
 * Matches the existing pattern used by all UDS service handlers in the
 * codebase (uds_svc_diagcomm, uds_svc_data, etc.).
 *
 * @param[in]     req      Parsed UDS request
 * @param[out]    rsp      Response structure (positive or negative)
 * @param[in,out] context  Opaque context pointer (type depends on service)
 * @return true to send the response, false to suppress (SPRMIB)
 */
typedef bool (*uds_service_handler_t)(const uds_request_t *req,
                                       uds_response_t      *rsp,
                                       void                *context);

/* ======================================================================== *
 * RCRRP State Tracking                                                    *
 * ======================================================================== */

/**
 * @brief RCRRP tracking state for asynchronous response handling.
 *
 * Used by uds_service_dispatch_ex() to communicate to the caller whether a
 * handler took longer than the configured rcrrp_delay_ms window and therefore
 * requires an RCRRP (requestCorrectlyReceived-ResponsePending, NRC 0x78) to
 * be sent before the final response.
 */
typedef enum {
    RCRRP_NONE            = 0, /**< No RCRRP pending or sent */
    RCRRP_SENT            = 1, /**< RCRRP has been sent (handler exceeded delay) */
    RCRRP_PENDING_HANDLER = 2  /**< Handler is executing, decision pending */
} uds_rcrrp_state_t;

/* ======================================================================== *
 * Service Table Entry                                                      *
 * ======================================================================== */

/** @brief A single entry in the static service dispatch table */
typedef struct {
    uint8_t                sid;              /**< @brief Request SID */
    uds_service_handler_t  handler;          /**< @brief Handler function pointer */
    uint8_t                session_mask;     /**< @brief Bitmask of available sessions */
    bool                   active;           /**< @brief Whether this entry is in use */
    uint16_t               rcrrp_delay_ms;   /**< @brief RCRRP pre-send delay window in ms (0 = disabled) */
    uds_rcrrp_state_t      rcrrp_state;      /**< @brief Current RCRRP tracking state */
} uds_service_entry_t;

/* ======================================================================== *
 * API Functions                                                           *
 * ======================================================================== */

/**
 * @brief Register a service handler in the dispatch table (extended).
 *
 * Adds an entry mapping @p sid to @p handler with the given session
 * availability mask and optional RCRRP pre-send delay.  Fails if the
 * table is full or if @p sid is already registered.
 *
 * @param[in] sid             Request SID to register
 * @param[in] handler         Handler function pointer (must not be NULL)
 * @param[in] session_mask    Bitmask of sessions where this service is available
 * @param[in] rcrrp_delay_ms  RCRRP pre-send delay window in ms (0 = disabled)
 * @return true on success, false if table is full or SID already exists
 */
bool uds_service_register_ex(uint8_t sid, uds_service_handler_t handler,
                              uint8_t session_mask, uint16_t rcrrp_delay_ms);

/**
 * @brief Register a service handler in the dispatch table.
 *
 * Convenience wrapper that calls uds_service_register_ex() with
 * rcrrp_delay_ms = UDS_RCRRP_DELAY_MS (default 0, backward compatible).
 *
 * Adds an entry mapping @p sid to @p handler with the given session
 * availability mask.  Fails if the table is full or if @p sid is already
 * registered.
 *
 * @param[in] sid           Request SID to register
 * @param[in] handler       Handler function pointer (must not be NULL)
 * @param[in] session_mask  Bitmask of sessions where this service is available
 * @return true on success, false if table is full or SID already exists
 */
bool uds_service_register(uint8_t sid, uds_service_handler_t handler, uint8_t session_mask);

/**
 * @brief Extended dispatch function with RCRRP delay tracking.
 *
 * Routes a parsed UDS request to the appropriate registered handler.
 * If the handler's entry has rcrrp_delay_ms > 0, records a timestamp
 * before invoking the handler and checks elapsed time on return.  When
 * the elapsed time exceeds the configured delay, the entry's rcrrp_state
 * is set to RCRRP_SENT so the caller can issue a 0x78 RCRRP before the
 * final response.
 *
 * Performs the same validation and NRC suppression as uds_service_dispatch():
 *   1. Basic input validation (non-NULL pointers)
 *   2. SID lookup — not found → NRC 0x11 (physical) or silent (functional)
 *   3. Session availability — mismatch → NRC 0x7F
 *   4. Records timestamp (if rcrrp_delay_ms > 0)
 *   5. Delegates to the registered handler
 *   6. Checks elapsed time vs rcrrp_delay_ms, updates rcrrp_state
 *   7. Applies SPRMIB and functional-addressing NRC suppression per Tables 4-7
 *
 * @param[in]     req              Parsed UDS request (must not be NULL)
 * @param[out]    rsp              Response structure (populated on success/error)
 * @param[in]     current_session  Current diagnostic session identifier (0x01, 0x02, 0x03)
 * @param[in]     ta_type          Target address type (physical or functional)
 * @param[in,out] context          Opaque context forwarded to the handler
 * @return true if a response should be sent (rsp is populated), false to suppress
 */
bool uds_service_dispatch_ex(const uds_request_t *req,
                              uds_response_t      *rsp,
                              uint8_t              current_session,
                              uds_addressing_t     ta_type,
                              void                *context);

/**
 * @brief Main dispatch function (backward-compatible wrapper).
 *
 * Calls uds_service_dispatch_ex() for all handler dispatch.  Equivalent
 * to the original signature — no RCRRP tracking when delay is 0.
 *
 * Routes a parsed UDS request to the appropriate registered handler.
 * Performs the following checks in order:
 *   1. Basic input validation (non-NULL pointers)
 *   2. SID lookup — not found → NRC 0x11 (physical) or silent (functional)
 *   3. Session availability — mismatch → NRC 0x7F
 *   4. Delegates to the registered handler
 *   5. Applies SPRMIB and functional-addressing NRC suppression per Tables 4-7
 *
 * @param[in]     req              Parsed UDS request (must not be NULL)
 * @param[out]    rsp              Response structure (populated on success/error)
 * @param[in]     current_session  Current diagnostic session identifier (0x01, 0x02, 0x03)
 * @param[in]     ta_type          Target address type (physical or functional)
 * @param[in,out] context          Opaque context forwarded to the handler
 * @return true if a response should be sent (rsp is populated), false to suppress
 */
bool uds_service_dispatch(const uds_request_t *req,
                          uds_response_t      *rsp,
                          uint8_t              current_session,
                          uds_addressing_t     ta_type,
                          void                *context);

/**
 * @brief Initialise the dispatch table and register all built-in service handlers.
 *
 * Registers handlers for all 26 UDS services implemented in the codebase:
 *   - Diagnostic & Communication: 0x10, 0x11, 0x27, 0x28, 0x3E, 0x85, 0x86, 0x87
 *   - Data Transmission:          0x22, 0x23, 0x24, 0x2A, 0x2C, 0x2E, 0x3D
 *   - Stored Data:                0x14, 0x19
 *   - IO Control:                 0x2F
 *   - Routine Control:            0x31
 *   - Upload/Download:            0x34, 0x35, 0x36, 0x37, 0x38
 *   - Authentication:             0x29 (stub)
 *   - Secured Data Transmission:  0x84 (stub returning NRC 0x33)
 *
 * Must be called once during system initialisation before any dispatch.
 */
void uds_service_init(void);

/**
 * @brief Get the number of currently registered (active) services.
 *
 * @return Active service count
 */
uint8_t uds_service_get_count(void);

/**
 * @brief Find a registered service entry by SID.
 *
 * @param[in] sid  Service identifier to look up
 * @return Pointer to the entry, or NULL if not found
 */
const uds_service_entry_t* uds_service_find(uint8_t sid);

/**
 * @brief Clear all registered service entries.
 *
 * Resets the dispatch table to empty.  Useful for test isolation.
 */
void uds_service_clear(void);

#ifdef __cplusplus
}
#endif
