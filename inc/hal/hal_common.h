/*
 * hal_common.h
 * HAL Shared Types — status codes and callback type
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief HAL operation status codes.
 */
typedef enum {
    HAL_OK      =  0, /**< Operation completed successfully                    */
    HAL_ERROR   = -1, /**< Unspecified error                                   */
    HAL_TIMEOUT = -2, /**< Operation timed out                                 */
    HAL_BUSY    = -3  /**< Resource temporarily busy, retry later               */
} hal_status_t;

/**
 * @brief Generic HAL callback signature.
 */
typedef void (*hal_callback_t)(void);

#ifdef __cplusplus
}
#endif
