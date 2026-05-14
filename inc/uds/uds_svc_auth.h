/*
 * uds_svc_auth.h
 * Authentication Service (0x29) — Module Header
 *
 * Implements the Authentication service handler per ISO 14229-1:2020
 * Annex G.  Supports authenticationConfiguration, deAuthenticate, and
 * PKI stub subfunctions (verifyCertificate, proofOfOwnership,
 * transmitCertificate).
 *
 * PKI operations are stubbed and return NRC 0x34 (authenticationRequired).
 *
 * Wave 3c Task 19 — Authentication Service 0x29 Framework
 */

#pragma once

#include "uds/uds_core.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== *
 * Authentication Subfunction Identifiers (ISO 14229-1:2020 Table G.1)      *
 * ======================================================================== */

/** @brief Authentication subfunction values */
typedef enum {
    AUTH_SF_AUTHENTICATION_CONFIGURATION         = 0x01, /**< @brief 0x01 — authenticationConfiguration */
    AUTH_SF_DE_AUTHENTICATE                      = 0x02, /**< @brief 0x02 — deAuthenticate */
    AUTH_SF_VERIFY_CERTIFICATE_UNIDIRECTIONAL    = 0x11, /**< @brief 0x11 — verifyCertificateUnidirectional (stub) */
    AUTH_SF_VERIFY_CERTIFICATE_BIDIRECTIONAL     = 0x12, /**< @brief 0x12 — verifyCertificateBidirectional (stub) */
    AUTH_SF_PROOF_OF_OWNERSHIP                   = 0x13, /**< @brief 0x13 — proofOfOwnership (stub) */
    AUTH_SF_TRANSMIT_CERTIFICATE                 = 0x14  /**< @brief 0x14 — transmitCertificate (stub) */
} uds_auth_subfunction_t;

/* ======================================================================== *
 * Authentication State                                                     *
 * ======================================================================== */

/** @brief Authentication state machine states */
typedef enum {
    AUTH_STATE_NONE        = 0, /**< @brief Not authenticated, no pending authentication */
    AUTH_STATE_PENDING     = 1, /**< @brief Authentication in progress */
    AUTH_STATE_AUTHENTICATED = 2 /**< @brief Fully authenticated */
} uds_auth_state_t;

/* ======================================================================== *
 * Function Declarations                                                   *
 * ======================================================================== */

/**
 * @brief Initialise the authentication service state.
 *
 * Resets authentication state to AUTH_STATE_NONE.
 * Must be called once during system initialisation.
 */
void uds_svc_auth_init(void);

/**
 * @brief 0x29 — Authentication
 *
 * Dispatches to the subfunction identified by req->subfunction.value:
 *   - 0x01 authenticationConfiguration: returns config data (supported
 *         concepts list) in positive response
 *   - 0x02 deAuthenticate: clears auth state, returns success
 *   - 0x11 verifyCertificateUnidirectional: stub → NRC 0x34
 *   - 0x12 verifyCertificateBidirectional:   stub → NRC 0x34
 *   - 0x13 proofOfOwnership:                 stub → NRC 0x34
 *   - 0x14 transmitCertificate:              stub → NRC 0x34
 *
 * Session check: requires non-default diagnostic session.
 * Context: uds_session_context_t* (must be non-NULL)
 *
 * NRC: 0x12 (SFNS), 0x13 (IMLOIF), 0x22 (CNC), 0x34 (AR for stubs)
 *
 * @param[in]     req      Parsed UDS request
 * @param[out]    rsp      Response structure (positive or negative)
 * @param[in,out] context  Session context pointer (uds_session_context_t*)
 * @return true to send response, false if suppressed by SPRMIB
 */
bool uds_svc_authentication(const uds_request_t *req,
                            uds_response_t      *rsp,
                            void                *context);

/**
 * @brief Get the current authentication state.
 *
 * @return Current uds_auth_state_t value
 */
uds_auth_state_t uds_svc_auth_get_state(void);

#ifdef __cplusplus
}
#endif
