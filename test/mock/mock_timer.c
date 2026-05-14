/*
 * mock_timer.c
 * Mock Timer implementation — simulated-clock based PC simulation
 */

#include "mock_timer.h"

/* ------------------------------------------------------------------
 * Mock state
 * ----------------------------------------------------------------*/

/** @brief Current simulated time in milliseconds (absolute). */
static uint32_t mock_now;

/** @brief Absolute mock-time value at which the timer was started. */
static uint32_t mock_start_at;

/** @brief Duration of the running timer in milliseconds. */
static uint32_t mock_duration;

/** @brief true while a timer is active. */
static bool mock_running;

/** @brief Whether hal_timer_init() has been called. */
static bool mock_initialised;

/* ------------------------------------------------------------------
 * Mock control API
 * ----------------------------------------------------------------*/

void mock_timer_advance_ms(uint32_t ms)
{
    mock_now += ms;
}

void mock_timer_clear(void)
{
    mock_now         = 0u;
    mock_start_at    = 0u;
    mock_duration    = 0u;
    mock_running     = false;
    mock_initialised = false;
}

/* ------------------------------------------------------------------
 * Internal helpers
 * ----------------------------------------------------------------*/

/** @brief true if the running timer has expired. */
static bool mock_is_expired_raw(void)
{
    if (!mock_running) {
        return false;
    }
    /* Handle wraparound: expired if mock_now >= expiry */
    return (mock_now - mock_start_at) >= mock_duration;
}

/* ------------------------------------------------------------------
 * HAL interface implementation
 * ----------------------------------------------------------------*/

void hal_timer_init(void)
{
    mock_initialised = true;
    mock_running     = false;
    mock_duration    = 0u;
    mock_start_at    = 0u;
}

hal_status_t hal_timer_start(uint32_t timeout_ms)
{
    if (!mock_initialised) {
        return HAL_ERROR;
    }

    mock_start_at = mock_now;
    mock_duration = timeout_ms;
    mock_running  = true;

    return HAL_OK;
}

void hal_timer_stop(void)
{
    mock_running  = false;
    mock_duration = 0u;
    mock_start_at = 0u;
}

bool hal_timer_is_expired(void)
{
    return mock_is_expired_raw();
}

uint32_t hal_timer_get_remaining_ms(void)
{
    uint32_t elapsed;

    if (!mock_running) {
        return 0u;
    }

    elapsed = mock_now - mock_start_at;

    if (elapsed >= mock_duration) {
        return 0u;
    }

    return mock_duration - elapsed;
}
