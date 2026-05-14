/*
 * test_dtc.c
 * DTC State Machine Engine unit tests — Wave 3b Task 15
 *
 * TDD cycle: these tests are the RED phase; implementation must
 * make them GREEN.
 */

#include "unity.h"
#include "uds/uds_dtc.h"

/* ======================================================================== *
 * Test DTC codes                                                           *
 * ======================================================================== */
/* Upper nibble 0x0 = Powertrain */
#define DTC_POW_1  0x012345U
#define DTC_POW_2  0x013456U

/* Upper nibble 0x2 = Chassis */
#define DTC_CHASSIS_1  0x212345U

/* Upper nibble 0x4 = Body */
#define DTC_BODY_1  0x412345U

/* Upper nibble 0x6 = Network */
#define DTC_NET_1   0x612345U

void setUp(void)
{
    uds_dtc_init();
}

void tearDown(void)
{
    /* Nothing to clean up */
}

/* ======================================================================== *
 * Helper: verify a DTC record is fully cleared                             *
 * ======================================================================== */
static void assert_dtc_cleared(uint32_t dtc)
{
    uint8_t buf[8];
    uint16_t len;

    TEST_ASSERT_EQUAL_UINT8(0, uds_dtc_get_status(dtc));

    len = sizeof(buf);
    TEST_ASSERT_FALSE(uds_dtc_get_snapshot(dtc, buf, &len));
    TEST_ASSERT_EQUAL_UINT16(0, len);

    len = sizeof(buf);
    TEST_ASSERT_FALSE(uds_dtc_get_extended(dtc, buf, &len));
    TEST_ASSERT_EQUAL_UINT16(0, len);
}

/* ======================================================================== *
 * Test 1: Init → all DTCs cleared, count = 0                               *
 * ======================================================================== */
void test_init_clears_all_dtcs(void)
{
    /* Add a few DTCs first */
    uds_dtc_set_status(DTC_POW_1, DTC_TEST_FAILED, true);
    uds_dtc_set_status(DTC_CHASSIS_1, DTC_CONFIRMED, true);
    TEST_ASSERT_EQUAL_UINT16(2, uds_dtc_get_count(0xFF));

    /* Re-init */
    uds_dtc_init();

    /* All cleared */
    TEST_ASSERT_EQUAL_UINT16(0, uds_dtc_get_count(0xFF));
    assert_dtc_cleared(DTC_POW_1);
    assert_dtc_cleared(DTC_CHASSIS_1);
}

/* ======================================================================== *
 * Test 2: Set TEST_FAILED bit → status contains 0x01                       *
 * ======================================================================== */
void test_set_test_failed_bit(void)
{
    uds_dtc_set_status(DTC_POW_1, DTC_TEST_FAILED, true);

    uint8_t status = uds_dtc_get_status(DTC_POW_1);
    TEST_ASSERT_EQUAL_UINT8(DTC_TEST_FAILED, status);
}

/* ======================================================================== *
 * Test 3: Set multiple bits → correct composite status                     *
 * ======================================================================== */
void test_set_multiple_bits(void)
{
    uint8_t expected = (uint8_t)(DTC_TEST_FAILED |
                                 DTC_CONFIRMED |
                                 DTC_WARNING_INDICATOR);

    uds_dtc_set_status(DTC_POW_1, DTC_TEST_FAILED, true);
    uds_dtc_set_status(DTC_POW_1, DTC_CONFIRMED, true);
    uds_dtc_set_status(DTC_POW_1, DTC_WARNING_INDICATOR, true);

    uint8_t status = uds_dtc_get_status(DTC_POW_1);
    TEST_ASSERT_EQUAL_UINT8(expected, status);
}

/* ======================================================================== *
 * Test 4: Clear by group (Powertrain) → only that group's DTCs cleared     *
 * ======================================================================== */
