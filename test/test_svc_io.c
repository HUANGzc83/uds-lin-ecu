/*
 * test_svc_io.c
 * IO Control Service (0x2F) — Unit Tests
 *
 * Wave 3a Task 12 — IO Control Service (0x2F)
 *
 * Covers all four control modes and all specified NRC paths:
 *   0x13 (IMLOIF), 0x22 (CNC), 0x31 (ROOR), 0x33 (SAD)
 */

#include "unity.h"
#include "uds/uds_core.h"
#include "uds/uds_data.h"
#include "uds/uds_svc_io.h"
#include <string.h>

/* ======================================================================== *
 * Test Fixture                                                              *
 * ======================================================================== */

static uint8_t g_did_storage_1234[4];
static uint8_t g_did_storage_5678[4] = {0xAA, 0xBB, 0xCC, 0xDD};
static uint8_t g_did_storage_3ABC[4];

static bool unlocked;

void setUp(void)
{
    uds_did_init();
    uds_svc_io_init();
    unlocked = false;

    /* 0x1234: DID_READ_WRITE, 4 bytes — standard IO control testing */
    {
        uds_did_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        entry.did    = 0x1234;
        entry.len    = 4;
        entry.access = DID_READ_WRITE;
        entry.data   = g_did_storage_1234;
        uds_did_register(&entry);
    }

    /* 0x5678: DID_SECURED_READ, 4 bytes — requires unlock to read */
    {
        uds_did_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        entry.did    = 0x5678;
        entry.len    = 4;
        entry.access = DID_SECURED_READ;
        entry.data   = g_did_storage_5678;
        uds_did_register(&entry);
    }

    /* 0x3ABC: DID_SECURED_WRITE, 4 bytes — requires unlock to write */
    {
        uds_did_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        entry.did    = 0x3ABC;
        entry.len    = 4;
        entry.access = DID_SECURED_WRITE;
        entry.data   = g_did_storage_3ABC;
        uds_did_register(&entry);
    }
}

void tearDown(void) {}

/* ======================================================================== *
 * Helpers                                                                  *
 * ======================================================================== */

static bool call_handler(const uint8_t *raw, uint16_t raw_len,
                         uds_response_t *rsp, void *context)
{
    uds_request_t req;
    uds_status_t status = uds_parse_request(raw, raw_len, &req);
    TEST_ASSERT_EQUAL(UDS_OK, status);
    return uds_svc_io_control(&req, rsp, context);
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
 * 0x01 — returnControlToStandard                                           *
 * ======================================================================== */

void test_2f_return_to_standard(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x2F, 0x12, 0x34, 0x01};
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT8(INPUT_OUTPUT_CONTROL_BY_ID_RSP, rsp.sid);
    TEST_ASSERT_EQUAL_UINT8(0, rsp.subfunc_echo);
    TEST_ASSERT_NOT_NULL(rsp.data);
    TEST_ASSERT_EQUAL_UINT16(3, rsp.data_len);
    TEST_ASSERT_EQUAL_UINT8(0x12, rsp.data[0]);  /* DID high */
    TEST_ASSERT_EQUAL_UINT8(0x34, rsp.data[1]);  /* DID low  */
    TEST_ASSERT_EQUAL_UINT8(0x01, rsp.data[2]);  /* controlMode */
}

/* ======================================================================== *
 * 0x02 — resetToDefault                                                    *
 * ======================================================================== */

void test_2f_reset_to_default(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x2F, 0x12, 0x34, 0x02};
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT8(INPUT_OUTPUT_CONTROL_BY_ID_RSP, rsp.sid);
    TEST_ASSERT_EQUAL_UINT8(0, rsp.subfunc_echo);
    TEST_ASSERT_EQUAL_UINT16(3, rsp.data_len);
    TEST_ASSERT_EQUAL_UINT8(0x12, rsp.data[0]);
    TEST_ASSERT_EQUAL_UINT8(0x34, rsp.data[1]);
    TEST_ASSERT_EQUAL_UINT8(0x02, rsp.data[2]);
}

/* ======================================================================== *
 * 0x03 — freezeCurrentState                                                *
 * ======================================================================== */

void test_2f_freeze_current_state(void)
{
    /* Pre-fill the DID data */
    g_did_storage_1234[0] = 0x11;
    g_did_storage_1234[1] = 0x22;
    g_did_storage_1234[2] = 0x33;
    g_did_storage_1234[3] = 0x44;

    uds_response_t rsp;
    uint8_t raw[] = {0x2F, 0x12, 0x34, 0x03};
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT8(INPUT_OUTPUT_CONTROL_BY_ID_RSP, rsp.sid);
    TEST_ASSERT_EQUAL_UINT8(0, rsp.subfunc_echo);
    TEST_ASSERT_NOT_NULL(rsp.data);
    TEST_ASSERT_EQUAL_UINT16(7, rsp.data_len);
    TEST_ASSERT_EQUAL_UINT8(0x12, rsp.data[0]);  /* DID high   */
    TEST_ASSERT_EQUAL_UINT8(0x34, rsp.data[1]);  /* DID low    */
    TEST_ASSERT_EQUAL_UINT8(0x03, rsp.data[2]);  /* controlMode */
    TEST_ASSERT_EQUAL_UINT8(0x11, rsp.data[3]);  /* frozen byte 0 */
    TEST_ASSERT_EQUAL_UINT8(0x22, rsp.data[4]);  /* frozen byte 1 */
    TEST_ASSERT_EQUAL_UINT8(0x33, rsp.data[5]);  /* frozen byte 2 */
    TEST_ASSERT_EQUAL_UINT8(0x44, rsp.data[6]);  /* frozen byte 3 */
}

