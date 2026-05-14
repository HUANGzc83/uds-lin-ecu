/*
 * uds_lin_transport.c
 * LIN Transport Layer Implementation — ISO 17987-3 Frame Encode/Decode
 *
 * Implements UDS-over-LIN diagnostic framing:
 *   - Single Frame (SF): UDS payload <= 6 bytes in one LIN frame
 *   - First Frame (FF): start of multi-frame transmission
 *   - Consecutive Frames (CF): continuation of multi-frame transmission
 *
 * Wave 2 Task 8 — LIN Transport Adapter
 */

#include "uds/uds_lin_transport.h"
#include <string.h>

/* ======================================================================== *
 * Transport Constants (private)                                             *
 * ======================================================================== */

/** @brief Maximum UDS payload bytes in a First Frame (8 - 1 NAD - 1 PCI - 1 length) */
#define LIN_FF_PAYLOAD_LEN   5

/** @brief Maximum UDS payload bytes in a Consecutive Frame (8 - 1 NAD - 1 PCI) */
#define LIN_CF_PAYLOAD_LEN   6

/** @brief Sequence number range (1..15 per ISO 17987-3) */
#define LIN_CF_SEQ_MAX       15

/* ======================================================================== *
 * Transport State (static, private to translation unit)                     *
 * ======================================================================== */

/** @brief Transmit state machine */
static lin_transport_state_t g_tx_state = LIN_TX_IDLE;

/** @brief Receive state machine */
static lin_transport_state_t g_rx_state = LIN_RX_IDLE;

/** @brief Configured NAD for receive-side address filtering */
static uint8_t g_expected_nad = LIN_NAD_DEFAULT;

/** @brief Multi-frame reassembly buffer (up to 4095 bytes per ISO 17987-3) */
static uint8_t g_rx_buffer[LIN_FF_MAX_LEN];

/** @brief Number of bytes currently written into the reassembly buffer */
static uint16_t g_rx_buffer_len;

/** @brief Total expected length for the multi-frame message being received */
static uint16_t g_rx_expected_len;

/** @brief Last received CF sequence number (0 before any CF received) */
static uint8_t g_rx_seq_num;

/** @brief Next TX CF sequence number (1..15, rolls over) */
static uint8_t g_tx_seq_num;

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
 * lin_tx_encode — Encode UDS PDU into one or more LIN frames               *
 * ======================================================================== */

lin_status_t lin_tx_encode(const lin_diag_pdu_t *pdu,
                           lin_frame_t *frames,
                           uint8_t *frame_count,
                           uint8_t max_frames)
{
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
        g_tx_state = LIN_TX_SF;

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

    g_tx_state = LIN_TX_FF;
    g_tx_seq_num = 0;

    /* Consecutive Frames */
    while (offset < data_len)
    {
        uint8_t remaining = (uint8_t)(data_len - offset);
        uint8_t cf_payload = LIN_CF_PAYLOAD_LEN;
        if (remaining < cf_payload)
            cf_payload = remaining;

        g_tx_seq_num = (g_tx_seq_num % LIN_CF_SEQ_MAX) + 1;

        frames[frame_idx].data[0] = nad;
        frames[frame_idx].data[1] = LIN_PCI_CF | g_tx_seq_num;
        (void)memcpy(&frames[frame_idx].data[2], src + offset, cf_payload);
        offset += cf_payload;
        frame_idx++;

        g_tx_state = LIN_TX_CF;
    }

    *frame_count = frame_idx;
    return LIN_OK;
}

/* ======================================================================== *
 * lin_rx_decode — Decode a single LIN frame into a UDS PDU                 *
 * ======================================================================== */

lin_status_t lin_rx_decode(const lin_frame_t *frame,
                           lin_diag_pdu_t *pdu)
{
    if (frame == NULL || pdu == NULL)
        return LIN_PCI_ERROR;

    /* --- NAD check --- */
    if (frame->data[0] != g_expected_nad)
        return LIN_NAD_MISMATCH;

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

        g_rx_state = LIN_RX_SF;
        return LIN_OK;
    }

    /* --- First Frame (FF) — start of multi-frame --- */
    if (pci_type == LIN_PCI_FF)
    {
        uint16_t total_len = ((uint16_t)(pci_byte & 0x0F) << 8) | frame->data[2];

        if (total_len < (LIN_SF_MAX_LEN + 1) || total_len > LIN_FF_MAX_LEN)
            return LIN_PCI_ERROR;

        g_rx_expected_len = total_len;
        g_rx_buffer_len   = 0;
        g_rx_seq_num      = 0;
        g_rx_state        = LIN_RX_FF;

        /* Copy first 5 payload bytes into reassembly buffer */
        uint8_t copy_len = LIN_FF_PAYLOAD_LEN;
        if (total_len < copy_len)
            copy_len = (uint8_t)total_len;
        (void)memcpy(g_rx_buffer, &frame->data[3], copy_len);
        g_rx_buffer_len = copy_len;

        pdu->nad      = frame->data[0];
        pdu->pci      = pci_byte;
        pdu->uds_data = g_rx_buffer;
        pdu->data_len = g_rx_buffer_len;

        /* Transition to expecting CF if more data remains */
        if (g_rx_buffer_len < g_rx_expected_len)
            g_rx_state = LIN_RX_CF;

        return LIN_OK;
    }

    /* --- Consecutive Frame (CF) --- */
    if (pci_type == LIN_PCI_CF)
    {
        if (g_rx_state != LIN_RX_CF)
            return LIN_PCI_ERROR;

        uint8_t seq = pci_byte & LIN_PCI_LEN_MASK;
        uint8_t expected_seq = (uint8_t)((g_rx_seq_num % LIN_CF_SEQ_MAX) + 1);

        if (seq != expected_seq)
            return LIN_PCI_ERROR;

        g_rx_seq_num = seq;

        uint16_t remaining = g_rx_expected_len - g_rx_buffer_len;
        uint8_t copy_len = LIN_CF_PAYLOAD_LEN;
        if (copy_len > remaining)
            copy_len = (uint8_t)remaining;

        (void)memcpy(g_rx_buffer + g_rx_buffer_len, &frame->data[2], copy_len);
        g_rx_buffer_len += copy_len;

        pdu->nad      = frame->data[0];
        pdu->pci      = pci_byte;
        pdu->uds_data = g_rx_buffer;
        pdu->data_len = g_rx_buffer_len;

        if (g_rx_buffer_len >= g_rx_expected_len)
        {
            g_rx_state = LIN_RX_IDLE;
            pdu->data_len = g_rx_expected_len;
        }

        return LIN_OK;
    }

    /* --- Unrecognized PCI type --- */
    return LIN_PCI_ERROR;
}

/* ======================================================================== *
 * lin_transport_reset — Clear transport state machine                      *
 * ======================================================================== */

void lin_transport_reset(void)
{
    g_tx_state       = LIN_TX_IDLE;
    g_rx_state       = LIN_RX_IDLE;
    g_rx_buffer_len  = 0;
    g_rx_expected_len = 0;
    g_rx_seq_num     = 0;
    g_tx_seq_num     = 0;
    (void)memset(g_rx_buffer, 0, sizeof(g_rx_buffer));
}

void lin_transport_set_nad(uint8_t nad)
{
    g_expected_nad = nad;
}
