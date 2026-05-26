/*
 * uds_svc_upload.c
 * Upload/Download Services — Implementation
 *
 * Implements handler functions for UDS upload/download services:
 *   0x34 — RequestDownload
 *   0x35 — RequestUpload
 *   0x36 — TransferData
 *   0x37 — RequestTransferExit
 *   0x38 — RequestFileTransfer
 *
 * Services 0x34, 0x35, 0x36, 0x37 do NOT have subfunctions per ISO 14229-1.
 * The parser consumes byte[1] as subfunction — these handlers reconstruct
 * the original byte[1] via UDS_REQ_BYTE1() for dataFormatIdentifier and
 * blockSequenceCounter.
 *
 * Service 0x38 (RequestFileTransfer) has a subfunction but returns NRC 0x31
 * as a stub (no filesystem access implemented).
 *
 * Context for handlers: uds_session_context_t* for session validation.
 * Security check: uds_security_is_unlocked().
 *
 * Wave 3c Task 14 — Upload/Download Services
 */

#include "uds/uds_svc_upload.h"
#include "uds/uds_session.h"
#include "uds/uds_security.h"
#include "uds/uds_svc_data.h"    /* for UDS_REQ_BYTE1 macro */
#include "uds/uds_svc_util.h"    /* uds_set_neg_rsp, uds_set_pos_rsp, uds_should_suppress */
#include <string.h>              /* memset, memcpy */

/* ======================================================================== *
 * Static Transfer Context                                                  *
 * ======================================================================== */

/** @brief Active transfer state (only one transfer at a time) */
static uds_transfer_context_t g_transfer_ctx;

/* ======================================================================== *
 * Memory Region Table                                                     *
 * ======================================================================== */

/** @brief Configurable memory regions for upload/download address validation */
static uds_transfer_mem_region_t g_mem_regions[UDS_MEM_REGION_MAX];

/** @brief Number of valid entries in g_mem_regions */
static uint16_t g_mem_region_count = 0u;

/* ======================================================================== *
 * Internal Helpers — Validation                                           *
 * ======================================================================== */

/**
 * @brief Check whether the current session allows upload/download operations.
 *
 * Upload/download requires programmingSession (0x02) or extendedSession (0x03).
 *
 * @param[in] ctx  Session context (must not be NULL)
 * @return true if session is valid for transfer operations
 */
static bool is_transfer_session_allowed(const uds_session_context_t *ctx)
{
    uint8_t s = ctx->current_session;
    return (s == UDS_PROGRAMMING_SESSION || s == UDS_EXTENDED_SESSION);
}

/**
 * @brief Check whether a memory address range falls within any configured
 *        upload/download memory region.
 *
 * @param[in]  address  Start address
 * @param[in]  size     Number of bytes to verify
 * @return true if the range is fully within a registered region
 */
static bool is_memory_range_valid(uint32_t address, uint32_t size)
{
    if (size == 0u)
    {
        return false;
    }

    /* Guard against overflow: address + size - 1 must not wrap */
    if (address + (size - 1u) < address)
    {
        return false;
    }

    for (uint16_t i = 0u; i < g_mem_region_count; i++)
    {
        const uds_transfer_mem_region_t *reg = &g_mem_regions[i];

        if (address >= reg->start_addr &&
            (address + size - 1u) <= reg->end_addr)
        {
            return true;
        }
    }
    return false;
}

/* ======================================================================== *
 * Shared Logic: 0x34 RequestDownload / 0x35 RequestUpload                 *
 * ======================================================================== */

/**
 * @brief Common implementation for RequestDownload and RequestUpload.
 *
 * @param[in]  req        Parsed UDS request
 * @param[out] rsp        Response structure
 * @param[in]  context    Session context
 * @param[in]  direction  UDS_TRANSFER_DOWNLOAD or UDS_TRANSFER_UPLOAD
 * @param[in]  rsp_sid    Positive response SID
 * @return true to send response
 */
