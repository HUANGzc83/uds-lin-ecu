/*
 * hal_nvm_posix.c
 * POSIX reference implementation of the NVM HAL.
 *
 * Backs the non-volatile memory abstraction with a regular file
 * (uds_nvm.bin by default).  Read/write operations use pread(2) /
 * pwrite(2) at the byte offset given by @p address.  Erase fills
 * the region with 0xFF to simulate Flash erase behaviour.
 *
 * Compile with UDS_PLATFORM_POSIX (set by hal_platform.h when
 * __linux__ is defined).
 */

#include "hal/hal_platform.h"

#ifdef UDS_PLATFORM_POSIX

/* _POSIX_C_SOURCE must be defined before any system header so that
 * pread(2), pwrite(2), lseek(2), open(2) etc. are visible under
 * -std=c11 (which otherwise hides POSIX functions). */
#define _POSIX_C_SOURCE 200809L

#include "hal/hal_nvm.h"
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

/* Configuration --------------------------------------------------*/

/**
 * @brief Path to the file used as simulated NVM storage.
 *
 * Override at build time with -DUDS_NVM_FILE_PATH=\"/tmp/uds_nvm.bin\".
 */
#ifndef UDS_NVM_FILE_PATH
#   define UDS_NVM_FILE_PATH  "uds_nvm.bin"
#endif

/** @brief Maximum simulated NVM size in bytes (64 kB). */
#define POSIX_NVM_SIZE  (64u * 1024u)

/* Static state ---------------------------------------------------*/

/** File descriptor for the NVM file, or -1 if not open. */
static int nvm_fd = -1;

/** Whether hal_nvm_init() has been called. */
static bool nvm_initialised;

/* Internal helpers -----------------------------------------------*/

/**
 * @brief Validate that [address, address+len) fits within the
 *        simulated NVM file.
 */
static hal_status_t validate_range(uint32_t address, uint16_t len)
{
    if (address >= POSIX_NVM_SIZE) {
        return HAL_ERROR;
    }
    if ((uint32_t)address + (uint32_t)len > POSIX_NVM_SIZE) {
        return HAL_ERROR;
    }
    return HAL_OK;
}

/**
 * @brief Ensure the NVM file is at least POSIX_NVM_SIZE bytes.
 *
 * On a real MCU this corresponds to verifying the Flash/EEPROM
 * sector exists and is accessible.
 */
static hal_status_t ensure_file_size(void)
{
    off_t current_end;

    current_end = lseek(nvm_fd, 0, SEEK_END);
    if (current_end < 0) {
        return HAL_ERROR;
    }

    if ((uint64_t)current_end < POSIX_NVM_SIZE) {
    static const uint8_t erased_page[4096u];
        uint64_t remaining = POSIX_NVM_SIZE - (uint64_t)current_end;
        ssize_t written;

        while (remaining > 0u) {
            size_t chunk = (remaining > sizeof(erased_page))
                               ? sizeof(erased_page)
                               : (size_t)remaining;
            written = write(nvm_fd, erased_page, chunk);
            if (written < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return HAL_ERROR;
            }
            remaining -= (uint64_t)written;
        }
    }

    return HAL_OK;
}

/* HAL interface implementation ----------------------------------*/

hal_status_t hal_nvm_init(void)
{
    int fd;

    if (nvm_initialised) {
        return HAL_OK;
    }

    fd = open(UDS_NVM_FILE_PATH, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        return HAL_ERROR;
    }

    nvm_fd = fd;

    if (ensure_file_size() != HAL_OK) {
        close(nvm_fd);
        nvm_fd = -1;
        return HAL_ERROR;
    }

    nvm_initialised = true;
    return HAL_OK;
}

hal_status_t hal_nvm_read(uint32_t address, uint8_t *data, uint16_t len)
{
    ssize_t nread;
    uint16_t total = 0u;

    if (!nvm_initialised || nvm_fd < 0) {
        return HAL_ERROR;
    }
    if (data == NULL) {
        return HAL_ERROR;
    }
    if (validate_range(address, len) != HAL_OK) {
        return HAL_ERROR;
    }

    while (total < len) {
        nread = pread(nvm_fd, data + total, (size_t)(len - total),
                      (off_t)(address + total));
        if (nread < 0) {
            if (errno == EINTR) {
                continue;
            }
            return HAL_ERROR;
        }
        if (nread == 0) {
            memset(data + total, 0x00, (size_t)(len - total));
            break;
        }
        total += (uint16_t)nread;
    }

    return HAL_OK;
}

hal_status_t hal_nvm_write(uint32_t address, const uint8_t *data, uint16_t len)
{
    ssize_t nwritten;
    uint16_t total = 0u;

    if (!nvm_initialised || nvm_fd < 0) {
        return HAL_ERROR;
    }
    if (data == NULL) {
        return HAL_ERROR;
    }
    if (validate_range(address, len) != HAL_OK) {
        return HAL_ERROR;
    }

    while (total < len) {
        nwritten = pwrite(nvm_fd, data + total, (size_t)(len - total),
                          (off_t)(address + total));
        if (nwritten < 0) {
            if (errno == EINTR) {
                continue;
            }
            return HAL_ERROR;
        }
        total += (uint16_t)nwritten;
    }

    return HAL_OK;
}

hal_status_t hal_nvm_erase(uint32_t address, uint16_t len)
{
    uint32_t i;
    uint8_t erase_byte = 0xFFu;

    if (!nvm_initialised || nvm_fd < 0) {
        return HAL_ERROR;
    }
    if (validate_range(address, len) != HAL_OK) {
        return HAL_ERROR;
    }

    for (i = 0u; i < len; i++) {
        ssize_t nwritten;
        off_t offset = (off_t)(address + i);

        do {
            nwritten = pwrite(nvm_fd, &erase_byte, 1u, offset);
            if (nwritten < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return HAL_ERROR;
            }
        } while (nwritten == 0);
    }

    return HAL_OK;
}

#endif
