/*
 * hal_timer_posix.c
 * POSIX reference implementation of the Timer HAL.
 *
 * Uses clock_gettime(CLOCK_MONOTONIC) to implement a single-shot
 * count-down timer in milliseconds.  On MCU targets this would be
 * backed by a hardware timer/counter peripheral.
 *
 * Compile with UDS_PLATFORM_POSIX (set by hal_platform.h when
 * __linux__ is defined).
 */

#include "hal/hal_platform.h"

#ifdef UDS_PLATFORM_POSIX

/* _POSIX_C_SOURCE must be defined before any system header so that
 * clock_gettime(2) and CLOCK_MONOTONIC are visible under -std=c11. */
#define _POSIX_C_SOURCE 199309L

#include "hal/hal_timer.h"
#include <time.h>

/* Static state ---------------------------------------------------*/

/** Absolute CLOCK_MONOTONIC timestamp (ns) at which the timer started. */
static uint64_t timer_start_ns;

/** Duration of the running timer in nanoseconds. */
static uint64_t timer_duration_ns;

/** true while a timer is active. */
static bool timer_running;

/** Whether hal_timer_init() has been called. */
static bool timer_initialised;

/* Internal helpers -----------------------------------------------*/

/**
 * @brief Read the CLOCK_MONOTONIC counter as a uint64_t of nanoseconds.
 */
static uint64_t now_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * @brief true if the running timer has expired.
 */
static bool is_expired_raw(void)
{
    uint64_t elapsed;

    if (!timer_running) {
        return false;
    }

    elapsed = now_ns() - timer_start_ns;
    return elapsed >= timer_duration_ns;
}

/* HAL interface implementation ----------------------------------*/

void hal_timer_init(void)
{
    timer_initialised = true;
    timer_running     = false;
    timer_duration_ns = 0u;
    timer_start_ns    = 0u;
}

hal_status_t hal_timer_start(uint32_t timeout_ms)
{
    if (!timer_initialised) {
        return HAL_ERROR;
    }

    timer_start_ns    = now_ns();
    timer_duration_ns = (uint64_t)timeout_ms * 1000000ULL;
    timer_running     = true;

    return HAL_OK;
}

void hal_timer_stop(void)
{
    timer_running     = false;
    timer_duration_ns = 0u;
    timer_start_ns    = 0u;
}

bool hal_timer_is_expired(void)
{
    return is_expired_raw();
}

uint32_t hal_timer_get_remaining_ms(void)
{
    uint64_t elapsed_ns;
    uint64_t remaining_ns;

    if (!timer_running) {
        return 0u;
    }

    elapsed_ns = now_ns() - timer_start_ns;

    if (elapsed_ns >= timer_duration_ns) {
        return 0u;
    }

    remaining_ns = timer_duration_ns - elapsed_ns;

    /* Round up to the next whole millisecond so a nearly-expired timer
     * never reports 0 before it truly has expired. */
    return (uint32_t)((remaining_ns + 999999ULL) / 1000000ULL);
}

#endif
