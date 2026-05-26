/*
 * uds_svc_io.c
 * IO Control Service (0x2F) — Implementation
 *
 * Implements the handler for UDS service 0x2F (InputOutputControlByIdentifier)
 * per ISO 14229-1:2020 Section 12.3.
 *
 * The handler:
 *   - Parses DID (2 bytes) + controlMode (1 byte) + optional controlParameter
 *   - Validates DID existence via uds_did_find()
 *   - Checks security access for DIDs with secured flags
 *   - Maintains a static IO override table (DID + override value)
 *   - Supports four control modes: returnControlToStandard, resetToDefault,
 *     freezeCurrentState, shortTermAdjustment
 *
 * Positive response format (via uds_serialize_response):
 *   [0x6F][DID_high][DID_low][controlMode][controlParameter...]
 *
 * Negative response format (via same serialization, sid=0x7F):
 *   [0x7F][0x2F][NRC]
 *
 * Wave 3a Task 12 — IO Control Service (0x2F)
 */

#include "uds/uds_svc_io.h"
#include "uds/uds_data.h"
#include "uds/uds_svc_util.h"
#include <string.h>   /* memset, memcpy */

/* ======================================================================== *
 * IO Override Table                                                         *
 * ======================================================================== *
 * Stores per-DID override values for shortTermAdjustment.
 * Entries are created by shortTermAdjustment and cleared by
 * returnControlToStandard, resetToDefault, or uds_svc_io_reset().
 */

/** @brief A single IO override entry */
typedef struct {
    uint16_t did;                /**< @brief DID this override belongs to */
    uint8_t  value[IO_OVERRIDE_VALUE_MAX]; /**< @brief Override value data */
    uint8_t  value_len;          /**< @brief Number of valid bytes in value[] */
    bool     active;             /**< @brief Whether this entry is in use */
} uds_io_override_entry_t;

/** @brief Static array of IO override entries */
static uds_io_override_entry_t g_io_overrides[IO_OVERRIDE_MAX];

/* ======================================================================== *
 * Static Response Buffer                                                   *
 * ======================================================================== *
 * Provides stable memory that rsp->data can point to.
 * Safe because services are invoked sequentially (single-threaded).
 */

/** @brief Large buffer for response data (DID + controlMode + IO data) */
static uint8_t g_rsp_buf[256];

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

/**
 * @brief Reconstruct the first data byte (raw[1]) from the parsed subfunction.
 */
static inline uint8_t reconstruct_byte1(const uds_request_t *req)
{
    return (uint8_t)(req->subfunction.value | ((uint8_t)req->subfunction.suppress_rsp << 7));
}

/**
 * @brief Extract the 16-bit DID from a parsed 0x2F request.
 *
 * DID is in raw[1..2].  raw[1] was consumed by the parser as subfunction;
 * we reconstruct it via reconstruct_byte1().  raw[2] is req->data[0].
 */
static inline uint16_t extract_did(const uds_request_t *req)
{
    return ((uint16_t)reconstruct_byte1(req) << 8) | req->data[0];
}

/* ======================================================================== *
 * IO Override Table Helpers                                                *
 * ======================================================================== */

/**
 * @brief Find an IO override entry by DID.
 *
 * @param[in] did  DID to search for
 * @return Pointer to the entry, or NULL if not found
 */
static uds_io_override_entry_t* find_override(uint16_t did)
{
    for (uint8_t i = 0u; i < IO_OVERRIDE_MAX; i++)
    {
        if (g_io_overrides[i].active && g_io_overrides[i].did == did)
        {
            return &g_io_overrides[i];
        }
    }
    return NULL;
}

/**
 * @brief Find a free slot in the IO override table.
 *
 * @return Index, or IO_OVERRIDE_MAX if table is full
 */
static uint8_t find_free_override_slot(void)
{
    for (uint8_t i = 0u; i < IO_OVERRIDE_MAX; i++)
    {
        if (!g_io_overrides[i].active)
        {
            return i;
        }
    }
    return IO_OVERRIDE_MAX;
}

/**
 * @brief Remove an IO override entry by DID.
 *
 * Silently succeeds if no override exists for the DID.
 *
 * @param[in] did  DID to remove the override for
 */
static void remove_override(uint16_t did)
{
    uds_io_override_entry_t *entry = find_override(did);
    if (entry != NULL)
    {
        memset(entry, 0, sizeof(*entry));
    }
}

/**
 * @brief Check whether a DID entry requires security unlocking for IO control.
 *
 * @param[in] entry  DID entry to check
 * @return true if the DID is secured and requires unlocked state
 */
static bool did_requires_unlock(const uds_did_entry_t *entry)
{
    return (entry->access == DID_SECURED_READ ||
            entry->access == DID_SECURED_WRITE);
}

/* ======================================================================== *
 * 0x2F — InputOutputControlByIdentifier                                    *
 * ======================================================================== */