void test_clear_by_group_powertrain(void)
{
    /* Set up DTCs from different groups */
    uds_dtc_set_status(DTC_POW_1,     DTC_CONFIRMED, true);
    uds_dtc_set_status(DTC_POW_2,     DTC_TEST_FAILED, true);
    uds_dtc_set_status(DTC_CHASSIS_1, DTC_CONFIRMED, true);
    uds_dtc_set_status(DTC_BODY_1,    DTC_PENDING, true);
    uds_dtc_set_status(DTC_NET_1,     DTC_CONFIRMED, true);

    TEST_ASSERT_EQUAL_UINT16(5, uds_dtc_get_count(0xFF));

    /* Clear Powertrain group (upper nibble 0x0) */
    uds_dtc_clear(0x00);

    /* Powertrain DTCs should be cleared */
    assert_dtc_cleared(DTC_POW_1);
    assert_dtc_cleared(DTC_POW_2);

    /* Other group DTCs should still be present */
    TEST_ASSERT_EQUAL_UINT8(DTC_CONFIRMED,  uds_dtc_get_status(DTC_CHASSIS_1));
    TEST_ASSERT_EQUAL_UINT8(DTC_PENDING,    uds_dtc_get_status(DTC_BODY_1));
    TEST_ASSERT_EQUAL_UINT8(DTC_CONFIRMED,  uds_dtc_get_status(DTC_NET_1));

    /* Total remaining = 3 */
    TEST_ASSERT_EQUAL_UINT16(3, uds_dtc_get_count(0xFF));
}

/* ======================================================================== *
 * Test 5: Clear all (0xFFFFFF) → all cleared                               *
 * ======================================================================== */
void test_clear_all(void)
{
    uds_dtc_set_status(DTC_POW_1,     DTC_CONFIRMED, true);
    uds_dtc_set_status(DTC_CHASSIS_1, DTC_TEST_FAILED, true);
    uds_dtc_set_status(DTC_BODY_1,    DTC_PENDING, true);

    TEST_ASSERT_EQUAL_UINT16(3, uds_dtc_get_count(0xFF));

    uds_dtc_clear(0xFFFFFFU);

    TEST_ASSERT_EQUAL_UINT16(0, uds_dtc_get_count(0xFF));
    assert_dtc_cleared(DTC_POW_1);
    assert_dtc_cleared(DTC_CHASSIS_1);
    assert_dtc_cleared(DTC_BODY_1);
}

/* ======================================================================== *
 * Test 6: Get by status mask → returns correct DTCs                        *
 * ======================================================================== */
void test_get_by_status_mask(void)
{
    /* Set up: three confirmed DTCs, one pending */
    uds_dtc_set_status(DTC_POW_1,     DTC_CONFIRMED, true);
    uds_dtc_set_status(DTC_CHASSIS_1, DTC_CONFIRMED, true);
    uds_dtc_set_status(DTC_BODY_1,    DTC_PENDING, true);
    uds_dtc_set_status(DTC_NET_1,     DTC_CONFIRMED, true);

    /* Get all DTCs matching DTC_CONFIRMED */
    uint32_t result[4];
    uint16_t count = uds_dtc_get_by_status_mask(DTC_CONFIRMED, result, 4);

    TEST_ASSERT_EQUAL_UINT16(3, count);

    /* Verify confirmed DTCs are in the result list (order not important) */
    bool found_pow1    = false;
    bool found_chassis = false;
    bool found_net     = false;

    for (uint16_t i = 0; i < count; i++)
    {
        if (result[i] == DTC_POW_1)     found_pow1    = true;
        if (result[i] == DTC_CHASSIS_1) found_chassis = true;
        if (result[i] == DTC_NET_1)     found_net     = true;
    }

    TEST_ASSERT_TRUE(found_pow1);
    TEST_ASSERT_TRUE(found_chassis);
    TEST_ASSERT_TRUE(found_net);
}

/* ======================================================================== *
 * Test 7: DTC count by mask → correct count                                *
 * ======================================================================== */
void test_dtc_count_by_mask(void)
{
    /* Mix of confirmed, pending, and failed DTCs */
    uds_dtc_set_status(DTC_POW_1,     DTC_CONFIRMED, true);
    uds_dtc_set_status(DTC_POW_2,     DTC_TEST_FAILED, true);
    uds_dtc_set_status(DTC_CHASSIS_1, DTC_CONFIRMED | DTC_WARNING_INDICATOR, true);
    uds_dtc_set_status(DTC_BODY_1,    DTC_PENDING, true);

    /* Count all */
    TEST_ASSERT_EQUAL_UINT16(4, uds_dtc_get_count(0xFF));

    /* Count only confirmed */
    TEST_ASSERT_EQUAL_UINT16(2, uds_dtc_get_count(DTC_CONFIRMED));

    /* Count only pending */
    TEST_ASSERT_EQUAL_UINT16(1, uds_dtc_get_count(DTC_PENDING));

    /* Count only test-failed */
    TEST_ASSERT_EQUAL_UINT16(1, uds_dtc_get_count(DTC_TEST_FAILED));

    /* Count only warning-indicator */
    TEST_ASSERT_EQUAL_UINT16(1, uds_dtc_get_count(DTC_WARNING_INDICATOR));

    /* Count with zero mask (no DTCs should have no bits set after init) */
    TEST_ASSERT_EQUAL_UINT16(0, uds_dtc_get_count(0x00));
}