static bool request_transfer_common(const uds_request_t  *req,
                                    uds_response_t       *rsp,
                                    void                 *context,
                                    uds_transfer_direction_t direction)
{
    static uint8_t rsp_buf[4];

    /* --- CNC: validate context (session context required) --- */
    if (context == NULL)
    {
        rsp->sid          = 0x7F;
        rsp->subfunc_echo = req->sid;
        rsp_buf[0]        = (uint8_t)NRC_CONDITIONS_NOT_CORRECT;
        rsp->data         = rsp_buf;
        rsp->data_len     = 1;
        return true;
    }

    uds_session_context_t *sctx = (uds_session_context_t *)context;

    /* --- CNC: session must be programming or extended --- */
    if (!is_transfer_session_allowed(sctx))
    {
        rsp->sid          = 0x7F;
        rsp->subfunc_echo = req->sid;
        rsp_buf[0]        = (uint8_t)NRC_CONDITIONS_NOT_CORRECT;
        rsp->data         = rsp_buf;
        rsp->data_len     = 1;
        return true;
    }

    /* --- SAD: security must be unlocked --- */
    if (!uds_security_is_unlocked())
    {
        rsp->sid          = 0x7F;
        rsp->subfunc_echo = req->sid;
        rsp_buf[0]        = (uint8_t)NRC_SECURITY_ACCESS_DENIED;
        rsp->data         = rsp_buf;
        rsp->data_len     = 1;
        return true;
    }

    /* --- IMLOIF: need exactly 8 bytes of data (addr(4) + size(4)) --- */
    /* The parser consumed byte[1] as subfunction.  req->data points to
     * raw+2 which contains [addr(4)][size(4)], so we need data_len >= 8. */
    if (req->data_len < 8u || req->data == NULL)
    {
        rsp->sid          = 0x7F;
        rsp->subfunc_echo = req->sid;
        rsp_buf[0]        = (uint8_t)NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
        rsp->data         = rsp_buf;
        rsp->data_len     = 1;
        return true;
    }

    /* --- Parse: dataFormatIdentifier (reconstructed from byte[1]) --- */
    /* uint8_t dfi = UDS_REQ_BYTE1(req);  — stored but not used in this impl */

    /* --- Parse: memoryAddress (4 bytes, big-endian) --- */
    uint32_t mem_addr = ((uint32_t)req->data[0] << 24) |
                        ((uint32_t)req->data[1] << 16) |
                        ((uint32_t)req->data[2] <<  8) |
                        ((uint32_t)req->data[3]);

    /* --- Parse: memorySize (4 bytes, big-endian) --- */
    uint32_t mem_size = ((uint32_t)req->data[4] << 24) |
                        ((uint32_t)req->data[5] << 16) |
                        ((uint32_t)req->data[6] <<  8) |
                        ((uint32_t)req->data[7]);

    /* --- Check size against buffer capacity first --- */
    /* If size exceeds our buffer, reject before checking memory range. */
    if (mem_size > UDS_TRANSFER_BUFFER_SIZE)
    {
        rsp->sid          = 0x7F;
        rsp->subfunc_echo = req->sid;
        rsp_buf[0]        = (uint8_t)NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED;
        rsp->data         = rsp_buf;
        rsp->data_len     = 1;
        return true;
    }

    /* --- ROOR / UDNA: check memory address and size validity --- */
    if (mem_size == 0u || !is_memory_range_valid(mem_addr, mem_size))
    {
        rsp->sid          = 0x7F;
        rsp->subfunc_echo = req->sid;
        rsp_buf[0]        = (uint8_t)NRC_REQUEST_OUT_OF_RANGE;
        rsp->data         = rsp_buf;
        rsp->data_len     = 1;
        return true;
    }

    /* --- UDNA: refuse if a transfer is already active --- */
    if (g_transfer_ctx.active)
    {
        rsp->sid          = 0x7F;
        rsp->subfunc_echo = req->sid;
        rsp_buf[0]        = (uint8_t)NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED;
        rsp->data         = rsp_buf;
        rsp->data_len     = 1;
        return true;
    }

    /* --- Set up transfer context --- */
    g_transfer_ctx.active            = true;
    g_transfer_ctx.direction         = direction;
    g_transfer_ctx.memory_addr       = mem_addr;
    g_transfer_ctx.remaining_size    = mem_size;
    g_transfer_ctx.max_block_length  = UDS_DEFAULT_MAX_BLOCK_LENGTH;
    g_transfer_ctx.block_seq_counter = 0u;   /* first TransferData expects BSC=1 */
    g_transfer_ctx.buffer_length     = 0u;

    (void)memset(g_transfer_ctx.buffer, 0, UDS_TRANSFER_BUFFER_SIZE);

    /* --- Build positive response: maxBlockLength (2 bytes, big-endian) --- */
    uint16_t max_blk = g_transfer_ctx.max_block_length;
    rsp_buf[0] = (uint8_t)((max_blk >> 8) & 0xFFu);
    rsp_buf[1] = (uint8_t)( max_blk        & 0xFFu);

    (void)uds_set_pos_rsp(rsp, req->sid, rsp_buf, 2);
    return true;
}

