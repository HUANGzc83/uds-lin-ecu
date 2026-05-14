/*
 * uds_svc_stored.h
 * Stored Data Services — Module Header
 *
 * Implements handler functions for UDS stored data services:
 *   0x14 — ClearDiagnosticInformation
 *   0x19 — ReadDTCInformation
 *
 * Each handler follows the pattern:
 *   bool handler(const uds_request_t *req, uds_response_t *rsp, void *context);
 *
 * Return value: true  = send response (rsp populated with positive or negative)
 *               false = suppress response (SPRMIB bit set on positive result)
 *
 * Wave 3c Task 11 — Stored Data Services
 */

#pragma once

#include "uds/uds_core.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== *
 * Context Structure                                                        *
 * ======================================================================== */

/**
 * @brief Context for ClearDiagnosticInformation (0x14).
 *
 * The sctx field points to a uds_session_context_t (defined in
 * uds/uds_session.h) for session type checking.  The unlocked field
 * indicates whether security access has been granted.
 */
typedef struct {
    void *sctx;        /**< @brief Pointer to uds_session_context_t (for session check) */
    bool *unlocked;    /**< @brief Pointer to security unlock flag */
} uds_svc_stored_ctx_t;

/* ======================================================================== *
 * Handler Function Declarations                                            *
 * ======================================================================== */

/**
 * @brief 0x14 — ClearDiagnosticInformation
 *
 * Clears DTC records matching the given groupOfDTC.  The 3-byte group
 * identifier is parsed from the request (byte[1] is consumed as subfunction
 * by the parser and reconstructed in the handler).
 *
 * Request:  [0x14][groupOfDTC_hi][groupOfDTC_mid][groupOfDTC_lo]
 * Response: [0x54]   (via serializer: [0x54][0x00] due to subfunc_echo)
 *
 * Context: uds_svc_stored_ctx_t* (must be non-NULL)
 *
 * Session: requires non-default session
 * Security: requires unlocked
 *
 * NRC: 0x13 (IMLOIF), 0x22 (CNC), 0x33 (SAD)
 *
 * @param[in]     req      Parsed UDS request
 * @param[out]    rsp      Response structure
 * @param[in,out] context  Pointer to uds_svc_stored_ctx_t
 * @return true to send response
 */
bool uds_svc_clear_diagnostic_info(const uds_request_t *req,
                                   uds_response_t      *rsp,
                                   void                *context);

/**
 * @brief 0x19 — ReadDTCInformation
 *
 * Supports the following SubFunction values:
 *   0x01 — reportNumberOfDTCByStatusMask
 *   0x02 — reportDTCByStatusMask
 *   0x06 — reportDTCExtendedDataRecordByDTCNumber
 *   0x0A — reportSupportedDTC
 *
 * Calls into the DTC engine (uds_dtc.h) for all data operations.
 *
 * Request:  [0x19][subfunction][data...]
 * Response: [0x59][subfunction][data...]
 *
 * Context: unused (pass NULL)
 *
 * Session: available in all sessions
 *
 * NRC: 0x12 (SFNS), 0x13 (IMLOIF), 0x31 (ROOR)
 *
 * @param[in]     req      Parsed UDS request
 * @param[out]    rsp      Response structure
 * @param[in,out] context  Unused (pass NULL)
 * @return true to send response, false if suppressed by SPRMIB
 */
bool uds_svc_read_dtc_info(const uds_request_t *req,
                           uds_response_t      *rsp,
                           void                *context);

#ifdef __cplusplus
}
#endif
