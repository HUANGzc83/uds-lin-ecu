/*
 * uds_service.c
 * Master Service Dispatch (UDS Request Router) — Implementation
 *
 * Central dispatch engine for all UDS service handlers.  Maintains a
 * static dispatch table with session-aware routing, supports handler
 * registration, and applies functional-addressing NRC suppression per
 * ISO 14229-1 Tables 4-7.
 *
 * Wave 3d Task 18 — Master Service Dispatch (UDS Request Router)
 * Wave 1  Task  8 — Configurable RCRRP pre-send delay + async tracking
 */

/*
 * _POSIX_C_SOURCE must be defined before any system header so that
 * clock_gettime(2) and CLOCK_MONOTONIC are visible under -std=c11.
 */
#define _POSIX_C_SOURCE 199309L

#include "uds/uds_service.h"
#include "uds/uds_svc_diagcomm.h"
#include "uds/uds_svc_data.h"
#include "uds/uds_svc_stored.h"
#include "uds/uds_svc_io.h"
#include "uds/uds_svc_routine.h"
#include "uds/uds_svc_upload.h"
#include "uds/uds_svc_auth.h"
#include <stddef.h>   /* NULL */
#include <time.h>     /* clock_gettime(), CLOCK_MONOTONIC */

/* ======================================================================== *
 * Static Dispatch Table                                                     *
 * ======================================================================== */

/** @brief Static array of registered service entries */
static uds_service_entry_t g_service_table[UDS_SERVICE_TABLE_MAX];

/* ======================================================================== *
 * Internal Helpers                                                          *
 * ======================================================================== */

/**
 * @brief Get current time in milliseconds from CLOCK_MONOTONIC.
 *
 * Used by uds_service_dispatch_ex() for RCRRP elapsed-time tracking.
 * On non-POSIX platforms this is a stub returning 0 (RCRRP delay check
 * will never trigger — the feature requires platform time support).
 */
static uint32_t uds_get_time_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000U + (uint32_t)(ts.tv_nsec / 1000000U));
}

/**
 * @brief Internal mutable entry lookup (avoids const cast in dispatch).
 *
 * Identical to uds_service_find() but returns a non-const pointer so the
 * dispatch function can update rcrrp_state inline.
 */
static uds_service_entry_t* find_entry_mut(uint8_t sid)
{
    for (uint8_t i = 0; i < UDS_SERVICE_TABLE_MAX; i++)
    {
        if (g_service_table[i].active && g_service_table[i].sid == sid)
            return &g_service_table[i];
    }
    return NULL;
}

/* ======================================================================== *
 * Stub Handlers for Services without a dedicated implementation            *
 * ======================================================================== */

/**
 * @brief Stub handler for 0x84 — SecuredDataTransmission.
 *
 * Returns NRC 0x33 (securityAccessDenied) per implementation policy.
 * Secured data transmission requires additional security credentials
 * that are not available in this implementation.
 */
static bool secured_data_stub(const uds_request_t *req,
                              uds_response_t      *rsp,
                              void                *context)
{
    (void)context;
    static uint8_t nrc_buf[1];

    rsp->sid          = 0x7F;
    rsp->subfunc_echo = req->sid;
    nrc_buf[0]        = NRC_SECURITY_ACCESS_DENIED;
    rsp->data         = nrc_buf;
    rsp->data_len     = 1;
    return true;
}

/* ======================================================================== *
 * Dispatch Engine                                                           *
 * ======================================================================== */

bool uds_service_register_ex(uint8_t sid, uds_service_handler_t handler,
                              uint8_t session_mask, uint16_t rcrrp_delay_ms)
{
    if (handler == NULL)
        return false;

    /* Check for duplicate SID and find first free slot */
    bool found_slot = false;
    uint8_t slot = 0;

    for (uint8_t i = 0; i < UDS_SERVICE_TABLE_MAX; i++)
    {
        if (g_service_table[i].active && g_service_table[i].sid == sid)
            return false;  /* Duplicate SID */

        if (!found_slot && !g_service_table[i].active)
        {
            slot = i;
            found_slot = true;
        }
    }

    if (!found_slot)
        return false;  /* Table full */

    /* Register the entry */
    g_service_table[slot].sid             = sid;
    g_service_table[slot].handler         = handler;
    g_service_table[slot].session_mask    = session_mask;
    g_service_table[slot].active          = true;
    g_service_table[slot].rcrrp_delay_ms  = rcrrp_delay_ms;
    g_service_table[slot].rcrrp_state     = RCRRP_NONE;

    return true;
}

