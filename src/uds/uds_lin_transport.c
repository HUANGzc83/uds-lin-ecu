/*
 * uds_lin_transport.c
 * LIN Transport Layer Implementation — ISO 17987-3 Frame Encode/Decode
 *
 * Implements UDS-over-LIN diagnostic framing:
 *   - Single Frame (SF): UDS payload <= 6 bytes in one LIN frame
 *   - First Frame (FF): start of multi-frame transmission
 *   - Consecutive Frames (CF): continuation of multi-frame transmission
 *   - Flow Control (FC): rate-control for multi-frame reception
 *
 * All core functions accept an explicit lin_transport_ctx_t* for
 * full reentrancy.  Backward-compatible wrappers (without ctx)
 * delegate to the internal default context g_default_ctx.
 *
 * Wave 2 Task 8 — LIN Transport Adapter
 * Wave 2 Task 9 — Context Migration + Bug Fixes
 */

#include "uds/uds_lin_transport.h"
#include <string.h>

/* ======================================================================== *
 * Transport Constants (private)                                             *
 * ======================================================================== */

/** @brief Maximum UDS payload bytes in a First Frame (8 - 1 NAD - 1 PCI - 2 length) */
#define LIN_FF_PAYLOAD_LEN   5

/** @brief Maximum UDS payload bytes in a Consecutive Frame (8 - 1 NAD - 1 PCI) */
#define LIN_CF_PAYLOAD_LEN   6

/** @brief Sequence number range (1..15 per ISO 17987-3) */
#define LIN_CF_SEQ_MAX       15

/** @brief Upper reserved bytes in LIN frame data (data[4..7] = sizeof(uint32_t)) */
#define LIN_FRAME_UPPER_RESERVED_SIZE  (sizeof(uint32_t))

/* ======================================================================== *
 * Static Context Pool                                                       *
 * ======================================================================== */

/** @brief Default context used by backward-compatible wrapper functions */
static lin_transport_ctx_t g_default_ctx;

/** @brief Pre-allocated pool for additional transport instances */
static lin_transport_ctx_t g_pool[LIN_TRANSPORT_MAX_INSTANCES];

/** @brief Pool slot usage flags (true = in use) */
static bool g_pool_used[LIN_TRANSPORT_MAX_INSTANCES];

/* ======================================================================== *
 * Initialisation Helper                                                     *
 * ======================================================================== */

/**
 * @brief Initialise a transport context to default/reset state.
 */
static void ctx_init(lin_transport_ctx_t *ctx, uint8_t nad)
{
    ctx->tx_state          = LIN_TX_IDLE;
    ctx->rx_state          = LIN_RX_IDLE;
    ctx->expected_nad      = nad;
    ctx->accept_functional = true;
    ctx->rx_buffer_len     = 0;
    ctx->rx_expected_len   = 0;
    ctx->rx_seq_num        = 0;
    ctx->tx_seq_num        = 0;
    ctx->wft_counter       = 0;
    ctx->cf_in_block       = 0;
    ctx->tx_stream_data    = NULL;
    ctx->tx_stream_remaining = 0;
    ctx->tx_stream_active  = false;
    ctx->send_frame_cb     = NULL;
    ctx->last_tx_time      = 0;
    ctx->last_rx_time      = 0;

    ctx->rx_fc_params.fc_status  = LIN_FC_CONTINUE;
    ctx->rx_fc_params.block_size = LIN_DEFAULT_BS;
    ctx->rx_fc_params.stmin      = LIN_DEFAULT_STMIN;

    ctx->tx_fc_params.fc_status  = LIN_FC_CONTINUE;
    ctx->tx_fc_params.block_size = LIN_DEFAULT_BS;
    ctx->tx_fc_params.stmin      = LIN_DEFAULT_STMIN;

    (void)memset(ctx->rx_buffer, 0, sizeof(ctx->rx_buffer));
    (void)memset(ctx->sf_buffer, 0, sizeof(ctx->sf_buffer));
    (void)memset(ctx->tx_stream_buf, 0, sizeof(ctx->tx_stream_buf));
}

/* ======================================================================== *
 * Internal Helpers                                                          *
 * ======================================================================== */

/**
 * @brief Compute the number of LIN frames needed to encode a PDU.
 * @param data_len  UDS payload length.
 * @return Number of frames required (0 if length exceeds max).
 */
