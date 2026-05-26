/*
 * uds_svc_auth.c
 * Authentication Service (0x29) — Implementation
 *
 * Implements the 0x29 Authentication handler per ISO 14229-1:2020 Annex G.
 * Supports subfunctions:
 *   - 0x01 authenticationConfiguration: returns manufacturer-specific config
 *   - 0x02 deAuthenticate: clears auth state
 *   - 0x11–0x14 PKI operations: stubbed → NRC 0x34 (authenticationRequired)
 *
 * The handler checks the current diagnostic session — only non-default
 * sessions are permitted (NRC 0x22 in defaultSession).
 *
 * Wave 3c Task 19 — Authentication Service 0x29 Framework
 */

#include "uds/uds_svc_auth.h"
#include "uds/uds_session.h"
#include "uds/uds_security.h"
#include "uds/uds_svc_util.h"
#include <string.h>   /* memset */

/* ======================================================================== *
 * Static Response Buffers                                                   *
 * ======================================================================== */

/**
 * @brief Buffer for short response data.
 * Safe to re-use because services are invoked sequentially (single-threaded).
 */
static uint8_t g_rsp_data_buf[8];

/* ======================================================================== *
 * Authentication State                                                     *
 * ======================================================================== */

/** @brief Current authentication state */
static uds_auth_state_t g_auth_state = AUTH_STATE_NONE;

/* ======================================================================== *
 * Supported Subfunction Lookup                                              *
 * ======================================================================== */

/**
 * @brief Check whether a subfunction value is a known Authentication subfunction.
 *
 * Known subfunctions: 0x01, 0x02, 0x11, 0x12, 0x13, 0x14.
 *
 * @param[in] subfn  Subfunction value to check
 * @return true if known, false otherwise
 */
static bool is_auth_subfn_supported(uint8_t subfn)
{
    switch (subfn)
    {
    case AUTH_SF_AUTHENTICATION_CONFIGURATION:
    case AUTH_SF_DE_AUTHENTICATE:
    case AUTH_SF_VERIFY_CERTIFICATE_UNIDIRECTIONAL:
    case AUTH_SF_VERIFY_CERTIFICATE_BIDIRECTIONAL:
    case AUTH_SF_PROOF_OF_OWNERSHIP:
    case AUTH_SF_TRANSMIT_CERTIFICATE:
        return true;
    default:
        return false;
    }
}

/**
 * @brief Check if a subfunction is a PKI stub (returns NRC 0x34).
 *
 * @param[in] subfn  Subfunction value
 * @return true if subfunction is a PKI stub
 */
static inline bool is_pki_stub(uint8_t subfn)
{
    return (subfn == AUTH_SF_VERIFY_CERTIFICATE_UNIDIRECTIONAL ||
            subfn == AUTH_SF_VERIFY_CERTIFICATE_BIDIRECTIONAL ||
            subfn == AUTH_SF_PROOF_OF_OWNERSHIP               ||
            subfn == AUTH_SF_TRANSMIT_CERTIFICATE);
}

/* ======================================================================== *
 * Internal Helpers                                                          *
 * ======================================================================== */

/**
 * @brief Local 3-arg wrapper for negative response (subfunc_echo = sid).
 *
 * Uses data_len = 1 (just NRC byte) to match test expectations.
 */
static void set_neg_rsp_local(uds_response_t *rsp, uint8_t sid, uds_nrc_t nrc)
{
    static uint8_t nrc_buf[1];
    nrc_buf[0] = (uint8_t)nrc;
    rsp->sid          = 0x7F;
    rsp->subfunc_echo = sid;
    rsp->data         = nrc_buf;
    rsp->data_len     = 1;
}

/* ======================================================================== *
 * Subfunction Handlers                                                     *
 * ======================================================================== */

/**
 * @brief Handle authenticationConfiguration (0x01).
 *
 * Returns manufacturer-specific configuration data indicating the
 * supported authentication concepts.
 *
 * Positive response payload:
 *   Byte 0: number of supported authentication concepts (0x01)
 *   Byte 1: concept identifier (0x01 = certificate-based)
 *
 * @param[out] rsp  Response structure
 * @param[in]  req  Parsed request
 * @return true always (signal to send response)
 */
