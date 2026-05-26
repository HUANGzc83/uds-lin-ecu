/*
 * uds_svc_routine.c
 * Routine Control Service (0x31) — Implementation
 *
 * Implements RoutineControl handler with Start/Stop/RequestResults
 * subfunctions, a pluggable routine registry, and default routines
 * (eraseMemory 0xFF00, checkProgrammingIntegrity 0xFF01).
 *
 * Each handler follows the pattern:
 *   bool handler(const uds_request_t *req, uds_response_t *rsp, void *context);
 *
 * Return value: true  = send response (rsp populated with positive or negative)
 *               false = suppress response (SPRMIB bit set on positive result)
 *
 * Positive response format:
 *   [0x71][subfunction][routineStatusRecord...]
 *
 * Negative response format:
 *   [0x7F][0x31][NRC]
 *
 * Wave 3b Task 13 — Routine Control Service (0x31 RoutineControl)
 */

#include "uds/uds_svc_routine.h"
#include "uds/uds_svc_util.h"    /* uds_set_neg_rsp, uds_set_pos_rsp, uds_should_suppress */
#include <string.h>   /* memset, memcpy */

/* ======================================================================== *
 * Routine Registry                                                         *
 * ======================================================================== */

/** @brief Static array of routine entries */
static uds_routine_entry_t g_routine_registry[UDS_ROUTINE_MAX];

/**
 * @brief Check whether a subfunction value is a valid RoutineControl subfunction.
 *
 * Valid: 0x01 (Start), 0x02 (Stop), 0x03 (RequestResults).
 *
 * @param[in] subfn  Subfunction value to check
 * @return true if valid, false otherwise
 */
static bool is_subfn_valid(uint8_t subfn)
{
    return (subfn == ROUTINE_SUBFN_START ||
            subfn == ROUTINE_SUBFN_STOP  ||
            subfn == ROUTINE_SUBFN_REQUEST_RESULTS);
}

/**
 * @brief Check if any routine is currently running (other than the specified one).
 *
 * Used to enforce the "only one routine at a time" constraint.
 *
 * @param[in] exclude_id  Routine ID to exclude from the check (or 0 to check all)
 * @return true if at least one other routine is running, false otherwise
 */
static bool is_any_other_running(uint16_t exclude_id)
{
    for (uint8_t i = 0u; i < UDS_ROUTINE_MAX; i++)
    {
        if (g_routine_registry[i].active &&
            g_routine_registry[i].is_running &&
            g_routine_registry[i].routine_id != exclude_id)
        {
            return true;
        }
    }
    return false;
}

/* ======================================================================== *
 * Default Routines — Stubs                                                 *
 * ======================================================================== */

/**
 * @brief 0xFF00 — Erase Memory (stub).
 *
 * Accepts no parameters.  Returns a single byte status: 0x00 = success.
 *
 * @param[in]     params     Control option record data (unused)
 * @param[in]     params_len Length of params (must be 0)
 * @param[out]    resp       Response buffer for routineStatusRecord
 * @param[in,out] resp_len   On input: capacity. On output: bytes written.
 * @return true always (stub always succeeds)
 */
static bool routine_erase_memory(const uint8_t *params,
                                  uint16_t       params_len,
                                  uint8_t       *resp,
                                  uint16_t      *resp_len)
{
    (void)params;

    /* Expect no controlOptionRecord parameters */
    if (params_len != 0u)
    {
        return false;
    }

    /* Return status: 0x00 = erase successful */
    if (resp != NULL && *resp_len >= 1u)
    {
        resp[0] = 0x00u;
        *resp_len = 1u;
        return true;
    }

    return false;
}

/**
 * @brief 0xFF01 — Check Programming Integrity (stub).
 *
 * Accepts no parameters.  Returns a checksum status byte: 0x00 = OK.
 *
 * @param[in]     params     Control option record data (unused)
 * @param[in]     params_len Length of params (must be 0)
 * @param[out]    resp       Response buffer for routineStatusRecord
 * @param[in,out] resp_len   On input: capacity. On output: bytes written.
 * @return true always (stub always succeeds)
 */
static bool routine_check_integrity(const uint8_t *params,
                                     uint16_t       params_len,
                                     uint8_t       *resp,
                                     uint16_t      *resp_len)
{
    (void)params;

    /* Expect no controlOptionRecord parameters */
    if (params_len != 0u)
    {
        return false;
    }

    /* Return status: 0x00 = integrity check passed */
    if (resp != NULL && *resp_len >= 1u)
    {
        resp[0] = 0x00u;
        *resp_len = 1u;
        return true;
    }

    return false;
}

/* ======================================================================== *
 * 0x31 — RoutineControl                                                    *
 * ======================================================================== */

