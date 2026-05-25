/*
 * uds_svc_diagcomm.c
 * Diagnostic & Communication Management Services — Implementation
 *
 * Implements handler functions for UDS services 0x10, 0x11, 0x27, 0x28,
 * 0x3E, 0x85, 0x86, and 0x87 per ISO 14229-1:2020.
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
 * Wave 3a Task 9 — Diagnostic & Communication Management Services
 */

#include "uds/uds_svc_diagcomm.h"
#include "uds/uds_session.h"
#include "uds/uds_security.h"
#include <string.h>   /* memset, memcpy */

/** @brief Static array of event records */
static uds_event_record_t g_event_store[UDS_EVENT_STORE_MAX];

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
static void set_neg_rsp(uds_response_t *rsp, uint8_t req_sid, uint8_t nrc, uint8_t *buf)
{
    /* Pack NRC into the buffer byte */
    buf[0] = nrc;

    rsp->sid          = 0x7F;           /* negative response prefix */
    rsp->subfunc_echo = req_sid;        /* original SID */
    rsp->data         = buf;            /* points to NRC byte */
    rsp->data_len     = 1;
}

/**
 * @brief Build a positive response for a service with a SubFunction.
 *
 * @param[out] rsp       Response structure to fill
 * @param[in]  rsp_sid   Positive response SID
 * @param[in]  subfunc   SubFunction value to echo
 * @param[in]  data      Optional payload data pointer (may be NULL)
 * @param[in]  data_len  Payload length in bytes
 */
static void set_pos_rsp(uds_response_t *rsp, uint8_t rsp_sid,
                        uint8_t subfunc, const uint8_t *data, uint16_t data_len)
{
    rsp->sid          = rsp_sid;
    rsp->subfunc_echo = subfunc;
    rsp->data         = data;
    rsp->data_len     = data_len;
}

/**
 * @brief Check if the positive response should be suppressed per SPRMIB.
 *
 * Per ISO 14229-1 Section 8.7: suppressPosRspMsgIndicationBit suppresses
 * ONLY positive responses, NOT negative ones.  Negative responses are
 * always sent for physical addressing.
 *
 * @param[in] req  Parsed request (checked for suppress_rsp bit)
 * @return true if response should be suppressed, false otherwise
 */
static inline bool should_suppress(const uds_request_t *req)
{
    return req->subfunction.suppress_rsp;
}

/* ======================================================================== *
 * ECU Reset Subfunction Validation                                         *
 * ======================================================================== */

/**
 * @brief Check whether a reset type is supported.
 *
 * Supported: 0x01 (hardReset), 0x02 (keyOffOnReset), 0x03 (softReset).
 *
 * @param[in] reset_type  Reset type subfunction value
 * @return true if supported, false otherwise
 */
static bool is_reset_type_supported(uint8_t reset_type)
{
    return (reset_type == 0x01u ||
            reset_type == 0x02u ||
            reset_type == 0x03u);
}

/* ======================================================================== *
 * Communication Control — ControlType Validation                           *
 * ======================================================================== */

/**
 * @brief Check whether a CommunicationControl controlType is valid.
 *
 * Valid: 0x00 (enableRxAndTx), 0x01 (enableRxAndDisableTx),
 *        0x02 (disableRxAndEnableTx), 0x03 (disableRxAndTx).
 *
 * @param[in] ctrl  Control type value
 * @return true if valid, false otherwise
 */
static bool is_control_type_valid(uint8_t ctrl)
{
    return (ctrl <= 0x03u);
}

/**
 * @brief Check whether a CommunicationControl communicationType is valid.
 *
 * Valid: 0x01 (normal), 0x02 (networkManagement), 0x03 (normalAndNetworkMgmt).
 *
 * @param[in] comm_type  Communication type value
 * @return true if valid, false otherwise
 */
static bool is_comm_type_valid(uint8_t comm_type)
{
    return (comm_type >= 0x01u && comm_type <= 0x03u);
}

/* ======================================================================== *
 * DTC Setting Subfunction Validation                                       *
 * ======================================================================== */

/**
 * @brief Check whether a DTC setting type is supported.
 *
 * Supported: 0x01 (on), 0x02 (off).
 *
 * @param[in] setting  DTC setting type value
 * @return true if supported, false otherwise
 */
static bool is_dtc_setting_supported(uint8_t setting)
{
    return (setting == 0x01u || setting == 0x02u);
}

/* ======================================================================== *
 * ResponseOnEvent Subfunction Validation                                   *
 * ======================================================================== */

/**
 * @brief Check whether a ResponseOnEvent subfunction is valid.
 *
 * Valid: 0x00..0x04.
 *
 * @param[in] subfn  Subfunction value
 * @return true if valid, false otherwise
 */