/* ======================================================================== *
 * 0x34 — RequestDownload                                                  *
 * ======================================================================== */

bool uds_svc_request_download(const uds_request_t *req,
                               uds_response_t      *rsp,
                               void                *context)
{
    return request_transfer_common(req, rsp, context,
                                   UDS_TRANSFER_DOWNLOAD);
}

/* ======================================================================== *
 * 0x35 — RequestUpload                                                    *
 * ======================================================================== */

bool uds_svc_request_upload(const uds_request_t *req,
                             uds_response_t      *rsp,
                             void                *context)
{
    return request_transfer_common(req, rsp, context,
                                   UDS_TRANSFER_UPLOAD);
}

/* ======================================================================== *
 * 0x36 — TransferData                                                     *
 * ======================================================================== */

bool uds_svc_transfer_data(const uds_request_t *req,
                             uds_response_t      *rsp,
                             void                *context)
{
    static uint8_t rsp_buf[4];
    (void)context;

    /* --- CNC: no active transfer --- */
    if (!g_transfer_ctx.active)
    {
        rsp->sid          = 0x7F;
        rsp->subfunc_echo = req->sid;
        rsp_buf[0]        = (uint8_t)NRC_CONDITIONS_NOT_CORRECT;
        rsp->data         = rsp_buf;
        rsp->data_len     = 1;
        return true;
    }

    /* --- Reconstruct blockSequenceCounter from byte[1] --- */
    /* The parser consumed byte[1] as subfunction. Recover it. */
    uint8_t bsc = UDS_REQ_BYTE1(req);

    /* --- WBSC: validate block sequence counter --- */
    uint8_t expected_bsc = (uint8_t)(g_transfer_ctx.block_seq_counter + 1u);

    if (bsc != expected_bsc)
    {
        rsp->sid          = 0x7F;
        rsp->subfunc_echo = req->sid;
        rsp_buf[0]        = (uint8_t)NRC_WRONG_BLOCK_SEQUENCE_COUNTER;
        rsp->data         = rsp_buf;
        rsp->data_len     = 1;
        return true;
    }

    /* --- Update sequence counter --- */
    g_transfer_ctx.block_seq_counter = bsc;

    /* === Download direction === */
    if (g_transfer_ctx.direction == UDS_TRANSFER_DOWNLOAD)
    {
        /* --- IMLOIF: download must have at least 1 data byte --- */
        if (req->data_len == 0u || req->data == NULL)
        {
            rsp->sid          = 0x7F;
            rsp->subfunc_echo = req->sid;
            rsp_buf[0]        = (uint8_t)NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
            rsp->data         = rsp_buf;
            rsp->data_len     = 1;
            return true;
        }

        /* --- ROOR: clamp data to remaining size --- */
        uint16_t write_len = req->data_len;
        if ((uint32_t)write_len > g_transfer_ctx.remaining_size)
        {
            write_len = (uint16_t)g_transfer_ctx.remaining_size;
        }

        /* --- TDS: check buffer overflow --- */
        if ((uint32_t)(g_transfer_ctx.buffer_length + write_len) >
            UDS_TRANSFER_BUFFER_SIZE)
        {
            rsp->sid          = 0x7F;
            rsp->subfunc_echo = req->sid;
            rsp_buf[0]        = (uint8_t)NRC_TRANSFER_DATA_SUSPENDED;
            rsp->data         = rsp_buf;
            rsp->data_len     = 1;
            return true;
        }

        /* --- Copy data into transfer buffer --- */
        (void)memcpy(&g_transfer_ctx.buffer[g_transfer_ctx.buffer_length],
                     req->data, write_len);
        g_transfer_ctx.buffer_length += write_len;

        /* --- Update remaining size --- */
        if ((uint32_t)write_len >= g_transfer_ctx.remaining_size)
        {
            g_transfer_ctx.remaining_size = 0u;
        }
        else
        {
            g_transfer_ctx.remaining_size -= (uint32_t)write_len;
        }
    }
    /* === Upload direction === */
    else if (g_transfer_ctx.direction == UDS_TRANSFER_UPLOAD)
    {
        /* --- Determine how many bytes to send in this block --- */
        uint16_t read_len = g_transfer_ctx.max_block_length;
        if (read_len > g_transfer_ctx.remaining_size)
        {
            read_len = (uint16_t)g_transfer_ctx.remaining_size;
        }
        if (read_len > UDS_TRANSFER_BUFFER_SIZE)
        {
            read_len = UDS_TRANSFER_BUFFER_SIZE;
        }

        /* --- ROOR: if no data left, reject --- */
        if (read_len == 0u || g_transfer_ctx.buffer_length == 0u)
        {
            rsp->sid          = 0x7F;
            rsp->subfunc_echo = req->sid;
            rsp_buf[0]        = (uint8_t)NRC_REQUEST_OUT_OF_RANGE;
            rsp->data         = rsp_buf;
            rsp->data_len     = 1;
            return true;
        }

        /* --- Build response with data from buffer --- */
        uint16_t copy_len = read_len;
        if (copy_len > g_transfer_ctx.buffer_length)
        {
            copy_len = g_transfer_ctx.buffer_length;
        }

        /* Response: [0x76][bsc][data...] using bsc as subfunc_echo
         * and data pointer for the payload bytes. */
        (void)uds_set_pos_rsp(rsp, TRANSFER_DATA,
                              g_transfer_ctx.buffer, copy_len);
        rsp->subfunc_echo = bsc;

        /* --- Advance the buffer position --- */
        uint32_t new_remaining = (copy_len >= g_transfer_ctx.buffer_length)
                                 ? 0u
                                 : (g_transfer_ctx.buffer_length - copy_len);

        if (new_remaining > 0u && copy_len > 0u)
        {
            (void)memmove(g_transfer_ctx.buffer,
                          &g_transfer_ctx.buffer[copy_len],
                          new_remaining);
        }

        g_transfer_ctx.buffer_length = (uint16_t)new_remaining;

        if ((uint32_t)copy_len >= g_transfer_ctx.remaining_size)
        {
            g_transfer_ctx.remaining_size = 0u;
        }
        else
        {
            g_transfer_ctx.remaining_size -= (uint32_t)copy_len;
        }

        return true;
    }

    /* === Download direction: response is [0x76][bsc] === */
    /* The BSC is echoed in the subfunction byte position which aligns
     * with the UDS wire format for TransferData response. */
    (void)uds_set_pos_rsp(rsp, TRANSFER_DATA, NULL, 0);
    rsp->subfunc_echo = bsc;
    return true;
}

