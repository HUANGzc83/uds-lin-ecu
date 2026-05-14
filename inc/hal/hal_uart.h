/*
 * hal_uart.h
 * UART / LIN Abstraction Interface
 *
 * Pure abstract HAL for UART (used as physical layer for LIN).
 * All functions shall be implemented by MCU-specific back-ends.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "hal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise the UART (LIN) peripheral.
 *
 * Configures baud rate, 8N1 frame format, and enables the peripheral.
 * Must be called once before any other UART HAL function.
 *
 * @param[in]  baudrate     Desired baud rate in bits per second
 * @return HAL_OK           Peripheral initialised successfully
 * @return HAL_ERROR        Initialisation failed (e.g. invalid baudrate)
 */
hal_status_t hal_uart_init(uint32_t baudrate);

/**
 * @brief Transmit a block of data over UART (blocking).
 *
 * Waits until all bytes have been shifted out or an error occurs.
 *
 * @param[in]  data         Pointer to the data buffer to send
 * @param[in]  len          Number of bytes to transmit
 * @return HAL_OK           Transmission completed
 * @return HAL_ERROR        Transmit error (e.g. framing error, overrun)
 */
hal_status_t hal_uart_send(const uint8_t *data, uint16_t len);

/**
 * @brief Receive a block of data over UART (blocking with timeout).
 *
 * Waits for len bytes to arrive or returns HAL_TIMEOUT if the
 * specified time elapses before the transfer is complete.
 *
 * @param[out] data         Pointer to the receive buffer
 * @param[in]  len          Number of bytes to receive
 * @param[in]  timeout_ms   Maximum wait time in milliseconds
 * @return HAL_OK           Reception completed
 * @return HAL_TIMEOUT      Timeout expired before all bytes received
 * @return HAL_ERROR        Receive error
 */
hal_status_t hal_uart_receive(uint8_t *data, uint16_t len, uint32_t timeout_ms);

/**
 * @brief Register a callback for incoming UART data.
 *
 * The callback is invoked from interrupt context when a character is
 * received. Only one callback may be active at a time; passing NULL
 * disables the current callback.
 *
 * @param[in]  cb           Callback function pointer (or NULL to disable)
 */
void hal_uart_set_rx_callback(hal_callback_t cb);

/**
 * @brief Enable or disable the UART receive interrupt.
 *
 * @param[in]  enable       true  = enable IRQ
 *                          false = disable IRQ
 */
void hal_uart_enable_irq(bool enable);

#ifdef __cplusplus
}
#endif
