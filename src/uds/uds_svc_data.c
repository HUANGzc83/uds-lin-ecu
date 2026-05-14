/*
 * uds_svc_data.c
 * Data Transmission Services — Implementation
 *
 * Implements handler functions for UDS data transmission services:
 * 0x22, 0x23, 0x24, 0x2A, 0x2C, 0x2E, and 0x3D per ISO 14229-1:2020.
 *
 * Each handler validates the request, performs the service, and populates
 * a uds_response_t with either a positive response or a negative response.
 * Return true to send, false to suppress (SPRMIB).
 *
 * Wave 3a Task 10 — Data Transmission Services
 */

#include "uds/uds_svc_data.h"
#include "uds/uds_data.h"
#include <string.h>   /* memset, memcpy */

/* ======================================================================== *
 * Static Response / Data Buffers                                           *
 * ======================================================================== *
 * These buffers provide stable memory that rsp->data can point to.
 * They are safe because services are invoked sequentially (single-threaded).
 */

/** @brief Large buffer for response data (DID data, scaling info, etc.) */
static uint8_t g_rsp_buf[256];

/** @brief Simulated RAM buffer for Read/WriteMemoryByAddress */
static uint8_t g_memory_buf[MEMORY_BUF_SIZE];

/* ======================================================================== *
 * Memory Region Table (for 0x23 / 0x3D)                                   *
 * ======================================================================== */

/** @brief Static table of virtual memory regions */
static const uds_mem_region_t g_memory_regions[] = {
    {0x00000000, 0x000003FF, g_memory_buf},  /* 1 KB simulated RAM */
};
#define MEMORY_REGION_COUNT \
    ((uint16_t)(sizeof(g_memory_regions) / sizeof(g_memory_regions[0])))

/* ======================================================================== *
 * Dynamic DID Storage (for 0x2C)                                          *
 * ======================================================================== */

/** @brief Static array of dynamic DID entries */
static uds_dynamic_did_entry_t g_dynamic_dids[DYNAMIC_DID_MAX];

/* ======================================================================== *
 * Periodic DID Schedule (for 0x2A)                                        *
 * ======================================================================== */

/** @brief Static array of periodic DID schedule entries */
static uds_periodic_did_entry_t g_periodic_dids[PERIODIC_DID_MAX];

/* ======================================================================== *
 * Internal Helpers                                                         *
 * ======================================================================== */

/**
 * @brief Populate a uds_response_t with a negative response.
 *
 * Uses 0x7F as the "SID" so that uds_serialize_response produces:
 *   [0x7F][request_SID][NRC]  — the standard UDS negative response format.
 */
static void set_neg_rsp(uds_response_t *rsp, uint8_t req_sid, uint8_t nrc)
{
    g_rsp_buf[0] = nrc;
    rsp->sid          = 0x7F;
    rsp->subfunc_echo = req_sid;
    rsp->data         = g_rsp_buf;
    rsp->data_len     = 1;
}

/**
 * @brief Build a positive response.
 *
 * For services without a subfunction, subfunc_echo should be 0.
 *
 * @param[out] rsp       Response structure to fill
 * @param[in]  rsp_sid   Positive response SID
 * @param[in]  subfunc   Subfunction echo (0 for services without subfunction)
 * @param[in]  data      Payload data pointer (may be NULL if data_len == 0)
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
 * For services without a subfunction, the parser may interpret the first
 * data byte's bit 7 as suppress_rsp.  This helper handles that case.
 */
static inline bool should_suppress(const uds_request_t *req)
{
    return req->subfunction.suppress_rsp;
}

/* ======================================================================== *
 * Memory Region Helpers (for 0x23 / 0x3D)                                 *
 * ======================================================================== */

/**
 * @brief Parse addressAndLengthFormatIdentifier and extract address + size.
 *
 * IMPORTANT: The parser consumes byte[1] as subfunction.  For services
 * without a subfunction (0x23, 0x3D), the format byte is reconstructed
 * via UDS_REQ_BYTE1(req).  The address and size bytes start at req->data[0].
 *
 * Format byte: (addr_nibble << 4) | mem_size_nibble
 * where nibble = number_of_bytes - 1.
 *
 * @param[in]  req        Parsed request
 * @param[out] address    Parsed address (big-endian from req->data[0..])
 * @param[out] mem_size   Parsed memory size (big-endian)
 * @param[out] addr_bytes Out: number of address bytes parsed
 * @param[out] size_bytes Out: number of size bytes parsed
 * @return true if parsing succeeded, false on error (IMLOIF)
 */