static bool handle_auth_config(const uds_request_t *req, uds_response_t *rsp)
{
    /* IMLOIF: no additional data bytes expected for configuration query */
    if (req->data_len != 0u)
    {
        set_neg_rsp_local(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return true;
    }

    /* --- SPRMIB check --- */
    if (uds_should_suppress(req))
    {
        return false;
    }

    /* Build manufacturer-specific config payload */
    g_rsp_data_buf[0] = 0x01u;  /* number of supported concepts */
    g_rsp_data_buf[1] = 0x01u;  /* concept 1: certificate-based authentication */

    uds_set_pos_rsp(rsp, req->sid, g_rsp_data_buf, 2);
    rsp->subfunc_echo = AUTH_SF_AUTHENTICATION_CONFIGURATION;
    return true;
}

/**
 * @brief Handle deAuthenticate (0x02).
 *
 * Clears the authentication state and returns success immediately.
 *
 * @param[out] rsp  Response structure
 * @param[in]  req  Parsed request
 * @return true always (signal to send response)
 */
static bool handle_deauth(const uds_request_t *req, uds_response_t *rsp)
{
    /* IMLOIF: no additional data bytes expected */
    if (req->data_len != 0u)
    {
        set_neg_rsp_local(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return true;
    }

    /* Clear authentication state */
    g_auth_state = AUTH_STATE_NONE;

    /* --- SPRMIB check --- */
    if (uds_should_suppress(req))
    {
        return false;
    }

    /* Positive response with no additional data */
    uds_set_pos_rsp(rsp, req->sid, NULL, 0);
    rsp->subfunc_echo = AUTH_SF_DE_AUTHENTICATE;
    return true;
}

/**
 * @brief Handle PKI stub subfunctions (0x11–0x14).
 *
 * All PKI operations are stubbed and return NRC 0x34
 * (authenticationRequired).
 *
 * @param[out] rsp    Response structure
 * @param[in]  req    Parsed request
 * @return true always (signal to send response)
 */
static bool handle_pki_stub(const uds_request_t *req, uds_response_t *rsp)
{
    /* PKI operations are not implemented — return authenticationRequired */
    (void)req;
    set_neg_rsp_local(rsp, req->sid, NRC_AUTHENTICATION_REQUIRED);
    return true;
}

/* ======================================================================== *
 * 0x29 — Authentication                                                    *
 * ======================================================================== */

bool uds_svc_authentication(const uds_request_t *req,
                            uds_response_t      *rsp,
                            void                *context)
{
    /* --- Validate context (session context required) --- */
    if (context == NULL)
    {
        set_neg_rsp_local(rsp, req->sid, NRC_CONDITIONS_NOT_CORRECT);
        return true;
    }

    const uds_session_context_t *sctx = (const uds_session_context_t *)context;

    /* --- CNC: must be in a non-default diagnostic session --- */
    if (sctx->current_session == UDS_DEFAULT_SESSION)
    {
        set_neg_rsp_local(rsp, req->sid, NRC_CONDITIONS_NOT_CORRECT);
        return true;
    }

    uint8_t subfn = req->subfunction.value;

    /* --- SFNS: check subfunction is known --- */
    if (!is_auth_subfn_supported(subfn))
    {
        set_neg_rsp_local(rsp, req->sid, NRC_SUB_FUNCTION_NOT_SUPPORTED);
        return true;
    }

    /* --- Dispatch by subfunction --- */
    if (subfn == AUTH_SF_AUTHENTICATION_CONFIGURATION)
    {
        return handle_auth_config(req, rsp);
    }
    else if (subfn == AUTH_SF_DE_AUTHENTICATE)
    {
        return handle_deauth(req, rsp);
    }
    else if (is_pki_stub(subfn))
    {
        return handle_pki_stub(req, rsp);
    }

    /* Should not be reachable (validated above), but guard defensively */
    set_neg_rsp_local(rsp, req->sid, NRC_SUB_FUNCTION_NOT_SUPPORTED);
    return true;
}

/* ======================================================================== *
 * Initialisation & State Access                                            *
 * ======================================================================== */

void uds_svc_auth_init(void)
{
    g_auth_state = AUTH_STATE_NONE;
    memset(g_rsp_data_buf, 0, sizeof(g_rsp_data_buf));
}

uds_auth_state_t uds_svc_auth_get_state(void)
{
    return g_auth_state;
}
