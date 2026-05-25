/*
 * test_security.c
 * UDS Security Access State Machine unit tests — Wave 3b Task 16
 *
 * Tests the security access state machine per ISO 14229-1 Annex I:
 *   - Init state (locked, counters zero)
 *   - requestSeed / sendKey sequence
 *   - Valid / invalid key validation
 *   - Sequence error (sendKey without requestSeed)
 *   - Already-unlocked behaviour
 *   - Exceeded attempt count → delay
 *   - Delay-active rejection
 *   - Force re-lock
 *
 * Uses the mock timer HAL so that delay timing can be controlled
 * explicitly in test code.
 */

#include "unity.h"
#include "uds/uds_security.h"
#include "mock_timer.h"
#include <string.h>

/* ======================================================================== *
 * setUp / tearDown                                                         *
 * ======================================================================== */

void setUp(void)
{
    mock_timer_clear();
    uds_security_init();
}

void tearDown(void)
{
    /* nothing to tear down */
}

/* ======================================================================== *
 * Helpers                                                                  *
 * ======================================================================== */

/** @brief Security level 1 subfunctions */
#define REQ_LEVEL1  1u   /* requestSeed  for level 1 */
#define KEY_LEVEL1  2u   /* sendKey      for level 1 */

/** @brief Security level 2 subfunctions */
#define REQ_LEVEL2  3u   /* requestSeed  for level 2 */
#define KEY_LEVEL2  4u   /* sendKey      for level 2 */

/** @brief Buffer sized for default 8-byte seed/key */
static uint8_t  seed_buf[SECURITY_SEED_SIZE];
static uint8_t  key_buf[SECURITY_KEY_SIZE];

/**
 * @brief Compute CRC-8 over a buffer (polynomial 0x07, init 0xFF).
 *
 * Matches the CRC-8 used in the default key validation callback.
 */
static uint8_t crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFFu;
    for (uint8_t i = 0u; i < len; i++)
    {
        crc ^= data[i];
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
    return crc;
}

/* ======================================================================== *
 * 1. Init → locked, counters zero, delay not active                       *
 * ======================================================================== */
void test_init_locked_and_zero_counters(void)
{
    /* All levels should be locked after init */
    TEST_ASSERT_FALSE(uds_security_is_unlocked());
    TEST_ASSERT_TRUE(uds_security_is_locked_for_level(REQ_LEVEL1));
    TEST_ASSERT_TRUE(uds_security_is_locked_for_level(KEY_LEVEL1));

    /* Delay should not be active */
    TEST_ASSERT_EQUAL_UINT32(0, uds_security_get_delay_remaining_ms());
}

/* ======================================================================== *
 * 2. requestSeed → returns non-zero seed, NRC_POSITIVE_RESPONSE            *
 * ======================================================================== */
void test_request_seed_returns_nonzero_seed(void)
{
    uds_nrc_t nrc = 0xFF;
    uint8_t   len = sizeof(seed_buf);
    uds_status_t status;

    status = uds_security_request_seed(REQ_LEVEL1, seed_buf, &len, &nrc);

    TEST_ASSERT_EQUAL(UDS_OK, status);
    TEST_ASSERT_EQUAL(NRC_POSITIVE_RESPONSE, nrc);
    TEST_ASSERT_EQUAL(SECURITY_SEED_SIZE, len);
    TEST_ASSERT_TRUE(seed_buf[0] != 0 || seed_buf[1] != 0 ||
                     seed_buf[2] != 0 || seed_buf[3] != 0 ||
                     seed_buf[4] != 0 || seed_buf[5] != 0 ||
                     seed_buf[6] != 0 || seed_buf[7] != 0);
}

/* ======================================================================== *
 * 3. sendKey with valid key → unlocked                                    *
 * ======================================================================== */
