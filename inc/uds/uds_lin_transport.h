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
    uint8_t  data_len; /**< @brief Length of the UDS payload data in bytes */
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
    LIN_TX_IDLE    = 0, /**< @brief Transmitter idle — no transmission in progress */
    LIN_TX_SF,          /**< @brief Transmitter sending a single frame */
    LIN_TX_FF,          /**< @brief Transmitter sending the first frame of a multi-frame message */
    LIN_TX_CF,          /**< @brief Transmitter sending consecutive frames */
    LIN_TX_WAIT_BS,     /**< @brief Transmitter waiting for block size acknowledgment */

    /* --- Receive states --- */
    LIN_RX_IDLE,        /**< @brief Receiver idle — no reception in progress */
    LIN_RX_SF,          /**< @brief Receiver expecting a single frame */
    LIN_RX_FF,          /**< @brief Receiver expecting the first frame of a multi-frame message */
    LIN_RX_CF           /**< @brief Receiver expecting consecutive frames */
} lin_transport_state_t;

/* ======================================================================== *
 * LIN Status / Error Codes                                                 *
 * ======================================================================== */

/** @brief LIN transport layer status and error codes */
typedef enum {
    LIN_OK           = 0, /**< @brief Operation completed successfully */
    LIN_CRC_ERROR,        /**< @brief CRC mismatch detected in received frame */
    LIN_TIMEOUT,          /**< @brief LIN bus timeout occurred */
    LIN_BUS_ERROR,        /**< @brief LIN bus error (bit error, framing error, etc.) */
    LIN_NAD_MISMATCH,     /**< @brief Received NAD does not match expected node address */
    LIN_PCI_ERROR         /**< @brief PCI protocol violation (invalid type, length, or sequence) */
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

#ifdef __cplusplus
}
#endif
