/*
 * mock_uart.c
 * Mock UART implementation — ring-buffer based PC simulation
 */

#include "mock_uart.h"

#include <string.h> /* memcpy */

/* ------------------------------------------------------------------
 * Internal constants
 * ----------------------------------------------------------------*/

/** @brief Size of the sent-data ring buffer (must be power of two). */
#define MOCK_UART_BUF_SIZE 256u

/** @brief Mask for ring buffer index wrap-around. */
#define MOCK_UART_BUF_MASK (MOCK_UART_BUF_SIZE - 1u)

/* ------------------------------------------------------------------
 * Mock state
 * ----------------------------------------------------------------*/

/** Ring buffer of bytes sent via hal_uart_send(). */
static uint8_t  mock_sent_buf[MOCK_UART_BUF_SIZE];
static uint16_t mock_sent_head;   /**< Producer index for hal_uart_send() */
static uint16_t mock_sent_tail;   /**< Consumer index for mock_uart_get_sent_data() */

/** Ring buffer of bytes injected for hal_uart_receive(). */
static uint8_t  mock_rx_buf[MOCK_UART_BUF_SIZE];
static uint16_t mock_rx_head;     /**< Producer index for mock_uart_inject_rx_data() */
static uint16_t mock_rx_tail;     /**< Consumer index for hal_uart_receive() */

/** Registered RX callback (may be NULL). */
static hal_callback_t mock_rx_cb;

/** IRQ enabled flag. */
static bool mock_irq_enabled;

/** Baudrate configured via hal_uart_init(). */
static uint32_t mock_baudrate;

/** Whether hal_uart_init() has been called. */
static bool mock_initialised;

/* ------------------------------------------------------------------
 * Internal helpers
 * ----------------------------------------------------------------*/

/** @brief Number of bytes currently in the sent ring buffer. */
static uint16_t mock_sent_count(void)
{
    return (uint16_t)(mock_sent_head - mock_sent_tail);
}

/** @brief Number of free slots in the sent ring buffer. */
static uint16_t mock_sent_free(void)
{
    return MOCK_UART_BUF_SIZE - mock_sent_count();
}

/** @brief Number of bytes currently in the RX ring buffer. */
static uint16_t mock_rx_count(void)
{
    return (uint16_t)(mock_rx_head - mock_rx_tail);
}

/** @brief Number of free slots in the RX ring buffer. */
static uint16_t mock_rx_free(void)
{
    return MOCK_UART_BUF_SIZE - mock_rx_count();
}

/* ------------------------------------------------------------------
 * Mock control API
 * ----------------------------------------------------------------*/

void mock_uart_get_sent_data(uint8_t *buf, uint16_t *len)
{
    uint16_t count;
    uint16_t i;
    uint16_t idx;

    count = mock_sent_count();
    if (count > 0u) {
        for (i = 0u; i < count; i++) {
            idx = (mock_sent_tail + i) & MOCK_UART_BUF_MASK;
            buf[i] = mock_sent_buf[idx];
        }
    }
    *len = count;
}

void mock_uart_inject_rx_data(const uint8_t *data, uint16_t len)
{
    uint16_t i;

    for (i = 0u; i < len; i++) {
        /* Silently discard if buffer is full (test should size correctly). */
        if (mock_rx_free() > 0u) {
            mock_rx_buf[mock_rx_head & MOCK_UART_BUF_MASK] = data[i];
            mock_rx_head++;
        }
    }
}

void mock_uart_clear(void)
{
    mock_sent_head  = 0u;
    mock_sent_tail  = 0u;
    mock_rx_head    = 0u;
    mock_rx_tail    = 0u;
    mock_rx_cb      = NULL;
    mock_irq_enabled = false;
    mock_baudrate   = 0u;
    mock_initialised = false;

    /* Wipe buffers to catch uninitialised reads. */
    (void)memset(mock_sent_buf, 0xAA, sizeof(mock_sent_buf));
    (void)memset(mock_rx_buf,   0xBB, sizeof(mock_rx_buf));
}

/* ------------------------------------------------------------------
 * HAL interface implementation
 * ----------------------------------------------------------------*/

hal_status_t hal_uart_init(uint32_t baudrate)
{
    if (baudrate == 0u) {
        return HAL_ERROR;
    }

    mock_baudrate    = baudrate;
    mock_initialised = true;

    /* Keep existing ring buffer contents; test may inject before init. */

    return HAL_OK;
}

hal_status_t hal_uart_send(const uint8_t *data, uint16_t len)
{
    uint16_t i;

    if (!mock_initialised) {
        return HAL_ERROR;
    }
    if (data == NULL) {
        return HAL_ERROR;
    }

    for (i = 0u; i < len; i++) {
        /* Silently discard if buffer is full. */
        if (mock_sent_free() > 0u) {
            mock_sent_buf[mock_sent_head & MOCK_UART_BUF_MASK] = data[i];
            mock_sent_head++;
        }
    }

    return HAL_OK;
}

hal_status_t hal_uart_receive(uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    uint16_t i;
    uint16_t available;
    uint16_t to_copy;

    (void)timeout_ms; /* not needed — RX buffer is synchronous */

    if (!mock_initialised) {
        return HAL_ERROR;
    }
    if (data == NULL) {
        return HAL_ERROR;
    }

    available = mock_rx_count();

    if (available < len) {
        return HAL_TIMEOUT;
    }

    to_copy = (len < available) ? len : available;

    for (i = 0u; i < to_copy; i++) {
        data[i] = mock_rx_buf[mock_rx_tail & MOCK_UART_BUF_MASK];
        mock_rx_tail++;
    }

    return (to_copy == len) ? HAL_OK : HAL_TIMEOUT;
}

void hal_uart_set_rx_callback(hal_callback_t cb)
{
    mock_rx_cb = cb;
}

void hal_uart_enable_irq(bool enable)
{
    mock_irq_enabled = enable;
}
