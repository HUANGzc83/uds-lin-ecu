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
 * @brief Populate a uds_response_t with a negative response.
 *
 * Uses 0x7F as the "SID" so that uds_serialize_response produces:
 *   [0x7F][request_SID][NRC]  — the standard UDS negative response format.
 *
 * @param[out] rsp      Response structure to fill
 * @param[in]  req_sid  Original request SID
 * @param[in]  nrc      Negative response code
 */
static void set_neg_rsp(uds_response_t *rsp, uint8_t req_sid, uint8_t nrc)
{
    g_rsp_data_buf[0] = nrc;

    rsp->sid          = 0x7F;           /* negative response prefix */
    rsp->subfunc_echo = req_sid;        /* original SID */
    rsp->data         = g_rsp_data_buf;
    rsp->data_len     = 1;
}

/**
 * @brief Build a positive response for the Authentication service.
 *
 * @param[out] rsp       Response structure to fill
 * @param[in]  subfunc   SubFunction value to echo
 * @param[in]  data      Optional payload data pointer (may be NULL)
 * @param[in]  data_len  Payload length in bytes
 */
static void set_pos_rsp(uds_response_t *rsp,
                        uint8_t subfunc, const uint8_t *data, uint16_t data_len)
{
    rsp->sid          = AUTHENTICATION_RSP;
    rsp->subfunc_echo = subfunc;
    rsp->data         = data;
    rsp->data_len     = data_len;
}

/**
 * @brief Check if the positive response should be suppressed per SPRMIB.
 *
 * Per ISO 14229-1 Section 8.7: suppressPosRspMsgIndicationBit suppresses
 * ONLY positive responses, NOT negative ones.
 *
 * @param[in] req  Parsed request
 * @return true if response should be suppressed, false otherwise
 */
static inline bool should_suppress(const uds_request_t *req)
{
    return req->subfunction.suppress_rsp;
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
        set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return true;
    }

    /* --- SPRMIB check --- */
    if (should_suppress(req))
    {
        return false;
    }

    /* Build manufacturer-specific config payload */
    g_rsp_data_buf[0] = 0x01u;  /* number of supported concepts */
    g_rsp_data_buf[1] = 0x01u;  /* concept 1: certificate-based authentication */

    set_pos_rsp(rsp, AUTH_SF_AUTHENTICATION_CONFIGURATION, g_rsp_data_buf, 2);
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
        set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return true;
    }

    /* Clear authentication state */
    g_auth_state = AUTH_STATE_NONE;

    /* --- SPRMIB check --- */
    if (should_suppress(req))
    {
        return false;
    }

    /* Positive response with no additional data */
    set_pos_rsp(rsp, AUTH_SF_DE_AUTHENTICATE, NULL, 0);
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
    set_neg_rsp(rsp, req->sid, NRC_AUTHENTICATION_REQUIRED);
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
        set_neg_rsp(rsp, req->sid, NRC_CONDITIONS_NOT_CORRECT);
        return true;
    }

    const uds_session_context_t *sctx = (const uds_session_context_t *)context;

    /* --- CNC: must be in a non-default diagnostic session --- */
    if (sctx->current_session == UDS_DEFAULT_SESSION)
    {
        set_neg_rsp(rsp, req->sid, NRC_CONDITIONS_NOT_CORRECT);
        return true;
    }

    uint8_t subfn = req->subfunction.value;

    /* --- SFNS: check subfunction is known --- */
    if (!is_auth_subfn_supported(subfn))
    {
        set_neg_rsp(rsp, req->sid, NRC_SUB_FUNCTION_NOT_SUPPORTED);
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
    set_neg_rsp(rsp, req->sid, NRC_SUB_FUNCTION_NOT_SUPPORTED);
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
