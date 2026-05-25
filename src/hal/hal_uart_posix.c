/*
 * hal_uart_posix.c
 * POSIX reference implementation of the UART HAL.
 *
 * Opens a serial device (/dev/ttyS0 by default) and performs
 * blocking send/receive via read(2)/write(2) with select(2)
 * time-out on receive.  Intended as a PC-side reference for MCU
 * HAL porting — not for production use on Linux (no termios
 * baud-rate configuration is performed; the device is used as-is
 * with whatever settings the kernel / init system has applied).
 *
 * Compile with UDS_PLATFORM_POSIX (set by hal_platform.h when
 * __linux__ is defined).
 */

#include "hal/hal_platform.h"

#ifdef UDS_PLATFORM_POSIX

/* _POSIX_C_SOURCE must be defined before any system header so that
 * read(2), write(2), select(2), open(2), fcntl(2) etc. are visible
 * under -std=c11 (which otherwise hides POSIX functions). */
#define _POSIX_C_SOURCE 200112L

#include "hal/hal_uart.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <errno.h>

/* Static state ---------------------------------------------------*/

/** File descriptor for the opened serial port, or -1 if not open. */
static int uart_fd = -1;

/** Registered RX callback (may be NULL — unused on POSIX). */
static hal_callback_t posix_rx_cb;

/** IRQ enabled flag (meaningful only on MCU; stored for API completeness). */
static bool posix_irq_enabled;

/** Whether hal_uart_init() has been called successfully. */
static bool posix_initialised;

/* HAL interface implementation ----------------------------------*/

hal_status_t hal_uart_init(uint32_t baudrate)
{
    int fd;
    (void)baudrate;  /* baud-rate configuration requires termios(3);
                      * omitted to keep dependency list minimal.  On a
                      * real MCU the peripheral clock divider is set here. */

    if (posix_initialised) {
        return HAL_OK;
    }

    if (baudrate == 0u) {
        return HAL_ERROR;
    }

    fd = open(UDS_HAL_UART_DEVICE, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd < 0) {
        return HAL_ERROR;
    }

    {
        int flags = fcntl(fd, F_GETFL);
        if (flags < 0) {
            close(fd);
            return HAL_ERROR;
        }
        flags &= ~O_NDELAY;
        if (fcntl(fd, F_SETFL, flags) < 0) {
            close(fd);
            return HAL_ERROR;
        }
    }

    uart_fd            = fd;
    posix_rx_cb        = NULL;
    posix_irq_enabled  = false;
    posix_initialised  = true;

    return HAL_OK;
}

hal_status_t hal_uart_send(const uint8_t *data, uint16_t len)
{
    ssize_t written;
    uint16_t total = 0u;

    if (!posix_initialised || uart_fd < 0) {
        return HAL_ERROR;
    }
    if (data == NULL) {
        return HAL_ERROR;
    }

    while (total < len) {
        written = write(uart_fd, data + total, (size_t)(len - total));
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return HAL_ERROR;
        }
        total += (uint16_t)written;
    }

    return HAL_OK;
}

hal_status_t hal_uart_receive(uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    fd_set read_fds;
    struct timeval tv;
    int sel_ret;
    ssize_t nread;
    uint16_t total = 0u;

    if (!posix_initialised || uart_fd < 0) {
        return HAL_ERROR;
    }
    if (data == NULL) {
        return HAL_ERROR;
    }
    if (len == 0u) {
        return HAL_OK;
    }

    while (total < len) {
        FD_ZERO(&read_fds);
        FD_SET(uart_fd, &read_fds);

        tv.tv_sec  = (long)(timeout_ms / 1000u);
        tv.tv_usec = (long)((timeout_ms % 1000u) * 1000u);

        sel_ret = select(uart_fd + 1, &read_fds, NULL, NULL, &tv);
        if (sel_ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            return HAL_ERROR;
        }
        if (sel_ret == 0) {
            return (total > 0u) ? HAL_OK : HAL_TIMEOUT;
        }

        nread = read(uart_fd, data + total, (size_t)(len - total));
        if (nread < 0) {
            if (errno == EINTR) {
                continue;
            }
            return HAL_ERROR;
        }
        if (nread == 0) {
            return HAL_ERROR;
        }

        total += (uint16_t)nread;
    }

    return HAL_OK;
}

void hal_uart_set_rx_callback(hal_callback_t cb)
{
    posix_rx_cb = cb;
}

void hal_uart_enable_irq(bool enable)
{
    posix_irq_enabled = enable;
}

#endif