bool uds_service_register(uint8_t sid, uds_service_handler_t handler, uint8_t session_mask)
{
    return uds_service_register_ex(sid, handler, session_mask, UDS_RCRRP_DELAY_MS);
}

bool uds_service_dispatch_ex(const uds_request_t *req,
                              uds_response_t      *rsp,
                              uint8_t              current_session,
                              uds_addressing_t     ta_type,
                              void                *context)
{
    /* ---- Input validation ---- */
    if (req == NULL || rsp == NULL)
        return false;

    /* ---- SID lookup (mutable — need to update rcrrp_state) ---- */
    uds_service_entry_t *entry = find_entry_mut(req->sid);

    if (entry == NULL || !entry->active)
    {
        /*
         * Service not supported.
         * Per ISO 14229-1 Table 4: for functional addressing, NRC 0x11
         * shall not be sent — silently suppress the response.
         */
        if (ta_type == UDS_FUNCTIONAL)
            return false;

        /* Physical addressing: return NRC 0x11 */
        {
            static uint8_t nrc_buf[1];
            rsp->sid          = 0x7F;
            rsp->subfunc_echo = req->sid;
            nrc_buf[0]        = NRC_SERVICE_NOT_SUPPORTED;
            rsp->data         = nrc_buf;
            rsp->data_len     = 1;
        }
        return true;
    }

    /* ---- Session availability check ---- */
    /*
     * session_mask uses bits 0-2 for default/programming/extended.
     * current_session values are 0x01, 0x02, 0x03.
     * Mapping: bit = (current_session - 1)
     */
    {
        /* Guard: current_session 0 or >3 would cause UB in (1u << (current_session - 1u)) */
        if (current_session == 0u || current_session > 3u)
        {
            static uint8_t nrc_buf[1];
            rsp->sid          = 0x7F;
            rsp->subfunc_echo = 0;
            nrc_buf[0]        = NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION;
            rsp->data         = nrc_buf;
            rsp->data_len     = 1;
            return true;
        }
        uint8_t session_bit = (uint8_t)(1u << (current_session - 1u));
        if ((entry->session_mask & session_bit) == 0u)
        {
            /* Service not supported in active session — NRC 0x7F */
            static uint8_t nrc_buf[1];
            rsp->sid          = 0x7F;
            rsp->subfunc_echo = req->sid;
            nrc_buf[0]        = NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION;
            rsp->data         = nrc_buf;
            rsp->data_len     = 1;
            return true;
        }
    }

    /*
     * ---- RCRRP delay tracking ----
     * If the handler's entry has a non-zero delay window, record the
     * current time and set the state to PENDING_HANDLER so that we can
     * check elapsed time after the handler returns.
     *
     * In a single-threaded context the handler runs synchronously, so
     * the check is: if elapsed >= rcrrp_delay_ms, the handler took too
     * long and an RCRRP should precede the final response; otherwise
     * no RCRRP is needed.
     */
    uint32_t rcrrp_start_ms = 0;

    if (entry->rcrrp_delay_ms > 0U)
    {
        rcrrp_start_ms       = uds_get_time_ms();
        entry->rcrrp_state   = RCRRP_PENDING_HANDLER;
    }

    /* ---- Delegate to the registered handler ---- */
    bool send_response = entry->handler(req, rsp, context);

    /* ---- Post-handler RCRRP check ---- */
    if (entry->rcrrp_delay_ms > 0U)
    {
        uint32_t elapsed_ms = uds_get_time_ms() - rcrrp_start_ms;

        if (elapsed_ms >= (uint32_t)entry->rcrrp_delay_ms)
        {
            /* Handler exceeded the delay window — caller should send RCRRP */
            entry->rcrrp_state = RCRRP_SENT;
        }
        else
        {
            /* Handler completed within the window — no RCRRP needed */
            entry->rcrrp_state = RCRRP_NONE;
        }
    }

    /*
     * ---- Apply functional-addressing NRC suppression ----
     * Per ISO 14229-1 Tables 4-7: the following NRCs shall not be sent
     * when the request was received via functional addressing:
     *   - 0x11 (serviceNotSupported)
     *   - 0x12 (subFunctionNotSupported)
     *   - 0x31 (requestOutOfRange)
     *
     * If the handler returned a negative response with one of these NRCs
     * and the request was functional, suppress the response entirely.
     */
    if (send_response && ta_type == UDS_FUNCTIONAL)
    {
        /* Check if the response is negative (indicated by SID == 0x7F) */
        if (rsp->sid == 0x7F && rsp->data != NULL && rsp->data_len >= 1)
        {
            uint8_t nrc = rsp->data[rsp->data_len - 1];
            if (nrc == NRC_SERVICE_NOT_SUPPORTED                ||
                nrc == NRC_SUB_FUNCTION_NOT_SUPPORTED           ||
                nrc == NRC_REQUEST_OUT_OF_RANGE)
            {
                send_response = false;
            }
        }
    }

    return send_response;
}