bool uds_svc_routine_control(const uds_request_t *req,
                              uds_response_t      *rsp,
                              void                *context)
{
    uint8_t subfn = req->subfunction.value;

    /* --------------------------------------------------------------- *
     * SFNS: validate subfunction                                      *
     * --------------------------------------------------------------- */
    if (!is_subfn_valid(subfn))
    {
        (void)uds_set_neg_rsp(rsp, req->sid, NRC_SUB_FUNCTION_NOT_SUPPORTED, subfn);
        return true;
    }

    /* --------------------------------------------------------------- *
     * IMLOIF: need at least 2 bytes for routineID                     *
     * --------------------------------------------------------------- */
    if (req->data_len < 2u || req->data == NULL)
    {
        (void)uds_set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT, subfn);
        return true;
    }

    /* Extract routineID (big-endian, 2 bytes) and controlOptionRecord */
    uint16_t routine_id = (uint16_t)((uint16_t)req->data[0] << 8) | req->data[1];
    const uint8_t *ctrl_params = (req->data_len > 2u) ? req->data + 2 : NULL;
    uint16_t       ctrl_len    = (req->data_len > 2u) ? (req->data_len - 2u) : 0u;

    /* --------------------------------------------------------------- *
     * ROOR: find routine in registry                                  *
     * --------------------------------------------------------------- */
    uds_routine_entry_t *entry = NULL;
    for (uint8_t i = 0u; i < UDS_ROUTINE_MAX; i++)
    {
        if (g_routine_registry[i].active &&
            g_routine_registry[i].routine_id == routine_id)
        {
            entry = &g_routine_registry[i];
            break;
        }
    }

    if (entry == NULL)
    {
        (void)uds_set_neg_rsp(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE, subfn);
        return true;
    }

    /* --------------------------------------------------------------- *
     * SAD: check security if required                                 *
     * --------------------------------------------------------------- */
    if (entry->security_level > 0u)
    {
        bool unlocked = (context != NULL) ? *((const bool *)context) : false;
        if (!unlocked)
        {
            (void)uds_set_neg_rsp(rsp, req->sid, NRC_SECURITY_ACCESS_DENIED, subfn);
            return true;
        }
    }

    /* --------------------------------------------------------------- *
     * Dispatch by subfunction                                         *
     * --------------------------------------------------------------- */

    /* Scratch buffer for callback responses */
    uint8_t callback_resp[UDS_ROUTINE_RSP_MAX];
    uint16_t callback_resp_len = sizeof(callback_resp);
    bool callback_ok = false;

    switch (subfn)
    {
    case ROUTINE_SUBFN_START:
        /* CNC: check if the routine is already running */
        if (entry->is_running)
        {
            (void)uds_set_neg_rsp(rsp, req->sid, NRC_CONDITIONS_NOT_CORRECT, subfn);
            return true;
        }
        /* CNC: check if any other routine is running (one-at-a-time) */
        if (is_any_other_running(routine_id))
        {
            (void)uds_set_neg_rsp(rsp, req->sid, NRC_CONDITIONS_NOT_CORRECT, subfn);
            return true;
        }
        /* SFNS: start must be supported (have a callback) */
        if (entry->start_fn == NULL)
        {
            (void)uds_set_neg_rsp(rsp, req->sid, NRC_SUB_FUNCTION_NOT_SUPPORTED, subfn);
            return true;
        }
        /* Invoke start callback */
        callback_resp_len = sizeof(callback_resp);
        callback_ok = entry->start_fn(ctrl_params, ctrl_len,
                                       callback_resp, &callback_resp_len);
        if (!callback_ok)
        {
            (void)uds_set_neg_rsp(rsp, req->sid, NRC_GENERAL_PROGRAMMING_FAILURE, subfn);
            return true;
        }
        /* Mark as running */
        entry->is_running = true;
        break;

    case ROUTINE_SUBFN_STOP:
        /* CNC: check if the routine is running */
        if (!entry->is_running)
        {
            (void)uds_set_neg_rsp(rsp, req->sid, NRC_CONDITIONS_NOT_CORRECT, subfn);
            return true;
        }
        /* SFNS: stop must be supported */
        if (entry->stop_fn == NULL)
        {
            (void)uds_set_neg_rsp(rsp, req->sid, NRC_SUB_FUNCTION_NOT_SUPPORTED, subfn);
            return true;
        }
        /* Invoke stop callback */
        callback_resp_len = sizeof(callback_resp);
        callback_ok = entry->stop_fn(ctrl_params, ctrl_len,
                                       callback_resp, &callback_resp_len);
        if (!callback_ok)
        {
            (void)uds_set_neg_rsp(rsp, req->sid, NRC_GENERAL_PROGRAMMING_FAILURE, subfn);
            return true;
        }
        /* Clear running flag */
        entry->is_running = false;
        break;

    case ROUTINE_SUBFN_REQUEST_RESULTS:
        /* SFNS: requestResults must be supported */
        if (entry->results_fn == NULL)
        {
            (void)uds_set_neg_rsp(rsp, req->sid, NRC_SUB_FUNCTION_NOT_SUPPORTED, subfn);
            return true;
        }
        /* Invoke results callback (routine does not need to be running) */
        callback_resp_len = sizeof(callback_resp);
        callback_ok = entry->results_fn(ctrl_params, ctrl_len,
                                         callback_resp, &callback_resp_len);
        if (!callback_ok)
        {
            (void)uds_set_neg_rsp(rsp, req->sid, NRC_GENERAL_PROGRAMMING_FAILURE, subfn);
            return true;
        }
        break;

    default:
        /* Should not reach (validated above), but defensive */
        (void)uds_set_neg_rsp(rsp, req->sid, NRC_SUB_FUNCTION_NOT_SUPPORTED, subfn);
        return true;
    }

    /* --------------------------------------------------------------- *
     * SPRMIB check — suppress only positive responses                 *
     * --------------------------------------------------------------- */
    if (uds_should_suppress(req))
    {
        return false;
    }

    /* --------------------------------------------------------------- *
     * Build positive response with routineStatusRecord                *
     * --------------------------------------------------------------- */
    (void)uds_set_pos_rsp(rsp, ROUTINE_CONTROL, callback_resp, callback_resp_len);
    rsp->subfunc_echo = subfn;
    return true;
}

