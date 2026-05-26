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
#include "uds/uds_svc_util.h"
#include <string.h>   /* memset, memcpy */

/** @brief ECU reset type subfunction values (ISO 14229-1 Table 45) */
#define ECU_RESET_HARD        0x01u /**< hardReset — simulates power-on reset */
#define ECU_RESET_KEY_OFF_ON  0x02u /**< keyOffOnReset — ignition cycle reset  */
#define ECU_RESET_SOFT        0x03u /**< softReset — software-initiated reset  */

/** @brief Static array of event records */
static uds_event_record_t g_event_store[UDS_EVENT_STORE_MAX];

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
    return (reset_type == ECU_RESET_HARD ||
            reset_type == ECU_RESET_KEY_OFF_ON ||
            reset_type == ECU_RESET_SOFT);
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
    static uint8_t rsp_buf[4];

    /* --- Validate context --- */
    if (context == NULL)
    {
        uds_set_neg_rsp(rsp, req->sid, NRC_CONDITIONS_NOT_CORRECT, req->sid);
        return true;
    }

    uds_session_context_t *sctx = UDS_CTX_SESSION(context);
    uint8_t session_type = req->subfunction.value;

    /* --- IMLOIF: no additional data bytes expected --- */
    if (req->data_len != 0u)
    {
        uds_set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT, req->sid);
        return true;
    }

    /* --- Attempt session switch --- */
    uds_nrc_t nrc = NRC_GENERAL_REJECT;
    uds_status_t status = uds_session_switch(sctx, session_type, &nrc);

    if (status != UDS_OK)
    {
        uds_set_neg_rsp(rsp, req->sid, (uint8_t)nrc, req->sid);
        return true;
    }

    /* --- SPRMIB check (suppress only positive response) --- */
    if (uds_should_suppress(req))
    {
        return false;
    }

    /* --- Build sessionParameterRecord (P2Server_max + P2*Server_max) --- */
    const uds_std_return_t *params = uds_session_get_params(sctx);

    rsp_buf[0] = (uint8_t)((params->p2_server_max >> 8) & 0xFFu);
    rsp_buf[1] = (uint8_t)( params->p2_server_max       & 0xFFu);
    rsp_buf[2] = (uint8_t)((params->p2_star_server_max >> 8) & 0xFFu);
    rsp_buf[3] = (uint8_t)( params->p2_star_server_max       & 0xFFu);

    uds_set_pos_rsp(rsp, req->sid, rsp_buf, 4);
    rsp->subfunc_echo = session_type;
    return true;
}

/* ======================================================================== *
 * 0x11 — ECUReset                                                          *
 * ======================================================================== */

bool uds_svc_ecu_reset(const uds_request_t *req,
                       uds_response_t      *rsp,
                       void                *context)
{
    static uint8_t rsp_buf[4];
    (void)context;

    uint8_t reset_type = req->subfunction.value;

    /* --- IMLOIF: no additional data expected --- */
    if (req->data_len != 0u)
    {
        uds_set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT, req->sid);
        return true;
    }

    /* --- SFNS: check reset type is supported --- */
    if (!is_reset_type_supported(reset_type))
    {
        uds_set_neg_rsp(rsp, req->sid, NRC_SUB_FUNCTION_NOT_SUPPORTED, req->sid);
        return true;
    }

    /* --- SPRMIB check --- */
    if (uds_should_suppress(req))
    {
        return false;
    }

    /* --- Build positive response with powerDownTime = 0x00 --- */
    rsp_buf[0] = 0x00u;

    uds_set_pos_rsp(rsp, req->sid, rsp_buf, 1);
    rsp->subfunc_echo = reset_type;
    return true;
}

/* ======================================================================== *
 * 0x27 — SecurityAccess                                                    *
 * ======================================================================== */

bool uds_svc_security_access(const uds_request_t *req,
                             uds_response_t      *rsp,
                             void                *context)
{
    static uint8_t rsp_buf[SECURITY_SEED_SIZE];
    uint8_t level = req->subfunction.value;

    /* --- SFNS: level must be 1..0x7F --- */
    if (level == 0u || level > 0x7Fu)
    {
        uds_set_neg_rsp(rsp, req->sid, NRC_SUB_FUNCTION_NOT_SUPPORTED, req->sid);
        return true;
    }

    /* Dispatch by odd/even subfunction */
    if ((level & 0x01u) != 0u)
    {
        /* ---- requestSeed (odd subfunction) ---- */
        /* IMLOIF: no additional data bytes expected */
        if (req->data_len != 0u)
        {
            uds_set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT, req->sid);
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
            uds_set_neg_rsp(rsp, req->sid, out_nrc, req->sid);
            return true;
        }

        /* --- SPRMIB check --- */
        if (uds_should_suppress(req))
        {
            return false;
        }

        /* Build positive response with seed data */
    uds_set_pos_rsp(rsp, req->sid, rsp_buf, seed_len);
    rsp->subfunc_echo = level;
    return true;
    }
    else
    {
        /* ---- sendKey (even subfunction) ---- */
        /* IMLOIF: at least 1 key byte required */
        if (req->data_len == 0u || req->data == NULL)
        {
            uds_set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT, req->sid);
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
            uds_set_neg_rsp(rsp, req->sid, out_nrc, req->sid);
            return true;
        }

        /* Update caller's unlock flag if context provided */
        if (context != NULL)
        {
            *UDS_CTX_UNLOCKED(context) = true;
        }

        /* --- SPRMIB check --- */
        if (uds_should_suppress(req))
        {
            return false;
        }

        /* Build positive response (no additional data) */
        uds_set_pos_rsp(rsp, req->sid, NULL, 0);
        rsp->subfunc_echo = level;
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
    (void)context;

    uint8_t control_type = req->subfunction.value;

    /* --- IMLOIF: at least 1 data byte (communicationType) required --- */
    if (req->data_len < 1u || req->data == NULL)
    {
        uds_set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT, req->sid);
        return true;
    }

    uint8_t comm_type = req->data[0];

    /* --- SFNS: check controlType --- */
    if (!is_control_type_valid(control_type))
    {
        uds_set_neg_rsp(rsp, req->sid, NRC_SUB_FUNCTION_NOT_SUPPORTED, req->sid);
        return true;
    }

    /* --- ROOR: check communicationType --- */
    if (!is_comm_type_valid(comm_type))
    {
        uds_set_neg_rsp(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE, req->sid);
        return true;
    }

    /* --- SPRMIB check --- */
    if (uds_should_suppress(req))
    {
        return false;
    }

    /* Build positive response */
    uds_set_pos_rsp(rsp, req->sid, NULL, 0);
    rsp->subfunc_echo = control_type;
    return true;
}