static uint8_t calc_tx_frame_count(uint16_t data_len)
{
    if (data_len == 0 || data_len > LIN_FF_MAX_LEN)
        return 0;
    if (data_len <= LIN_SF_MAX_LEN)
        return 1;

    uint16_t remaining = data_len;
    uint8_t  count = 1;  /* FF */

    remaining -= LIN_FF_PAYLOAD_LEN;

    while (remaining > 0) {
        count++;
        if (remaining >= LIN_CF_PAYLOAD_LEN)
            remaining -= LIN_CF_PAYLOAD_LEN;
        else
            remaining = 0;
    }

    return count;
}

/* ======================================================================== *
 * lin_tx_encode_ctx — Encode UDS PDU into one or more LIN frames           *
 * ======================================================================== */

lin_status_t lin_tx_encode_ctx(lin_transport_ctx_t *ctx,
                               const lin_diag_pdu_t *pdu,
                               lin_frame_t *frames,
                               uint8_t *frame_count,
                               uint8_t max_frames)
{
    (void)ctx;

    /* --- Input validation --- */
    if (pdu == NULL || frames == NULL || frame_count == NULL)
        return LIN_PCI_ERROR;
    if (pdu->data_len == 0 || pdu->uds_data == NULL)
        return LIN_PCI_ERROR;
    if (pdu->data_len > LIN_FF_MAX_LEN)
        return LIN_PCI_ERROR;

    uint8_t  nad      = pdu->nad;
    uint16_t data_len = pdu->data_len;
    const uint8_t *src = pdu->uds_data;

    /* --- Single Frame (SF) --- */
    if (data_len <= LIN_SF_MAX_LEN)
    {
        if (max_frames < 1)
        {
            *frame_count = 0;
            return LIN_PCI_ERROR;
        }

        frames[0].data[0] = nad;
        frames[0].data[1] = LIN_PCI_SF | (uint8_t)data_len;
        (void)memcpy(&frames[0].data[2], src, data_len);
        *frame_count = 1;
        ctx->tx_state = LIN_TX_SF;

        return LIN_OK;
    }

    /* --- Multi-frame: First Frame (FF) + Consecutive Frames (CF) --- */
    uint8_t needed = calc_tx_frame_count(data_len);
    if (max_frames < needed)
    {
        *frame_count = 0;
        return LIN_PCI_ERROR;
    }

    uint16_t offset = 0;
    uint8_t  frame_idx = 0;

    /* First Frame */
    frames[frame_idx].data[0] = nad;
    frames[frame_idx].data[1] = LIN_PCI_FF | (uint8_t)((data_len >> 8) & 0x0F);
    frames[frame_idx].data[2] = (uint8_t)(data_len & 0xFF);

    uint8_t ff_payload = LIN_FF_PAYLOAD_LEN;
    if (data_len < ff_payload)
        ff_payload = (uint8_t)data_len;
    (void)memcpy(&frames[frame_idx].data[3], src + offset, ff_payload);
    offset += ff_payload;
    frame_idx++;

    ctx->tx_state = LIN_TX_FF;
    ctx->tx_seq_num = 0;

    /* Consecutive Frames */
    while (offset < data_len)
    {
        uint8_t remaining = (uint8_t)(data_len - offset);
        uint8_t cf_payload = LIN_CF_PAYLOAD_LEN;
        if (remaining < cf_payload)
            cf_payload = remaining;

        ctx->tx_seq_num = (ctx->tx_seq_num % LIN_CF_SEQ_MAX) + 1;

        /* Zero-pad CF to avoid stale data in unused bytes */
        (void)memset(frames[frame_idx].data, 0, LIN_FRAME_SIZE);

        frames[frame_idx].data[0] = nad;
        frames[frame_idx].data[1] = LIN_PCI_CF | ctx->tx_seq_num;
        (void)memcpy(&frames[frame_idx].data[2], src + offset, cf_payload);
        offset += cf_payload;
        frame_idx++;

        ctx->tx_state = LIN_TX_CF;
    }

    *frame_count = frame_idx;
    return LIN_OK;
}

/* ======================================================================== *
 * lin_rx_decode_ctx — Decode a single LIN frame into a UDS PDU             *
 * ======================================================================== */

