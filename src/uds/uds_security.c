/*
 * uds_security.c
 * UDS Security Access State Machine — Implementation
 *
 * Implements the security access state machine per ISO 14229-1:2020 Annex I.
 * Provides seed/key based access control with per-level lock state, attempt
 * counting, delay-on-failure, and a pluggable key validation callback.
 *
 * Wave 3b Task 16 — Security Access State Machine
 */

#include "uds/uds_security.h"
#include "hal/hal_timer.h"
#include <string.h>   /* memset, memcpy */

/* ======================================================================== *
 * LFSR Seed Generation                                                     *
 * ======================================================================== *
 * Default 32-bit LFSR (x^32 + x^22 + x^2 + x^1 + 1, polynomial 0x80200003)
 * for deterministic pseudo-random seed generation. The LFSR is
 * re-initialised per level so that different security levels produce
 * different seed sequences.
 *
 * Default LFSR is for demonstration only.
 * ======================================================================== */

/** @brief Default LFSR initialisation seed (avoids all-zero lock-up) */
#define LFSR_DEFAULT_SEED  0xACE1ACE1u

/** @brief Global LFSR state, advanced after each seed generation. */
static uint32_t lfsr_state;

/**
 * @brief Initialise the LFSR with a given 32-bit seed.
 *
 * @param[in]  seed  32-bit seed value
 */
static void lfsr_init(uint32_t seed)
{
    lfsr_state = seed;
    /* LFSR with all zeros would lock up — ensure non-zero state */
    if (lfsr_state == 0u)
    {
        lfsr_state = LFSR_DEFAULT_SEED;
    }
}

/**
 * @brief Generate and return a single pseudo-random byte from the LFSR.
 *
 * Advances the 32-bit LFSR (right-shifting Galois, polynomial 0x80200003)
 * by 8 steps and collects the shifted-out bit from each step into the
 * result byte (MSB first).
 *
 * @return Next pseudo-random byte
 */
static uint8_t lfsr_next_byte(void)
{
    uint8_t result = 0u;

    for (uint8_t i = 0u; i < 8u; i++)
    {
        result <<= 1u;
        uint32_t bit = lfsr_state & 1u;
        result      |= (uint8_t)bit;
        lfsr_state >>= 1u;
        if (bit != 0u)
        {
            lfsr_state ^= 0x80200003u;
        }
    }

    return result;
}

/**
 * @brief Fill a buffer with pseudo-random bytes seeded by the security level.
 *
 * @param[in]  level  Security level number (1-based)
 * @param[out] buf    Buffer to fill
 * @param[in]  len    Number of bytes to generate
 */
static void generate_seed(uint8_t level, uint8_t *buf, uint8_t len)
{
    lfsr_init(0xACE1ACE1u ^ ((uint32_t)level << 16u) ^ (uint32_t)level);
    for (uint8_t i = 0u; i < len; i++)
    {
        buf[i] = lfsr_next_byte();
    }
}

/* ======================================================================== *
 * Per-Level State                                                          *
 * ======================================================================== */

/**
 * @brief Maximum number of distinct security levels (covering odd subfunction
 *        values 1..31).
 */
#define SECURITY_NUM_LEVELS  16u

/** @brief Per-security-level state record. */
typedef struct {
    bool    unlocked;       /**< Level is unlocked */
    bool    seed_sent;      /**< seed was requested and key not yet received */
    uint8_t seed[SECURITY_SEED_SIZE]; /**< Last seed generated for this level */
    uint8_t attempt_count;  /**< Consecutive failed key validation attempts */
} level_state_t;

/** @brief Module-level static state. */
static struct {
    level_state_t levels[SECURITY_NUM_LEVELS]; /**< Per-level state array */
    uint8_t  current_level_idx;  /**< Index of the level with pending seed */
    bool     seed_pending;       /**< True when seed is sent awaiting key */
    bool     delay_active;       /**< True while delay timer is running */
    security_key_validate_t validate_cb; /**< Key validation callback */
} sec_state;

/* ======================================================================== *
 * Internal Helpers                                                         *
 * ======================================================================== */

/**
 * @brief Convert a SecurityAccess subfunction value to a level index.
 *
 * Subfunction values are paired: odd = requestSeed, even = sendKey.
 * Level index = (subfunction - 1) / 2.
 *   subfunction 1 or 2 → index 0  (security level 1)
 *   subfunction 3 or 4 → index 1  (security level 2)
 *   etc.
 *
 * @param[in]  subfn  Subfunction value (1..31)
 * @return Level index into the state array
 */
static uint8_t subfn_to_idx(uint8_t subfn)
{
    return (subfn - 1u) / 2u;
}

/**
 * @brief Derive the 1-based security level number from a subfunction value.
 *
 * @param[in]  subfn  Subfunction value
 * @return Security level number (1-based)
 */
