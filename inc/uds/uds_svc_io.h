/*
 * uds_svc_io.h
 * IO Control Service (0x2F) — Module Header
 *
 * Implements the handler for UDS service 0x2F (InputOutputControlByIdentifier)
 * per ISO 14229-1:2020 Section 12.3.
 *
 * Supported control modes:
 *   0x01 — returnControlToStandard
 *   0x02 — resetToDefault
 *   0x03 — freezeCurrentState
 *   0x04 — shortTermAdjustment
 *
 * The handler manages a static IO override table that stores per-DID
 * override values.  Overrides are cleared on session end via
 * uds_svc_io_reset().
 *
 * Each handler follows the pattern:
 *   bool handler(const uds_request_t *req, uds_response_t *rsp, void *context);
 *
 * Return value: true  = send response (rsp populated with positive or negative)
 *               false = suppress response (SPRMIB bit set on positive result)
 *
 * Context: bool* (pointer to unlocked flag, must be non-NULL)
 *
 * Wave 3a Task 12 — IO Control Service (0x2F)
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

/** @brief Maximum number of concurrent IO override entries */
#ifndef IO_OVERRIDE_MAX
#define IO_OVERRIDE_MAX 16
#endif

/** @brief Maximum size of a single IO override value in bytes */
#ifndef IO_OVERRIDE_VALUE_MAX
#define IO_OVERRIDE_VALUE_MAX 8
#endif

/* ======================================================================== *
 * Control Mode Enum                                                        *
 * ======================================================================== */

/** @brief IO control mode values per ISO 14229-1 Table 214 */
typedef enum {
    IO_CONTROL_RETURN_TO_STANDARD   = 0x01, /**< @brief 0x01 — returnControlToStandard */
    IO_CONTROL_RESET_TO_DEFAULT     = 0x02, /**< @brief 0x02 — resetToDefault */
    IO_CONTROL_FREEZE_CURRENT_STATE = 0x03, /**< @brief 0x03 — freezeCurrentState */
    IO_CONTROL_SHORT_TERM_ADJUST    = 0x04  /**< @brief 0x04 — shortTermAdjustment */
} uds_io_control_mode_t;

/* ======================================================================== *
 * Handler Function Declaration                                              *
 * ======================================================================== */

/**
 * @brief 0x2F — InputOutputControlByIdentifier
 *
 * Controls the IO behaviour of a specified DID.  The request contains:
 *   [0x2F][DID_high][DID_low][controlMode][controlParameter...]
 *
 * Response formats:
 *   returnControlToStandard (0x01):
 *     [0x6F][DID_high][DID_low][controlMode]
 *   resetToDefault (0x02):
 *     [0x6F][DID_high][DID_low][controlMode]
 *   freezeCurrentState (0x03):
 *     [0x6F][DID_high][DID_low][controlMode][frozenData...]
 *   shortTermAdjustment (0x04):
 *     [0x6F][DID_high][DID_low][controlMode][controlParameter...]
 *
 * Context: bool* (pointer to unlocked flag, must be non-NULL)
 *
 * NRC: 0x13 (IMLOIF), 0x22 (CNC), 0x31 (ROOR), 0x33 (SAD)
 *
 * @param[in]     req      Parsed UDS request
 * @param[out]    rsp      Response structure (positive or negative)
 * @param[in,out] context  Pointer to bool unlocked flag (must be non-NULL)
 * @return true to send response, false if suppressed by SPRMIB
 */
bool uds_svc_io_control(const uds_request_t *req,
                        uds_response_t      *rsp,
                        void                *context);

/* ======================================================================== *
 * IO State Management APIs                                                 *
 * ======================================================================== */

/**
 * @brief Initialise the IO override table.
 *
 * Clears all override entries.  Must be called once at system startup.
 */
void uds_svc_io_init(void);

/**
 * @brief Reset all IO overrides (clear the override table).
 *
 * Called when the diagnostic session transitions to defaultSession
 * or when a session ends, to restore all DIDs to standard/default values.
 */
void uds_svc_io_reset(void);

/**
 * @brief Check whether a specific DID has an active IO override.
 *
 * @param[in] did  Data Identifier to query
 * @return true if an override entry exists for this DID, false otherwise
 */
bool uds_svc_io_has_override(uint16_t did);

#ifdef __cplusplus
}
#endif
