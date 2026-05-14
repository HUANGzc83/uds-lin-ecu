/*
 * test_svc_data.c
 * Data Transmission Services — Unit Tests
 *
 * Wave 3a Task 10 — Data Transmission Services
 */

#include "unity.h"
#include "uds/uds_core.h"
#include "uds/uds_data.h"
#include "uds/uds_svc_data.h"
#include <string.h>

/* Static storage for custom DIDs */
static uint8_t g_did_storage_1234[4];
static uint8_t g_did_storage_5678[4] = {0x11, 0x22, 0x33, 0x44};
static uint8_t g_did_storage_9ABC[4];

static bool unlocked;

void setUp(void)
{
    uds_did_init();
    uds_svc_data_init();
    unlocked = false;

    /* 0x1234: READ_WRITE, 4 bytes */
    {
        uds_did_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        entry.did      = 0x1234;
        entry.len      = 4;
        entry.access   = DID_READ_WRITE;
        entry.data     = g_did_storage_1234;
        uds_did_register(&entry);
    }
    /* 0x5678: SECURED_READ, 4 bytes */
    {
        uds_did_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        entry.did      = 0x5678;
        entry.len      = 4;
        entry.access   = DID_SECURED_READ;
        entry.data     = g_did_storage_5678;
        uds_did_register(&entry);
    }
    /* 0x9ABC: SECURED_WRITE, 4 bytes */
    {
        uds_did_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        entry.did      = 0x9ABC;
        entry.len      = 4;
        entry.access   = DID_SECURED_WRITE;
        entry.data     = g_did_storage_9ABC;
        uds_did_register(&entry);
    }
}

void tearDown(void) {}

static bool call_handler(const uint8_t *raw, uint16_t raw_len,
                         uds_response_t *rsp, void *context,
                         bool (*handler)(const uds_request_t *, uds_response_t *, void *))
{
    uds_request_t req;
    uds_status_t status = uds_parse_request(raw, raw_len, &req);
    TEST_ASSERT_EQUAL(UDS_OK, status);
    return handler(&req, rsp, context);
}

static void check_neg_rsp(const uds_response_t *rsp,
                          uint8_t expected_req_sid,
                          uint8_t expected_nrc)
{
    TEST_ASSERT_EQUAL_UINT8(0x7F, rsp->sid);
    TEST_ASSERT_EQUAL_UINT8(expected_req_sid, rsp->subfunc_echo);
    TEST_ASSERT_NOT_NULL(rsp->data);
    TEST_ASSERT_EQUAL_UINT8(1, rsp->data_len);
    TEST_ASSERT_EQUAL_UINT8(expected_nrc, rsp->data[0]);
}

/* ======================================================================== *
 * 0x22 — ReadDataByIdentifier                                             *
 * ======================================================================== */

void test_22_read_known_did(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x22, 0xF1, 0x90};
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked,
                               uds_svc_read_data_by_id);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT8(READ_DATA_BY_IDENTIFIER_RSP, rsp.sid);
    TEST_ASSERT_EQUAL_UINT8(0, rsp.subfunc_echo);
    TEST_ASSERT_NOT_NULL(rsp.data);
    TEST_ASSERT_EQUAL_UINT16(19, rsp.data_len);
    TEST_ASSERT_EQUAL_UINT8(0xF1, rsp.data[0]);
    TEST_ASSERT_EQUAL_UINT8(0x90, rsp.data[1]);
    TEST_ASSERT_EQUAL_UINT8('W', rsp.data[2]);
}

void test_22_read_unknown_did(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x22, 0x00, 0x01};
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked,
                               uds_svc_read_data_by_id);
    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x22, NRC_REQUEST_OUT_OF_RANGE);
}

void test_22_read_secured_without_unlock(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x22, 0x56, 0x78};
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked,
                               uds_svc_read_data_by_id);
    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x22, NRC_SECURITY_ACCESS_DENIED);
}

void test_22_null_context(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x22, 0xF1, 0x90};
    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_read_data_by_id);
    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x22, NRC_CONDITIONS_NOT_CORRECT);
}

void test_22_short_request(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x22};
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked,
                               uds_svc_read_data_by_id);
    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x22, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

/* ======================================================================== *
 * 0x23 — ReadMemoryByAddress                                              *
 * ======================================================================== */

void test_23_read_memory(void)
{
    uint8_t *mem = uds_svc_data_get_memory_buf();
    mem[0x10] = 0xAA;
    mem[0x11] = 0xBB;
    mem[0x12] = 0xCC;
    mem[0x13] = 0xDD;

    uds_response_t rsp;
    uint8_t raw[] = {0x23, 0x31, 0x00, 0x00, 0x00, 0x10, 0x00, 0x04};
    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_read_memory_by_address);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT8(READ_MEMORY_BY_ADDRESS_RSP, rsp.sid);
    TEST_ASSERT_EQUAL_UINT16(11, rsp.data_len);
    TEST_ASSERT_EQUAL_UINT8(0x31, rsp.data[0]);
    TEST_ASSERT_EQUAL_UINT8(0xAA, rsp.data[7]);
}

void test_23_invalid_address(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x23, 0x31, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x04};
    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_read_memory_by_address);
    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x23, NRC_REQUEST_OUT_OF_RANGE);
}

void test_23_short_request(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x23, 0x31};
    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_read_memory_by_address);
    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x23, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

/* ======================================================================== *
 * 0x24 — ReadScalingDataByIdentifier                                      *
 * ======================================================================== */

void test_24_read_scaling_data(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x24, 0xF1, 0x90};
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked,
                               uds_svc_read_scaling_data_by_id);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT8(READ_SCALING_DATA_BY_ID_RSP, rsp.sid);
    TEST_ASSERT_EQUAL_UINT8(0, rsp.subfunc_echo);
    TEST_ASSERT_EQUAL_UINT16(24, rsp.data_len);
    TEST_ASSERT_EQUAL_UINT8(0x01, rsp.data[19]);
}