void test_send_key_valid_unlocks(void)
{
    uds_nrc_t nrc = 0xFF;
    uint8_t   len = sizeof(seed_buf);
    uds_status_t status;

    /* Request a seed */
    uds_security_request_seed(REQ_LEVEL1, seed_buf, &len, &nrc);
    TEST_ASSERT_EQUAL(NRC_POSITIVE_RESPONSE, nrc);

    /* Build valid key: XOR accumulator of key must match CRC-8 of seed */
    memset(key_buf, 0x00, sizeof(key_buf));
    key_buf[0] = crc8(seed_buf, SECURITY_SEED_SIZE);

    status = uds_security_send_key(KEY_LEVEL1, key_buf, sizeof(key_buf), &nrc);

    TEST_ASSERT_EQUAL(UDS_OK, status);
    TEST_ASSERT_EQUAL(NRC_POSITIVE_RESPONSE, nrc);
    TEST_ASSERT_TRUE(uds_security_is_unlocked());
    TEST_ASSERT_FALSE(uds_security_is_locked_for_level(REQ_LEVEL1));
    TEST_ASSERT_FALSE(uds_security_is_locked_for_level(KEY_LEVEL1));
}

/* ======================================================================== *
 * 4. sendKey with invalid key → NRC 0x35, still locked                    *
 * ======================================================================== */
void test_send_key_invalid_returns_nrc35(void)
{
    uds_nrc_t nrc = 0xFF;
    uint8_t   len = sizeof(seed_buf);
    uds_status_t status;

    /* Request a seed */
    uds_security_request_seed(REQ_LEVEL1, seed_buf, &len, &nrc);
    TEST_ASSERT_EQUAL(NRC_POSITIVE_RESPONSE, nrc);

    /* Build invalid key: key[0] == seed[0] (not complemented) */
    memset(key_buf, 0xBB, sizeof(key_buf));
    key_buf[0] = seed_buf[0];

    status = uds_security_send_key(KEY_LEVEL1, key_buf, sizeof(key_buf), &nrc);

    TEST_ASSERT_EQUAL(UDS_OK, status);  /* no parse error */
    TEST_ASSERT_EQUAL(NRC_INVALID_KEY, nrc);
    TEST_ASSERT_FALSE(uds_security_is_unlocked());
}

/* ======================================================================== *
 * 5. sendKey without requestSeed → NRC 0x24 (requestSequenceError)        *
 * ======================================================================== */
void test_send_key_without_request_returns_nrc24(void)
{
    uds_nrc_t nrc = 0xFF;
    uds_status_t status;

    /* Attempt sendKey directly without requestSeed */
    memset(key_buf, 0xCC, sizeof(key_buf));

    status = uds_security_send_key(KEY_LEVEL1, key_buf, sizeof(key_buf), &nrc);

    TEST_ASSERT_EQUAL(UDS_OK, status);
    TEST_ASSERT_EQUAL(NRC_REQUEST_SEQUENCE_ERROR, nrc);
    TEST_ASSERT_FALSE(uds_security_is_unlocked());
}

/* ======================================================================== *
 * 6. Already unlocked → requestSeed returns seed=0x0000                    *
 * ======================================================================== */
void test_request_seed_already_unlocked_returns_zero(void)
{
    uds_nrc_t nrc = 0xFF;
    uint8_t   len = sizeof(seed_buf);
    uds_status_t status;

    /* Unlock level 1 first */
    uds_security_request_seed(REQ_LEVEL1, seed_buf, &len, &nrc);
    memset(key_buf, 0x00, sizeof(key_buf));
    key_buf[0] = crc8(seed_buf, SECURITY_SEED_SIZE);
    uds_security_send_key(KEY_LEVEL1, key_buf, sizeof(key_buf), &nrc);
    TEST_ASSERT_TRUE(uds_security_is_unlocked());

    /* Now request seed again for same level → should return zeros */
    memset(seed_buf, 0xFF, sizeof(seed_buf));
    len = sizeof(seed_buf);
    nrc = 0xFF;
    status = uds_security_request_seed(REQ_LEVEL1, seed_buf, &len, &nrc);

    TEST_ASSERT_EQUAL(UDS_OK, status);
    TEST_ASSERT_EQUAL(NRC_POSITIVE_RESPONSE, nrc);
    TEST_ASSERT_EQUAL(SECURITY_SEED_SIZE, len);
    /* Seed should be all zeros */
    for (uint8_t i = 0; i < SECURITY_SEED_SIZE; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(0, seed_buf[i]);
    }
}

