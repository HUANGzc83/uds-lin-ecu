/*
 * hal_timer.h
 * Timer Abstraction Interface
 *
 * Single-shot timer abstraction used for protocol time-outs and
 * scheduling. MCU-specific back-ends shall implement a free-running
 * hardware timer counting in milliseconds.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "hal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise the timer hardware.
 *
 * Prepares a hardware timer channel for single-shot operation.
 * Must be called once before hal_timer_start().
 */
void hal_timer_init(void);

/**
 * @brief Start (or restart) a single-shot timer.
 *
 * The timer counts down from @p timeout_ms.  If a timer is already
 * running it is reset to the new value.
 *
 * @param[in]  timeout_ms   Time-out value in milliseconds
 * @return HAL_OK           Timer started
 * @return HAL_ERROR        Start failed (e.g. out of timer channels)
 */
hal_status_t hal_timer_start(uint32_t timeout_ms);

/**
 * @brief Stop the currently running timer.
 *
 * After this call hal_timer_is_expired() returns false.
 */
void hal_timer_stop(void);

/**
 * @brief Check whether the timer has expired.
 *
 * @return true             Timer has reached zero
 * @return false            Timer is still counting (or was never started)
 */
bool hal_timer_is_expired(void);

/**
 * @brief Get the time remaining before expiry.
 *
 * @return Remaining time in milliseconds (0 if expired or not started)
 */
uint32_t hal_timer_get_remaining_ms(void);

#ifdef __cplusplus
}
#endif