bool uds_service_dispatch(const uds_request_t *req,
                          uds_response_t      *rsp,
                          uint8_t              current_session,
                          uds_addressing_t     ta_type,
                          void                *context)
{
    return uds_service_dispatch_ex(req, rsp, current_session, ta_type, context);
}

void uds_service_init(void)
{
    /* Clear the table (resets all fields including RCRRP state) */
    uds_service_clear();

    /*
     * ================================================================
     * Diagnostic & Communication Management Services  (Task 9)
     *   SIDs: 0x10, 0x11, 0x27, 0x28, 0x3E, 0x85, 0x86, 0x87
     * ================================================================
     */
    if (!uds_service_register(DIAGNOSTIC_SESSION_CONTROL, uds_svc_diagnostic_session_control, UDS_SESSION_ALL))
        {
            /* Table full — should not happen with default config */
        }
    if (!uds_service_register(ECU_RESET, uds_svc_ecu_reset, UDS_SESSION_ALL))
        {
            /* Table full — should not happen with default config */
        }
    if (!uds_service_register(SECURITY_ACCESS, uds_svc_security_access, UDS_SESSION_ALL))
        {
            /* Table full — should not happen with default config */
        }
    if (!uds_service_register(COMMUNICATION_CONTROL, uds_svc_communication_control, UDS_SESSION_ALL))
        {
            /* Table full — should not happen with default config */
        }
    if (!uds_service_register(TESTER_PRESENT, uds_svc_tester_present, UDS_SESSION_ALL))
        {
            /* Table full — should not happen with default config */
        }
    if (!uds_service_register(CONTROL_DTC_SETTING, uds_svc_control_dtc_setting, UDS_SESSION_NON_DEFAULT))
        {
            /* Table full — should not happen with default config */
        }
    if (!uds_service_register(RESPONSE_ON_EVENT, uds_svc_response_on_event, UDS_SESSION_NON_DEFAULT))
        {
            /* Table full — should not happen with default config */
        }
    if (!uds_service_register(LINK_CONTROL, uds_svc_link_control, UDS_SESSION_NON_DEFAULT))
        {
            /* Table full — should not happen with default config */
        }

    /*
     * ================================================================
     * Data Transmission Services  (Task 10)
     *   SIDs: 0x22, 0x23, 0x24, 0x2A, 0x2C, 0x2E, 0x3D
     * ================================================================
     */
    if (!uds_service_register(READ_DATA_BY_IDENTIFIER, uds_svc_read_data_by_id, UDS_SESSION_ALL))
        {
            /* Table full — should not happen with default config */
        }
    if (!uds_service_register(READ_MEMORY_BY_ADDRESS, uds_svc_read_memory_by_address, UDS_SESSION_NON_DEFAULT))
        {
            /* Table full — should not happen with default config */
        }
    if (!uds_service_register(READ_SCALING_DATA_BY_ID, uds_svc_read_scaling_data_by_id, UDS_SESSION_ALL))
        {
            /* Table full — should not happen with default config */
        }
    if (!uds_service_register(READ_DATA_BY_PERIODIC_ID, uds_svc_read_data_by_periodic_id, UDS_SESSION_ALL))
        {
            /* Table full — should not happen with default config */
        }
    if (!uds_service_register(DYNAMICALLY_DEFINE_DATA_ID, uds_svc_dynamically_define_data_id, UDS_SESSION_NON_DEFAULT))
        {
            /* Table full — should not happen with default config */
        }
    if (!uds_service_register(WRITE_DATA_BY_IDENTIFIER, uds_svc_write_data_by_id, UDS_SESSION_NON_DEFAULT))
        {
            /* Table full — should not happen with default config */
        }
    if (!uds_service_register(WRITE_MEMORY_BY_ADDRESS, uds_svc_write_memory_by_address, UDS_SESSION_NON_DEFAULT))
        {
            /* Table full — should not happen with default config */
        }

    /*
     * ================================================================
     * Stored Data Services  (Task 11)
     *   SIDs: 0x14, 0x19
     * ================================================================
     */
    if (!uds_service_register(CLEAR_DIAGNOSTIC_INFO, uds_svc_clear_diagnostic_info, UDS_SESSION_NON_DEFAULT))
        {
            /* Table full — should not happen with default config */
        }
    if (!uds_service_register(READ_DTC_INFO, uds_svc_read_dtc_info, UDS_SESSION_ALL))
        {
            /* Table full — should not happen with default config */
        }

    /*
     * ================================================================
     * IO Control Service  (Task 12)
     *   SID: 0x2F
     * ================================================================
     */
    if (!uds_service_register(INPUT_OUTPUT_CONTROL_BY_ID, uds_svc_io_control, UDS_SESSION_NON_DEFAULT))
        {
            /* Table full — should not happen with default config */
        }

    /*
     * ================================================================
     * Routine Control Service  (Task 13)
     *   SID: 0x31
     * ================================================================
     */
    if (!uds_service_register(ROUTINE_CONTROL, uds_svc_routine_control, UDS_SESSION_ALL))
        {
            /* Table full — should not happen with default config */
        }

    /*
     * ================================================================
     * Upload / Download Services  (Task 14)
     *   SIDs: 0x34, 0x35, 0x36, 0x37, 0x38
     * ================================================================
     */
    if (!uds_service_register(REQUEST_DOWNLOAD, uds_svc_request_download, UDS_SESSION_PROGRAMMING))
        {
            /* Table full — should not happen with default config */
        }
    if (!uds_service_register(REQUEST_UPLOAD, uds_svc_request_upload, UDS_SESSION_PROGRAMMING))
        {
            /* Table full — should not happen with default config */
        }
    if (!uds_service_register(TRANSFER_DATA, uds_svc_transfer_data, UDS_SESSION_PROGRAMMING))
        {
            /* Table full — should not happen with default config */
        }
    if (!uds_service_register(REQUEST_TRANSFER_EXIT, uds_svc_request_transfer_exit, UDS_SESSION_PROGRAMMING))
        {
            /* Table full — should not happen with default config */
        }
    if (!uds_service_register(REQUEST_FILE_TRANSFER, uds_svc_request_file_transfer, UDS_SESSION_PROGRAMMING))
        {
            /* Table full — should not happen with default config */
        }

    /*
     * ================================================================
     * Authentication Service  (Task 19)
     *   SID: 0x29
     * ================================================================
     */
    if (!uds_service_register(AUTHENTICATION, uds_svc_authentication, UDS_SESSION_NON_DEFAULT))
        {
            /* Table full — should not happen with default config */
        }

    /*
     * ================================================================
     * Secured Data Transmission  (stub returning NRC 0x33)
     *   SID: 0x84
     * ================================================================
     */
    if (!uds_service_register(SECURED_DATA_TRANSMISSION, secured_data_stub, UDS_SESSION_ALL))
        {
            /* Table full — should not happen with default config */
        }
}

uint8_t uds_service_get_count(void)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < UDS_SERVICE_TABLE_MAX; i++)
    {
        if (g_service_table[i].active)
            count++;
    }
    return count;
}

const uds_service_entry_t* uds_service_find(uint8_t sid)
{
    return find_entry_mut(sid);
}

void uds_service_clear(void)
{
    for (uint8_t i = 0; i < UDS_SERVICE_TABLE_MAX; i++)
    {
        g_service_table[i].handler         = NULL;
        g_service_table[i].active          = false;
        g_service_table[i].rcrrp_delay_ms  = 0U;
        g_service_table[i].rcrrp_state     = RCRRP_NONE;
    }
}