/* ======================================================================== *
 * 7. After max_attempts+1 bad keys → NRC 0x36 (exceedNumberOfAttempts)    *
 * ======================================================================== */
void test_exceed_attempts_returns_nrc36(void)
{
    uds_nrc_t nrc = 0xFF;
    uint8_t   len = sizeof(seed_buf);
    uds_status_t status;

    /* Send SECURITY_MAX_ATTEMPTS bad keys */
    for (uint8_t i = 0; i < SECURITY_MAX_ATTEMPTS; i++)
    {
        /* Fresh seed each attempt */
        uds_security_request_seed(REQ_LEVEL1, seed_buf, &len, &nrc);

        memset(key_buf, 0xDD, sizeof(key_buf));
        key_buf[0] = seed_buf[0];  /* intentionally wrong */
        status = uds_security_send_key(KEY_LEVEL1, key_buf, sizeof(key_buf), &nrc);

        TEST_ASSERT_EQUAL(UDS_OK, status);
        TEST_ASSERT_EQUAL(NRC_INVALID_KEY, nrc);
        TEST_ASSERT_FALSE(uds_security_is_unlocked());
    }

    /* The (SECURITY_MAX_ATTEMPTS+1)-th attempt → NRC 0x36 */
    uds_security_request_seed(REQ_LEVEL1, seed_buf, &len, &nrc);
    memset(key_buf, 0xEE, sizeof(key_buf));
    key_buf[0] = seed_buf[0];
    status = uds_security_send_key(KEY_LEVEL1, key_buf, sizeof(key_buf), &nrc);

    TEST_ASSERT_EQUAL(UDS_OK, status);
    TEST_ASSERT_EQUAL(NRC_EXCEED_NUMBER_OF_ATTEMPTS, nrc);

    /* Delay timer should now be active */
    TEST_ASSERT_TRUE(uds_security_get_delay_remaining_ms() > 0);
}

/* ======================================================================== *
 * 8. During delay → NRC 0x37 (requiredTimeDelayNotExpired)                 *
 * ======================================================================== */
void test_during_delay_returns_nrc37(void)
{
    uds_nrc_t nrc = 0xFF;
    uint8_t   len = sizeof(seed_buf);
    uds_status_t status;

    /* Trigger delay by exceeding max attempts */
    for (uint8_t i = 0; i <= SECURITY_MAX_ATTEMPTS; i++)
    {
        uds_security_request_seed(REQ_LEVEL1, seed_buf, &len, &nrc);
        memset(key_buf, 0xDD, sizeof(key_buf));
        key_buf[0] = seed_buf[0];
        uds_security_send_key(KEY_LEVEL1, key_buf, sizeof(key_buf), &nrc);
    }

    /* Now in delay — requestSeed should return NRC 0x37 */
    nrc = 0xFF;
    len = sizeof(seed_buf);
    status = uds_security_request_seed(REQ_LEVEL1, seed_buf, &len, &nrc);

    TEST_ASSERT_EQUAL(UDS_OK, status);
    TEST_ASSERT_EQUAL(NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED, nrc);
}

/* ======================================================================== *
 * 9. After delay expires → operation resumes normally                      *
 * ======================================================================== */
void test_after_delay_expires_request_seed_works(void)
{
    uds_nrc_t nrc = 0xFF;
    uint8_t   len = sizeof(seed_buf);
    uds_status_t status;

    /* Trigger delay */
    for (uint8_t i = 0; i <= SECURITY_MAX_ATTEMPTS; i++)
    {
        uds_security_request_seed(REQ_LEVEL1, seed_buf, &len, &nrc);
        memset(key_buf, 0xDD, sizeof(key_buf));
        key_buf[0] = seed_buf[0];
        uds_security_send_key(KEY_LEVEL1, key_buf, sizeof(key_buf), &nrc);
    }

    /* Advance past the delay period */
    mock_timer_advance_ms(SECURITY_DELAY_MS + 1);

    /* Now requestSeed should succeed again */
    nrc = 0xFF;
    len = sizeof(seed_buf);
    status = uds_security_request_seed(REQ_LEVEL1, seed_buf, &len, &nrc);

    TEST_ASSERT_EQUAL(UDS_OK, status);
    TEST_ASSERT_EQUAL(NRC_POSITIVE_RESPONSE, nrc);
    TEST_ASSERT_TRUE(len > 0);
}

