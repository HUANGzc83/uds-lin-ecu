/*
 * uds_core.c
 * UDS Core Protocol Engine — Parser / Serializer
 *
 * Wave 2 Task 6 — UDS PDU Parser/Serializer
 *
 * Implements generic request parsing and response serialization per
 * ISO 14229-1:2020. No service-specific dispatch logic — this layer
 * handles wire-format extraction and assembly only.
 */

#include "uds/uds_core.h"
#include <string.h>   /* memcpy */

/* ======================================================================== *
 *  uds_parse_request                                                       *
 * ======================================================================== *
 * Parse raw UDS request bytes:
 *   [0]      = SID
 *   [1]      = optional SubFunction byte (if len >= 2)
 *                bit 7   = suppressPosRspMsgIndicationBit
 *                bits 6-0 = SubFunction value
 *   [2...]   = remaining data bytes
 *
 * The SubFunction byte is ALWAYS extracted when len >= 2.  Services that
 * do not use a SubFunction (e.g. ReadDataByIdentifier) will have byte [1]
 * interpreted as the first data byte via the data pointer — callers with
 * service-specific knowledge adjust accordingly.
 * ======================================================================== */
uds_status_t uds_parse_request(const uint8_t *raw, uint16_t len, uds_request_t *req)
{
    /* ----- Validate inputs ----- */
    if (req == NULL)
    {
        return UDS_ERR_PARSE;
    }

    if (len == 0)
    {
        return UDS_ERR_TOO_SHORT;
    }

    if (raw == NULL)
    {
        return UDS_ERR_PARSE;
    }

    /* ----- Extract SID ----- */
    req->sid = raw[0];

    /* ----- Extract SubFunction (if present) ----- */
    if (len >= 2)
    {
        uint8_t sf_byte = raw[1];
        req->subfunction.value        = sf_byte & 0x7F;   /* bits 6:0 */
        req->subfunction.suppress_rsp = (sf_byte >> 7) & 1; /* bit 7   */
        req->data     = raw + 2;
        req->data_len = len - 2;
    }
    else
    {
        req->subfunction.value        = 0;
        req->subfunction.suppress_rsp = 0;
        req->data     = NULL;
        req->data_len = 0;
    }

    return UDS_OK;
}

/* ======================================================================== *
 *  uds_serialize_response                                                  *
 * ======================================================================== *
 * Serialize a structured response into a byte buffer:
 *   buf[0]           = SID (positive response)
 *   buf[1]           = subfunction echo
 *   buf[2...]        = payload data
 *
 * Total bytes = 1 (SID) + 1 (subfunc_echo) + data_len.
 * ======================================================================== */
uds_status_t uds_serialize_response(const uds_response_t *rsp, uint8_t *buf, uint16_t *len)
{
    /* ----- Validate inputs ----- */
    if (rsp == NULL || buf == NULL || len == NULL)
    {
        return UDS_ERR_PARSE;
    }

    uint16_t capacity = *len;
    uint16_t needed   = 1 + 1 + rsp->data_len;   /* SID + subfunc_echo + data */

    if (capacity < needed)
    {
        return UDS_ERR_SERIALIZE;
    }

    /* ----- Serialize ----- */
    buf[0] = rsp->sid;
    buf[1] = rsp->subfunc_echo;

    if (rsp->data_len > 0 && rsp->data != NULL)
    {
        (void)memcpy(buf + 2, rsp->data, rsp->data_len);
    }

    *len = needed;
    return UDS_OK;
}

/* ======================================================================== *
 *  uds_serialize_negative_response                                         *
 * ======================================================================== *
 * Format: [0x7F][SID][NRC]  — exactly 3 bytes.
 * ======================================================================== */
uds_status_t uds_serialize_negative_response(uint8_t sid, uds_nrc_t nrc,
                                              uint8_t *buf, uint16_t *len)
{
    /* ----- Validate inputs ----- */
    if (buf == NULL || len == NULL)
    {
        return UDS_ERR_PARSE;
    }

    if (*len < 3)
    {
        return UDS_ERR_SERIALIZE;
    }

    /* ----- Serialize ----- */
    buf[0] = 0x7F;
    buf[1] = sid;
    buf[2] = (uint8_t)nrc;

    *len = 3;
    return UDS_OK;
}

/* ======================================================================== *
 *  uds_is_positive_response                                                *
 * ======================================================================== *
 * A positive response PDU has a first byte that is NOT 0x7F
 * (the negative response prefix defined in ISO 14229-1).
 * ======================================================================== */
bool uds_is_positive_response(const uint8_t *data)
{
    /* A NULL or zero-length response is trivially not positive. */
    if (data == NULL)
    {
        return false;
    }

    return (data[0] != 0x7F);
}

/* ======================================================================== *
 *  uds_sid_to_response_sid                                                 *
 * ======================================================================== *
 * Per ISO 14229-1: positive response SID = request SID + 0x40.
 * ======================================================================== */
uint8_t uds_sid_to_response_sid(uint8_t sid)
{
    return (uint8_t)(sid + 0x40);
}