lin_status_t lin_rx_decode_ctx(lin_transport_ctx_t *ctx,
                               const lin_frame_t *frame,
                               lin_diag_pdu_t *pdu)
{
    if (frame == NULL || pdu == NULL)
        return LIN_PCI_ERROR;

    /* --- NAD check --- */
    if (frame->data[0] != ctx->expected_nad)
    {
        if (!(frame->data[0] == LIN_NAD_FUNCTIONAL && ctx->accept_functional))
            return LIN_NAD_MISMATCH;
    }

    uint8_t pci_byte = frame->data[1];
    uint8_t pci_type = pci_byte & LIN_PCI_MASK;

    /* --- Single Frame (SF) --- */
    if (pci_type == LIN_PCI_SF)
    {
        uint8_t len = pci_byte & LIN_PCI_LEN_MASK;

        if (len > LIN_SF_MAX_LEN)
            return LIN_PCI_ERROR;

        pdu->nad      = frame->data[0];
        pdu->pci      = pci_byte;
        pdu->uds_data = (uint8_t *)&frame->data[2];
        pdu->data_len = len;

        ctx->rx_state = LIN_RX_SF;
        return LIN_OK;
    }

    /* --- First Frame (FF) — start of multi-frame --- */
    if (pci_type == LIN_PCI_FF)
    {
        uint16_t total_len = ((uint16_t)(pci_byte & 0x0F) << 8) | frame->data[2];

        if (total_len < (LIN_SF_MAX_LEN + 1) || total_len > LIN_FF_MAX_LEN)
            return LIN_PCI_ERROR;

        ctx->rx_expected_len = total_len;
        ctx->rx_buffer_len   = 0;
        ctx->rx_seq_num      = 0;
        ctx->rx_state        = LIN_RX_FF;

        /* Copy first 5 payload bytes into reassembly buffer */
        uint8_t copy_len = LIN_FF_PAYLOAD_LEN;
        if (total_len < copy_len)
            copy_len = (uint8_t)total_len;
        (void)memcpy(ctx->rx_buffer, &frame->data[3], copy_len);
        ctx->rx_buffer_len = copy_len;

        pdu->nad      = frame->data[0];
        pdu->pci      = pci_byte;
        pdu->uds_data = ctx->rx_buffer;
        pdu->data_len = ctx->rx_buffer_len;

        /* Transition to expecting CF if more data remains */
        if (ctx->rx_buffer_len < ctx->rx_expected_len)
            ctx->rx_state = LIN_RX_CF;

        return LIN_OK;
    }

    /* --- Consecutive Frame (CF) --- */
    if (pci_type == LIN_PCI_CF)
    {
        if (ctx->rx_state != LIN_RX_CF)
            return LIN_PCI_ERROR;

        uint8_t seq = pci_byte & LIN_PCI_LEN_MASK;
        uint8_t expected_seq = (uint8_t)((ctx->rx_seq_num % LIN_CF_SEQ_MAX) + 1);

        if (seq != expected_seq)
            return LIN_PCI_ERROR;

        ctx->rx_seq_num = seq;

        uint16_t remaining = ctx->rx_expected_len - ctx->rx_buffer_len;
        uint8_t copy_len = LIN_CF_PAYLOAD_LEN;
        if (copy_len > remaining)
            copy_len = (uint8_t)remaining;

        (void)memcpy(ctx->rx_buffer + ctx->rx_buffer_len, &frame->data[2], copy_len);
        ctx->rx_buffer_len += copy_len;

        pdu->nad      = frame->data[0];
        pdu->pci      = pci_byte;
        pdu->uds_data = ctx->rx_buffer;
        pdu->data_len = ctx->rx_buffer_len;

        if (ctx->rx_buffer_len >= ctx->rx_expected_len)
        {
            ctx->rx_state = LIN_RX_IDLE;
            pdu->data_len = ctx->rx_expected_len;
        }

        return LIN_OK;
    }

    /* --- Flow Control (FC) — received by a sender waiting for FC --- */
    if (pci_type == LIN_PCI_FC)
    {
        ctx->rx_fc_params.fc_status =
            (lin_fc_status_t)(frame->data[2] & 0x0F);
        ctx->rx_fc_params.block_size = frame->data[3];
        ctx->rx_fc_params.stmin      = frame->data[4];

        if (ctx->rx_fc_params.fc_status == LIN_FC_CONTINUE)
        {
            ctx->cf_in_block = 0;
            if (ctx->tx_state == LIN_TX_WAIT_FC)
                ctx->tx_state = LIN_TX_CF;
        }
        else if (ctx->rx_fc_params.fc_status == LIN_FC_WAIT)
        {
            ctx->wft_counter++;
            if (ctx->wft_counter > LIN_MAX_WFT_FRAMES)
                return LIN_FC_WAIT_EXCEEDED;
        }

        pdu->nad      = frame->data[0];
        pdu->pci      = pci_byte;
        pdu->uds_data = NULL;
        pdu->data_len = 0;

        return LIN_OK;
    }

    /* --- Unrecognized PCI type --- */
    return LIN_PCI_ERROR;
}

