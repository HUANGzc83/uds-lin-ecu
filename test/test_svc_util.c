/*
 * test_svc_util.c
 * Shared UDS Response Helper Function Unit Tests — Wave 1 Task 8
 *
 * Tests uds_set_neg_rsp(), uds_set_pos_rsp(), and uds_should_suppress().
 *
 * This is the RED (test) stage — functions are declared in uds_svc_util.h
 * but NOT yet implemented.  Expected: link failure with undefined references.
 */

#include "unity.h"
#include "uds/uds_core.h"
#include "uds/uds_svc_util.h"

void setUp(void)
{
    /* Called before each test */
}

void tearDown(void)
{
    /* Called after each test */
}

/* ======================================================================== *
 * uds_set_neg_rsp — 4-param negative response helper                       *
 * ======================================================================== */

/* NULL response pointer → UDS_ERR_PARSE */
void test_set_neg_rsp_null(void)
{
    uds_status_t status = uds_set_neg_rsp(NULL, 0x10,
                                           NRC_GENERAL_REJECT, 0);
    TEST_ASSERT_EQUAL(UDS_ERR_PARSE, status);
}

/* 4-param version with subfunction echo populated */
void test_set_neg_rsp_subfunc(void)
{
    uds_response_t rsp;
    uds_status_t status;

    status = uds_set_neg_rsp(&rsp, 0x10, NRC_CONDITIONS_NOT_CORRECT, 0x03);
    TEST_ASSERT_EQUAL(UDS_OK, status);

    /* Negative response prefix */
    TEST_ASSERT_EQUAL(0x7F, rsp.sid);

    /* Subfunction echo (4th param) stored in rsp.subfunc_echo */
    TEST_ASSERT_EQUAL(0x03, rsp.subfunc_echo);

    /* Data payload holds {request_SID, NRC} */
    TEST_ASSERT_NOT_NULL(rsp.data);
    TEST_ASSERT_EQUAL(2, rsp.data_len);
    TEST_ASSERT_EQUAL(0x10, rsp.data[0]);   /* request SID */
    TEST_ASSERT_EQUAL(0x22, rsp.data[1]);   /* NRC_CONDITIONS_NOT_CORRECT */
}

/* ======================================================================== *
 * uds_set_neg_rsp_sid — 3-param wrapper for SID-only services              *
 * ======================================================================== */

/* SID-only service: subfunc_echo defaults to 0 */
void test_set_neg_rsp_sid_only(void)
{
    uds_response_t rsp;
    uds_status_t status;

    status = uds_set_neg_rsp_sid(&rsp, 0x22, NRC_SERVICE_NOT_SUPPORTED);
    TEST_ASSERT_EQUAL(UDS_OK, status);

    TEST_ASSERT_EQUAL(0x7F, rsp.sid);
    TEST_ASSERT_EQUAL(0,    rsp.subfunc_echo);   /* no subfunction */
    TEST_ASSERT_NOT_NULL(rsp.data);
    TEST_ASSERT_EQUAL(2, rsp.data_len);
    TEST_ASSERT_EQUAL(0x22, rsp.data[0]);         /* request SID */
    TEST_ASSERT_EQUAL(0x11, rsp.data[1]);         /* NRC_SERVICE_NOT_SUPPORTED */
}

/* ======================================================================== *
 * uds_set_neg_rsp — consistency with uds_serialize_negative_response       *
 * ======================================================================== */

/* Data stored by helper should match data that the existing serializer uses */
void test_set_neg_rsp_consistency(void)
{
    uint8_t expected[3];
    uint16_t elen = sizeof(expected);

    /* Existing serializer produces: [0x7F][0x10][0x10] */
    uds_serialize_negative_response(
        0x10, NRC_GENERAL_REJECT, expected, &elen);
    TEST_ASSERT_EQUAL(3, elen);

    uds_response_t rsp;
    uds_set_neg_rsp(&rsp, 0x10, NRC_GENERAL_REJECT, 0);

    /* Helper stores {0x10, 0x10} — matches bytes [1] and [2] of serializer */
    TEST_ASSERT_EQUAL(expected[1], rsp.data[0]);   /* request SID */
    TEST_ASSERT_EQUAL(expected[2], rsp.data[1]);   /* NRC */
}

/* ======================================================================== *
 * uds_set_pos_rsp — 4-param positive response helper (SID-only)            *
 * ======================================================================== */

/* NULL response pointer → UDS_ERR_PARSE */
void test_set_pos_rsp_null(void)
{
    uds_status_t status = uds_set_pos_rsp(NULL, 0x22, NULL, 0);
    TEST_ASSERT_EQUAL(UDS_ERR_PARSE, status);
}

