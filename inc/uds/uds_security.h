/*
 * uds_security.h
 * UDS Security Access State Machine — Module Header
 *
 * Implements the security access state machine per ISO 14229-1:2020 Annex I.
 * Provides seed/key based access control configurable per security level
 * with attempt counting, delay-on-failure, and a pluggable key validation
 * callback.
 *
 * Wave 3b Task 16 — Security Access State Machine
 */

#pragma once

#include "uds/uds_core.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== *
 * Configuration Macros                                                     *
 * ======================================================================== */

/**
 * @brief Maximum number of consecutive failed key validation attempts
 *        before the delay timer activates and NRC 0x36 is returned.
 *
 * Default: 5
 */
#ifndef SECURITY_MAX_ATTEMPTS
#define SECURITY_MAX_ATTEMPTS  5u
#endif

/**
 * @brief Delay duration in milliseconds after exceeding the maximum
 *        number of failed attempts.
 *
 * During this delay all SecurityAccess requests return NRC 0x37.
 * Default: 10000 ms (10 seconds)
 */
#ifndef SECURITY_DELAY_MS
#define SECURITY_DELAY_MS  10000u
#endif

/**
 * @brief Size of the seed value in bytes.
 *
 * The default LFSR generates SECURITY_SEED_SIZE bytes per request.
 * Default: 8
 */
#ifndef SECURITY_SEED_SIZE
#define SECURITY_SEED_SIZE  8u
#endif

/**
 * @brief Size of the key value in bytes.
 *
 * The default CRC-8 key validation operates over the full key buffer.
 * Default: 8
 */
#ifndef SECURITY_KEY_SIZE
#define SECURITY_KEY_SIZE  8u
#endif

/* ======================================================================== *
 * Key Validation Callback                                                  *
 * ======================================================================== */

/**
 * @brief Pluggable key validation callback.
 *
 * The callback receives the security level number (1-based), the seed
 * that was previously generated via requestSeed, and the key supplied
 * by the tester via sendKey.
 *
 * @param[in] level     Security level number (1-based)
 * @param[in] seed      Seed buffer previously returned by requestSeed
 * @param[in] seed_len  Length of the seed in bytes
 * @param[in] key       Key buffer supplied by the tester
 * @param[in] key_len   Length of the key in bytes
 * @return true if the key is valid (matched), false otherwise
 */
typedef bool (*security_key_validate_t)(uint8_t level,
                                        const uint8_t *seed,
                                        uint8_t seed_len,
                                        const uint8_t *key,
                                        uint8_t key_len);

/* ======================================================================== *
 * Function Declarations                                                   *
 * ======================================================================== */

/**
 * @brief Initialise the security access state machine.
 *
 * Resets all levels to LOCKED state, clears attempt counters, deactivates
 * the delay timer, and installs the default key validation callback.
 *
 * Must be called once before any other security API.
 */
void uds_security_init(void);

/**
 * @brief Install a custom key validation callback.
 *
 * The callback is invoked during uds_security_send_key() to determine
 * whether the supplied key is valid for the given seed.
 *
 * @param[in] cb  Callback function pointer (NULL restores the default)
 */
void uds_security_set_key_validate_cb(security_key_validate_t cb);

/**
 * @brief Process a SecurityAccess requestSeed subfunction.
 *
 * If the level is already unlocked, returns a zero-filled seed.
 * If the delay timer is active, sets @p nrc to NRC 0x37.
 * Otherwise generates a seed, transitions to SEED_SENT state, and
 * returns the seed.
 *
 * @param[in]  level    Subfunction value (odd: 1, 3, 5, ...)
 * @param[out] seed_buf Buffer to receive the seed
 * @param[in,out] seed_len On input: capacity of seed_buf.
 *                       On success: number of seed bytes written.
 * @param[out] nrc      Set to NRC on failure; NRC_POSITIVE_RESPONSE on success
 * @return UDS_OK on success, UDS_ERR_PARSE on NULL pointer
 */
uds_status_t uds_security_request_seed(uint8_t   level,
                                       uint8_t  *seed_buf,
                                       uint8_t  *seed_len,
                                       uds_nrc_t *nrc);

/**
 * @brief Process a SecurityAccess sendKey subfunction.
 *
 * If no seed was previously requested (requestSequenceError), sets @p nrc
 * to NRC 0x24.  If the attempt counter has reached the maximum, sets @p nrc
 * to NRC 0x36 and starts the delay timer.  Otherwise invokes the key
 * validation callback — on match the level is unlocked, on mismatch the
 * attempt counter is incremented and @p nrc is set to NRC 0x35.
 *
 * @param[in]  level   Subfunction value (even: 2, 4, 6, ...)
 * @param[in]  key     Key buffer supplied by the tester
 * @param[in]  key_len Length of the key in bytes
 * @param[out] nrc     Set to NRC on failure; NRC_POSITIVE_RESPONSE on success
 * @return UDS_OK on success, UDS_ERR_PARSE on NULL pointer
 */
uds_status_t uds_security_send_key(uint8_t   level,
                                   const uint8_t *key,
                                   uint8_t   key_len,
                                   uds_nrc_t *nrc);

/**
 * @brief Force re-lock the security access module.
 *
 * Resets all levels to LOCKED state and clears the seed-sent flag.
 * Does NOT clear the delay timer or attempt counters.
 */
void uds_security_lock(void);

/**
 * @brief Check whether any security level is currently unlocked.
 *
 * @return true if at least one level is unlocked, false otherwise
 */
bool uds_security_is_unlocked(void);

/**
 * @brief Check whether a specific level (subfunction) is locked.
 *
 * @param[in] level  Subfunction value (odd or even for the target level)
 * @return true if the level is locked, false if unlocked
 */
bool uds_security_is_locked_for_level(uint8_t level);

/**
 * @brief Get the remaining time before the delay expires.
 *
 * @return Remaining delay in milliseconds, or 0 if delay is not active
 */
uint32_t uds_security_get_delay_remaining_ms(void);

#ifdef __cplusplus
}
#endif