static bool is_event_subfn_valid(uint8_t subfn)
{
    return (subfn <= 0x04u);
}

/* ======================================================================== *
 * 0x10 — DiagnosticSessionControl                                         *
 * ======================================================================== */

bool uds_svc_diagnostic_session_control(const uds_request_t *req,
                                        uds_response_t      *rsp,
                                        void                *context)
{
    uint8_t rsp_buf[4];

    /* --- Validate context --- */
    if (context == NULL)
    {
        set_neg_rsp(rsp, req->sid, NRC_CONDITIONS_NOT_CORRECT, rsp_buf);
        return true;
    }

    uds_session_context_t *sctx = UDS_CTX_SESSION(context);
    uint8_t session_type = req->subfunction.value;

    /* --- IMLOIF: no additional data bytes expected --- */
    if (req->data_len != 0u)
    {
        set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT, rsp_buf);
        return true;
    }

    /* --- Attempt session switch --- */
    uds_nrc_t nrc = NRC_GENERAL_REJECT;
    uds_status_t status = uds_session_switch(sctx, session_type, &nrc);

    if (status != UDS_OK)
    {
        set_neg_rsp(rsp, req->sid, (uint8_t)nrc, rsp_buf);
        return true;
    }

    /* --- SPRMIB check (suppress only positive response) --- */
    if (should_suppress(req))
    {
        return false;
    }

    /* --- Build sessionParameterRecord (P2Server_max + P2*Server_max) --- */
    const uds_std_return_t *params = uds_session_get_params(sctx);

    rsp_buf[0] = (uint8_t)((params->p2_server_max >> 8) & 0xFFu);
    rsp_buf[1] = (uint8_t)( params->p2_server_max       & 0xFFu);
    rsp_buf[2] = (uint8_t)((params->p2_star_server_max >> 8) & 0xFFu);
    rsp_buf[3] = (uint8_t)( params->p2_star_server_max       & 0xFFu);

    set_pos_rsp(rsp, DIAGNOSTIC_SESSION_CONTROL_RSP,
                session_type, rsp_buf, 4);
    return true;
}

/* ======================================================================== *
 * 0x11 — ECUReset                                                          *
 * ======================================================================== */

bool uds_svc_ecu_reset(const uds_request_t *req,
                       uds_response_t      *rsp,
                       void                *context)
{
    uint8_t rsp_buf[4];
    (void)context;

    uint8_t reset_type = req->subfunction.value;

    /* --- IMLOIF: no additional data expected --- */
    if (req->data_len != 0u)
    {
        set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT, rsp_buf);
        return true;
    }

    /* --- SFNS: check reset type is supported --- */
    if (!is_reset_type_supported(reset_type))
    {
        set_neg_rsp(rsp, req->sid, NRC_SUB_FUNCTION_NOT_SUPPORTED, rsp_buf);
        return true;
    }

    /* --- SPRMIB check --- */
    if (should_suppress(req))
    {
        return false;
    }

    /* --- Build positive response with powerDownTime = 0x00 --- */
    rsp_buf[0] = 0x00u;

    set_pos_rsp(rsp, ECU_RESET_RSP, reset_type, rsp_buf, 1);
    return true;
}

/* ======================================================================== *
 * 0x27 — SecurityAccess                                                    *
 * ======================================================================== */

bool uds_svc_security_access(const uds_request_t *req,
                             uds_response_t      *rsp,
                             void                *context)
{
    uint8_t rsp_buf[SECURITY_SEED_SIZE];
    uint8_t level = req->subfunction.value;

    /* --- SFNS: level must be 1..0x7F --- */
    if (level == 0u || level > 0x7Fu)
    {
        set_neg_rsp(rsp, req->sid, NRC_SUB_FUNCTION_NOT_SUPPORTED, rsp_buf);
        return true;
    }

    /* Dispatch by odd/even subfunction */
    if ((level & 0x01u) != 0u)
    {
        /* ---- requestSeed (odd subfunction) ---- */
        /* IMLOIF: no additional data bytes expected */
        if (req->data_len != 0u)
        {
            set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT, rsp_buf);
            return true;
        }

        /* Call security module */
        uint8_t seed_len = sizeof(rsp_buf);
        uds_nrc_t nrc    = NRC_GENERAL_REJECT;

        uds_status_t status = uds_security_request_seed(level, rsp_buf,
                                                         &seed_len, &nrc);

        if (status != UDS_OK || nrc != NRC_POSITIVE_RESPONSE)
        {
            /* Map failure NRC */
            uint8_t out_nrc = (status != UDS_OK)
                                ? NRC_GENERAL_REJECT
                                : (uint8_t)nrc;
            set_neg_rsp(rsp, req->sid, out_nrc, rsp_buf);
            return true;
        }

        /* --- SPRMIB check --- */
        if (should_suppress(req))
        {
            return false;
        }

        /* Build positive response with seed data */
        set_pos_rsp(rsp, SECURITY_ACCESS_RSP, level,
                    rsp_buf, seed_len);
        return true;
    }
    else
    {
        /* ---- sendKey (even subfunction) ---- */
        /* IMLOIF: at least 1 key byte required */
        if (req->data_len == 0u || req->data == NULL)
        {
            set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT, rsp_buf);
            return true;
        }

        /* Call security module */
        uds_nrc_t nrc = NRC_GENERAL_REJECT;

        uds_status_t status = uds_security_send_key(level, req->data,
                                                     req->data_len, &nrc);

        if (status != UDS_OK || nrc != NRC_POSITIVE_RESPONSE)
        {
            uint8_t out_nrc = (status != UDS_OK)
                                ? NRC_GENERAL_REJECT
                                : (uint8_t)nrc;
            set_neg_rsp(rsp, req->sid, out_nrc, rsp_buf);
            return true;
        }

        /* Update caller's unlock flag if context provided */
        if (context != NULL)
        {
            *UDS_CTX_UNLOCKED(context) = true;
        }

        /* --- SPRMIB check --- */
        if (should_suppress(req))
        {
            return false;
        }

        /* Build positive response (no additional data) */
        set_pos_rsp(rsp, SECURITY_ACCESS_RSP, level, NULL, 0);
        return true;
    }
}