bool uds_svc_io_control(const uds_request_t *req,
                        uds_response_t      *rsp,
                        void                *context)
{
    /* --- Validate context (must provide unlocked flag) --- */
    if (context == NULL)
    {
        set_neg_rsp_local(rsp, req->sid, NRC_CONDITIONS_NOT_CORRECT);
        return true;
    }

    bool unlocked = *((const bool *)context);

    /* --- IMLOIF: need at least 2 data bytes (DID_low + controlMode) --- */
    /* After parser: data[0] = DID_low, data[1] = controlMode */
    if (req->data_len < 2u || req->data == NULL)
    {
        set_neg_rsp_local(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return true;
    }

    uint16_t did         = extract_did(req);
    uint8_t  control_mode = req->data[1];

    /* --- ROOR: DID must exist in registry --- */
    const uds_did_entry_t *entry = uds_did_find(did);
    if (entry == NULL)
    {
        set_neg_rsp_local(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return true;
    }

    /* --- ROOR: validate control mode --- */
    if (control_mode < IO_CONTROL_RETURN_TO_STANDARD ||
        control_mode > IO_CONTROL_SHORT_TERM_ADJUST)
    {
        set_neg_rsp_local(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return true;
    }

    /* --- SAD: check security access for secured DIDs --- */
    if (did_requires_unlock(entry) && !unlocked)
    {
        set_neg_rsp_local(rsp, req->sid, NRC_SECURITY_ACCESS_DENIED);
        return true;
    }

    /* ==================================================================== *
     * Process Control Mode                                                 *
     * ==================================================================== */

    switch (control_mode)
    {
    case IO_CONTROL_RETURN_TO_STANDARD:
    case IO_CONTROL_RESET_TO_DEFAULT:
    {
        /* --- IMLOIF: no additional data expected beyond DID + controlMode --- */
        if (req->data_len != 2u)
        {
            set_neg_rsp_local(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
            return true;
        }

        /* Remove any active override for this DID */
        remove_override(did);

        /* --- SPRMIB check --- */
        if (uds_should_suppress(req))
        {
            return false;
        }

        /* Build positive response: [0x6F][DID_h][DID_l][controlMode] */
        g_rsp_buf[0] = (uint8_t)(did >> 8);
        g_rsp_buf[1] = (uint8_t)(did & 0xFFu);
        g_rsp_buf[2] = control_mode;
        uds_set_pos_rsp(rsp, INPUT_OUTPUT_CONTROL_BY_ID,
                    g_rsp_buf, 3);
        return true;
    }

    case IO_CONTROL_FREEZE_CURRENT_STATE:
    {
        /* --- IMLOIF: no additional data expected beyond DID + controlMode --- */
        if (req->data_len != 2u)
        {
            set_neg_rsp_local(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
            return true;
        }

        /* Read current DID value into response buffer (offset 3: DID_h, DID_l, ctrlMode) */
        uint16_t did_data_len = sizeof(g_rsp_buf) - 3u;
        if (!uds_did_read(did, g_rsp_buf + 3u, &did_data_len, unlocked))
        {
            /* Should not happen if DID exists (checked above), but handle defensively */
            set_neg_rsp_local(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
            return true;
        }

        /* --- SPRMIB check --- */
        if (uds_should_suppress(req))
        {
            return false;
        }

        /* Build positive response: [0x6F][DID_h][DID_l][ctrlMode][frozenData...] */
        g_rsp_buf[0] = (uint8_t)(did >> 8);
        g_rsp_buf[1] = (uint8_t)(did & 0xFFu);
        g_rsp_buf[2] = control_mode;

        uds_set_pos_rsp(rsp, INPUT_OUTPUT_CONTROL_BY_ID,
                    g_rsp_buf, 3u + did_data_len);
        return true;
    }

    case IO_CONTROL_SHORT_TERM_ADJUST:
    {
        /* --- IMLOIF: need at least 1 byte of controlParameter --- */
        /* data[0]=DID_low, data[1]=controlMode, data[2+]=controlParameter */
        if (req->data_len < 3u)
        {
            set_neg_rsp_local(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
            return true;
        }

        const uint8_t *param_data   = req->data + 2u;
        uint16_t       param_len    = req->data_len - 2u;

        if (param_len > IO_OVERRIDE_VALUE_MAX)
        {
            param_len = IO_OVERRIDE_VALUE_MAX;
        }

        /* Store or update the override entry */
        uds_io_override_entry_t *override_entry = find_override(did);
        if (override_entry == NULL)
        {
            /* Create new entry */
            uint8_t slot = find_free_override_slot();
            if (slot >= IO_OVERRIDE_MAX)
            {
                set_neg_rsp_local(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
                return true;
            }
            override_entry = &g_io_overrides[slot];
            override_entry->did = did;
            override_entry->active = true;
        }

        memset(override_entry->value, 0, IO_OVERRIDE_VALUE_MAX);
        memcpy(override_entry->value, param_data, param_len);
        override_entry->value_len = (uint8_t)param_len;

        /* --- SPRMIB check --- */
        if (uds_should_suppress(req))
        {
            return false;
        }

        /* Build positive response: [0x6F][DID_h][DID_l][ctrlMode][ctrlParam...] */
        uint16_t resp_len = 3u + (uint16_t)param_len;
        if (resp_len > sizeof(g_rsp_buf))
        {
            /* Truncate to fit buffer if needed */
            resp_len = sizeof(g_rsp_buf);
            param_len = resp_len - 3u;
        }

        g_rsp_buf[0] = (uint8_t)(did >> 8);
        g_rsp_buf[1] = (uint8_t)(did & 0xFFu);
        g_rsp_buf[2] = control_mode;
        memcpy(g_rsp_buf + 3u, param_data, param_len);

        uds_set_pos_rsp(rsp, INPUT_OUTPUT_CONTROL_BY_ID,
                    g_rsp_buf, resp_len);
        return true;
    }

    default:
        /* Should not reach here (validated above), but handle defensively */
        set_neg_rsp_local(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return true;
    }
}

/* ======================================================================== *
 * IO State Management                                                      *
 * ======================================================================== */

void uds_svc_io_init(void)
{
    memset(g_io_overrides, 0, sizeof(g_io_overrides));
}

void uds_svc_io_reset(void)
{
    uds_svc_io_init();
}

bool uds_svc_io_has_override(uint16_t did)
{
    return (find_override(did) != NULL);
}