/* ======================================================================== *
 * lin_transport_reset_ctx — Clear transport state machine                  *
 * ======================================================================== */

void lin_transport_reset_ctx(lin_transport_ctx_t *ctx)
{
    ctx->tx_state        = LIN_TX_IDLE;
    ctx->rx_state        = LIN_RX_IDLE;
    ctx->rx_buffer_len   = 0;
    ctx->rx_expected_len = 0;
    ctx->rx_seq_num      = 0;
    ctx->tx_seq_num      = 0;
    ctx->wft_counter     = 0;
    ctx->cf_in_block     = 0;
    ctx->tx_stream_active = false;
    ctx->tx_stream_data   = NULL;
    ctx->tx_stream_remaining = 0;
    ctx->expected_nad    = LIN_NAD_DEFAULT;
    ctx->accept_functional = true;
    (void)memset(ctx->rx_buffer,     0, sizeof(ctx->rx_buffer));
    (void)memset(ctx->tx_stream_buf, 0, sizeof(ctx->tx_stream_buf));
}

/* ======================================================================== *
 * lin_transport_set_nad_ctx — Set receive filter NAD                       *
 * ======================================================================== */

void lin_transport_set_nad_ctx(lin_transport_ctx_t *ctx, uint8_t nad)
{
    ctx->expected_nad = nad;
}

/* ======================================================================== *
 * lin_transport_set_accept_functional_ctx — Enable/disable NAD 0x7E        *
 * ======================================================================== */

void lin_transport_set_accept_functional_ctx(lin_transport_ctx_t *ctx, bool enable)
{
    ctx->accept_functional = enable;
}

/* ======================================================================== *
 * lin_tx_encode_fc_ctx — Encode a Flow Control (FC) frame                  *
 * ======================================================================== */

lin_status_t lin_tx_encode_fc_ctx(lin_transport_ctx_t *ctx,
                                  uint8_t nad,
                                  const lin_fc_params_t *params,
                                  lin_frame_t *frame)
{
    (void)ctx;
    if (frame == NULL || params == NULL)
        return LIN_PCI_ERROR;

    (void)memset(&frame->data[4], 0, LIN_FRAME_UPPER_RESERVED_SIZE);
    frame->data[0] = nad;
    frame->data[1] = LIN_PCI_FC | (uint8_t)params->fc_status;
    frame->data[2] = (uint8_t)params->fc_status;
    frame->data[3] = params->block_size;
    frame->data[4] = params->stmin;

    return LIN_OK;
}

/* ======================================================================== *
 * lin_transport_get_fc_params_ctx — Get received FC parameters             *
 * ======================================================================== */

void lin_transport_get_fc_params_ctx(lin_transport_ctx_t *ctx,
                                     lin_fc_params_t *params)
{
    if (params != NULL)
        *params = ctx->rx_fc_params;
}

/* ======================================================================== *
 * lin_transport_set_fc_params_ctx — Set local FC parameters                *
 * ======================================================================== */

void lin_transport_set_fc_params_ctx(lin_transport_ctx_t *ctx,
                                     uint8_t stmin, uint8_t bs)
{
    ctx->tx_fc_params.stmin      = stmin;
    ctx->tx_fc_params.block_size = bs;
    ctx->tx_fc_params.fc_status  = LIN_FC_CONTINUE;
}