void test_24_null_context(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x24, 0xF1, 0x90};
    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_read_scaling_data_by_id);
    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x24, NRC_CONDITIONS_NOT_CORRECT);
}

/* ======================================================================== *
 * 0x2A — ReadDataByPeriodicIdentifier                                     *
 * ======================================================================== */

void test_2a_schedule_did(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x2A, 0x01, 0x01, 0xF1, 0x90};
    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_read_data_by_periodic_id);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT8(READ_DATA_BY_PERIODIC_ID_RSP, rsp.sid);
    TEST_ASSERT_EQUAL_UINT8(0x01, rsp.subfunc_echo);
}

void test_2a_invalid_mode(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x2A, 0xFF, 0x01, 0xF1, 0x90};
    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_read_data_by_periodic_id);
    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x2A, NRC_REQUEST_OUT_OF_RANGE);
}

/* ======================================================================== *
 * 0x2C — DynamicallyDefineDataIdentifier                                  *
 * ======================================================================== */

void test_2c_define_did(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x2C, 0x01, 0xAA, 0xBB, 0xF1, 0x90, 0x00, 0x00, 0x00, 0x11};
    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_dynamically_define_data_id);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT8(DYNAMICALLY_DEFINE_DATA_ID_RSP, rsp.sid);
    TEST_ASSERT_EQUAL_UINT16(2, rsp.data_len);
    TEST_ASSERT_EQUAL_UINT8(0xAA, rsp.data[0]);
}

void test_2c_delete_did(void)
{
    /* First define DID 0xCCDD */
    {
        uds_response_t rsp;
        uint8_t raw[] = {0x2C, 0x01, 0xCC, 0xDD, 0xF1, 0x90, 0x00, 0x00, 0x00, 0x11};
        call_handler(raw, sizeof(raw), &rsp, NULL, uds_svc_dynamically_define_data_id);
    }
    /* Then delete it */
    {
        uds_response_t rsp;
        uint8_t raw[] = {0x2C, 0x03, 0xCC, 0xDD};
        bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                                   uds_svc_dynamically_define_data_id);
        TEST_ASSERT_TRUE(result);
        TEST_ASSERT_EQUAL_UINT8(DYNAMICALLY_DEFINE_DATA_ID_RSP, rsp.sid);
    }
}

/* ======================================================================== *
 * 0x2E — WriteDataByIdentifier                                            *
 * ======================================================================== */

void test_2e_write_known_did(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x2E, 0x12, 0x34, 0xAA, 0xBB, 0xCC, 0xDD};
    unlocked = true;
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked,
                               uds_svc_write_data_by_id);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT8(WRITE_DATA_BY_IDENTIFIER_RSP, rsp.sid);
}

void test_2e_write_readonly_did(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x2E, 0xF1, 0x90, 0xAA, 0xBB, 0xCC, 0xDD};
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked,
                               uds_svc_write_data_by_id);
    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x2E, NRC_REQUEST_OUT_OF_RANGE);
}

void test_2e_write_secured_without_unlock(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x2E, 0x9A, 0xBC, 0xAA, 0xBB, 0xCC, 0xDD};
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked,
                               uds_svc_write_data_by_id);
    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x2E, NRC_SECURITY_ACCESS_DENIED);
}

void test_2e_null_context(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x2E, 0x12, 0x34, 0xAA, 0xBB, 0xCC, 0xDD};
    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_write_data_by_id);
    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x2E, NRC_CONDITIONS_NOT_CORRECT);
}

/* ======================================================================== *
 * 0x3D — WriteMemoryByAddress                                             *
 * ======================================================================== */

void test_3d_write_memory(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x3D, 0x31, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04,
                     0x11, 0x22, 0x33, 0x44};
    unlocked = true;
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked,
                               uds_svc_write_memory_by_address);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT8(WRITE_MEMORY_BY_ADDRESS_RSP, rsp.sid);
    TEST_ASSERT_EQUAL_UINT16(7, rsp.data_len);

    uint8_t *mem = uds_svc_data_get_memory_buf();
    TEST_ASSERT_EQUAL_UINT8(0x11, mem[0x20]);
}

void test_3d_invalid_address(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x3D, 0x31, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x04,
                     0xAA, 0xBB, 0xCC, 0xDD};
    unlocked = true;
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked,
                               uds_svc_write_memory_by_address);
    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x3D, NRC_REQUEST_OUT_OF_RANGE);
}

/* ======================================================================== *
 * Test Runner                                                              *
 * ======================================================================== */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_22_read_known_did);
    RUN_TEST(test_22_read_unknown_did);
    RUN_TEST(test_22_read_secured_without_unlock);
    RUN_TEST(test_22_null_context);
    RUN_TEST(test_22_short_request);
    RUN_TEST(test_23_read_memory);
    RUN_TEST(test_23_invalid_address);
    RUN_TEST(test_23_short_request);
    RUN_TEST(test_24_read_scaling_data);
    RUN_TEST(test_24_null_context);
    RUN_TEST(test_2a_schedule_did);
    RUN_TEST(test_2a_invalid_mode);
    RUN_TEST(test_2c_define_did);
    RUN_TEST(test_2c_delete_did);
    RUN_TEST(test_2e_write_known_did);
    RUN_TEST(test_2e_write_readonly_did);
    RUN_TEST(test_2e_write_secured_without_unlock);
    RUN_TEST(test_2e_null_context);
    RUN_TEST(test_3d_write_memory);
    RUN_TEST(test_3d_invalid_address);
    return UNITY_END();
}
