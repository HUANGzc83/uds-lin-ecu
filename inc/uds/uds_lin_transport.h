/*
 * uds_lin_transport.h
 * LIN Transport Layer Types & Protocol Definitions
 *
 * ISO 17987-3 LIN diagnostic frame layout, PCI (Protocol Control Information)
 * encoding, transport state machine definitions, and LIN-specific error codes
 * for UDS-over-LIN diagnostic communication.
 *
 * Wave 1 Task 4 — LIN Transport Layer Types & Protocol Definitions
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "uds_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== *
 * LIN Bus & Protocol Constants                                             *
 * ======================================================================== */

/** @brief Default LIN bus baud rate in bits per second */
#define LIN_BAUDRATE      19200

/** @brief Default node address for diagnostics (NAD = 0x01) */
#define LIN_NAD_DEFAULT   0x01

/** @brief Functional (broadcast) NAD address per ISO 14229-7 §12.1 */
#define LIN_NAD_FUNCTIONAL  0x7E

/** @brief LIN frame data field size in bytes (8 as per LIN 2.x / ISO 17987-3) */
#define LIN_FRAME_SIZE    8

/** @brief Maximum UDS data bytes in a single frame (8 - 1 NAD - 1 PCI) */
#define LIN_SF_MAX_LEN    6

/** @brief Maximum multi-frame payload length in bytes per ISO 17987-3 */
#define LIN_FF_MAX_LEN    4095

/* ======================================================================== *
 * LIN Frame & Diagnostic PDU Types                                        *
 * ======================================================================== */

/** @brief Raw LIN frame data field — 8 bytes as per LIN 2.x / ISO 17987-3 */
typedef struct {
    uint8_t data[LIN_FRAME_SIZE]; /**< @brief 8-byte LIN frame data field */
} lin_frame_t;

/** @brief LIN diagnostic PDU parsed from a raw LIN frame */
typedef struct {
    uint8_t  nad;      /**< @brief Node Address for Diagnostic (NAD, 1 byte) */
    uint8_t  pci;      /**< @brief Protocol Control Information (PCI, 1 byte) */
    uint8_t *uds_data; /**< @brief Pointer to the UDS message payload within the frame */
    uint16_t data_len; /**< @brief Length of the UDS payload data in bytes */
} lin_diag_pdu_t;

/* ======================================================================== *
 * PCI (Protocol Control Information) Encoding                              *
 * ======================================================================== *
 * The PCI byte is split into a 3-bit type field (bits 7:5) and a 5-bit
 * length / sequence field (bits 4:0).
 *
 *   Type   | Value | Description
 *   -------|-------|-----------------------------------
 *   0 (SF) | 0x00  | Single frame (UDS data fits in one LIN frame)
 *   1 (FF) | 0x20  | First frame of a multi-frame transmission
 *   2 (CF) | 0x40  | Consecutive frame (continuation of multi-frame)
 *
 * Masks are provided to extract the type and length fields individually.
 * ======================================================================== */

/** @brief PCI type mask — isolates the 3-bit frame type field (bits 7:5) */
#define LIN_PCI_MASK      0xE0

/** @brief PCI length mask — isolates the 5-bit length/sequence field (bits 4:0) */
#define LIN_PCI_LEN_MASK  0x1F

/** @brief PCI type value for Single Frame (SF) — type field = 0 */
#define LIN_PCI_SF        0x00

/** @brief PCI type value for First Frame (FF) — type field = 1 */
#define LIN_PCI_FF        0x20

/** @brief PCI type value for Consecutive Frame (CF) — type field = 2 */
#define LIN_PCI_CF        0x40

/** @brief PCI type value for Flow Control (FC) — type field = 3 per ISO 14229-7 */
#define LIN_PCI_FC        0x60

/* ======================================================================== *
 * Flow Control (FC) Parameters                                              *
 * ======================================================================== */

/** @brief FC Frame Status - Continue / Wait / Overflow */
typedef enum {
    LIN_FC_CONTINUE   = 0x00, /**< @brief Continue transmission (BS frames can be sent) */
    LIN_FC_WAIT      = 0x01, /**< @brief Wait - sender shall wait before next CF */
    LIN_FC_OVERFLOW  = 0x02  /**< @brief Overflow - receiver cannot accept more data */
} lin_fc_status_t;

/** @brief Flow Control parameters extracted from FC frame */
typedef struct {
    lin_fc_status_t  fc_status;   /**< @brief FC status (Continue/Wait/Overflow) */
    uint8_t          block_size;   /**< @brief Number of CFs that can be sent (BS) */
    uint8_t          stmin;        /**< @brief Minimum separation time between CFs (STmin) */
} lin_fc_params_t;