/* ======================================================================== *
 * lin_transport_check_timeout_ctx — Check transport timeout                 *
 * ======================================================================== *
 * Uses separate TX/RX timestamps to avoid conflation:
 *   - N_Bs (0): time between FF sent and FC received → uses last_tx_time
 *   - N_Br (1): time between FC sent and first CF  → uses last_tx_time
 *   - N_Cr (2): time between CFs                    → uses last_rx_time
 *   - N_As (3): master frame transmit time           → uses last_tx_time
 *   - N_Ar (4): slave frame transmit time            → uses last_rx_time
 */

bool lin_transport_check_timeout_ctx(lin_transport_ctx_t *ctx,
                                     uint8_t timeout_type,
                                     uint32_t current_time)
{
    uint32_t elapsed;

    switch (timeout_type)
    {
    case 0: /* N_Bs — FF → FC (TX-side, waiting for response) */
        elapsed = current_time - ctx->last_tx_time;
        return elapsed >= LIN_TIMEOUT_N_BS;

    case 1: /* N_Br — FC → first CF (TX-side, after sending FC) */
        elapsed = current_time - ctx->last_tx_time;
        return elapsed >= LIN_TIMEOUT_N_BR;

    case 2: /* N_Cr — CF → CF gap (RX-side, waiting for next CF) */
        elapsed = current_time - ctx->last_rx_time;
        return elapsed >= LIN_TIMEOUT_N_CR;

    case 3: /* N_As — master frame TX */
        elapsed = current_time - ctx->last_tx_time;
        return elapsed >= LIN_TIMEOUT_N_AS;

    case 4: /* N_Ar — slave frame TX */
        elapsed = current_time - ctx->last_rx_time;
        return elapsed >= LIN_TIMEOUT_N_AR;

    default:
        return false;
    }
}

/* ======================================================================== *
 * lin_transport_record_tx_time_ctx — Record frame transmission time        *
 * ======================================================================== */

void lin_transport_record_tx_time_ctx(lin_transport_ctx_t *ctx, uint32_t tx_time)
{
    ctx->last_tx_time = tx_time;
}

/* ======================================================================== *
 * lin_transport_record_rx_time_ctx — Record frame reception time           *
 * ======================================================================== */

void lin_transport_record_rx_time_ctx(lin_transport_ctx_t *ctx, uint32_t rx_time)
{
    ctx->last_rx_time = rx_time;
}

/* ======================================================================== *
 * lin_transport_register_send_callback_ctx — Register TX callback          *
 * ======================================================================== */

void lin_transport_register_send_callback_ctx(lin_transport_ctx_t *ctx,
                                              lin_send_frame_fn_t cb)
{
    ctx->send_frame_cb = cb;
}

/* ======================================================================== *
 * lin_tx_begin_stream_ctx — Begin streaming multi-frame transmission       *
 * ======================================================================== *
 * Fixes dangling-pointer bug: the caller's PDU data is COPIED into
 * ctx->tx_stream_buf so ctx->tx_stream_data remains valid even after
 * the caller's buffer goes out of scope.
 */

lin_status_t lin_tx_begin_stream_ctx(lin_transport_ctx_t *ctx,
                                     const lin_diag_pdu_t *pdu,
                                     lin_frame_t *ff_out)
{
    if (ctx == NULL || pdu == NULL || ff_out == NULL)
        return LIN_PCI_ERROR;
    if (pdu->data_len == 0 || pdu->uds_data == NULL)
        return LIN_PCI_ERROR;
    if (pdu->data_len > LIN_FF_MAX_LEN)
        return LIN_PCI_ERROR;

    /* Single-frame: encode directly as SF */
    if (pdu->data_len <= LIN_SF_MAX_LEN)
    {
        (void)memset(ff_out->data, 0, LIN_FRAME_SIZE);
        ff_out->data[0] = pdu->nad;
        ff_out->data[1] = LIN_PCI_SF | (uint8_t)pdu->data_len;
        (void)memcpy(&ff_out->data[2], pdu->uds_data, pdu->data_len);

        ctx->tx_state         = LIN_TX_SF;
        ctx->tx_stream_active = false;
        ctx->tx_stream_data   = NULL;
        ctx->tx_stream_remaining = 0;
        return LIN_OK;
    }

    /* Multi-frame: encode FF, copy remaining payload to internal buffer */
    uint16_t total_len = pdu->data_len;

    (void)memset(ff_out->data, 0, LIN_FRAME_SIZE);
    ff_out->data[0] = pdu->nad;
    ff_out->data[1] = LIN_PCI_FF | (uint8_t)((total_len >> 8) & 0x0F);
    ff_out->data[2] = (uint8_t)(total_len & 0xFF);

    uint8_t ff_payload = LIN_FF_PAYLOAD_LEN;
    if (total_len < ff_payload)
        ff_payload = (uint8_t)total_len;
    (void)memcpy(&ff_out->data[3], pdu->uds_data, ff_payload);

    /* Copy REMAINING payload into internal buffer (fixes dangling pointer) */
    uint16_t remaining = total_len - ff_payload;
    if (remaining > 0)
    {
        (void)memcpy(ctx->tx_stream_buf, pdu->uds_data + ff_payload, remaining);
        ctx->tx_stream_data     = ctx->tx_stream_buf;
        ctx->tx_stream_remaining = remaining;
    }
    else
    {
        ctx->tx_stream_data     = NULL;
        ctx->tx_stream_remaining = 0;
    }

    ctx->tx_state         = LIN_TX_WAIT_FC;
    ctx->tx_seq_num       = 0;
    ctx->tx_stream_active = true;
    ctx->cf_in_block      = 0;

    return LIN_OK;
}

