/*
 * mock_timer.h
 * Mock Timer HAL — PC simulation stub for unit testing
 *
 * Implements the hal_timer.h interface using a simulated time that
 * the test advances explicitly via mock_timer_advance_ms().
 *
 * Include this file (via mock_all.h) in test translation units that
 * exercise protocol time-out logic.
 */

#ifndef MOCK_TIMER_H
#define MOCK_TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include "hal/hal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------
 * Mock control API
 * ----------------------------------------------------------------*/

/**
 * @brief Advance the simulated clock by a given number of milliseconds.
 *
 * After this call the internal counter (mock_now) is incremented,
 * which may cause the running timer to expire.
 *
 * @param[in]  ms     Number of milliseconds to advance
 */
void mock_timer_advance_ms(uint32_t ms);

/**
 * @brief Reset the simulated clock and stop any running timer.
 */
void mock_timer_clear(void);

/* ------------------------------------------------------------------
 * HAL interface implementation (declarations match hal_timer.h)
 * ----------------------------------------------------------------*/

void         hal_timer_init(void);
hal_status_t hal_timer_start(uint32_t timeout_ms);
void         hal_timer_stop(void);
bool         hal_timer_is_expired(void);
uint32_t     hal_timer_get_remaining_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_TIMER_H */