/* ======================================================================== *
 * Transport Layer Timeout Values (ISO 14229-7 / ISO 17987-3)                *
 * ======================================================================== */

/** @brief N_As - Time for master to transmit a frame (ms) */
#define LIN_TIMEOUT_N_AS      50

/** @brief N_Ar - Time for slave to transmit a frame (ms) */
#define LIN_TIMEOUT_N_AR      50

/** @brief N_Bs - Time between FF and FC (ms) */
#define LIN_TIMEOUT_N_BS      150

/** @brief N_Br - Time between FC and first CF (ms) */
#define LIN_TIMEOUT_N_BR      150

/** @brief N_Cr - Time between CFs (ms) */
#define LIN_TIMEOUT_N_CR      150

/** @brief N_WFTmax - Maximum number of wait frames allowed */
#define LIN_MAX_WFT_FRAMES    3

/** @brief Default STmin value (ms) - 10ms per ISO 17987-3 */
#define LIN_DEFAULT_STMIN     10

/** @brief Default BlockSize (0 = unlimited) */
#define LIN_DEFAULT_BS        0

/* ======================================================================== *
 * Standard LIN Diagnostic Frame Identifiers                                *
 * ======================================================================== */

/** @brief LIN diagnostic request frame ID (master request, NAD + PCI + UDS) */
#define LIN_DIAG_REQUEST_ID   0x3C

/** @brief LIN diagnostic response frame ID (slave response, NAD + PCI + UDS) */
#define LIN_DIAG_RESPONSE_ID  0x3D

/* ======================================================================== *
 * LIN Transport State Machine                                              *
 * ======================================================================== */

/** @brief LIN transport layer state machine states (transmit and receive) */
typedef enum {
    /* --- Transmit states --- */
    LIN_TX_IDLE     = 0,
    LIN_TX_SF,
    LIN_TX_FF,
    LIN_TX_WAIT_FC,     /**< @brief Waiting for Flow Control from receiver */
    LIN_TX_CF,
    LIN_TX_BLOCK_WAIT, /**< @brief Waiting between CF blocks (BS exceeded) */

    /* --- Receive states --- */
    LIN_RX_IDLE,
    LIN_RX_SF,
    LIN_RX_FF,
    LIN_RX_FC,         /**< @brief Received FC, processing parameters */
    LIN_RX_CF
} lin_transport_state_t;

/* ======================================================================== *
 * LIN Status / Error Codes                                                 *
 * ======================================================================== */

/** @brief LIN transport layer status and error codes */
typedef enum {
    LIN_OK            = 0,
    LIN_CRC_ERROR,
    LIN_TIMEOUT,
    LIN_BUS_ERROR,
    LIN_NAD_MISMATCH,
    LIN_PCI_ERROR,
    LIN_FC_WAIT_EXCEEDED,
    LIN_BUFFER_OVERFLOW,
    LIN_SEQ_ERROR
} lin_status_t;

/* ======================================================================== *
 * LIN Transport API Function Declarations                                  *
 * ======================================================================== */

/**
 * @brief Encode a UDS diagnostic PDU into one or more LIN frames.
 *
 * Handles both Single Frame (SF, UDS data <= 6 bytes) and multi-frame
 * (First Frame + Consecutive Frames, UDS data 7-4095 bytes) encoding
 * per ISO 17987-3.
 *
 * @param pdu         Pointer to the diagnostic PDU to encode.
 * @param frames      Output buffer for encoded LIN frames.
 * @param frame_count Output: number of frames written.
 * @param max_frames  Capacity of the frames buffer.
 * @return LIN_OK on success, or an appropriate error code.
 */
lin_status_t lin_tx_encode(const lin_diag_pdu_t *pdu,
                           lin_frame_t *frames,
                           uint8_t *frame_count,
                           uint8_t max_frames);

/**
 * @brief Decode a single LIN frame into a diagnostic PDU.
 *
 * For Single Frames (SF), the PDU is populated directly from the frame.
 * For multi-frame transmissions, state is tracked internally — call
 * repeatedly with First Frame (FF) then Consecutive Frames (CF) until
 * the full message is reassembled. The received NAD is checked against
 * the configured address and LIN_NAD_MISMATCH is returned on mismatch.
 *
 * @param frame  Pointer to the received LIN frame.
 * @param pdu    Output: decoded diagnostic PDU.
 * @return LIN_OK on success, or an appropriate error code.
 */
lin_status_t lin_rx_decode(const lin_frame_t *frame,
                           lin_diag_pdu_t *pdu);

/**
 * @brief Reset the LIN transport state machine.
 *
 * Clears TX/RX states, reassembly buffer, and sequence counters.
 * Should be called on bus wakeup, error recovery, or session start.
 */