/* ======================================================================== *
 * 0x28 — CommunicationControl                                              *
 * ======================================================================== */

bool uds_svc_communication_control(const uds_request_t *req,
                                   uds_response_t      *rsp,
                                   void                *context)
{
    uint8_t rsp_buf[4];
    (void)context;

    uint8_t control_type = req->subfunction.value;

    /* --- IMLOIF: at least 1 data byte (communicationType) required --- */
    if (req->data_len < 1u || req->data == NULL)
    {
        set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT, rsp_buf);
        return true;
    }

    uint8_t comm_type = req->data[0];

    /* --- SFNS: check controlType --- */
    if (!is_control_type_valid(control_type))
    {
        set_neg_rsp(rsp, req->sid, NRC_SUB_FUNCTION_NOT_SUPPORTED, rsp_buf);
        return true;
    }

    /* --- ROOR: check communicationType --- */
    if (!is_comm_type_valid(comm_type))
    {
        set_neg_rsp(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE, rsp_buf);
        return true;
    }

    /* --- SPRMIB check --- */
    if (should_suppress(req))
    {
        return false;
    }

    /* Build positive response */
    set_pos_rsp(rsp, COMMUNICATION_CONTROL_RSP, control_type, NULL, 0);
    return true;
}

/* ======================================================================== *
 * 0x3E — TesterPresent                                                     *
 * ======================================================================== */

bool uds_svc_tester_present(const uds_request_t *req,
                            uds_response_t      *rsp,
                            void                *context)
{
    uint8_t rsp_buf[4];
    (void)context;

    /* --- IMLOIF: no additional data expected --- */
    if (req->data_len != 0u)
    {
        set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT, rsp_buf);
        return true;
    }

    /* --- SPRMIB check (common case for tester present) --- */
    if (should_suppress(req))
    {
        return false;
    }

    /* Build positive response: echo subfunction value (w/o suppress bit) */
    set_pos_rsp(rsp, TESTER_PRESENT_RSP, req->subfunction.value, NULL, 0);
    return true;
}

/* ======================================================================== *
 * 0x85 — ControlDTCSetting                                                 *
 * ======================================================================== */

bool uds_svc_control_dtc_setting(const uds_request_t *req,
                                 uds_response_t      *rsp,
                                 void                *context)
{
    uint8_t rsp_buf[4];
    uint8_t dtc_setting = req->subfunction.value;

    /* --- Validate context (unlocked flag required) --- */
    if (context == NULL)
    {
        set_neg_rsp(rsp, req->sid, NRC_CONDITIONS_NOT_CORRECT, rsp_buf);
        return true;
    }

    bool unlocked = *UDS_CTX_UNLOCKED(context);

    /* --- SAD: security must be unlocked --- */
    if (!unlocked)
    {
        set_neg_rsp(rsp, req->sid, NRC_SECURITY_ACCESS_DENIED, rsp_buf);
        return true;
    }

    /* --- SFNS: check dtcSettingType --- */
    if (!is_dtc_setting_supported(dtc_setting))
    {
        set_neg_rsp(rsp, req->sid, NRC_SUB_FUNCTION_NOT_SUPPORTED, rsp_buf);
        return true;
    }

    /* --- SPRMIB check --- */
    if (should_suppress(req))
    {
        return false;
    }

    /* Build positive response */
    set_pos_rsp(rsp, CONTROL_DTC_SETTING_RSP, dtc_setting, NULL, 0);
    return true;
}