/* ======================================================================== *
 * 10. lock() → re-locked, is_unlocked=false                                *
 * ======================================================================== */
void test_lock_relocks(void)
{
    uds_nrc_t nrc = 0xFF;
    uint8_t   len = sizeof(seed_buf);

    /* Unlock level 1 */
    uds_security_request_seed(REQ_LEVEL1, seed_buf, &len, &nrc);
    memset(key_buf, 0x00, sizeof(key_buf));
    key_buf[0] = crc8(seed_buf, SECURITY_SEED_SIZE);
    uds_security_send_key(KEY_LEVEL1, key_buf, sizeof(key_buf), &nrc);
    TEST_ASSERT_TRUE(uds_security_is_unlocked());

    /* Force re-lock */
    uds_security_lock();

    TEST_ASSERT_FALSE(uds_security_is_unlocked());
    TEST_ASSERT_TRUE(uds_security_is_locked_for_level(REQ_LEVEL1));
}

/* ======================================================================== *
 * 11. Multiple levels: level 2 independent of level 1                      *
 * ======================================================================== */
void test_multiple_levels_independent(void)
{
    uds_nrc_t nrc = 0xFF;
    uint8_t   len = sizeof(seed_buf);

    /* Unlock level 1 */
    uds_security_request_seed(REQ_LEVEL1, seed_buf, &len, &nrc);
    memset(key_buf, 0x00, sizeof(key_buf));
    key_buf[0] = crc8(seed_buf, SECURITY_SEED_SIZE);
    uds_security_send_key(KEY_LEVEL1, key_buf, sizeof(key_buf), &nrc);
    TEST_ASSERT_TRUE(uds_security_is_unlocked());
    TEST_ASSERT_FALSE(uds_security_is_locked_for_level(REQ_LEVEL1));

    /* Level 2 should still be locked */
    TEST_ASSERT_TRUE(uds_security_is_locked_for_level(REQ_LEVEL2));

    /* Unlock level 2 independently */
    uds_security_request_seed(REQ_LEVEL2, seed_buf, &len, &nrc);
    memset(key_buf, 0x00, sizeof(key_buf));
    key_buf[0] = crc8(seed_buf, SECURITY_SEED_SIZE);
    uds_security_send_key(KEY_LEVEL2, key_buf, sizeof(key_buf), &nrc);
    TEST_ASSERT_FALSE(uds_security_is_locked_for_level(REQ_LEVEL2));

    /* lock() should lock all levels */
    uds_security_lock();
    TEST_ASSERT_TRUE(uds_security_is_locked_for_level(REQ_LEVEL1));
    TEST_ASSERT_TRUE(uds_security_is_locked_for_level(REQ_LEVEL2));
}

/* ======================================================================== *
 * 12. Null pointer protection for seed_buf/seed_len                        *
 * ======================================================================== */
void test_request_seed_null_pointer(void)
{
    uds_nrc_t nrc = 0xFF;
    uint8_t   len = sizeof(seed_buf);

    uds_status_t status;

    /* NULL seed_buf */
    status = uds_security_request_seed(REQ_LEVEL1, NULL, &len, &nrc);
    TEST_ASSERT_NOT_EQUAL(UDS_OK, status);

    /* NULL seed_len */
    status = uds_security_request_seed(REQ_LEVEL1, seed_buf, NULL, &nrc);
    TEST_ASSERT_NOT_EQUAL(UDS_OK, status);

    /* NULL nrc */
    status = uds_security_request_seed(REQ_LEVEL1, seed_buf, &len, NULL);
    TEST_ASSERT_NOT_EQUAL(UDS_OK, status);
}