void lin_transport_reset(void);

/**
 * @brief Set the expected NAD for receive-side address filtering.
 *
 * All received frames are checked against this NAD; a mismatch
 * returns LIN_NAD_MISMATCH.  Default is LIN_NAD_DEFAULT (0x01).
 *
 * @param nad  Node Address for Diagnostic to filter on
 */
void lin_transport_set_nad(uint8_t nad);

/**
 * @brief Enable or disable acceptance of the functional (broadcast) NAD 0x7E.
 *
 * Per ISO 14229-7 §12.1:
 *   - Diagnostic class III slave nodes shall support NAD 0x7E (default: true).
 *   - Diagnostic class II slave nodes shall ignore NAD 0x7E (set to false).
 *
 * @param enable  true to accept NAD 0x7E (Class III), false to reject (Class II)
 */
void lin_transport_set_accept_functional(bool enable);

/**
 * @brief Encode a Flow Control (FC) frame for multi-frame rate control.
 *
 * @param nad       Target NAD (source address for FC)
 * @param params    FC parameters (block size, STmin)
 * @param frame     Output: encoded LIN frame
 * @return LIN_OK on success, or LIN_PCI_ERROR on invalid parameters
 */
lin_status_t lin_tx_encode_fc(uint8_t nad, const lin_fc_params_t *params, lin_frame_t *frame);

/**
 * @brief Get current FC parameters (STmin, BS) from receiver side.
 *
 * @param params Output: current FC parameters
 */
void lin_transport_get_fc_params(lin_fc_params_t *params);

/**
 * @brief Set local FC parameters to use when acting as sender.
 *
 * @param stmin  Minimum separation time (ms, 0-127 or 0xF1-0xF9 = 100-900us)
 * @param bs     Block size (0 = unlimited)
 */
void lin_transport_set_fc_params(uint8_t stmin, uint8_t bs);

/**
 * @brief Check if transport timeout has occurred.
 *
 * @param timeout_type Timeout type to check (N_Bs, N_Cr, etc.)
 * @param current_time Current timestamp in milliseconds
 * @return true if timeout has occurred, false otherwise
 */
bool lin_transport_check_timeout(uint8_t timeout_type, uint32_t current_time);

/**
 * @brief Record frame transmission time for timeout tracking.
 *
 * @param tx_time Current timestamp in milliseconds
 */
void lin_transport_record_tx_time(uint32_t tx_time);

/**
 * @brief Record frame reception time for timeout tracking.
 *
 * @param rx_time Current timestamp in milliseconds
 */
void lin_transport_record_rx_time(uint32_t rx_time);

/**
 * @brief Callback type for sending LIN frames from the transport layer.
 *
 * When the LIN slave transport layer needs to send a Flow Control frame,
 * it invokes this callback.  The application layer provides the actual
 * UART/LIN hardware access.
 */
typedef void (*lin_send_frame_fn_t)(const lin_frame_t *frame);

/**
 * @brief Register a callback for sending LIN frames (e.g. Flow Control).
 *
 * Must be called before the first multi-frame reception.  If not set,
 * FC frames are silently suppressed and the sender may time out.
 *
 * @param cb  Callback function for sending a LIN frame
 */
void lin_transport_register_send_callback(lin_send_frame_fn_t cb);

/**
 * @brief Begin a streaming multi-frame transmission.
 *
 * Encodes and returns the First Frame (FF).  Subsequent Consecutive
 * Frames (CF) are obtained by calling lin_tx_next_cf().  This API
 * respects Block Size (BS) flow control — after BS CFs have been
 * produced, lin_tx_next_cf() returns false until another FC Continue
 * is received.
 *
 * @param pdu     Diagnostic PDU to transmit
 * @param ff_out  Output: the First Frame
 * @return LIN_OK on success, LIN_PCI_ERROR on invalid input
 */
lin_status_t lin_tx_begin_stream(const lin_diag_pdu_t *pdu, lin_frame_t *ff_out);

/**
 * @brief Obtain the next Consecutive Frame (CF) in a streaming transmission.
 *
 * Must be called after lin_tx_begin_stream() and only while
 * lin_tx_has_more() returns true.  Respects Block Size (BS):
 * after sending BS CFs, pauses and returns false until the next
 * FC Continue arrives (automatically detected via lin_rx_decode).
 *
 * @param cf_out  Output: the next Consecutive Frame
 * @return true if a CF was produced, false if done or waiting for FC
 */
bool lin_tx_next_cf(lin_frame_t *cf_out);

/**
 * @brief Check whether more CFs remain in the streaming transmission.
 *
 * @return true if lin_tx_next_cf() can produce more frames
 */
bool lin_tx_has_more(void);

/**
 * @brief Abort an active streaming transmission and reset TX state.
 */