/* ======================================================================== *
 * 0x37 — RequestTransferExit                                              *
 * ======================================================================== */

bool uds_svc_request_transfer_exit(const uds_request_t *req,
                                    uds_response_t      *rsp,
                                    void                *context)
{
    static uint8_t rsp_buf[4];
    (void)context;

    /* --- CNC: no active transfer --- */
    if (!g_transfer_ctx.active)
    {
        rsp->sid          = 0x7F;
        rsp->subfunc_echo = req->sid;
        rsp_buf[0]        = (uint8_t)NRC_CONDITIONS_NOT_CORRECT;
        rsp->data         = rsp_buf;
        rsp->data_len     = 1;
        return true;
    }

    /* --- IMLOIF: no additional data expected --- */
    if (req->data_len != 0u)
    {
        rsp->sid          = 0x7F;
        rsp->subfunc_echo = req->sid;
        rsp_buf[0]        = (uint8_t)NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
        rsp->data         = rsp_buf;
        rsp->data_len     = 1;
        return true;
    }

    /* --- Clean up transfer context --- */
    g_transfer_ctx.active            = false;
    g_transfer_ctx.direction         = UDS_TRANSFER_NONE;
    g_transfer_ctx.memory_addr       = 0u;
    g_transfer_ctx.remaining_size    = 0u;
    g_transfer_ctx.block_seq_counter = 0u;
    g_transfer_ctx.buffer_length     = 0u;

    /* --- Build positive response (no additional data) --- */
    (void)uds_set_pos_rsp(rsp, REQUEST_TRANSFER_EXIT, NULL, 0);
    return true;
}

