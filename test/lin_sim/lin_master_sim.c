/*
 * lin_master_sim.c
 * LIN Master Simulator Implementation — PC Test Harness
 *
 * Implements a LIN master node in software, using mock_uart for
 * frame-level I/O and the uds_lin_transport encode/decode API
 * for LIN diagnostic framing (SF, FF, CF per ISO 17987-3).
 *
 * Wave 3d Task 20 — LIN Master Simulator for PC Test Harness
 */

#include "lin_sim/lin_master_sim.h"
#include "hal/hal_uart.h"
#include <string.h>

/* ======================================================================== *
 * Internal State                                                           *
 * ======================================================================== */

/** @brief Whether lin_sim_init() has been called successfully. */
static bool g_sim_initialised;

/** @brief Configured baud rate (used for timing calculations). */
static uint32_t g_sim_baudrate;

/** @brief Maximum number of LIN frames for encoding (fits uint8_t max_frames). */
#define LIN_SIM_MAX_FRAMES 64u

/* ======================================================================== *
 * lin_sim_init — Initialise the master simulator                           *
 * ======================================================================== */

int lin_sim_init(uint8_t nad, uint32_t baudrate)
{
    if (baudrate == 0u)
        return -1;

    g_sim_baudrate   = baudrate;
    g_sim_initialised = true;

    /* Configure UART HAL at the requested baud rate */
    hal_uart_init(baudrate);

    /* Set the transport-layer NAD filter so lin_rx_decode accepts
     * responses addressed to this node */
    lin_transport_set_nad(nad);

    /* Ensure a clean transport state-machine before first use */
    lin_transport_reset();

    return 0;
}

/* ======================================================================== *
 * lin_sim_send_request — Encode and transmit a UDS diagnostic request      *
 * ======================================================================== */

int lin_sim_send_request(uint8_t nad, const uint8_t *data, uint16_t len)
{
    if (!g_sim_initialised)
        return -1;
    if (data == NULL || len == 0u)
        return -1;

    /* Build the diagnostic PDU */
    lin_diag_pdu_t pdu;
    pdu.nad      = nad;
    pdu.pci      = 0u;
    pdu.uds_data = (uint8_t *)data;
    pdu.data_len = (uint8_t)len;   /* safe: len <= LIN_FF_MAX_LEN (4095), but
                                    * transport's PDU uses uint8_t; caller is
                                    * responsible for not exceeding UINT8_MAX */

    /* Encode the PDU into one or more LIN frames.
     * max_frames is uint8_t, so cap at 64.  With 5 bytes per FF and
     * 6 per CF, 64 frames can hold ~383 bytes — enough for any
     * UDS request that fits within the PDU's uint8_t data_len. */
    lin_frame_t  frames[LIN_SIM_MAX_FRAMES];
    uint8_t      frame_count = 0u;

    /* Zero-initialise frame data so unused bytes are deterministic. */
    (void)memset(frames, 0, sizeof(frames));

    lin_status_t lst = lin_tx_encode(&pdu, frames, &frame_count,
                                     LIN_SIM_MAX_FRAMES);
    if (lst != LIN_OK)
        return -1;

    /* Transmit each frame over the UART */
    for (uint8_t i = 0u; i < frame_count; i++)
    {
        hal_status_t hst = hal_uart_send(frames[i].data, LIN_FRAME_SIZE);
        if (hst != HAL_OK)
            return -1;
    }

    return (int)frame_count;
}

/* ======================================================================== *
 * lin_sim_receive_response — Receive and decode a UDS diagnostic response  *
 * ======================================================================== */

int lin_sim_receive_response(uint8_t *buf, uint16_t max_len,
                             uint32_t timeout_ms)
{
    if (!g_sim_initialised)
        return -1;
    if (buf == NULL || max_len == 0u)
        return -1;

    /* Fresh state for each receive transaction */
    lin_transport_reset();

    /* ---- Read the first LIN frame ------------------------------------ */
    lin_frame_t      frame;
    lin_diag_pdu_t   pdu;

    hal_status_t hst = hal_uart_receive(frame.data, LIN_FRAME_SIZE, timeout_ms);
    if (hst != HAL_OK)
        return -1;

    lin_status_t lst = lin_rx_decode(&frame, &pdu);
    if (lst != LIN_OK)
        return -1;

    uint8_t pci_type = frame.data[1] & LIN_PCI_MASK;

    /* ---- Single Frame (SF) — copy payload and return --------------- */
    if (pci_type == LIN_PCI_SF)
    {
        if (pdu.data_len > max_len)
            return -1;

        (void)memcpy(buf, pdu.uds_data, pdu.data_len);
        return (int)pdu.data_len;
    }

    /* ---- First Frame (FF) — read CFs until reassembly is complete -- */
    if (pci_type == LIN_PCI_FF)
    {
        /* Parse the 12-bit total length from the FF header */
        uint16_t total_len = ((uint16_t)(frame.data[1] & 0x0Fu) << 8u)
                           | frame.data[2];

        /* Read consecutive frames until we have the full message */
        while (pdu.data_len < total_len)
        {
            hst = hal_uart_receive(frame.data, LIN_FRAME_SIZE, timeout_ms);
            if (hst != HAL_OK)
                return -1;

            lst = lin_rx_decode(&frame, &pdu);
            if (lst != LIN_OK)
                return -1;
        }

        /* Reassembly complete — copy the full payload */
        if (pdu.data_len > max_len)
            return -1;

        (void)memcpy(buf, pdu.uds_data, pdu.data_len);
        return (int)pdu.data_len;
    }

    /* Unexpected PCI type (should not happen after successful decode) */
    return -1;
}

/* ======================================================================== *
 * lin_sim_calc_frame_time_ms — Frame transmission time                     *
 * ======================================================================== */

uint32_t lin_sim_calc_frame_time_ms(uint32_t baudrate)
{
    if (baudrate == 0u)
        return 0u;

    /* LIN 2.x / ISO 17987-3 uses 8N1 framing = 10 bits per byte.
     * Transmission time for one 8-byte frame:
     *   bits = LIN_FRAME_SIZE * 10
     *   time_ms = ceil(bits * 1000 / baudrate)
     */
    uint32_t total_bits = (uint32_t)LIN_FRAME_SIZE * 10u;
    uint32_t time_ms    = (total_bits * 1000u) / baudrate;

    /* Round up if there is a remainder */
    if ((total_bits * 1000u) % baudrate != 0u)
        time_ms++;

    return time_ms;
}
