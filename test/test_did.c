/*
 * test_did.c
 * DID Registry & Data Storage unit tests — Wave 3b Task 17
 *
 * TDD cycle: these tests are the RED phase; implementation must
 * make them GREEN.
 */

#include "unity.h"
#include "uds/uds_data.h"
#include <string.h>

void setUp(void)
{
    uds_did_init();
}

void tearDown(void)
{
    /* Nothing to clean up */
}

/* ======================================================================== *
 * Test 1: Init → default DIDs registered, count > 0                        *
 * ======================================================================== */
void test_init_registers_default_dids(void)
{
    uint16_t count = uds_did_get_count();
    TEST_ASSERT_GREATER_THAN_UINT16(0, count);
    TEST_ASSERT_EQUAL_UINT16(4, count);
}

/* ======================================================================== *
 * Test 2: Read known DID (0xF190) → returns VIN, correct length            *
 * ======================================================================== */
void test_read_known_did_vin(void)
{
    uint8_t buf[32];
    uint16_t len = sizeof(buf);

    bool result = uds_did_read(0xF190, buf, &len, false);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT16(17, len);
    TEST_ASSERT_EQUAL_UINT8('W', buf[0]);
    TEST_ASSERT_EQUAL_UINT8('E', buf[16]);
}

/* ======================================================================== *
 * Test 3: Read unknown DID → returns false                                 *
 * ======================================================================== */
void test_read_unknown_did(void)
{
    uint8_t buf[32];
    uint16_t len = sizeof(buf);

    bool result = uds_did_read(0x0001, buf, &len, false);
    TEST_ASSERT_FALSE(result);
}

/* ======================================================================== *
 * Test 4: Write to READ_ONLY DID → returns false (access denied)           *
 * ======================================================================== */
void test_write_read_only_did(void)
{
    /* 0xF190 is READ_ONLY */
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    bool result = uds_did_write(0xF190, data, sizeof(data), false);
    TEST_ASSERT_FALSE(result);
}

/* ======================================================================== *
 * Test 5: Write to DID with write access → succeeds                        *
 * ======================================================================== */
void test_write_did_with_write_access(void)
{
    uint8_t storage[8] = {0};
    uds_did_entry_t entry;

    memset(&entry, 0, sizeof(entry));
    entry.did       = 0x1234;
    entry.len       = 8;
    entry.access    = DID_WRITE_ONLY;
    entry.data      = storage;
    entry.on_read   = NULL;
    entry.on_write  = NULL;
    uds_did_register(&entry);

    uint8_t write_data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    bool result = uds_did_write(0x1234, write_data, sizeof(write_data), false);
    TEST_ASSERT_TRUE(result);

    /* Verify data actually written to storage */
    TEST_ASSERT_EQUAL_UINT8(0xAA, storage[0]);
    TEST_ASSERT_EQUAL_UINT8(0xBB, storage[1]);
    TEST_ASSERT_EQUAL_UINT8(0xCC, storage[2]);
    TEST_ASSERT_EQUAL_UINT8(0xDD, storage[3]);
}

/* ======================================================================== *
 * Test 6: Register custom DID → found in registry                          *
 * ======================================================================== */
void test_register_custom_did(void)
{
    uint8_t storage[4] = {0};
    uds_did_entry_t entry;

    memset(&entry, 0, sizeof(entry));
    entry.did       = 0x5678;
    entry.len       = 4;
    entry.access    = DID_READ_WRITE;
    entry.data      = storage;
    entry.on_read   = NULL;
    entry.on_write  = NULL;

    bool reg_result = uds_did_register(&entry);
    TEST_ASSERT_TRUE(reg_result);

    /* Verify via find */
    const uds_did_entry_t *found = uds_did_find(0x5678);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_UINT16(0x5678, found->did);
    TEST_ASSERT_EQUAL_UINT16(4, found->len);
    TEST_ASSERT_EQUAL(DID_READ_WRITE, found->access);

    /* Verify count increased */
    TEST_ASSERT_EQUAL_UINT16(5, uds_did_get_count());
}

/* ======================================================================== *
 * Test 7: Read secured DID without unlock → returns false                  *
 * ======================================================================== */
void test_read_secured_did_without_unlock(void)
{
    uint8_t storage[4] = {0x11, 0x22, 0x33, 0x44};
    uds_did_entry_t entry;

    memset(&entry, 0, sizeof(entry));
    entry.did       = 0x9ABC;
    entry.len       = 4;
    entry.access    = DID_SECURED_READ;
    entry.data      = storage;
    entry.on_read   = NULL;
    entry.on_write  = NULL;
    uds_did_register(&entry);

    uint8_t buf[4];
    uint16_t len = sizeof(buf);

    bool result = uds_did_read(0x9ABC, buf, &len, false);
    TEST_ASSERT_FALSE(result);
}

/* ======================================================================== *
 * Test 8: Read secured DID with unlock → returns data                      *
 * ======================================================================== */
void test_read_secured_did_with_unlock(void)
{
    uint8_t expected[] = {0x11, 0x22, 0x33, 0x44};
    uint8_t storage[4] = {0x11, 0x22, 0x33, 0x44};
    uds_did_entry_t entry;

    memset(&entry, 0, sizeof(entry));
    entry.did       = 0x9ABC;
    entry.len       = 4;
    entry.access    = DID_SECURED_READ;
    entry.data      = storage;
    entry.on_read   = NULL;
    entry.on_write  = NULL;
    uds_did_register(&entry);

    uint8_t buf[4];
    uint16_t len = sizeof(buf);

    bool result = uds_did_read(0x9ABC, buf, &len, true);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT16(4, len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buf, 4);
}

/* ======================================================================== *
 * Test runner                                                               *
 * ======================================================================== */
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_registers_default_dids);
    RUN_TEST(test_read_known_did_vin);
    RUN_TEST(test_read_unknown_did);
    RUN_TEST(test_write_read_only_did);
    RUN_TEST(test_write_did_with_write_access);
    RUN_TEST(test_register_custom_did);
    RUN_TEST(test_read_secured_did_without_unlock);
    RUN_TEST(test_read_secured_did_with_unlock);

    return UNITY_END();
}