/* ======================================================================== *
 * 0x38 — RequestFileTransfer (Stub)                                       *
 * ======================================================================== */

bool uds_svc_request_file_transfer(const uds_request_t *req,
                                    uds_response_t      *rsp,
                                    void                *context)
{
    static uint8_t rsp_buf[4];

    /* --- CNC: validate context --- */
    if (context == NULL)
    {
        rsp->sid          = 0x7F;
        rsp->subfunc_echo = req->sid;
        rsp_buf[0]        = (uint8_t)NRC_CONDITIONS_NOT_CORRECT;
        rsp->data         = rsp_buf;
        rsp->data_len     = 1;
        return true;
    }

    uds_session_context_t *sctx = (uds_session_context_t *)context;

    /* --- CNC: session check --- */
    if (!is_transfer_session_allowed(sctx))
    {
        rsp->sid          = 0x7F;
        rsp->subfunc_echo = req->sid;
        rsp_buf[0]        = (uint8_t)NRC_CONDITIONS_NOT_CORRECT;
        rsp->data         = rsp_buf;
        rsp->data_len     = 1;
        return true;
    }

    /* --- SAD: security must be unlocked --- */
    if (!uds_security_is_unlocked())
    {
        rsp->sid          = 0x7F;
        rsp->subfunc_echo = req->sid;
        rsp_buf[0]        = (uint8_t)NRC_SECURITY_ACCESS_DENIED;
        rsp->data         = rsp_buf;
        rsp->data_len     = 1;
        return true;
    }

    /* --- Validate subfunction --- */
    uint8_t subfn = req->subfunction.value;

    /* Supported subfunctions: 0x01 (addFile), 0x02 (deleteFile),
     * 0x03 (replaceFile), 0x04 (readFile) */
    if (subfn < 0x01u || subfn > 0x04u)
    {
        rsp->sid          = 0x7F;
        rsp->subfunc_echo = req->sid;
        rsp_buf[0]        = (uint8_t)NRC_SUB_FUNCTION_NOT_SUPPORTED;
        rsp->data         = rsp_buf;
        rsp->data_len     = 1;
        return true;
    }

    /* --- IMLOIF: need at least 1 data byte (filePathLength) --- */
    if (req->data_len < 1u || req->data == NULL)
    {
        rsp->sid          = 0x7F;
        rsp->subfunc_echo = req->sid;
        rsp_buf[0]        = (uint8_t)NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
        rsp->data         = rsp_buf;
        rsp->data_len     = 1;
        return true;
    }

    /* --- Stub: filesystem operations not implemented, return ROOR --- */
    rsp->sid          = 0x7F;
    rsp->subfunc_echo = req->sid;
    rsp_buf[0]        = (uint8_t)NRC_REQUEST_OUT_OF_RANGE;
    rsp->data         = rsp_buf;
    rsp->data_len     = 1;
    return true;
}

/* ======================================================================== *
 * Transfer Context Management API                                         *
 * ======================================================================== */

void uds_svc_transfer_init(void)
{
    (void)memset(&g_transfer_ctx, 0, sizeof(g_transfer_ctx));
    g_transfer_ctx.direction = UDS_TRANSFER_NONE;

    /* Clear memory region table */
    (void)memset(g_mem_regions, 0, sizeof(g_mem_regions));
    g_mem_region_count = 0u;
}

const uds_transfer_context_t* uds_svc_transfer_get_context(void)
{
    return &g_transfer_ctx;
}

uint8_t* uds_svc_transfer_get_buffer(void)
{
    return g_transfer_ctx.buffer;
}

void uds_svc_transfer_register_memory_region(uint32_t start_addr,
                                              uint32_t end_addr)
{
    if (g_mem_region_count >= UDS_MEM_REGION_MAX)
    {
        return;  /* table full */
    }

    if (end_addr < start_addr)
    {
        return;  /* invalid range */
    }

    g_mem_regions[g_mem_region_count].start_addr = start_addr;
    g_mem_regions[g_mem_region_count].end_addr   = end_addr;
    g_mem_region_count++;
}

uint16_t uds_svc_transfer_get_region_count(void)
{
    return g_mem_region_count;
}

const uds_transfer_mem_region_t* uds_svc_transfer_get_regions(void)
{
    return g_mem_regions;
}
