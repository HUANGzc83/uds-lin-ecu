/*
 * mock_uart.h
 * Mock UART HAL — PC simulation stub for unit testing
 *
 * Implements the hal_uart.h interface using a ring buffer to store
 * sent bytes for test assertions and a separate ring buffer to inject
 * received bytes.
 *
 * Include this file (via mock_all.h) in test translation units that
 * exercise UART / LIN physical layer code.
 */

#ifndef MOCK_UART_H
#define MOCK_UART_H

#include <stdint.h>
#include <stdbool.h>
#include "hal/hal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------
 * Mock control API
 * ----------------------------------------------------------------*/

/**
 * @brief Copy all bytes that have been sent since the last clear.
 *
 * @param[out] buf      Destination buffer (at least 256 bytes)
 * @param[out] len      Set to the number of bytes copied
 */
void mock_uart_get_sent_data(uint8_t *buf, uint16_t *len);

/**
 * @brief Inject data that hal_uart_receive() will return.
 *
 * Injected bytes are enqueued in an internal ring buffer.  Each call
 * to hal_uart_receive() consumes bytes from this buffer.
 *
 * @param[in]  data     Pointer to the data to enqueue
 * @param[in]  len      Number of bytes to enqueue
 */
void mock_uart_inject_rx_data(const uint8_t *data, uint16_t len);

/**
 * @brief Reset all mock UART state to default.
 *
 * Clears sent-buffer, RX injection buffer, callback pointer, IRQ
 * enabled flag, and all internal indices.  Does NOT free memory.
 */
void mock_uart_clear(void);

/* ------------------------------------------------------------------
 * HAL interface implementation (declarations match hal_uart.h)
 * ----------------------------------------------------------------*/

hal_status_t hal_uart_init(uint32_t baudrate);
hal_status_t hal_uart_send(const uint8_t *data, uint16_t len);
hal_status_t hal_uart_receive(uint8_t *data, uint16_t len, uint32_t timeout_ms);
void         hal_uart_set_rx_callback(hal_callback_t cb);
void         hal_uart_enable_irq(bool enable);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_UART_H */
