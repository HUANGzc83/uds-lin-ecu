/*
 * hal_cfg.h
 * HAL Configuration Macros
 *
 * Platform- / application-wide tuning constants.
 * Users may override defaults via compiler -D flags or by editing
 * this file before building.
 *
 * IMPORTANT: If a setting is defined externally (-D flag), this
 * header respects the external value.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------
 * Endianness
 * ----------------------------------------------------------------*/

/**
 * @brief Target byte order.
 *
 * Define as 1 for little-endian, 0 for big-endian.
 * Default: little-endian (most embedded targets).
 */
#ifndef HAL_ENDIAN_LITTLE
#define HAL_ENDIAN_LITTLE 1
#endif

/* ------------------------------------------------------------------
 * UART Buffer Sizes
 * ----------------------------------------------------------------*/

/**
 * @brief Size of the UART receive ring buffer.
 *
 * Must be a power of two for efficient modulo operations.
 */
#ifndef HAL_UART_RX_BUFFER_SIZE
#define HAL_UART_RX_BUFFER_SIZE 256
#endif

/**
 * @brief Size of the UART transmit ring buffer.
 *
 * Must be a power of two for efficient modulo operations.
 */
#ifndef HAL_UART_TX_BUFFER_SIZE
#define HAL_UART_TX_BUFFER_SIZE 256
#endif

#ifdef __cplusplus
}
#endif
