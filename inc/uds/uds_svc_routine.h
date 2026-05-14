/*
 * uds_svc_routine.h
 * Routine Control Service (0x31) — Module Header
 *
 * Implements RoutineControl service per ISO 14229-1:2020 Section 12.2.
 * Provides a pluggable routine registry with Start/Stop/RequestResults
 * subfunctions and default routines (eraseMemory, checkProgrammingIntegrity).
 *
 * Wave 3b Task 13 — Routine Control Service (0x31 RoutineControl)
 */

#pragma once

#include "uds/uds_core.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== *
 * Configuration Macros                                                     *
 * ======================================================================== */

/** @brief Maximum number of routine entries in the static registry */
#ifndef UDS_ROUTINE_MAX
#define UDS_ROUTINE_MAX  16u
#endif

/** @brief Maximum size of routine status record (response data) in bytes */
#ifndef UDS_ROUTINE_RSP_MAX
#define UDS_ROUTINE_RSP_MAX  8u
#endif

/** @brief Maximum size of control option record (request data) in bytes */
#ifndef UDS_ROUTINE_REQ_MAX
#define UDS_ROUTINE_REQ_MAX  8u
#endif

/* ======================================================================== *
 * Subfunction Identifiers — ISO 14229-1 Table 47                           *
 * ======================================================================== */

/** @brief RoutineControl subfunction values */
typedef enum {
    ROUTINE_SUBFN_START           = 0x01, /**< @brief 0x01 — startRoutine */
    ROUTINE_SUBFN_STOP            = 0x02, /**< @brief 0x02 — stopRoutine */
    ROUTINE_SUBFN_REQUEST_RESULTS = 0x03  /**< @brief 0x03 — requestRoutineResults */
} uds_routine_subfn_t;

/* ======================================================================== *
 * Routine Callback Type                                                    *
 * ======================================================================== */

/**
 * @brief Routine operation callback signature.
 *
 * Invoked when a routine's start/stop/results operation is triggered.
 * The callback receives the controlOptionRecord parameters (if any) and
 * must fill the response buffer with the routineStatusRecord data.
 *
 * @param[in]     params     Pointer to controlOptionRecord data (may be NULL)
 * @param[in]     params_len Length of controlOptionRecord in bytes
 * @param[out]    resp       Buffer for routineStatusRecord response data
 * @param[in,out] resp_len   On input: capacity of resp buffer.
 *                           On success: number of bytes written to resp.
 * @return true if the operation succeeded, false on failure
 */
typedef bool (*routine_fn_t)(const uint8_t *params,
                              uint16_t       params_len,
                              uint8_t       *resp,
                              uint16_t      *resp_len);

/* ======================================================================== *
 * Routine Entry Structure                                                  *
 * ======================================================================== */

/** @brief Session availability bitmask for routine entries */
typedef enum {
    ROUTINE_AVAIL_DEFAULT     = 0x01, /**< @brief Available in defaultSession */
    ROUTINE_AVAIL_PROGRAMMING = 0x02, /**< @brief Available in programmingSession */
    ROUTINE_AVAIL_EXTENDED    = 0x04  /**< @brief Available in extendedSession */
} uds_routine_avail_t;

/** @brief A single routine entry in the registry */
typedef struct {
    uint16_t      routine_id;      /**< @brief Routine identifier (2 bytes) */
    uint8_t       session_mask;    /**< @brief Bitmask of sessions where routine is available */
    uint8_t       security_level;  /**< @brief 0 = no security required, non-zero = requires unlock */
    routine_fn_t  start_fn;        /**< @brief Start routine callback (may be NULL if not supported) */
    routine_fn_t  stop_fn;         /**< @brief Stop routine callback (may be NULL if not supported) */
    routine_fn_t  results_fn;      /**< @brief RequestResults callback (may be NULL if not supported) */
    bool          active;          /**< @brief Whether this entry is registered */
    bool          is_running;      /**< @brief Whether this routine is currently executing */
} uds_routine_entry_t;

/* ======================================================================== *
 * Handler Function Declaration                                             *
 * ======================================================================== */

/**
 * @brief 0x31 — RoutineControl
 *
 * Handles Start (0x01), Stop (0x02), and RequestResults (0x03) subfunctions
 * for registered routines.  Validates the routineID (2 bytes), dispatches
 * to the appropriate callback, and returns the routineStatusRecord.
 *
 * Request:   [0x31][subfunction][routineID_hi][routineID_lo][controlOptionRecord...]
 * Response:  [0x71][subfunction][routineStatusRecord...]
 *
 * Context: bool* (pointer to unlocked flag, must be non-NULL for secured routines)
 *
 * NRC: 0x12 (SFNS), 0x13 (IMLOIF), 0x22 (CNC), 0x31 (ROOR),
 *      0x33 (SAD), 0x72 (GPF)
 *
 * @param[in]     req      Parsed UDS request
 * @param[out]    rsp      Response structure (positive or negative)
 * @param[in,out] context  Pointer to bool unlocked flag (may be NULL if no secured routines)
 * @return true to send response, false if suppressed by SPRMIB
 */
bool uds_svc_routine_control(const uds_request_t *req,
                              uds_response_t      *rsp,
                              void                *context);

/* ======================================================================== *
 * Routine Registry API                                                     *
 * ======================================================================== */

/**
 * @brief Initialise the routine registry.
 *
 * Clears all entries and registers the default routines:
 *   - 0xFF00: eraseMemory
 *   - 0xFF01: checkProgrammingIntegrity
 */
void uds_svc_routine_init(void);

/**
 * @brief Register a custom routine entry.
 *
 * The entry is copied into the static registry table.
 * Registration fails if the table is full or a routine with the same ID
 * already exists.
 *
 * @param[in] entry  Pointer to the routine entry to register
 * @return true if registered successfully, false on failure
 */
bool uds_svc_routine_register(const uds_routine_entry_t *entry);

/**
 * @brief Find a routine entry by its routineID.
 *
 * @param[in] routine_id  Routine identifier to find
 * @return Pointer to the entry, or NULL if not found
 */
const uds_routine_entry_t* uds_svc_routine_find(uint16_t routine_id);

/**
 * @brief Get the number of registered (active) routines.
 *
 * @return Active routine count
 */
uint8_t uds_svc_routine_get_count(void);

/**
 * @brief Get a pointer to the routine entry at index.
 *
 * @param[in] idx  Index into the registry (0-based)
 * @return Pointer to the entry, or NULL if idx is out of range
 */
const uds_routine_entry_t* uds_svc_routine_get_entry(uint8_t idx);

#ifdef __cplusplus
}
#endif