/* Positive response with payload data attached */
void test_set_pos_rsp_with_payload(void)
{
    uint8_t payload[] = {0x01, 0x02, 0x03};
    uds_response_t rsp;
    uds_status_t status;

    status = uds_set_pos_rsp(&rsp, 0x22, payload, sizeof(payload));
    TEST_ASSERT_EQUAL(UDS_OK, status);

    TEST_ASSERT_EQUAL(0x62, rsp.sid);            /* 0x22 + 0x40 */
    TEST_ASSERT_EQUAL(0,    rsp.subfunc_echo);   /* SID-only → 0 */
    TEST_ASSERT_EQUAL(payload, rsp.data);         /* pointer preserved */
    TEST_ASSERT_EQUAL(sizeof(payload), rsp.data_len);
}

/* Positive response with no payload (data=NULL, len=0) */
void test_set_pos_rsp_no_payload(void)
{
    uds_response_t rsp;
    uds_status_t status;

    status = uds_set_pos_rsp(&rsp, 0x3E, NULL, 0);
    TEST_ASSERT_EQUAL(UDS_OK, status);

    TEST_ASSERT_EQUAL(0x7E, rsp.sid);            /* 0x3E + 0x40 */
    TEST_ASSERT_EQUAL(0,    rsp.subfunc_echo);
    TEST_ASSERT_NULL(rsp.data);
    TEST_ASSERT_EQUAL(0, rsp.data_len);
}

/* ======================================================================== *
 * uds_set_pos_rsp — consistency with uds_serialize_response                *
 * ======================================================================== */

/* Helper + existing serializer should produce correct positive response */
void test_set_pos_rsp_consistency(void)
{
    uint8_t payload[] = {0xAA, 0xBB};
    uds_response_t rsp;
    uint8_t buf[16];
    uint16_t len = sizeof(buf);

    uds_set_pos_rsp(&rsp, 0x22, payload, sizeof(payload));

    uds_status_t status = uds_serialize_response(&rsp, buf, &len);
    TEST_ASSERT_EQUAL(UDS_OK, status);

    /* Expected wire format: [0x62][0x00][0xAA][0xBB] */
    TEST_ASSERT_EQUAL(4, len);
    TEST_ASSERT_EQUAL(0x62, buf[0]);   /* response SID = 0x22 + 0x40 */
    TEST_ASSERT_EQUAL(0x00, buf[1]);   /* subfunc_echo = 0 */
    TEST_ASSERT_EQUAL(0xAA, buf[2]);   /* payload[0] */
    TEST_ASSERT_EQUAL(0xBB, buf[3]);   /* payload[1] */
}

/* ======================================================================== *
 * uds_should_suppress — suppressPosRspMsgIndicationBit checks              *
 * ======================================================================== */

/* suppress_rsp bit SET (bit 7 = 1) → returns true */
void test_should_suppress_set(void)
{
    uint8_t raw[] = {0x10, 0x83};   /* 0x83 = (1<<7) | 0x03 */
    uds_request_t req;

    uds_parse_request(raw, sizeof(raw), &req);
    TEST_ASSERT_EQUAL(1, req.subfunction.suppress_rsp);   /* sanity */

    TEST_ASSERT_TRUE(uds_should_suppress(&req));
}

/* suppress_rsp bit CLEAR (bit 7 = 0) → returns false */
void test_should_suppress_clear(void)
{
    uint8_t raw[] = {0x10, 0x03};
    uds_request_t req;

    uds_parse_request(raw, sizeof(raw), &req);
    TEST_ASSERT_EQUAL(0, req.subfunction.suppress_rsp);   /* sanity */

    TEST_ASSERT_FALSE(uds_should_suppress(&req));
}

/* SID-only request (no subfunction byte) → suppress_rsp = 0 → false */
void test_should_suppress_sid_only(void)
{
    uint8_t raw[] = {0x22};
    uds_request_t req;

    uds_parse_request(raw, sizeof(raw), &req);

    TEST_ASSERT_FALSE(uds_should_suppress(&req));
}

/* NULL request pointer → returns false (defensive) */
void test_should_suppress_null(void)
{
    TEST_ASSERT_FALSE(uds_should_suppress(NULL));
}

/* ======================================================================== *
 * Test runner                                                              *
 * ======================================================================== */
int main(void)
{
    UNITY_BEGIN();

    /* uds_set_neg_rsp (4-param) and wrapper (3-param) */
    RUN_TEST(test_set_neg_rsp_null);
    RUN_TEST(test_set_neg_rsp_subfunc);
    RUN_TEST(test_set_neg_rsp_sid_only);
    RUN_TEST(test_set_neg_rsp_consistency);

    /* uds_set_pos_rsp (4-param) */
    RUN_TEST(test_set_pos_rsp_null);
    RUN_TEST(test_set_pos_rsp_with_payload);
    RUN_TEST(test_set_pos_rsp_no_payload);
    RUN_TEST(test_set_pos_rsp_consistency);

    /* uds_should_suppress */
    RUN_TEST(test_should_suppress_set);
    RUN_TEST(test_should_suppress_clear);
    RUN_TEST(test_should_suppress_sid_only);
    RUN_TEST(test_should_suppress_null);

    return UNITY_END();
}