static uint8_t subfn_to_level(uint8_t subfn)
{
    return subfn_to_idx(subfn) + 1u;
}

/**
 * @brief Default key validation callback.
 *
 * Computes CRC-8 over the seed buffer, then validates that the XOR
 * accumulator over the key buffer bytes matches the seed CRC-8 value.
 * This is intended for demonstration/reference only — real applications
 * should install a cryptographically sound callback.
 *
 * Default LFSR is for demonstration only.
 *
 * @param[in]  level     Security level number (1-based, unused)
 * @param[in]  seed      Seed buffer from requestSeed
 * @param[in]  seed_len  Length of seed in bytes
 * @param[in]  key       Key buffer from sendKey
 * @param[in]  key_len   Length of key in bytes
 * @return true if XOR accumulator of key matches CRC-8 of seed, false otherwise
 */
static bool default_key_validate(uint8_t       level,
                                 const uint8_t *seed,
                                 uint8_t       seed_len,
                                 const uint8_t *key,
                                 uint8_t       key_len)
{
    (void)level;

    if (seed_len == 0u || key_len == 0u)
    {
        return false;
    }

    /* Compute CRC-8 over seed buffer (polynomial 0x07, init 0xFF) */
    uint8_t crc = 0xFFu;
    for (uint8_t i = 0u; i < seed_len; i++)
    {
        crc ^= seed[i];
        for (uint8_t j = 0u; j < 8u; j++)
        {
            if (crc & 0x80u)
            {
                crc = (uint8_t)((crc << 1u) ^ 0x07u);
            }
            else
            {
                crc <<= 1u;
            }
        }
    }

    /* XOR accumulator over key bytes must match seed CRC-8 */
    uint8_t key_xor = 0u;
    for (uint8_t k = 0u; k < key_len; k++)
    {
        key_xor ^= key[k];
    }

    return (key_xor == crc);
}

/**
 * @brief Check whether a level index is valid (within range).
 *
 * @param[in]  idx  Level index to check
 * @return true if the index is valid, false otherwise
 */
static bool valid_idx(uint8_t idx)
{
    return (idx < SECURITY_NUM_LEVELS);
}

/* ======================================================================== *
 * uds_security_init                                                         *
 * ======================================================================== */
void uds_security_init(void)
{
    /* Clear all per-level state */
    memset(&sec_state, 0, sizeof(sec_state));

    /* Install default key validation callback */
    sec_state.validate_cb = default_key_validate;

    /* Ensure HAL timer is initialised (safe to call even if already inited) */
    hal_timer_init();
}

/* ======================================================================== *
 * uds_security_set_key_validate_cb                                          *
 * ======================================================================== */
void uds_security_set_key_validate_cb(security_key_validate_t cb)
{
    if (cb == NULL)
    {
        sec_state.validate_cb = default_key_validate;
    }
    else
    {
        sec_state.validate_cb = cb;
    }
}

/* ======================================================================== *
 * uds_security_request_seed                                                 *
 * ======================================================================== *
 * Behaviour per ISO 14229-1 Annex I:
 *   - If delay is active → NRC 0x37 (requiredTimeDelayNotExpired)
 *   - If level is already unlocked → seed = 0x0000
 *   - Otherwise → generate seed, set seed_sent, return seed
 * ======================================================================== */
uds_status_t uds_security_request_seed(uint8_t   level,
                                       uint8_t  *seed_buf,
                                       uint8_t  *seed_len,
                                       uds_nrc_t *nrc)
{
    if (seed_buf == NULL || seed_len == NULL || nrc == NULL)
    {
        return UDS_ERR_PARSE;
    }

    uint8_t idx = subfn_to_idx(level);

    if (!valid_idx(idx))
    {
        *nrc = NRC_SUB_FUNCTION_NOT_SUPPORTED;
        return UDS_OK;
    }

    /* Check for active delay */
    if (sec_state.delay_active)
    {
        if (hal_timer_is_expired())
        {
            /* Delay has expired — clear flag and proceed */
            sec_state.delay_active = false;
        }
        else
        {
            *nrc   = NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED;
            *seed_len = 0u;
            return UDS_OK;
        }
    }

    level_state_t *ls = &sec_state.levels[idx];

    /* Already unlocked → zero seed */
    if (ls->unlocked)
    {
        memset(seed_buf, 0, *seed_len);
        *nrc = NRC_POSITIVE_RESPONSE;
        return UDS_OK;
    }

    /* Generate seed */
    uint8_t copy_len = (*seed_len < SECURITY_SEED_SIZE) ? *seed_len
                                                        : SECURITY_SEED_SIZE;
    generate_seed(subfn_to_level(level), seed_buf, copy_len);
    *seed_len = copy_len;

    /* Store seed in per-level state for later key validation */
    memcpy(ls->seed, seed_buf, copy_len);

    /* Transition to SEED_SENT state */
    ls->seed_sent  = true;
    sec_state.seed_pending      = true;
    sec_state.current_level_idx = idx;

    *nrc = NRC_POSITIVE_RESPONSE;
    return UDS_OK;
}