static bool parse_mem_addr_format(const uds_request_t *req,
                                  uint32_t *address,
                                  uint32_t *mem_size,
                                  uint8_t  *addr_bytes,
                                  uint8_t  *size_bytes)
{
    if (req->data_len < 2 || req->data == NULL)
    {
        return false;
    }

    /* Reconstruct format byte from subfunction (which consumed byte[1]) */
    uint8_t fmt        = UDS_REQ_BYTE1(req);
    uint8_t addr_nib   = (fmt >> 4) & 0x0Fu;
    uint8_t size_nib   = fmt & 0x0Fu;

    uint8_t ab = addr_nib + 1;
    uint8_t sb = size_nib + 1;

    /* Validate nibbles: 0x0..0x3 means 1..4 bytes (must have at least 1) */
    if (ab > 4 || sb > 4 || addr_nib > 3 || size_nib > 3)
    {
        return false;
    }

    uint16_t total_needed = (uint16_t)ab + (uint16_t)sb;
    if (req->data_len < total_needed)
    {
        return false;
    }

    /* Parse address from req->data[0..ab-1] (big-endian) */
    *address = 0;
    for (uint8_t i = 0; i < ab; i++)
    {
        *address = (*address << 8) | req->data[i];
    }

    /* Parse memory size from req->data[ab..ab+sb-1] (big-endian) */
    *mem_size = 0;
    for (uint8_t i = 0; i < sb; i++)
    {
        *mem_size = (*mem_size << 8) | req->data[ab + i];
    }

    if (*mem_size == 0)
    {
        return false;   /* zero-length access is invalid */
    }

    if (addr_bytes != NULL) { *addr_bytes = ab; }
    if (size_bytes != NULL) { *size_bytes = sb; }
    return true;
}

/**
 * @brief Find the memory region that contains a given address range.
 *
 * @param[in]  address   Start address
 * @param[in]  mem_size  Number of bytes to access
 * @param[out] out_offset  Offset within the region's data buffer
 * @return Pointer to the region, or NULL if no region covers the range
 */
static const uds_mem_region_t* find_memory_region(uint32_t address,
                                                   uint32_t mem_size,
                                                   uint32_t *out_offset)
{
    if (mem_size == 0)
    {
        return NULL;
    }

    /* Guard against overflow: check that address+mem_size-1 doesn't wrap */
    if (address + (mem_size - 1) < address)
    {
        return NULL;   /* overflow — address range wraps past 2^32 */
    }

    for (uint16_t i = 0; i < MEMORY_REGION_COUNT; i++)
    {
        const uds_mem_region_t *reg = &g_memory_regions[i];

        if (address >= reg->start_addr &&
            (address + mem_size - 1) <= reg->end_addr)
        {
            *out_offset = address - reg->start_addr;
            return reg;
        }
    }
    return NULL;
}

/* ======================================================================== *
 * Dynamic DID Helpers (for 0x2C)                                          *
 * ======================================================================== */

/**
 * @brief Find the next free slot in the dynamic DID array.
 *
 * @return Index, or DYNAMIC_DID_MAX if full
 */
static uint8_t find_free_dynamic_slot(void)
{
    for (uint8_t i = 0; i < DYNAMIC_DID_MAX; i++)
    {
        if (!g_dynamic_dids[i].active)
        {
            return i;
        }
    }
    return DYNAMIC_DID_MAX;
}

/**
 * @brief Find a dynamic DID entry by its DID value.
 *
 * @param[in]  did  The dynamic DID to find
 * @return Pointer to entry, or NULL if not found
 */
static uds_dynamic_did_entry_t* find_dynamic_did(uint16_t did)
{
    for (uint8_t i = 0; i < DYNAMIC_DID_MAX; i++)
    {
        if (g_dynamic_dids[i].active && g_dynamic_dids[i].did == did)
        {
            return &g_dynamic_dids[i];
        }
    }
    return NULL;
}

/* ======================================================================== *
 * Periodic DID Helpers (for 0x2A)                                         *
 * ======================================================================== */

/**
 * @brief Find the next free slot in the periodic DID array.
 *
 * @return Index, or PERIODIC_DID_MAX if full
 */
