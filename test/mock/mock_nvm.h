/*
 * mock_nvm.h
 * Mock NVM HAL — PC simulation stub for unit testing
 *
 * Implements the hal_nvm.h interface using a static 4096-byte RAM
 * array.  The "erased" state is 0xFF (matching typical NOR Flash).
 *
 * Include this file (via mock_all.h) in test translation units that
 * exercise NVM read / write / erase operations.
 */

#ifndef MOCK_NVM_H
#define MOCK_NVM_H

#include <stdint.h>
#include <stdbool.h>
#include "hal/hal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Total simulated NVM size in bytes. */
#define MOCK_NVM_SIZE 4096

/* ------------------------------------------------------------------
 * Mock control API
 * ----------------------------------------------------------------*/

/**
 * @brief Return the size of the written region (highest byte address + 1).
 *
 * Scans from the end of the mock NVM array toward the beginning and
 * returns the index of the last non-0xFF byte plus one.  Returns 0 if
 * the entire array is in erased state.
 *
 * @return Number of bytes that are logically "used".
 */
uint32_t mock_nvm_get_used_size(void);

/**
 * @brief Fill the entire mock NVM array with 0xFF (erased state).
 *
 * Also resets internal tracking (e.g. used_size hint).
 */
void mock_nvm_clear(void);

/* ------------------------------------------------------------------
 * HAL interface implementation (declarations match hal_nvm.h)
 * ----------------------------------------------------------------*/

hal_status_t hal_nvm_read(uint32_t address, uint8_t *data, uint16_t len);
hal_status_t hal_nvm_write(uint32_t address, const uint8_t *data, uint16_t len);
hal_status_t hal_nvm_erase(uint32_t address, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_NVM_H */