/* ======================================================================== *
 * lin_tx_next_cf_ctx — Obtain next CF in streaming transmission            *
 * ======================================================================== */

bool lin_tx_next_cf_ctx(lin_transport_ctx_t *ctx, lin_frame_t *cf_out)
{
    if (ctx == NULL || cf_out == NULL)
        return false;
    if (!ctx->tx_stream_active)
        return false;

    /* Check block size — if BS > 0 and we've reached the limit, pause */
    if (ctx->rx_fc_params.block_size > 0 &&
        ctx->cf_in_block >= ctx->rx_fc_params.block_size)
    {
        ctx->tx_state = LIN_TX_BLOCK_WAIT;
        return false;
    }

    /* Must be in CF state (after FC Continue received) */
    if (ctx->tx_state != LIN_TX_CF)
        return false;

    uint8_t cf_payload = LIN_CF_PAYLOAD_LEN;
    if (ctx->tx_stream_remaining < cf_payload)
        cf_payload = (uint8_t)ctx->tx_stream_remaining;

    ctx->tx_seq_num = (ctx->tx_seq_num % LIN_CF_SEQ_MAX) + 1;

    (void)memset(cf_out->data, 0, LIN_FRAME_SIZE);
    cf_out->data[0] = ctx->expected_nad;
    cf_out->data[1] = LIN_PCI_CF | ctx->tx_seq_num;
    (void)memcpy(&cf_out->data[2], ctx->tx_stream_data, cf_payload);

    ctx->tx_stream_data      += cf_payload;
    ctx->tx_stream_remaining -= cf_payload;
    ctx->cf_in_block++;

    if (ctx->tx_stream_remaining == 0)
    {
        ctx->tx_stream_active = false;
        ctx->tx_state         = LIN_TX_IDLE;
    }

    return true;
}

/* ======================================================================== *
 * lin_tx_has_more_ctx — Check if more CFs remain in stream                 *
 * ======================================================================== */

bool lin_tx_has_more_ctx(lin_transport_ctx_t *ctx)
{
    return ctx->tx_stream_active && (ctx->tx_state == LIN_TX_CF);
}

/* ======================================================================== *
 * lin_tx_abort_stream_ctx — Abort active streaming transmission            *
 * ======================================================================== */

void lin_tx_abort_stream_ctx(lin_transport_ctx_t *ctx)
{
    ctx->tx_stream_active    = false;
    ctx->tx_stream_data      = NULL;
    ctx->tx_stream_remaining = 0;
    ctx->tx_state            = LIN_TX_IDLE;
    ctx->cf_in_block         = 0;
    (void)memset(ctx->tx_stream_buf, 0, sizeof(ctx->tx_stream_buf));
}

/* ======================================================================== *
 * lin_stmin_to_ms — Convert raw STmin byte to milliseconds                 *
 * ======================================================================== *
 * Pure function — no context needed.
 */

uint16_t lin_stmin_to_ms(uint8_t stmin_raw)
{
    if (stmin_raw <= 0x7F)
        return (uint16_t)stmin_raw;

    if (stmin_raw >= 0xF1 && stmin_raw <= 0xF9)
        return 0;  /* 100-900 µs → 0 ms (integer truncated) */

    return 0;  /* reserved */
}

