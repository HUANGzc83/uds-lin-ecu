/*
 * lin_master_sim.h
 * LIN Master Simulator for PC Test Harness
 *
 * Simulates a LIN master node for testing UDS-over-LIN diagnostic
 * communication on a PC.  Uses mock_uart for frame I/O and the
 * uds_lin_transport encode/decode API for LIN diagnostic framing.
 *
 * Wave 3d Task 20 — LIN Master Simulator for PC Test Harness
 */

#ifndef LIN_MASTER_SIM_H
#define LIN_MASTER_SIM_H

#include <stdint.h>
#include <stdbool.h>
#include "uds/uds_lin_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== *
 * Initialisation & Configuration                                           *
 * ======================================================================== */

/**
 * @brief Initialise the LIN master simulator.
 *
 * Configures the node address (NAD) and baud rate, initialises the
 * underlying UART HAL, and resets the LIN transport state machine.
 * Must be called before any other lin_sim_* function.
 *
 * @param nad       Node Address for Diagnostic (default: LIN_NAD_DEFAULT)
 * @param baudrate  LIN bus baud rate in bits per second (default: LIN_BAUDRATE)
 * @return 0 on success, -1 on invalid parameter (e.g. zero baudrate)
 */
int lin_sim_init(uint8_t nad, uint32_t baudrate);

/* ======================================================================== *
 * Diagnostic Request / Response                                            *
 * ======================================================================== */

/**
 * @brief Send a UDS diagnostic request as one or more LIN frames.
 *
 * Encodes the UDS payload into LIN frames (Single Frame or multi-frame
 * FF/CF sequence via lin_tx_encode()) and transmits each frame over
 * the mock UART interface (hal_uart_send()).
 *
 * @param nad   Node Address for Diagnostic (placed in each frame)
 * @param data  Pointer to the UDS request payload bytes
 * @param len   Number of UDS payload bytes (max LIN_FF_MAX_LEN)
 * @return Number of LIN frames transmitted on success,
 *         -1 on encoding or transmit error
 */
int lin_sim_send_request(uint8_t nad, const uint8_t *data, uint16_t len);

/**
 * @brief Receive a UDS diagnostic response via LIN frames.
 *
 * Reads raw LIN frames from the UART (hal_uart_receive()), decodes
 * them via lin_rx_decode(), and copies the reassembled UDS payload
 * into the caller's buffer.  Handles both Single Frame (SF) and
 * multi-frame (FF + CF) responses transparently.
 *
 * @param[out] buf         Output buffer for the UDS response payload
 * @param[in]  max_len     Capacity of the output buffer in bytes
 * @param[in]  timeout_ms  Maximum time to wait for the first frame
 * @return Number of UDS payload bytes received on success,
 *         -1 on timeout, protocol error, or buffer overflow
 */
int lin_sim_receive_response(uint8_t *buf, uint16_t max_len,
                             uint32_t timeout_ms);

/* ======================================================================== *
 * Utility / Timing Helpers                                                 *
 * ======================================================================== */

/**
 * @brief Calculate the transmission time of one LIN frame at a given baud.
 *
 * Uses 8N1 framing (10 bits per byte) and LIN_FRAME_SIZE (8 data bytes).
 *
 * @param baudrate  LIN bus baud rate in bits per second
 * @return Frame transmission time in milliseconds (rounded up)
 */
uint32_t lin_sim_calc_frame_time_ms(uint32_t baudrate);

#ifdef __cplusplus
}
#endif

#endif /* LIN_MASTER_SIM_H */