/* ======================================================================== *
 * 0x04 — shortTermAdjustment                                               *
 * ======================================================================== */

void test_2f_short_term_adjust(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x2F, 0x12, 0x34, 0x04, 0xAA, 0xBB};
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT8(INPUT_OUTPUT_CONTROL_BY_ID_RSP, rsp.sid);
    TEST_ASSERT_EQUAL_UINT8(0, rsp.subfunc_echo);
    TEST_ASSERT_NOT_NULL(rsp.data);
    TEST_ASSERT_EQUAL_UINT16(5, rsp.data_len);
    TEST_ASSERT_EQUAL_UINT8(0x12, rsp.data[0]);  /* DID high      */
    TEST_ASSERT_EQUAL_UINT8(0x34, rsp.data[1]);  /* DID low       */
    TEST_ASSERT_EQUAL_UINT8(0x04, rsp.data[2]);  /* controlMode   */
    TEST_ASSERT_EQUAL_UINT8(0xAA, rsp.data[3]);  /* param byte 0  */
    TEST_ASSERT_EQUAL_UINT8(0xBB, rsp.data[4]);  /* param byte 1  */

    /* Verify override was stored */
    TEST_ASSERT_TRUE(uds_svc_io_has_override(0x1234));
}

/* ======================================================================== *
 * Sequence: shortTermAdjustment → returnControlToStandard                  *
 * ======================================================================== */

void test_2f_adjust_then_return_to_standard(void)
{
    /* First set an override */
    {
        uds_response_t rsp;
        uint8_t raw[] = {0x2F, 0x12, 0x34, 0x04, 0x55};
        call_handler(raw, sizeof(raw), &rsp, &unlocked);
    }
    TEST_ASSERT_TRUE(uds_svc_io_has_override(0x1234));

    /* Then return to standard — should clear the override */
    {
        uds_response_t rsp;
        uint8_t raw[] = {0x2F, 0x12, 0x34, 0x01};
        bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked);
        TEST_ASSERT_TRUE(result);
    }
    TEST_ASSERT_FALSE(uds_svc_io_has_override(0x1234));
}

/* ======================================================================== *
 * Sequence: shortTermAdjustment → resetToDefault                           *
 * ======================================================================== */

void test_2f_adjust_then_reset_to_default(void)
{
    /* First set an override */
    {
        uds_response_t rsp;
        uint8_t raw[] = {0x2F, 0x12, 0x34, 0x04, 0x77};
        call_handler(raw, sizeof(raw), &rsp, &unlocked);
    }
    TEST_ASSERT_TRUE(uds_svc_io_has_override(0x1234));

    /* Then reset to default — should clear the override */
    {
        uds_response_t rsp;
        uint8_t raw[] = {0x2F, 0x12, 0x34, 0x02};
        bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked);
        TEST_ASSERT_TRUE(result);
    }
    TEST_ASSERT_FALSE(uds_svc_io_has_override(0x1234));
}

/* ======================================================================== *
 * Unknown DID → NRC 0x31 (ROOR)                                          *
 * ======================================================================== */

void test_2f_unknown_did(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x2F, 0xFF, 0xFF, 0x01};
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x2F, NRC_REQUEST_OUT_OF_RANGE);
}

/* ======================================================================== *
 * Invalid control mode → NRC 0x31 (ROOR)                                  *
 * ======================================================================== */

void test_2f_invalid_control_mode(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x2F, 0x12, 0x34, 0xFF};
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x2F, NRC_REQUEST_OUT_OF_RANGE);
}

/* ======================================================================== *
 * Security denied for SECURED_READ DID → NRC 0x33 (SAD)                   *
 * ======================================================================== */

void test_2f_security_denied_secured_read(void)
{
    /* DID 0x5678 has SECURED_READ access, and unlocked is false */
    uds_response_t rsp;
    uint8_t raw[] = {0x2F, 0x56, 0x78, 0x01};
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x2F, NRC_SECURITY_ACCESS_DENIED);
}

/* ======================================================================== *
 * Security denied for SECURED_WRITE DID → NRC 0x33 (SAD)                  *
 * ======================================================================== */

void test_2f_security_denied_secured_write(void)
{
    /* DID 0x3ABC has SECURED_WRITE access, and unlocked is false */
    uds_response_t rsp;
    uint8_t raw[] = {0x2F, 0x3A, 0xBC, 0x01};
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x2F, NRC_SECURITY_ACCESS_DENIED);
}