static uint8_t find_free_periodic_slot(void)
{
    for (uint8_t i = 0; i < PERIODIC_DID_MAX; i++)
    {
        if (!g_periodic_dids[i].active)
        {
            return i;
        }
    }
    return PERIODIC_DID_MAX;
}

/* ======================================================================== *
 * 0x22 — ReadDataByIdentifier                                             *
 * ======================================================================== */

bool uds_svc_read_data_by_id(const uds_request_t *req,
                              uds_response_t      *rsp,
                              void                *context)
{
    /* --- Validate context (must provide unlocked flag) --- */
    if (context == NULL)
    {
        set_neg_rsp(rsp, req->sid, NRC_CONDITIONS_NOT_CORRECT);
        return true;
    }

    bool unlocked = *((const bool *)context);

    /* --- IMLOIF: need at least 1 byte data (DID high lost in subfunction) --- */
    /* After parser: data points to raw+2 (DID low byte). We reconstruct DID
     * from byte[1] (via UDS_REQ_BYTE1) and data[0]. So data_len must be >=1 */
    if (req->data_len < 1 || req->data == NULL)
    {
        set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return true;
    }

    uint16_t did = UDS_REQ_DID(req);

    /* --- ROOR/SAD: check DID exists and read access --- */
    /* First check if DID exists at all */
    const uds_did_entry_t *entry = uds_did_find(did);
    if (entry == NULL)
    {
        set_neg_rsp(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return true;
    }

    uint16_t data_len = sizeof(g_rsp_buf);
    if (!uds_did_read(did, g_rsp_buf, &data_len, unlocked))
    {
        /* Determine whether ROOR or SAD based on access type */
        if (entry->access == DID_SECURED_READ && !unlocked)
        {
            set_neg_rsp(rsp, req->sid, NRC_SECURITY_ACCESS_DENIED);
        }
        else if (entry->access == DID_WRITE_ONLY ||
                 entry->access == DID_SECURED_WRITE)
        {
            /* These access types are not readable → ROOR */
            set_neg_rsp(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
        }
        else
        {
            set_neg_rsp(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
        }
        return true;
    }

    /* Build positive response: [0x62][DID_high][DID_low][data...] */
    /* We need to prepend the DID to the data in the response buffer.
     * Shift data right by 2 to make room for DID prefix. */
    if (data_len + 2 > sizeof(g_rsp_buf))
    {
        set_neg_rsp(rsp, req->sid, NRC_RESPONSE_TOO_LONG);
        return true;
    }

    /* Shift data to make room for DID at the front */
    memmove(g_rsp_buf + 2, g_rsp_buf, data_len);
    g_rsp_buf[0] = (uint8_t)(did >> 8);
    g_rsp_buf[1] = (uint8_t)(did & 0xFF);

    set_pos_rsp(rsp, READ_DATA_BY_IDENTIFIER_RSP, 0,
                g_rsp_buf, data_len + 2);
    return true;
}

/* ======================================================================== *
 * 0x23 — ReadMemoryByAddress                                              *
 * ======================================================================== */

bool uds_svc_read_memory_by_address(const uds_request_t *req,
                                     uds_response_t      *rsp,
                                     void                *context)
{
    (void)context;

    /* --- IMLOIF: parse format byte (in subfunction due to parser) + address + size --- */
    uint32_t address  = 0;
    uint32_t mem_size = 0;
    uint8_t  addr_bytes = 0;
    uint8_t  size_bytes = 0;

    if (!parse_mem_addr_format(req, &address, &mem_size,
                                &addr_bytes, &size_bytes))
    {
        set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return true;
    }

    /* --- ROOR: validate memory region --- */
    uint32_t region_offset = 0;
    const uds_mem_region_t *region = find_memory_region(address, mem_size,
                                                         &region_offset);
    if (region == NULL)
    {
        set_neg_rsp(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return true;
    }

    /* --- Read data from memory into response buffer --- */
    uint16_t data_offset = 1 + (uint16_t)addr_bytes + (uint16_t)size_bytes; /* fmt + addr + size */
    uint16_t total_resp  = data_offset + (uint16_t)mem_size;

    if (total_resp > sizeof(g_rsp_buf))
    {
        set_neg_rsp(rsp, req->sid, NRC_RESPONSE_TOO_LONG);
        return true;
    }

    /* Build response header: [fmt][addr_bytes...][size_bytes...] */
    g_rsp_buf[0] = UDS_REQ_BYTE1(req);  /* format byte */
    memcpy(g_rsp_buf + 1, req->data, addr_bytes);          /* address bytes */
    memcpy(g_rsp_buf + 1 + addr_bytes, req->data + addr_bytes, size_bytes);  /* size bytes */

    /* Append memory data */
    memcpy(g_rsp_buf + data_offset, region->data + region_offset, mem_size);

    set_pos_rsp(rsp, READ_MEMORY_BY_ADDRESS_RSP, 0,
                g_rsp_buf, total_resp);
    return true;
}

/* ======================================================================== *
 * 0x24 — ReadScalingDataByIdentifier                                      *
 * ======================================================================== */

bool uds_svc_read_scaling_data_by_id(const uds_request_t *req,
                                      uds_response_t      *rsp,
                                      void                *context)
{
    /* --- Validate context --- */
    if (context == NULL)
    {
        set_neg_rsp(rsp, req->sid, NRC_CONDITIONS_NOT_CORRECT);
        return true;
    }

    bool unlocked = *((const bool *)context);

    /* --- IMLOIF: need at least 1 byte data --- */
    if (req->data_len < 1 || req->data == NULL)
    {
        set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return true;
    }

    uint16_t did = UDS_REQ_DID(req);

    /* --- ROOR: check DID exists --- */
    const uds_did_entry_t *entry = uds_did_find(did);
    if (entry == NULL)
    {
        set_neg_rsp(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return true;
    }

    /* Read DID data */
    uint16_t data_len = sizeof(g_rsp_buf);
    if (!uds_did_read(did, g_rsp_buf + 2, &data_len, unlocked))
    {
        set_neg_rsp(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return true;
    }

    /* Prepend DID in response buffer */
    g_rsp_buf[0] = (uint8_t)(did >> 8);
    g_rsp_buf[1] = (uint8_t)(did & 0xFF);

    /* Append simplified scaling info: 2-byte scale (0x0100 = 1.0),
     * 2-byte offset (0x0000), 1-byte unit (0x01) */
    uint16_t did_data_len = data_len;
    uint16_t scaling_len = did_data_len + 5;

    if (2 + scaling_len > sizeof(g_rsp_buf))
    {
        set_neg_rsp(rsp, req->sid, NRC_RESPONSE_TOO_LONG);
        return true;
    }

    g_rsp_buf[2 + did_data_len + 0] = 0x01; /* scale factor high */
    g_rsp_buf[2 + did_data_len + 1] = 0x00; /* scale factor low */
    g_rsp_buf[2 + did_data_len + 2] = 0x00; /* offset high */
    g_rsp_buf[2 + did_data_len + 3] = 0x00; /* offset low */
    g_rsp_buf[2 + did_data_len + 4] = 0x01; /* unit */

    set_pos_rsp(rsp, READ_SCALING_DATA_BY_ID_RSP, 0,
                g_rsp_buf, 2 + scaling_len);
    return true;
}

/* ======================================================================== *
 * 0x2A — ReadDataByPeriodicIdentifier                                     *
 * ======================================================================== */

bool uds_svc_read_data_by_periodic_id(const uds_request_t *req,
                                       uds_response_t      *rsp,
                                       void                *context)
{
    (void)context;

    /* --- IMLOIF: need transmission mode (subfunction) + at least
     *    periodicDID(1) + source DID(2) = 3 data bytes --- */
    if (req->data_len < 3 || req->data == NULL)
    {
        set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return true;
    }

    uint8_t transmission_mode = req->subfunction.value;

    /* --- ROOR: validate transmission mode --- */
    if (transmission_mode < 0x01 || transmission_mode > 0x03)
    {
        set_neg_rsp(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return true;
    }

    uint8_t  periodic_did  = req->data[0];
    uint16_t source_did    = ((uint16_t)req->data[1] << 8) | req->data[2];

    /* --- ROOR: check source DID exists --- */
    if (uds_did_find(source_did) == NULL)
    {
        set_neg_rsp(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return true;
    }

    /* --- Store schedule entry --- */
    uint8_t slot = find_free_periodic_slot();
    if (slot >= PERIODIC_DID_MAX)
    {
        set_neg_rsp(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return true;
    }

    g_periodic_dids[slot].transmission_mode = transmission_mode;
    g_periodic_dids[slot].periodic_did      = periodic_did;
    g_periodic_dids[slot].source_did        = source_did;
    g_periodic_dids[slot].active            = true;

    /* --- SPRMIB: 0x2A has a real subfunction, so suppress check applies --- */
    if (should_suppress(req))
    {
        return false;
    }

    /* Build positive response with transmission mode echo */
    set_pos_rsp(rsp, READ_DATA_BY_PERIODIC_ID_RSP,
                transmission_mode, NULL, 0);
    return true;
}

/* ======================================================================== *
 * 0x2C — DynamicallyDefineDataIdentifier                                  *
 * ======================================================================== */

bool uds_svc_dynamically_define_data_id(const uds_request_t *req,
                                        uds_response_t      *rsp,
                                        void                *context)
{
    (void)context;

    /* --- IMLOIF: need at least 1 data byte (definitionMode) --- */
    /* After parser: data points to raw+2. definitionMode is in
     * req->subfunction.value (byte[1] of raw), and data[0] onward is payload. */
    if (req->data_len < 1 || req->data == NULL)
    {
        set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return true;
    }

    /* Reconstruct definitionMode from byte[1] (consumed as subfunction) */
    uint8_t definition_mode = UDS_REQ_BYTE1(req);

    switch (definition_mode)
    {
    case DYNAMIC_DID_DEFINE_BY_ID:
    {
        /* Format: [0x2C][0x01][targetDID_h][targetDID_l]
         *                [sourceDID_h][sourceDID_l][offset_h][offset_l][len_h][len_l]
         *                ... repeated for more sources
         * After parser: data = raw+2 = [targetDID_h, targetDID_l, source_defs...]
         * So data[0] = target DID high, data[1] = target DID low
         */
        if (req->data_len < 2)
        {
            set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
            return true;
        }

        uint16_t target_did = ((uint16_t)req->data[0] << 8) | req->data[1];
        uint16_t remaining = req->data_len - 2;

        /* Check for source definitions: each is 6 bytes (DID_h,DID_l,off_h,off_l,len_h,len_l) */
        if (remaining < 6 || (remaining % 6) != 0)
        {
            set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
            return true;
        }

        uint8_t num_sources = (uint8_t)(remaining / 6);
        if (num_sources == 0 || num_sources > DYNAMIC_DID_SOURCE_MAX)
        {
            set_neg_rsp(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
            return true;
        }

        /* Check target DID doesn't already exist as dynamic DID */
        if (find_dynamic_did(target_did) != NULL)
        {
            set_neg_rsp(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
            return true;
        }

        /* Find free slot */
        uint8_t slot = find_free_dynamic_slot();
        if (slot >= DYNAMIC_DID_MAX)
        {
            set_neg_rsp(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
            return true;
        }

        /* Fill dynamic DID entry */
        g_dynamic_dids[slot].did          = target_did;
        g_dynamic_dids[slot].source_count = num_sources;
        g_dynamic_dids[slot].active       = true;

        const uint8_t *src = req->data + 2;
        for (uint8_t i = 0; i < num_sources; i++)
        {
            g_dynamic_dids[slot].sources[i].source_type = 0; /* byId */
            g_dynamic_dids[slot].sources[i].did = ((uint16_t)src[0] << 8) | src[1];
            /* Note: offset and len are stored but simplified usage */
            g_dynamic_dids[slot].sources[i].addr = 0;
            g_dynamic_dids[slot].sources[i].len  = ((uint16_t)src[4] << 8) | src[5];
            src += 6;
        }

        /* Positive response: [0x6C][targetDID_h][targetDID_l] */
        g_rsp_buf[0] = (uint8_t)(target_did >> 8);
        g_rsp_buf[1] = (uint8_t)(target_did & 0xFF);
        set_pos_rsp(rsp, DYNAMICALLY_DEFINE_DATA_ID_RSP, 0,
                    g_rsp_buf, 2);
        return true;
    }

    case DYNAMIC_DID_DEFINE_BY_MEM:
    {
        /* Format: [0x2C][0x02][targetDID_h][targetDID_l]
         *                [addr_h][addr_mh][addr_ml][addr_l]
         *                [len_h][len_l]
         * After parser: data = raw+2 = [target DID hi, DID lo, address(4), size(2)]
         */
        if (req->data_len < 8)
        {
            set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
            return true;
        }

        uint16_t target_did = ((uint16_t)req->data[0] << 8) | req->data[1];

        if (find_dynamic_did(target_did) != NULL)
        {
            set_neg_rsp(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
            return true;
        }

        uint8_t slot = find_free_dynamic_slot();
        if (slot >= DYNAMIC_DID_MAX)
        {
            set_neg_rsp(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
            return true;
        }

        uint32_t mem_addr = ((uint32_t)req->data[2] << 24) |
                            ((uint32_t)req->data[3] << 16) |
                            ((uint32_t)req->data[4] << 8)  |
                            ((uint32_t)req->data[5]);
        uint16_t mem_len = ((uint16_t)req->data[6] << 8) | req->data[7];

        g_dynamic_dids[slot].did          = target_did;
        g_dynamic_dids[slot].source_count = 1;
        g_dynamic_dids[slot].active       = true;
        g_dynamic_dids[slot].sources[0].source_type = 1; /* byMem */
        g_dynamic_dids[slot].sources[0].addr = mem_addr;
        g_dynamic_dids[slot].sources[0].len  = mem_len;
        g_dynamic_dids[slot].sources[0].did  = 0;

        g_rsp_buf[0] = (uint8_t)(target_did >> 8);
        g_rsp_buf[1] = (uint8_t)(target_did & 0xFF);
        set_pos_rsp(rsp, DYNAMICALLY_DEFINE_DATA_ID_RSP, 0,
                    g_rsp_buf, 2);
        return true;
    }

    case DYNAMIC_DID_DELETE:
    {
        /* Format: [0x2C][0x03][targetDID_h][targetDID_l] */
        if (req->data_len < 2)
        {
            set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
            return true;
        }

        uint16_t target_did = ((uint16_t)req->data[0] << 8) | req->data[1];

        uds_dynamic_did_entry_t *entry = find_dynamic_did(target_did);
        if (entry == NULL)
        {
            set_neg_rsp(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
            return true;
        }

        entry->active = false;
        memset(entry, 0, sizeof(*entry));

        g_rsp_buf[0] = (uint8_t)(target_did >> 8);
        g_rsp_buf[1] = (uint8_t)(target_did & 0xFF);
        set_pos_rsp(rsp, DYNAMICALLY_DEFINE_DATA_ID_RSP, 0,
                    g_rsp_buf, 2);
        return true;
    }

    default:
        /* ROOR: unsupported definition mode */
        set_neg_rsp(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return true;
    }
}

/* ======================================================================== *
 * 0x2E — WriteDataByIdentifier                                            *
 * ======================================================================== */

bool uds_svc_write_data_by_id(const uds_request_t *req,
                               uds_response_t      *rsp,
                               void                *context)
{
    /* --- Validate context (must provide unlocked flag) --- */
    if (context == NULL)
    {
        set_neg_rsp(rsp, req->sid, NRC_CONDITIONS_NOT_CORRECT);
        return true;
    }

    bool unlocked = *((const bool *)context);

    /* --- IMLOIF: need at least 1 byte data (DID high in byte[1], DID low in data[0]) --- */
    if (req->data_len < 1 || req->data == NULL)
    {
        set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return true;
    }

    uint16_t did = UDS_REQ_DID(req);

    /* --- ROOR: check DID exists --- */
    const uds_did_entry_t *entry = uds_did_find(did);
    if (entry == NULL)
    {
        set_neg_rsp(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return true;
    }

    /* Write data starts at req->data[1] (data[0] is DID low byte).
     * data_len = req->data_len - 1 (because data[0] consumed as DID low). */
    const uint8_t *write_data = req->data + 1;
    uint16_t write_len = req->data_len - 1;

    if (write_len == 0)
    {
        set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return true;
    }

    /* Attempt write */
    if (!uds_did_write(did, write_data, write_len, unlocked))
    {
        /* Determine appropriate NRC */
        if (entry->access == DID_READ_ONLY ||
            entry->access == DID_SECURED_READ)
        {
            /* Not writable at all */
            set_neg_rsp(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
        }
        else if (entry->access == DID_SECURED_WRITE && !unlocked)
        {
            set_neg_rsp(rsp, req->sid, NRC_SECURITY_ACCESS_DENIED);
        }
        else
        {
            set_neg_rsp(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
        }
        return true;
    }

    /* Build positive response: [0x6E][DID_high][DID_low] */
    g_rsp_buf[0] = (uint8_t)(did >> 8);
    g_rsp_buf[1] = (uint8_t)(did & 0xFF);
    set_pos_rsp(rsp, WRITE_DATA_BY_IDENTIFIER_RSP, 0,
                g_rsp_buf, 2);
    return true;
}

/* ======================================================================== *
 * 0x3D — WriteMemoryByAddress                                             *
 * ======================================================================== */

bool uds_svc_write_memory_by_address(const uds_request_t *req,
                                      uds_response_t      *rsp,
                                      void                *context)
{
    /* --- Validate context (must provide unlocked flag) --- */
    if (context == NULL)
    {
        set_neg_rsp(rsp, req->sid, NRC_CONDITIONS_NOT_CORRECT);
        return true;
    }

    bool unlocked = *((const bool *)context);

    /* --- SAD: security must be unlocked --- */
    if (!unlocked)
    {
        set_neg_rsp(rsp, req->sid, NRC_SECURITY_ACCESS_DENIED);
        return true;
    }

    /* --- IMLOIF: parse format + address + size --- */
    uint32_t address  = 0;
    uint32_t mem_size = 0;
    uint8_t  addr_bytes = 0;
    uint8_t  size_bytes = 0;

    if (!parse_mem_addr_format(req, &address, &mem_size,
                                &addr_bytes, &size_bytes))
    {
        set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return true;
    }

    /* The format byte was consumed as subfunction (byte[1] of raw).
     * So req->data contains only addr_bytes + size_bytes + write_data.
     * header_len = 1 (fmt) + addr_bytes + size_bytes.
     * But req->data_len excludes the fmt byte. */
    uint16_t total_in_data = (uint16_t)addr_bytes + (uint16_t)size_bytes + (uint16_t)mem_size;

    if (req->data_len < total_in_data)
    {
        set_neg_rsp(rsp, req->sid, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return true;
    }

    /* Write data follows the address and size in req->data */
    const uint8_t *write_data = req->data + addr_bytes + size_bytes;

    /* --- ROOR: validate memory region --- */
    uint32_t region_offset = 0;
    const uds_mem_region_t *region = find_memory_region(address, mem_size,
                                                         &region_offset);
    if (region == NULL)
    {
        set_neg_rsp(rsp, req->sid, NRC_REQUEST_OUT_OF_RANGE);
        return true;
    }

    /* --- Write data to memory --- */
    memcpy(region->data + region_offset, write_data, mem_size);

    /* Build positive response: [0x7D][fmt][addr_bytes...][size_bytes...] */
    uint16_t header_len = 1 + (uint16_t)addr_bytes + (uint16_t)size_bytes;
    g_rsp_buf[0] = UDS_REQ_BYTE1(req);  /* format byte */
    memcpy(g_rsp_buf + 1, req->data, addr_bytes);
    memcpy(g_rsp_buf + 1 + addr_bytes, req->data + addr_bytes, size_bytes);

    set_pos_rsp(rsp, WRITE_MEMORY_BY_ADDRESS_RSP, 0,
                g_rsp_buf, header_len);
    return true;
}

/* ======================================================================== *
 * Initialisation & State Access                                           *
 * ======================================================================== */

void uds_svc_data_init(void)
{
    memset(g_memory_buf, 0, sizeof(g_memory_buf));
    memset(g_dynamic_dids, 0, sizeof(g_dynamic_dids));
    memset(g_periodic_dids, 0, sizeof(g_periodic_dids));
}

uint8_t* uds_svc_data_get_memory_buf(void)
{
    return g_memory_buf;
}

uint16_t uds_svc_data_get_memory_region_count(void)
{
    return MEMORY_REGION_COUNT;
}

const uds_mem_region_t* uds_svc_data_get_memory_regions(void)
{
    return g_memory_regions;
}

uint8_t uds_svc_data_get_dynamic_did_count(void)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < DYNAMIC_DID_MAX; i++)
    {
        if (g_dynamic_dids[i].active)
        {
            count++;
        }
    }
    return count;
}

const uds_dynamic_did_entry_t* uds_svc_data_get_dynamic_did(uint8_t idx)
{
    if (idx >= DYNAMIC_DID_MAX)
    {
        return NULL;
    }
    return &g_dynamic_dids[idx];
}

uint8_t uds_svc_data_get_periodic_did_count(void)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < PERIODIC_DID_MAX; i++)
    {
        if (g_periodic_dids[i].active)
        {
            count++;
        }
    }
    return count;
}

const uds_periodic_did_entry_t* uds_svc_data_get_periodic_did(uint8_t idx)
{
    if (idx >= PERIODIC_DID_MAX)
    {
        return NULL;
    }
    return &g_periodic_dids[idx];
}