/* ======================================================================== *
 * uds_security_send_key                                                     *
 * ======================================================================== *
 * Behaviour per ISO 14229-1 Annex I:
 *   - If no seed pending for this level → NRC 0x24 (requestSequenceError)
 *   - If attempt counter exceeded → NRC 0x36, start delay
 *   - Validate key via callback:
 *     - Match → unlocked, NRC_POSITIVE_RESPONSE
 *     - No match → increment counter, NRC 0x35
 * ======================================================================== */
uds_status_t uds_security_send_key(uint8_t        level,
                                   const uint8_t *key,
                                   uint8_t        key_len,
                                   uds_nrc_t     *nrc)
{
    if (key == NULL || nrc == NULL)
    {
        return UDS_ERR_PARSE;
    }

    uint8_t idx = subfn_to_idx(level);

    if (!valid_idx(idx))
    {
        *nrc = NRC_SUB_FUNCTION_NOT_SUPPORTED;
        return UDS_OK;
    }

    level_state_t *ls = &sec_state.levels[idx];

    /* Sequence error: sendKey without requestSeed */
    if (!ls->seed_sent)
    {
        *nrc = NRC_REQUEST_SEQUENCE_ERROR;
        return UDS_OK;
    }

    /* Check for active delay */
    if (sec_state.delay_active)
    {
        if (hal_timer_is_expired())
        {
            sec_state.delay_active = false;
        }
        else
        {
            *nrc = NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED;
            return UDS_OK;
        }
    }

    /* Check if maximum attempts already reached */
    if (ls->attempt_count >= SECURITY_MAX_ATTEMPTS)
    {
        /* Start the delay timer */
        (void)hal_timer_start(SECURITY_DELAY_MS);
        sec_state.delay_active = true;

        /* Reset the attempt counter for the next attempt window */
        ls->attempt_count = 0u;

        /* Clear seed_sent so the sequence can restart after delay */
        ls->seed_sent            = false;
        sec_state.seed_pending   = false;

        *nrc = NRC_EXCEED_NUMBER_OF_ATTEMPTS;
        return UDS_OK;
    }

    /* Invoke key validation callback */
    bool valid = sec_state.validate_cb(subfn_to_level(level),
                                       ls->seed,
                                       SECURITY_SEED_SIZE,
                                       key,
                                       key_len);

    if (valid)
    {
        /* Key accepted — unlock this level */
        ls->unlocked             = true;
        ls->seed_sent            = false;
        ls->attempt_count        = 0u;  /* reset counter on success */
        sec_state.seed_pending   = false;

        *nrc = NRC_POSITIVE_RESPONSE;
    }
    else
    {
        /* Key rejected — increment attempt counter.
         * The threshold check at the top of this function will catch
         * the next attempt and return NRC 0x36 + start delay. */
        ls->attempt_count++;
        ls->seed_sent            = false;
        sec_state.seed_pending   = false;

        *nrc = NRC_INVALID_KEY;
    }

    return UDS_OK;
}

/* ======================================================================== *
 * uds_security_lock                                                         *
 * ======================================================================== */
void uds_security_lock(void)
{
    /* Lock all levels — reset unlocked and seed_sent flags */
    for (uint8_t i = 0u; i < SECURITY_NUM_LEVELS; i++)
    {
        sec_state.levels[i].unlocked  = false;
        sec_state.levels[i].seed_sent = false;
    }

    sec_state.seed_pending = false;
}

/* ======================================================================== *
 * uds_security_is_unlocked                                                  *
 * ======================================================================== */
bool uds_security_is_unlocked(void)
{
    for (uint8_t i = 0u; i < SECURITY_NUM_LEVELS; i++)
    {
        if (sec_state.levels[i].unlocked)
        {
            return true;
        }
    }
    return false;
}

/* ======================================================================== *
 * uds_security_is_locked_for_level                                          *
 * ======================================================================== */
bool uds_security_is_locked_for_level(uint8_t level)
{
    uint8_t idx = subfn_to_idx(level);

    if (!valid_idx(idx))
    {
        return true; /* Invalid level → treat as locked */
    }

    return !sec_state.levels[idx].unlocked;
}

/* ======================================================================== *
 * uds_security_get_delay_remaining_ms                                       *
 * ======================================================================== */
uint32_t uds_security_get_delay_remaining_ms(void)
{
    if (!sec_state.delay_active)
    {
        return 0u;
    }

    uint32_t remaining = hal_timer_get_remaining_ms();

    /* If timer reports 0 but we think delay is active, re-check expiry */
    if (remaining == 0u && hal_timer_is_expired())
    {
        sec_state.delay_active = false;
        return 0u;
    }

    return remaining;
}