/* ======================================================================== *
 * IMLOIF — request too short (only SID + 1 byte)                          *
 * ======================================================================== */

void test_2f_imloif_short_request(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x2F, 0x12};  /* only 2 bytes — missing DID_low + controlMode */
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x2F, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

/* ======================================================================== *
 * CNC — null context                                                       *
 * ======================================================================== */

void test_2f_null_context(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x2F, 0x12, 0x34, 0x01};
    bool result = call_handler(raw, sizeof(raw), &rsp, NULL);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x2F, NRC_CONDITIONS_NOT_CORRECT);
}

/* ======================================================================== *
 * SPRMIB suppress — bit 7 set in raw[1] → response suppressed             *
 * ======================================================================== */

void test_2f_sprmib_suppress(void)
{
    /* Use VIN DID 0xF190: raw[1]=0xF1 → bit 7 = suppress_rsp */
    uds_response_t rsp;
    uint8_t raw[] = {0x2F, 0xF1, 0x90, 0x03};  /* freezeCurrentState with suppress */
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked);

    /* Suppressed — handler returns false, no positive response */
    TEST_ASSERT_FALSE(result);
}

/* ======================================================================== *
 * IMLOIF — freezeCurrentState with extra data                             *
 * ======================================================================== */

void test_2f_freeze_extra_data(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x2F, 0x12, 0x34, 0x03, 0x00};  /* mode 0x03 with unexpected byte */
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x2F, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

/* ======================================================================== *
 * IMLOIF — returnControlToStandard with extra data                         *
 * ======================================================================== */

void test_2f_return_extra_data(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x2F, 0x12, 0x34, 0x01, 0x00};  /* mode 0x01 with unexpected byte */
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x2F, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

/* ======================================================================== *
 * IMLOIF — shortTermAdjustment without control parameter                  *
 * ======================================================================== */

void test_2f_adjust_no_param(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x2F, 0x12, 0x34, 0x04};  /* mode 0x04 with no parameter data */
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x2F, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

/* ======================================================================== *
 * Security pass — unlocked SECURED_READ DID works                          *
 * ======================================================================== */

void test_2f_secured_read_with_unlock(void)
{
    unlocked = true;

    uds_response_t rsp;
    uint8_t raw[] = {0x2F, 0x56, 0x78, 0x03};  /* freezeCurrentState on secured DID */
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT8(INPUT_OUTPUT_CONTROL_BY_ID_RSP, rsp.sid);
    /* Frozen data should contain the pre-filled value 0xAA, 0xBB, 0xCC, 0xDD */
    TEST_ASSERT_EQUAL_UINT16(7, rsp.data_len);
    TEST_ASSERT_EQUAL_UINT8(0x56, rsp.data[0]);
    TEST_ASSERT_EQUAL_UINT8(0x78, rsp.data[1]);
    TEST_ASSERT_EQUAL_UINT8(0x03, rsp.data[2]);
    TEST_ASSERT_EQUAL_UINT8(0xAA, rsp.data[3]);
    TEST_ASSERT_EQUAL_UINT8(0xDD, rsp.data[6]);
}

/* ======================================================================== *
 * uds_svc_io_reset clears all overrides                                    *
 * ======================================================================== */

void test_2f_io_reset_clears_all_overrides(void)
{
    /* Set two overrides */
    {
        uds_response_t rsp;
        uint8_t raw[] = {0x2F, 0x12, 0x34, 0x04, 0x11};
        call_handler(raw, sizeof(raw), &rsp, &unlocked);
    }
    TEST_ASSERT_TRUE(uds_svc_io_has_override(0x1234));

    /* Reset all overrides */
    uds_svc_io_reset();
    TEST_ASSERT_FALSE(uds_svc_io_has_override(0x1234));
}

/* ======================================================================== *
 * Test Runner                                                              *
 * ======================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* Positive path — control modes */
    RUN_TEST(test_2f_return_to_standard);
    RUN_TEST(test_2f_reset_to_default);
    RUN_TEST(test_2f_freeze_current_state);
    RUN_TEST(test_2f_short_term_adjust);

    /* Sequence tests */
    RUN_TEST(test_2f_adjust_then_return_to_standard);
    RUN_TEST(test_2f_adjust_then_reset_to_default);

    /* NRC paths */
    RUN_TEST(test_2f_unknown_did);
    RUN_TEST(test_2f_invalid_control_mode);
    RUN_TEST(test_2f_security_denied_secured_read);
    RUN_TEST(test_2f_security_denied_secured_write);
    RUN_TEST(test_2f_imloif_short_request);
    RUN_TEST(test_2f_null_context);
    RUN_TEST(test_2f_sprmib_suppress);
    RUN_TEST(test_2f_freeze_extra_data);
    RUN_TEST(test_2f_return_extra_data);
    RUN_TEST(test_2f_adjust_no_param);

    /* Security positive path */
    RUN_TEST(test_2f_secured_read_with_unlock);

    /* IO state management */
    RUN_TEST(test_2f_io_reset_clears_all_overrides);

    return UNITY_END();
}