/* ======================================================================== *
 * lin_create_ctx — Allocate transport context from static pool             *
 * ======================================================================== */

lin_transport_ctx_t* lin_create_ctx(uint8_t nad)
{
    uint8_t i;
    for (i = 0; i < LIN_TRANSPORT_MAX_INSTANCES; i++)
    {
        if (!g_pool_used[i])
        {
            g_pool_used[i] = true;
            ctx_init(&g_pool[i], nad);
            return &g_pool[i];
        }
    }
    return NULL;
}

/* ======================================================================== *
 * lin_free_ctx — Return transport context to static pool                   *
 * ======================================================================== */

void lin_free_ctx(lin_transport_ctx_t *ctx)
{
    if (ctx == NULL)
        return;

    /* Check if ctx belongs to the pool */
    if (ctx >= g_pool && ctx < g_pool + LIN_TRANSPORT_MAX_INSTANCES)
    {
        uint8_t idx = (uint8_t)(ctx - g_pool);
        g_pool_used[idx] = false;
        ctx_init(ctx, 0);  /* wipe the slot */
    }
}

/* ======================================================================== *
 * Backward-Compatible Wrappers (delegate to g_default_ctx)                  *
 * ======================================================================== *
 * These functions preserve the original API signatures so existing callers
 * in sim/main_sim.c, test/test_lin_transport.c, and test/test_uds_integration.c
 * continue to work without modification.
 *
 * The g_default_ctx is initialised on first use and reset between tests via
 * lin_transport_reset().
 */

lin_status_t lin_tx_encode(const lin_diag_pdu_t *pdu,
                           lin_frame_t *frames,
                           uint8_t *frame_count,
                           uint8_t max_frames)
{
    return lin_tx_encode_ctx(&g_default_ctx, pdu, frames, frame_count, max_frames);
}

lin_status_t lin_rx_decode(const lin_frame_t *frame,
                           lin_diag_pdu_t *pdu)
{
    return lin_rx_decode_ctx(&g_default_ctx, frame, pdu);
}

void lin_transport_reset(void)
{
    lin_transport_reset_ctx(&g_default_ctx);
}

void lin_transport_set_nad(uint8_t nad)
{
    lin_transport_set_nad_ctx(&g_default_ctx, nad);
}

void lin_transport_set_accept_functional(bool enable)
{
    lin_transport_set_accept_functional_ctx(&g_default_ctx, enable);
}

lin_status_t lin_tx_encode_fc(uint8_t nad, const lin_fc_params_t *params, lin_frame_t *frame)
{
    return lin_tx_encode_fc_ctx(&g_default_ctx, nad, params, frame);
}

void lin_transport_get_fc_params(lin_fc_params_t *params)
{
    lin_transport_get_fc_params_ctx(&g_default_ctx, params);
}

void lin_transport_set_fc_params(uint8_t stmin, uint8_t bs)
{
    lin_transport_set_fc_params_ctx(&g_default_ctx, stmin, bs);
}

bool lin_transport_check_timeout(uint8_t timeout_type, uint32_t current_time)
{
    return lin_transport_check_timeout_ctx(&g_default_ctx, timeout_type, current_time);
}

void lin_transport_record_tx_time(uint32_t tx_time)
{
    lin_transport_record_tx_time_ctx(&g_default_ctx, tx_time);
}

void lin_transport_record_rx_time(uint32_t rx_time)
{
    lin_transport_record_rx_time_ctx(&g_default_ctx, rx_time);
}

void lin_transport_register_send_callback(lin_send_frame_fn_t cb)
{
    lin_transport_register_send_callback_ctx(&g_default_ctx, cb);
}

lin_status_t lin_tx_begin_stream(const lin_diag_pdu_t *pdu, lin_frame_t *ff_out)
{
    return lin_tx_begin_stream_ctx(&g_default_ctx, pdu, ff_out);
}

bool lin_tx_next_cf(lin_frame_t *cf_out)
{
    return lin_tx_next_cf_ctx(&g_default_ctx, cf_out);
}

bool lin_tx_has_more(void)
{
    return lin_tx_has_more_ctx(&g_default_ctx);
}

void lin_tx_abort_stream(void)
{
    lin_tx_abort_stream_ctx(&g_default_ctx);
}
