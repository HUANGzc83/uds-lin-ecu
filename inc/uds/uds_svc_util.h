/*
 * uds_svc_util.h
 * Shared UDS Service Handler Helper Functions — static inline implementations
 *
 * Extracts common response-setup patterns from individual service handlers
 * into reusable shared utilities.  All functions are static inline so that
 * no separate compilation unit is needed.
 *
 * Wave 1 Task 8 (RED) / Task 9 (GREEN) — Shared Response Helper Functions
 */

#pragma once

#include "uds/uds_core.h"
#include <stddef.h>

/* ======================================================================== *
 * Negative Response Helper                                                  *
 * ======================================================================== */

/**
 * @brief Populate a uds_response_t with a negative response (4-parameter).
 *
 * Sets rsp->sid = 0x7F, rsp->subfunc_echo = subfunc_echo, and stores
 * the request SID and NRC as a 2-byte data payload.
 *
 * After populating via this helper, call uds_serialize_response() to produce
 * the wire format: [0x7F][subfunc_echo][request_SID][NRC].
 * For the standard 3-byte format [0x7F][SID][NRC], use
 * uds_serialize_negative_response() or set subfunc_echo = 0 before
 * serializing.
 *
 * @param[out] rsp           Response struct to populate (must not be NULL)
 * @param[in]  sid           Original request SID
 * @param[in]  nrc           Negative response code
 * @param[in]  subfunc_echo  Subfunction value to echo (0 if none)
 * @return UDS_OK on success, UDS_ERR_PARSE if rsp is NULL
 */
static inline uds_status_t uds_set_neg_rsp(uds_response_t *rsp, uint8_t sid,
                                            uds_nrc_t nrc, uint8_t subfunc_echo)
{
    if (rsp == NULL)
    {
        return UDS_ERR_PARSE;
    }

    rsp->sid          = 0x7F;
    rsp->subfunc_echo = subfunc_echo;
    /* Store {request_SID, NRC} as a 2-byte payload in the response */
    {
        static uint8_t payload_buf[2];
        payload_buf[0] = sid;
        payload_buf[1] = (uint8_t)nrc;
        rsp->data     = payload_buf;
        rsp->data_len = 2;
    }
    return UDS_OK;
}

/**
 * @brief Convenience wrapper for SID-only services (subfunc_echo = 0).
 *
 * Calls uds_set_neg_rsp() with subfunc_echo = 0.
 *
 * @param[out] rsp   Response struct to populate (must not be NULL)
 * @param[in]  sid   Original request SID
 * @param[in]  nrc   Negative response code
 * @return UDS_OK on success, UDS_ERR_PARSE if rsp is NULL
 */
static inline uds_status_t uds_set_neg_rsp_sid(uds_response_t *rsp,
                                                 uint8_t sid,
                                                 uds_nrc_t nrc)
{
    return uds_set_neg_rsp(rsp, sid, nrc, 0);
}

/* ======================================================================== *
 * Positive Response Helper                                                  *
 * ======================================================================== */

/**
 * @brief Populate a uds_response_t with a positive response for services
 *        that do NOT have a subfunction parameter (subfunc_echo = 0).
 *
 * Sets rsp->sid = sid + 0x40, rsp->subfunc_echo = 0, and attaches the
 * payload pointer.  After calling, uds_serialize_response() produces:
 *   [sid+0x40][0x00][data...]
 *
 * For subfunction-based services, set rsp->subfunc_echo manually after
 * calling this helper.
 *
 * @param[out] rsp      Response struct to populate (must not be NULL)
 * @param[in]  sid      Request SID (helper adds 0x40 internally)
 * @param[in]  data     Pointer to payload data (may be NULL if data_len == 0)
 * @param[in]  data_len Length of payload data in bytes
 * @return UDS_OK on success, UDS_ERR_PARSE if rsp is NULL
 */
static inline uds_status_t uds_set_pos_rsp(uds_response_t *rsp, uint8_t sid,
                                            const uint8_t *data,
                                            uint16_t data_len)
{
    if (rsp == NULL)
    {
        return UDS_ERR_PARSE;
    }

    rsp->sid          = (uint8_t)(sid + 0x40);
    rsp->subfunc_echo = 0;
    rsp->data         = data;
    rsp->data_len     = data_len;

    return UDS_OK;
}

/* ======================================================================== *
 * Suppress Response Check                                                   *
 * ======================================================================== */

/**
 * @brief Check whether the positive response should be suppressed per SPRMIB.
 *
 * Per ISO 14229-1 Section 8.7: the suppressPosRspMsgIndicationBit shall
 * suppress ONLY positive responses, not negative responses.
 *
 * @param[in] req  Parsed request (may be NULL)
 * @return true if suppress_rsp bit is set, false otherwise (also false if
 *         req == NULL)
 */
static inline bool uds_should_suppress(const uds_request_t *req)
{
    if (req == NULL)
    {
        return false;
    }
    return req->subfunction.suppress_rsp;
}