/* ======================================================================== *
 * 0x3E — TesterPresent                                                     *
 * ======================================================================== */

bool uds_svc_tester_present(const uds_request_t *req,
                            uds_response_t      *rsp,
                            void                *context)
{
    (void)context;

    /* --- IMLOIF: no additional data expected --- */
    if (req->data_len != 0u)
    {
        uds_set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT, req->sid);
        return true;
    }

    /* --- SPRMIB check (common case for tester present) --- */
    if (uds_should_suppress(req))
    {
        return false;
    }

    /* Build positive response: echo subfunction value (w/o suppress bit) */
    uds_set_pos_rsp(rsp, req->sid, NULL, 0);
    rsp->subfunc_echo = req->subfunction.value;
    return true;
}

/* ======================================================================== *
 * 0x85 — ControlDTCSetting                                                 *
 * ======================================================================== */

bool uds_svc_control_dtc_setting(const uds_request_t *req,
                                 uds_response_t      *rsp,
                                 void                *context)
{
    uint8_t dtc_setting = req->subfunction.value;

    /* --- Validate context (unlocked flag required) --- */
    if (context == NULL)
    {
        uds_set_neg_rsp(rsp, req->sid, NRC_CONDITIONS_NOT_CORRECT, req->sid);
        return true;
    }

    bool unlocked = *UDS_CTX_UNLOCKED(context);

    /* --- SAD: security must be unlocked --- */
    if (!unlocked)
    {
        uds_set_neg_rsp(rsp, req->sid, NRC_SECURITY_ACCESS_DENIED, req->sid);
        return true;
    }

    /* --- SFNS: check dtcSettingType --- */
    if (!is_dtc_setting_supported(dtc_setting))
    {
        uds_set_neg_rsp(rsp, req->sid, NRC_SUB_FUNCTION_NOT_SUPPORTED, req->sid);
        return true;
    }

    /* --- SPRMIB check --- */
    if (uds_should_suppress(req))
    {
        return false;
    }

    /* Build positive response */
    uds_set_pos_rsp(rsp, req->sid, NULL, 0);
    rsp->subfunc_echo = dtc_setting;
    return true;
}

/* ======================================================================== *
 * 0x86 — ResponseOnEvent                                                   *
 * ======================================================================== */

bool uds_svc_response_on_event(const uds_request_t *req,
                               uds_response_t      *rsp,
                               void                *context)
{
    (void)context;

    uint8_t event_subfn = req->subfunction.value;

    /* --- SFNS: validate event subfunction --- */
    if (!is_event_subfn_valid(event_subfn))
    {
        uds_set_neg_rsp(rsp, req->sid, NRC_SUB_FUNCTION_NOT_SUPPORTED, req->sid);
        return true;
    }

    /* --- IMLOIF: at least 1 data byte (eventType) required --- */
    if (req->data_len < 1u || req->data == NULL)
    {
        uds_set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT, req->sid);
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
        uds_set_neg_rsp(rsp, req->sid, NRC_SUB_FUNCTION_NOT_SUPPORTED, req->sid);
        return true;
    }

    /* --- SPRMIB check --- */
    if (uds_should_suppress(req))
    {
        return false;
    }

    /* Build positive response */
    uds_set_pos_rsp(rsp, req->sid, NULL, 0);
    rsp->subfunc_echo = event_subfn;
    return true;
}

/* ======================================================================== *
 * 0x87 — LinkControl                                                       *
 * ======================================================================== */

bool uds_svc_link_control(const uds_request_t *req,
                          uds_response_t      *rsp,
                          void                *context)
{
    (void)context;

    uint8_t link_subfn = req->subfunction.value;

    /* --- IMLOIF: check data requirements per subfunction --- */
    if (link_subfn == 0x01u)
    {
        /* verifyBaudrateTransition: needs 3-byte baudrate identifier */
        if (req->data_len < 3u || req->data == NULL)
        {
            uds_set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT, req->sid);
            return true;
        }
    }
    else if (link_subfn == 0x02u)
    {
        /* transitionBaudrate: no additional data required */
        if (req->data_len != 0u)
        {
            uds_set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT, req->sid);
            return true;
        }
    }
    else
    {
        /* SFNS: unsupported subfunction */
        uds_set_neg_rsp(rsp, req->sid, NRC_SUB_FUNCTION_NOT_SUPPORTED, req->sid);
        return true;
    }

    /* --- SPRMIB check --- */
    if (uds_should_suppress(req))
    {
        return false;
    }

    /* Build positive response */
    uds_set_pos_rsp(rsp, req->sid, NULL, 0);
    rsp->subfunc_echo = link_subfn;
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