/* ======================================================================== *
 * 0x86 — ResponseOnEvent                                                   *
 * ======================================================================== */

bool uds_svc_response_on_event(const uds_request_t *req,
                               uds_response_t      *rsp,
                               void                *context)
{
    uint8_t rsp_buf[4];
    (void)context;

    uint8_t event_subfn = req->subfunction.value;

    /* --- SFNS: validate event subfunction --- */
    if (!is_event_subfn_valid(event_subfn))
    {
        set_neg_rsp(rsp, req->sid, NRC_SUB_FUNCTION_NOT_SUPPORTED, rsp_buf);
        return true;
    }

    /* --- IMLOIF: at least 1 data byte (eventType) required --- */
    if (req->data_len < 1u || req->data == NULL)
    {
        set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT, rsp_buf);
        return true;
    }

    uint8_t event_type = req->data[0];
    uint8_t event_data_len = (uint8_t)(req->data_len - 1u);
    const uint8_t *event_data = event_data_len > 0u ? req->data + 1 : NULL;

    /* --- Process subfunction --- */
    switch (event_subfn)
    {
    case 0x00u: /* stopResponseOnEvent */
        /* Clear all active events */
        uds_svc_event_store_clear();
        break;

    case 0x01u: /* startResponseOnEvent */
    case 0x03u: /* startResponseOnEventWithStorage */
    case 0x04u: /* startResponseOnEventWithStorageAndClearResponseOnEvent */
    {
        /* For 0x04, clear store then add */
        if (event_subfn == 0x04u)
        {
            uds_svc_event_store_clear();
        }

        /* Find empty slot and store event */
        uint8_t slot = 0u;
        bool slot_found = false;

        for (uint8_t i = 0u; i < UDS_EVENT_STORE_MAX; i++)
        {
            if (!g_event_store[i].active)
            {
                slot = i;
                slot_found = true;
                break;
            }
        }

        if (slot_found)
        {
            g_event_store[slot].event_type = event_type;
            g_event_store[slot].data_len   = (event_data_len > 8u) ? 8u : event_data_len;
            g_event_store[slot].active     = true;

            if (g_event_store[slot].data_len > 0u && event_data != NULL)
            {
                memcpy(g_event_store[slot].data, event_data,
                       g_event_store[slot].data_len);
            }
        }
        break;
    }

    case 0x02u: /* clearResponseOnEvent */
        uds_svc_event_store_clear();
        break;

    default:
        /* Should not reach here (validated above), but handle defensively */
        set_neg_rsp(rsp, req->sid, NRC_SUB_FUNCTION_NOT_SUPPORTED, rsp_buf);
        return true;
    }

    /* --- SPRMIB check --- */
    if (should_suppress(req))
    {
        return false;
    }

    /* Build positive response */
    set_pos_rsp(rsp, RESPONSE_ON_EVENT_RSP, event_subfn, NULL, 0);
    return true;
}

/* ======================================================================== *
 * 0x87 — LinkControl                                                       *
 * ======================================================================== */

bool uds_svc_link_control(const uds_request_t *req,
                          uds_response_t      *rsp,
                          void                *context)
{
    uint8_t rsp_buf[4];
    (void)context;

    uint8_t link_subfn = req->subfunction.value;

    /* --- IMLOIF: check data requirements per subfunction --- */
    if (link_subfn == 0x01u)
    {
        /* verifyBaudrateTransition: needs 3-byte baudrate identifier */
        if (req->data_len < 3u || req->data == NULL)
        {
            set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT, rsp_buf);
            return true;
        }
    }
    else if (link_subfn == 0x02u)
    {
        /* transitionBaudrate: no additional data required */
        if (req->data_len != 0u)
        {
            set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT, rsp_buf);
            return true;
        }
    }
    else
    {
        /* SFNS: unsupported subfunction */
        set_neg_rsp(rsp, req->sid, NRC_SUB_FUNCTION_NOT_SUPPORTED, rsp_buf);
        return true;
    }

    /* --- SPRMIB check --- */
    if (should_suppress(req))
    {
        return false;
    }

    /* Build positive response */
    set_pos_rsp(rsp, LINK_CONTROL_RSP, link_subfn, NULL, 0);
    return true;
}

/* ======================================================================== *
 * Event Store API                                                          *
 * ======================================================================== */

void uds_svc_event_store_init(void)
{
    memset(g_event_store, 0, sizeof(g_event_store));
}

void uds_svc_event_store_clear(void)
{
    uds_svc_event_store_init();
}

const uds_event_record_t* uds_svc_event_store_get(uint8_t *count)
{
    if (count != NULL)
    {
        *count = UDS_EVENT_STORE_MAX;
    }
    return g_event_store;
}