void lin_tx_abort_stream(void);

/**
 * @brief Convert raw STmin byte to milliseconds per ISO 17987-2 dual encoding.
 *
 * @note Per ISO 17987-2, STmin uses dual encoding:
 *         0x00–0x7F = milliseconds (0–127 ms),
 *         0xF1–0xF9 = 100–900 µs,
 *         all other values are reserved and return 0.
 *
 * @param stmin_raw  Raw STmin byte from an FC frame (data[3]).
 * @return STmin value in milliseconds (integer truncated for µs values).
 */
uint16_t lin_stmin_to_ms(uint8_t stmin_raw);

/* ======================================================================== *
 * Multi-Instance Transport Context                                          *
 * ======================================================================== */

/**
 * @brief Maximum number of concurrent LIN transport instances.
 *
 * Controls the size of the static allocation pool for lin_transport_ctx_t.
 * Increase this value when multiple LIN nodes are managed simultaneously.
 * Default is 1 (single-instance, backward-compatible behavior).
 */
#ifndef LIN_TRANSPORT_MAX_INSTANCES
#define LIN_TRANSPORT_MAX_INSTANCES  1
#endif

/**
 * @brief LIN transport layer context structure.
 *
 * Encapsulates all per-instance transport state including TX/RX state
 * machines, reassembly buffer, flow control parameters, streaming
 * transmission state, and timing tracking.
 *
 * Instances are allocated from a static pool via lin_create_ctx()
 * and released via lin_free_ctx().
 */
typedef struct {
    /* --- State machines --- */
    lin_transport_state_t tx_state;         /**< @brief Transmit state machine */
    lin_transport_state_t rx_state;         /**< @brief Receive state machine */

    /* --- Address filtering --- */
    uint8_t  expected_nad;                  /**< @brief Expected NAD for receive filtering */
    bool     accept_functional;             /**< @brief Accept functional NAD 0x7E (Class III) */

    /* --- Reassembly buffers --- */
    uint8_t  rx_buffer[LIN_FF_MAX_LEN];     /**< @brief Multi-frame reassembly buffer (up to 4095 bytes) */
    uint8_t  sf_buffer[LIN_SF_MAX_LEN];     /**< @brief Single-frame payload buffer */
    uint16_t rx_buffer_len;                 /**< @brief Bytes written into reassembly buffer */
    uint16_t rx_expected_len;               /**< @brief Total expected multi-frame message length */

    /* --- Sequence tracking --- */
    uint8_t  rx_seq_num;                    /**< @brief Last received CF sequence number */
    uint8_t  tx_seq_num;                    /**< @brief Next TX CF sequence number (1..15, rolls over) */

    /* --- Flow control --- */
    lin_fc_params_t rx_fc_params;           /**< @brief Received FC parameters (STmin, BS) from receiver */
    lin_fc_params_t tx_fc_params;           /**< @brief Local FC parameters for TX side */

    /* --- Wait / block tracking --- */
    uint8_t  wft_counter;                   /**< @brief Wait frame counter (N_WFTmax) */
    uint8_t  cf_in_block;                   /**< @brief CF counter within current block (BS handling) */

    /* --- Streaming TX --- */
    const uint8_t *tx_stream_data;          /**< @brief Remaining data pointer for CF generation */
    uint16_t       tx_stream_remaining;     /**< @brief Remaining data length for streaming TX */
    bool           tx_stream_active;        /**< @brief Streaming transmission active flag */

    /* --- Callbacks --- */
    lin_send_frame_fn_t send_frame_cb;      /**< @brief Callback for sending LIN frames */

    /* --- Timing --- */
    uint32_t last_tx_time;                  /**< @brief Last TX timestamp (ms) for timeout tracking */
    uint32_t last_rx_time;                  /**< @brief Last RX timestamp (ms) for timeout tracking */
} lin_transport_ctx_t;

/**
 * @brief Create a LIN transport context from the static pool.
 *
 * Searches the static pool for an unused slot, initialises all state
 * to default values, sets the expected NAD, and returns a pointer to
 * the context.  The context is ready for use immediately.
 *
 * @param nad  Node Address for Diagnostic to assign to this instance.
 * @return Pointer to the initialised transport context, or NULL if all
 *         pool slots are in use.
 */
lin_transport_ctx_t* lin_create_ctx(uint8_t nad);

/**
 * @brief Free a LIN transport context and return it to the static pool.
 *
 * Resets the context state and marks the slot as available for reuse.
 * The pointer must not be dereferenced after this call.
 *
 * @param ctx  Transport context to free (must not be NULL).
 */
void lin_free_ctx(lin_transport_ctx_t *ctx);

#ifdef __cplusplus
}
#endif
