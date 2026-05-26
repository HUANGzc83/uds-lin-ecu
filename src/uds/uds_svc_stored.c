/*
 * uds_svc_stored.c
 * Stored Data Services — Implementation
 *
 * Implements handler functions for UDS stored data services:
 *   0x14 — ClearDiagnosticInformation
 *   0x19 — ReadDTCInformation
 *
 * Each handler validates the request, performs the service, and populates
 * a uds_response_t with either a positive response or a negative response.
 * Return true to send, false to suppress (SPRMIB).
 *
 * Positive response format (via uds_serialize_response):
 *   [response_SID][subfunction_echo][data...]
 *
 * Negative response format (via same serialization, sid=0x7F):
 *   [0x7F][request_SID][NRC]
 *
 * Wave 3c Task 11 — Stored Data Services
 */

#include "uds/uds_svc_stored.h"
#include "uds/uds_dtc.h"
#include "uds/uds_session.h"
#include <string.h>   /* memset, memcpy */
#include "uds/uds_svc_util.h"

/**
 * @brief Compute the DTCStatusAvailabilityMask for a set of DTCs.
 *
 * Returns the bitwise OR of all status bytes for DTCs in the array.
 *
 * @param[in] dtcs   Array of DTC codes
 * @param[in] count  Number of DTCs in the array
 * @return Combined status availability mask
 */
static uint8_t compute_availability_mask(const uint32_t *dtcs, uint16_t count)
{
    uint8_t mask = 0;
    for (uint16_t i = 0; i < count; i++)
    {
        mask |= uds_dtc_get_status(dtcs[i]);
    }
    return mask;
}

/**
 * @brief Write DTC data records (3-byte DTC + 1-byte status) into a buffer.
 *
 * @param[in]  dtcs     Array of DTC codes
 * @param[in]  count    Number of DTCs
 * @param[out] buf      Output buffer
 * @param[in]  buf_size Capacity of output buffer
 * @return Number of bytes written
 */
static uint16_t write_dtc_records(const uint32_t *dtcs, uint16_t count,
                                  uint8_t *buf, uint16_t buf_size)
{
    uint16_t offset = 0;

    for (uint16_t i = 0; i < count; i++)
    {
        if (offset + 4 > buf_size)
        {
            break;  /* Buffer full — truncate */
        }

        /* 3-byte DTC, big-endian */
        buf[offset++] = (uint8_t)((dtcs[i] >> 16) & 0xFF);
        buf[offset++] = (uint8_t)((dtcs[i] >>  8) & 0xFF);
        buf[offset++] = (uint8_t)( dtcs[i]        & 0xFF);
        buf[offset++] = uds_dtc_get_status(dtcs[i]);
    }

    return offset;
}

/* ======================================================================== *
 * Internal Helper                                                          *
 * ======================================================================== */

/**
 * @brief Local 3-arg wrapper for negative response (subfunc_echo = sid).
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
 * ReadDTCInformation Subfunction Handlers                                  *
 * ======================================================================== */

/**
 * @brief Handle reportNumberOfDTCByStatusMask (0x01).
 *
 * Response: [0x59][0x01][statusMask][DTCStatusAvailabilityMask]
 *           [numberOfDTCByStatusMask_hi][numberOfDTCByStatusMask_lo]
 */