/* ======================================================================== *
 * Test 8: Snapshot data stored correctly                                   *
 * ======================================================================== */
void test_snapshot_data(void)
{
    uint8_t test_data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint8_t buf[8];
    uint16_t len;

    /* Store snapshot */
    bool stored = uds_dtc_set_snapshot(DTC_POW_1, test_data, sizeof(test_data));
    TEST_ASSERT_TRUE(stored);

    /* Also set a status bit (separate from snapshot) */
    uds_dtc_set_status(DTC_POW_1, DTC_CONFIRMED, true);

    /* Retrieve snapshot */
    len = sizeof(buf);
    bool retrieved = uds_dtc_get_snapshot(DTC_POW_1, buf, &len);
    TEST_ASSERT_TRUE(retrieved);
    TEST_ASSERT_EQUAL_UINT16(sizeof(test_data), len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(test_data, buf, sizeof(test_data));

    /* Verify status was unaffected */
    TEST_ASSERT_EQUAL_UINT8(DTC_CONFIRMED, uds_dtc_get_status(DTC_POW_1));
}

/* ======================================================================== *
 * Test 9: Extended data stored correctly                                   *
 * ======================================================================== */
void test_extended_data(void)
{
    uint8_t test_data[] = {0x11, 0x22, 0x33};
    uint8_t buf[8];
    uint16_t len;

    /* Store extended data */
    bool stored = uds_dtc_set_extended(DTC_CHASSIS_1, test_data, sizeof(test_data));
    TEST_ASSERT_TRUE(stored);

    /* Retrieve extended data */
    len = sizeof(buf);
    bool retrieved = uds_dtc_get_extended(DTC_CHASSIS_1, buf, &len);
    TEST_ASSERT_TRUE(retrieved);
    TEST_ASSERT_EQUAL_UINT16(sizeof(test_data), len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(test_data, buf, sizeof(test_data));
}

/* ======================================================================== *
 * Test 10: Get status for unknown DTC → returns 0                          *
 * ======================================================================== */
void test_get_status_unknown_dtc(void)
{
    uint8_t status = uds_dtc_get_status(0x999999U);
    TEST_ASSERT_EQUAL_UINT8(0, status);
}

/* ======================================================================== *
 * Test 11: Clear individual bits (clear DTC_TEST_FAILED)                   *
 * ======================================================================== */
void test_clear_individual_bits(void)
{
    uds_dtc_set_status(DTC_POW_1, DTC_TEST_FAILED | DTC_CONFIRMED, true);
    TEST_ASSERT_EQUAL_UINT8(DTC_TEST_FAILED | DTC_CONFIRMED,
                            uds_dtc_get_status(DTC_POW_1));

    /* Clear only the TEST_FAILED bit */
    uds_dtc_set_status(DTC_POW_1, DTC_TEST_FAILED, false);
    TEST_ASSERT_EQUAL_UINT8(DTC_CONFIRMED, uds_dtc_get_status(DTC_POW_1));
}

/* ======================================================================== *
 * Test 12: Clear a non-existing DTC group has no effect                    *
 * ======================================================================== */
void test_clear_non_existent_group(void)
{
    uds_dtc_set_status(DTC_POW_1, DTC_CONFIRMED, true);
    uds_dtc_set_status(DTC_CHASSIS_1, DTC_TEST_FAILED, true);

    /* Clear non-existent group (e.g. 0x8) */
    uds_dtc_clear(0x08);

    /* All DTCs should remain */
    TEST_ASSERT_EQUAL_UINT16(2, uds_dtc_get_count(0xFF));
}

/* ======================================================================== *
 * Test runner                                                               *
 * ======================================================================== */
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_clears_all_dtcs);
    RUN_TEST(test_set_test_failed_bit);
    RUN_TEST(test_set_multiple_bits);
    RUN_TEST(test_clear_by_group_powertrain);
    RUN_TEST(test_clear_all);
    RUN_TEST(test_get_by_status_mask);
    RUN_TEST(test_dtc_count_by_mask);
    RUN_TEST(test_snapshot_data);
    RUN_TEST(test_extended_data);
    RUN_TEST(test_get_status_unknown_dtc);
    RUN_TEST(test_clear_individual_bits);
    RUN_TEST(test_clear_non_existent_group);

    return UNITY_END();
}