void test_send_key_null_pointer(void)
{
    uds_nrc_t nrc = 0xFF;
    uint8_t   len = sizeof(seed_buf);

    uds_security_request_seed(REQ_LEVEL1, seed_buf, &len, &nrc);

    uds_status_t status;

    /* NULL key */
    status = uds_security_send_key(KEY_LEVEL1, NULL, sizeof(key_buf), &nrc);
    TEST_ASSERT_NOT_EQUAL(UDS_OK, status);

    /* NULL nrc */
    status = uds_security_send_key(KEY_LEVEL1, key_buf, sizeof(key_buf), NULL);
    TEST_ASSERT_NOT_EQUAL(UDS_OK, status);
}

/* ======================================================================== *
 * 13. Test custom callback can be installed                                *
 * ======================================================================== */

static bool custom_cb_called;
static bool custom_validate_cb(uint8_t level, const uint8_t *seed,
                                uint8_t seed_len, const uint8_t *key,
                                uint8_t key_len)
{
    (void)level;
    (void)seed;
    (void)seed_len;
    (void)key;
    (void)key_len;
    custom_cb_called = true;
    return true; /* always accept */
}

void test_custom_validate_callback(void)
{
    uds_nrc_t nrc = 0xFF;
    uint8_t   len = sizeof(seed_buf);

    custom_cb_called = false;
    uds_security_set_key_validate_cb(custom_validate_cb);

    /* Perform request/send cycle */
    uds_security_request_seed(REQ_LEVEL1, seed_buf, &len, &nrc);
    memset(key_buf, 0x00, sizeof(key_buf));
    uds_security_send_key(KEY_LEVEL1, key_buf, sizeof(key_buf), &nrc);

    TEST_ASSERT_TRUE(custom_cb_called);
    TEST_ASSERT_TRUE(uds_security_is_unlocked());
}

/* ======================================================================== *
 * 14. Seed for different levels differs                                    *
 * ======================================================================== */
void test_different_levels_produce_different_seeds(void)
{
    uds_nrc_t nrc = 0xFF;
    uint8_t   len = sizeof(seed_buf);
    uint8_t   seed2[SECURITY_SEED_SIZE];

    /* Get seed for level 1 */
    uds_security_request_seed(REQ_LEVEL1, seed_buf, &len, &nrc);
    uint8_t seed1_copy[SECURITY_SEED_SIZE];
    memcpy(seed1_copy, seed_buf, SECURITY_SEED_SIZE);

    /* Get seed for level 2 */
    len = sizeof(seed2);
    uds_security_request_seed(REQ_LEVEL2, seed2, &len, &nrc);

    /* Seeds should differ (different levels → different LFSR init) */
    bool all_same = true;
    for (uint8_t i = 0; i < SECURITY_SEED_SIZE; i++)
    {
        if (seed1_copy[i] != seed2[i])
        {
            all_same = false;
            break;
        }
    }
    TEST_ASSERT_FALSE(all_same);
}

/* ======================================================================== *
 * Test runner                                                              *
 * ======================================================================== */
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_locked_and_zero_counters);
    RUN_TEST(test_request_seed_returns_nonzero_seed);
    RUN_TEST(test_send_key_valid_unlocks);
    RUN_TEST(test_send_key_invalid_returns_nrc35);
    RUN_TEST(test_send_key_without_request_returns_nrc24);
    RUN_TEST(test_request_seed_already_unlocked_returns_zero);
    RUN_TEST(test_exceed_attempts_returns_nrc36);
    RUN_TEST(test_during_delay_returns_nrc37);
    RUN_TEST(test_after_delay_expires_request_seed_works);
    RUN_TEST(test_lock_relocks);
    RUN_TEST(test_multiple_levels_independent);
    RUN_TEST(test_request_seed_null_pointer);
    RUN_TEST(test_send_key_null_pointer);
    RUN_TEST(test_custom_validate_callback);
    RUN_TEST(test_different_levels_produce_different_seeds);

    return UNITY_END();
}