static bool handle_report_number_dtc(const uds_request_t *req,
                                     uds_response_t      *rsp)
{
    static uint8_t rsp_buf[4];

    /* IMLOIF: need exactly 1 data byte (statusMask) */
    if (req->data_len < 1 || req->data == NULL)
    {
        set_neg_rsp_local(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return true;
    }

    uint8_t status_mask = req->data[0];

    /* Get matching DTCs */
    uint32_t dtc_list[UDS_DTC_MAX];
    uint16_t count = uds_dtc_get_by_status_mask(status_mask, dtc_list, UDS_DTC_MAX);

    /* Compute DTCStatusAvailabilityMask */
    uint8_t avail_mask = compute_availability_mask(dtc_list, count);

    /* Build response: [statusMask][availMask][count_hi][count_lo] */
    rsp_buf[0] = status_mask;
    rsp_buf[1] = avail_mask;
    rsp_buf[2] = (uint8_t)((count >> 8) & 0xFF);
    rsp_buf[3] = (uint8_t)( count       & 0xFF);

    /* SPRMIB check */
    if (uds_should_suppress(req))
    {
        return false;
    }

    uds_set_pos_rsp(rsp, READ_DTC_INFO, rsp_buf, 4);
    rsp->subfunc_echo = 0x01;
    return true;
}

/**
 * @brief Handle reportDTCByStatusMask (0x02).
 *
 * Response: [0x59][0x02][statusMask][DTCStatusAvailabilityMask]
 *           [DTC_3bytes][status_1byte] ... (for each matching DTC)
 */
static bool handle_report_dtc_by_mask(const uds_request_t *req,
                                      uds_response_t      *rsp)
{
    static uint8_t rsp_buf[256];

    /* IMLOIF: need exactly 1 data byte (statusMask) */
    if (req->data_len < 1 || req->data == NULL)
    {
        set_neg_rsp_local(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return true;
    }

    uint8_t status_mask = req->data[0];

    /* Get matching DTCs */
    uint32_t dtc_list[UDS_DTC_MAX];
    uint16_t count = uds_dtc_get_by_status_mask(status_mask, dtc_list, UDS_DTC_MAX);

    /* Compute DTCStatusAvailabilityMask */
    uint8_t avail_mask = compute_availability_mask(dtc_list, count);

    /* Build response header: [statusMask][availMask] */
    rsp_buf[0] = status_mask;
    rsp_buf[1] = avail_mask;

    /* Append DTC records (3-byte DTC + 1-byte status each) */
    uint16_t data_offset = 2;
    uint16_t dtc_bytes = write_dtc_records(dtc_list, count,
                                           rsp_buf + data_offset,
                                           (uint16_t)(sizeof(rsp_buf) - data_offset));

    /* SPRMIB check */
    if (uds_should_suppress(req))
    {
        return false;
    }

    uds_set_pos_rsp(rsp, READ_DTC_INFO, rsp_buf,
                    (uint16_t)(data_offset + dtc_bytes));
    rsp->subfunc_echo = 0x02;
    return true;
}

/**
 * @brief Handle reportDTCExtendedDataRecordByDTCNumber (0x06).
 *
 * Request data: [DTC_hi][DTC_mid][DTC_lo][recordNumber]
 * Response: [0x59][0x06][DTC_hi][DTC_mid][DTC_lo][recordNumber][extData...]
 *
 * recordNumber = 1 → snapshot data
 * recordNumber = 2 → extended data
 * recordNumber = other → NRC 0x31 (ROOR)
 */
static bool handle_report_extended_data(const uds_request_t *req,
                                        uds_response_t      *rsp)
{
    static uint8_t rsp_buf[256];

    /* IMLOIF: need exactly 4 data bytes (DTC(3) + recordNumber(1)) */
    if (req->data_len < 4 || req->data == NULL)
    {
        set_neg_rsp_local(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return true;
    }

    /* Reconstruct 24-bit DTC from first 3 data bytes */
    uint32_t dtc = ((uint32_t)req->data[0] << 16) |
                   ((uint32_t)req->data[1] <<  8) |
                   ((uint32_t)req->data[2]);
    uint8_t record_number = req->data[3];

    /* Build response header: DTC (3 bytes) + recordNumber (1 byte) */
    rsp_buf[0] = req->data[0];
    rsp_buf[1] = req->data[1];
    rsp_buf[2] = req->data[2];
    rsp_buf[3] = record_number;

    uint16_t ext_len = 0;

    if (record_number == 1)
    {
        /* Snapshot record */
        uint16_t snap_len = 0;
        if (!uds_dtc_get_snapshot(dtc, rsp_buf + 4, &snap_len))
        {
            set_neg_rsp_local(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
            return true;
        }
        ext_len = snap_len;
    }
    else if (record_number == 2)
    {
        /* Extended data record */
        uint16_t ext_data_len = 0;
        if (!uds_dtc_get_extended(dtc, rsp_buf + 4, &ext_data_len))
        {
            set_neg_rsp_local(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
            return true;
        }
        ext_len = ext_data_len;
    }
    else
    {
        /* Unsupported record number */
        set_neg_rsp_local(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return true;
    }

    /* SPRMIB check */
    if (uds_should_suppress(req))
    {
        return false;
    }

    uds_set_pos_rsp(rsp, READ_DTC_INFO, rsp_buf, (uint16_t)(4 + ext_len));
    rsp->subfunc_echo = 0x06;
    return true;
}

/**
 * @brief Handle reportSupportedDTC (0x0A).
 *
 * Returns all DTCs that exist in the database regardless of status.
 * Uses statusMask = 0xFF to match all possible status bits.
 *
 * Response: [0x59][0x0A][statusAvailabilityMask][DTCStatusAvailabilityMask]
 *           [DTC_3bytes][status_1byte] ... (for each DTC)
 */
static bool handle_report_supported_dtc(const uds_request_t *req,
                                        uds_response_t      *rsp)
{
    static uint8_t rsp_buf[256];

    /* IMLOIF: no additional data expected */
    if (req->data_len != 0)
    {
        set_neg_rsp_local(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return true;
    }

    /* Get all DTCs with any status bit set (mask 0xFF) */
    uint32_t dtc_list[UDS_DTC_MAX];
    uint16_t count = uds_dtc_get_by_status_mask(0xFF, dtc_list, UDS_DTC_MAX);

    /* Compute DTCStatusAvailabilityMask */
    uint8_t avail_mask = compute_availability_mask(dtc_list, count);

    /* Build response: [statusMask=0xFF][availMask][DTC records...] */
    rsp_buf[0] = 0xFF;  /* statusAvailabilityMask: all bits supported */
    rsp_buf[1] = avail_mask;

    uint16_t data_offset = 2;
    uint16_t dtc_bytes = write_dtc_records(dtc_list, count,
                                           rsp_buf + data_offset,
                                           (uint16_t)(sizeof(rsp_buf) - data_offset));

    /* SPRMIB check */
    if (uds_should_suppress(req))
    {
        return false;
    }

    uds_set_pos_rsp(rsp, READ_DTC_INFO, rsp_buf,
                    (uint16_t)(data_offset + dtc_bytes));
    rsp->subfunc_echo = 0x0A;
    return true;
}

/* ======================================================================== *
 * 0x14 — ClearDiagnosticInformation                                        *
 * ======================================================================== */

bool uds_svc_clear_diagnostic_info(const uds_request_t *req,
                                   uds_response_t      *rsp,
                                   void                *context)
{
    /* --- CNC: context must be provided --- */
    if (context == NULL)
    {
        set_neg_rsp_local(rsp, req->sid, NRC_CONDITIONS_NOT_CORRECT);
        return true;
    }

    uds_svc_stored_ctx_t *ctx = (uds_svc_stored_ctx_t *)context;
    uds_session_context_t *sctx = (uds_session_context_t *)ctx->sctx;

    /* --- CNC: session must be non-default --- */
    if (sctx == NULL || sctx->current_session == UDS_DEFAULT_SESSION)
    {
        set_neg_rsp_local(rsp, req->sid, NRC_CONDITIONS_NOT_CORRECT);
        return true;
    }

    /* --- SAD: security must be unlocked --- */
    if (ctx->unlocked == NULL || !(*ctx->unlocked))
    {
        set_neg_rsp_local(rsp, req->sid, NRC_SECURITY_ACCESS_DENIED);
        return true;
    }

    /* --- IMLOIF: need exactly 3 bytes for groupOfDTC --- */
    /* Parser consumed byte[1] as subfunction; we reconstruct group_hi from
     * subfunction.value + suppress_rsp bit. Remaining data should be 2 bytes. */
    if (req->data_len != 2 || req->data == NULL)
    {
        set_neg_rsp_local(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return true;
    }

    /* Reconstruct 3-byte groupOfDTC:
     *   byte[1] of raw = subfunction.value | (suppress_rsp << 7)
     *   data[0] = group_mid
     *   data[1] = group_lo */
    uint8_t group_hi = (uint8_t)(req->subfunction.value |
                                 (req->subfunction.suppress_rsp << 7));
    uint32_t group_of_dtc = ((uint32_t)group_hi << 16) |
                            ((uint32_t)req->data[0] << 8) |
                            ((uint32_t)req->data[1]);

    /* Call DTC engine to clear matching records */
    uds_dtc_clear(group_of_dtc);

    /* Positive response: [0x54]  (no subfunction, no additional data).
     * The serializer will write [0x54][0x00] due to subfunc_echo always
     * being serialized; the dispatch layer handles this. */
    uds_set_pos_rsp(rsp, CLEAR_DIAGNOSTIC_INFO, NULL, 0);
    return true;
}

/* ======================================================================== *
 * 0x19 — ReadDTCInformation                                                *
 * ======================================================================== */

bool uds_svc_read_dtc_info(const uds_request_t *req,
                           uds_response_t      *rsp,
                           void                *context)
{
    (void)context;

    uint8_t subfn = req->subfunction.value;

    /* Dispatch by subfunction */
    switch (subfn)
    {
    case 0x01:
        return handle_report_number_dtc(req, rsp);

    case 0x02:
        return handle_report_dtc_by_mask(req, rsp);

    case 0x06:
        return handle_report_extended_data(req, rsp);

    case 0x0A:
        return handle_report_supported_dtc(req, rsp);

    default:
        /* SFNS: unsupported subfunction */
        set_neg_rsp_local(rsp, req->sid, NRC_SUB_FUNCTION_NOT_SUPPORTED);
        return true;
    }
}