/* ======================================================================== *
 * Routine Registry API                                                    *
 * ======================================================================== */

void uds_svc_routine_init(void)
{
    /* Clear entire registry */
    memset(g_routine_registry, 0, sizeof(g_routine_registry));

    /* --------------------------------------------------------------- *
     * Register default routine: 0xFF00 — eraseMemory                  *
     * Programming session only, security required, start only.        *
     * --------------------------------------------------------------- */
    uds_routine_entry_t erase_mem;
    memset(&erase_mem, 0, sizeof(erase_mem));
    erase_mem.routine_id     = 0xFF00u;
    erase_mem.session_mask   = ROUTINE_AVAIL_PROGRAMMING;
    erase_mem.security_level = 1u;   /* requires security unlock */
    erase_mem.start_fn       = routine_erase_memory;
    erase_mem.stop_fn        = NULL; /* stop not supported */
    erase_mem.results_fn     = NULL; /* requestResults not supported */
    if (!uds_svc_routine_register(&erase_mem))
        {
            /* Table full — should not happen with default config */
        }

    /* --------------------------------------------------------------- *
     * Register default routine: 0xFF01 — checkProgrammingIntegrity    *
     * Available in all sessions, no security required, start only.    *
     * --------------------------------------------------------------- */
    uds_routine_entry_t check_int;
    memset(&check_int, 0, sizeof(check_int));
    check_int.routine_id     = 0xFF01u;
    check_int.session_mask   = (uint8_t)(ROUTINE_AVAIL_DEFAULT |
                                          ROUTINE_AVAIL_PROGRAMMING |
                                          ROUTINE_AVAIL_EXTENDED);
    check_int.security_level = 0u;   /* no security required */
    check_int.start_fn       = routine_check_integrity;
    check_int.stop_fn        = NULL;
    check_int.results_fn     = NULL;
    if (!uds_svc_routine_register(&check_int))
        {
            /* Table full — should not happen with default config */
        }
}

bool uds_svc_routine_register(const uds_routine_entry_t *entry)
{
    if (entry == NULL)
    {
        return false;
    }

    /* Check for duplicate routine ID */
    for (uint8_t i = 0u; i < UDS_ROUTINE_MAX; i++)
    {
        if (g_routine_registry[i].active &&
            g_routine_registry[i].routine_id == entry->routine_id)
        {
            return false; /* duplicate ID */
        }
    }

    /* Find first empty slot */
    for (uint8_t i = 0u; i < UDS_ROUTINE_MAX; i++)
    {
        if (!g_routine_registry[i].active)
        {
            g_routine_registry[i] = *entry;
            g_routine_registry[i].active     = true;
            g_routine_registry[i].is_running = false;
            return true;
        }
    }

    return false; /* table full */
}

const uds_routine_entry_t* uds_svc_routine_find(uint16_t routine_id)
{
    for (uint8_t i = 0u; i < UDS_ROUTINE_MAX; i++)
    {
        if (g_routine_registry[i].active &&
            g_routine_registry[i].routine_id == routine_id)
        {
            return &g_routine_registry[i];
        }
    }
    return NULL;
}

uint8_t uds_svc_routine_get_count(void)
{
    uint8_t count = 0u;
    for (uint8_t i = 0u; i < UDS_ROUTINE_MAX; i++)
    {
        if (g_routine_registry[i].active)
        {
            count++;
        }
    }
    return count;
}

const uds_routine_entry_t* uds_svc_routine_get_entry(uint8_t idx)
{
    uint8_t found = 0u;
    for (uint8_t i = 0u; i < UDS_ROUTINE_MAX; i++)
    {
        if (g_routine_registry[i].active)
        {
            if (found == idx)
            {
                return &g_routine_registry[i];
            }
            found++;
        }
    }
    return NULL;
}
