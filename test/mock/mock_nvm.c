/*
 * mock_nvm.c
 * Mock NVM implementation — RAM-backed PC simulation
 */

#include "mock_nvm.h"

#include <string.h> /* memset, memcpy */

/* ------------------------------------------------------------------
 * Mock state
 * ----------------------------------------------------------------*/

/** @brief The simulated NVM storage — initialised to 0xFF (erased). */
static uint8_t mock_nvm_ram[MOCK_NVM_SIZE];

/** @brief Highest written address + 1 (tracked for used-size queries). */
static uint32_t mock_max_addr;

/* ------------------------------------------------------------------
 * Mock control API
 * ----------------------------------------------------------------*/

uint32_t mock_nvm_get_used_size(void)
{
    int32_t i;

    /* Scan backwards from the end to find the last non-0xFF byte. */
    for (i = (int32_t)MOCK_NVM_SIZE - 1; i >= 0; i--) {
        if (mock_nvm_ram[(uint32_t)i] != 0xFFu) {
            return (uint32_t)(i + 1);
        }
    }

    return 0u;
}

void mock_nvm_clear(void)
{
    (void)memset(mock_nvm_ram, 0xFF, sizeof(mock_nvm_ram));
    mock_max_addr = 0u;
}

/* ------------------------------------------------------------------
 * Internal helpers
 * ----------------------------------------------------------------*/

/** @brief Validate that [address, address+len) is within bounds. */
static hal_status_t validate_range(uint32_t address, uint16_t len)
{
    if (address >= MOCK_NVM_SIZE) {
        return HAL_ERROR;
    }
    if ((uint32_t)address + (uint32_t)len > MOCK_NVM_SIZE) {
        return HAL_ERROR;
    }
    return HAL_OK;
}

/* ------------------------------------------------------------------
 * HAL interface implementation
 * ----------------------------------------------------------------*/

hal_status_t hal_nvm_read(uint32_t address, uint8_t *data, uint16_t len)
{
    if (data == NULL) {
        return HAL_ERROR;
    }
    if (validate_range(address, len) != HAL_OK) {
        return HAL_ERROR;
    }

    (void)memcpy(data, &mock_nvm_ram[address], len);
    return HAL_OK;
}

hal_status_t hal_nvm_write(uint32_t address, const uint8_t *data, uint16_t len)
{
    uint32_t end;

    if (data == NULL) {
        return HAL_ERROR;
    }
    if (validate_range(address, len) != HAL_OK) {
        return HAL_ERROR;
    }

    (void)memcpy(&mock_nvm_ram[address], data, len);

    /* Track highest written address. */
    end = address + len;
    if (end > mock_max_addr) {
        mock_max_addr = end;
    }

    return HAL_OK;
}

hal_status_t hal_nvm_erase(uint32_t address, uint16_t len)
{
    if (validate_range(address, len) != HAL_OK) {
        return HAL_ERROR;
    }

    (void)memset(&mock_nvm_ram[address], 0xFF, len);

    return HAL_OK;
}
