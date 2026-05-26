/*
 * hal_nvm.h
 * Non-Volatile Memory Abstraction Interface
 *
 * Provides read / write / erase access to the on-chip NVM (e.g.
 * Flash or EEPROM).  MCU-specific back-ends shall handle sector
 * boundaries, write-before-erase constraints, and unlocking.
 */

#pragma once

#include <stdint.h>
#include "hal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read a block of data from NVM.
 *
 * @param[in]  address      Starting byte address in NVM
 * @param[out] data         Pointer to the destination buffer
 * @param[in]  len          Number of bytes to read
 * @return HAL_OK           Read completed
 * @return HAL_ERROR        Read failed (e.g. invalid address range)
 */
hal_status_t hal_nvm_init(void);

hal_status_t hal_nvm_read(uint32_t address, uint8_t *data, uint16_t len);

/**
 * @brief Write a block of data to NVM.
 *
 * The memory area should have been erased before writing (see
 * hal_nvm_erase).  Behaviour is undefined when writing to a
 * non-erased location on Flash-based back-ends.
 *
 * @param[in]  address      Starting byte address in NVM
 * @param[in]  data         Pointer to the source buffer
 * @param[in]  len          Number of bytes to write
 * @return HAL_OK           Write completed
 * @return HAL_ERROR        Write failed (e.g. alignment, locked region)
 */
hal_status_t hal_nvm_write(uint32_t address, const uint8_t *data, uint16_t len);

/**
 * @brief Erase a region of NVM.
 *
 * The actual erased size may be rounded up to the next sector /
 * page boundary depending on the back-end.
 *
 * @param[in]  address      Starting byte address in NVM
 * @param[in]  len          Number of bytes to erase (may be rounded up)
 * @return HAL_OK           Erase completed
 * @return HAL_ERROR        Erase failed
 */
hal_status_t hal_nvm_erase(uint32_t address, uint16_t len);

#ifdef __cplusplus
}
#endif
