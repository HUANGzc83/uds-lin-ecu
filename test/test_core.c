/*
 * test_core.c
 * UDS PDU Parser/Serializer unit tests — Wave 2 Task 6
 */

#include "unity.h"
#include "uds/uds_core.h"

void setUp(void)
{
    /* Called before each test */
}

void tearDown(void)
{
    /* Called after each test */
}

/* ======================================================================== *
 * Parse: DiagnosticSessionControl request (SID + SubFunction)              *
 * ======================================================================== */
void test_parse_dsc_request(void)
{
    uint8_t raw[] = {0x10, 0x03};
    uds_request_t req;
    uds_status_t status;

    status = uds_parse_request(raw, sizeof(raw), &req);
    TEST_ASSERT_EQUAL(UDS_OK, status);
    TEST_ASSERT_EQUAL(0x10, req.sid);
    TEST_ASSERT_EQUAL(0x03, req.subfunction.value);
    TEST_ASSERT_EQUAL(0,    req.subfunction.suppress_rsp);
}

/* ======================================================================== *
 * Parse: SubFunction with suppressPosRspMsgIndicationBit (bit 7)          *
 * ======================================================================== */
void test_parse_suppress_rsp_bit(void)
{
    uint8_t raw[] = {0x10, 0x83};   /* 0x83 = (1<<7) | 0x03 */
    uds_request_t req;
    uds_status_t status;

    status = uds_parse_request(raw, sizeof(raw), &req);
    TEST_ASSERT_EQUAL(UDS_OK, status);
    TEST_ASSERT_EQUAL(0x10, req.sid);
    TEST_ASSERT_EQUAL(0x03, req.subfunction.value);
    TEST_ASSERT_EQUAL(1,    req.subfunction.suppress_rsp);
}

/* ======================================================================== *
 * Parse: SID-only request (no SubFunction byte)                           *
 * ======================================================================== */
void test_parse_sid_only(void)
{
    uint8_t raw[] = {0x22};   /* ReadDataByIdentifier — SID only */
    uds_request_t req;
    uds_status_t status;

    status = uds_parse_request(raw, sizeof(raw), &req);
    TEST_ASSERT_EQUAL(UDS_OK, status);
    TEST_ASSERT_EQUAL(0x22, req.sid);
    TEST_ASSERT_EQUAL(0,    req.subfunction.value);
    TEST_ASSERT_EQUAL(0,    req.subfunction.suppress_rsp);
    TEST_ASSERT_EQUAL(0,    req.data_len);
}

/* ======================================================================== *
 * Serialize: positive response with subfunction echo and payload           *
 * ======================================================================== */
void test_serialize_positive_response(void)
{
    uint8_t buf[16];
    uint16_t len = sizeof(buf);
    uint8_t payload[] = {0x01, 0x02, 0x03};
    uds_response_t rsp;

    rsp.sid          = 0x50;                 /* DiagnosticSessionControl response */
    rsp.subfunc_echo = 0x01;                 /* defaultSession echo */
    rsp.data         = payload;
    rsp.data_len     = sizeof(payload);

    uds_status_t status = uds_serialize_response(&rsp, buf, &len);
    TEST_ASSERT_EQUAL(UDS_OK, status);
    TEST_ASSERT_EQUAL(1 + 1 + 3, len);       /* SID + subfunc_echo + payload */
    TEST_ASSERT_EQUAL(0x50, buf[0]);
    TEST_ASSERT_EQUAL(0x01, buf[1]);
    TEST_ASSERT_EQUAL(0x01, buf[2]);
    TEST_ASSERT_EQUAL(0x02, buf[3]);
    TEST_ASSERT_EQUAL(0x03, buf[4]);
}

/* ======================================================================== *
 * Serialize: positive response with no payload (subfunction echo only)    *
 * ======================================================================== */
void test_serialize_positive_response_no_payload(void)
{
    uint8_t buf[16];
    uint16_t len = sizeof(buf);
    uds_response_t rsp;

    rsp.sid          = 0x62;   /* ReadDataByIdentifier response */
    rsp.subfunc_echo = 0;
    rsp.data         = NULL;
    rsp.data_len     = 0;

    uds_status_t status = uds_serialize_response(&rsp, buf, &len);
    TEST_ASSERT_EQUAL(UDS_OK, status);
    TEST_ASSERT_EQUAL(2, len);       /* SID + subfunc_echo */
    TEST_ASSERT_EQUAL(0x62, buf[0]);
    TEST_ASSERT_EQUAL(0,    buf[1]); /* subfunc_echo = 0 */
}

/* ======================================================================== *
 * Serialize: negative response [0x7F][SID][NRC]                           *
 * ======================================================================== */
void test_serialize_negative_response(void)
{
    uint8_t buf[16];
    uint16_t len = sizeof(buf);

    uds_status_t status = uds_serialize_negative_response(
        0x10, NRC_SERVICE_NOT_SUPPORTED, buf, &len);

    TEST_ASSERT_EQUAL(UDS_OK, status);
    TEST_ASSERT_EQUAL(3, len);
    TEST_ASSERT_EQUAL(0x7F, buf[0]);
    TEST_ASSERT_EQUAL(0x10, buf[1]);
    TEST_ASSERT_EQUAL(0x11, buf[2]);  /* NRC_SERVICE_NOT_SUPPORTED */
}

/* ======================================================================== *
 * Parse: reject empty buffer (len == 0)                                   *
 * ======================================================================== */
void test_reject_empty(void)
{
    uds_request_t req;

    /* NULL raw with len=0 */
    uds_status_t status = uds_parse_request(NULL, 0, &req);
    TEST_ASSERT_EQUAL(UDS_ERR_TOO_SHORT, status);

    /* Non-NULL raw with len=0 */
    uint8_t dummy;
    status = uds_parse_request(&dummy, 0, &req);
    TEST_ASSERT_EQUAL(UDS_ERR_TOO_SHORT, status);
}

/* ======================================================================== *
 * Parse: reject truncated / NULL parameters                               *
 * ======================================================================== */
void test_reject_null_parameters(void)
{
    uint8_t raw[] = {0x10, 0x03};

    /* NULL req pointer */
    uds_status_t status = uds_parse_request(raw, sizeof(raw), NULL);
    TEST_ASSERT_EQUAL(UDS_ERR_PARSE, status);

    /* NULL raw pointer with len > 0 */
    status = uds_parse_request(NULL, sizeof(raw), &(uds_request_t){0});
    TEST_ASSERT_EQUAL(UDS_ERR_PARSE, status);
}

/* ======================================================================== *
 * uds_sid_to_response_sid: request SID → response SID mapping             *
 * ======================================================================== */
void test_sid_to_response_sid(void)
{
    TEST_ASSERT_EQUAL(0x50, uds_sid_to_response_sid(0x10));
    TEST_ASSERT_EQUAL(0x51, uds_sid_to_response_sid(0x11));
    TEST_ASSERT_EQUAL(0x62, uds_sid_to_response_sid(0x22));
    TEST_ASSERT_EQUAL(0x7E, uds_sid_to_response_sid(0x3E));
    TEST_ASSERT_EQUAL(0xC4, uds_sid_to_response_sid(0x84));
    TEST_ASSERT_EQUAL(0xC7, uds_sid_to_response_sid(0x87));
}

/* ======================================================================== *
 * uds_is_positive_response: distinguish positive vs negative response     *
 * ======================================================================== */
void test_is_positive_response(void)
{
    uint8_t pos_rsp[] = {0x50, 0x01, 0x02};
    uint8_t neg_rsp[] = {0x7F, 0x10, 0x11};
    uint8_t zero_rsp[] = {0x00};

    TEST_ASSERT_TRUE( uds_is_positive_response(pos_rsp));
    TEST_ASSERT_FALSE(uds_is_positive_response(neg_rsp));
    TEST_ASSERT_TRUE( uds_is_positive_response(zero_rsp));  /* 0x00 != 0x7F */
}

/* ======================================================================== *
 * Serialize: reject NULL parameters                                       *
 * ======================================================================== */
void test_serialize_reject_null(void)
{
    uint8_t buf[16];
    uint16_t len = sizeof(buf);
    uds_response_t rsp = {0};

    /* NULL response pointer */
    TEST_ASSERT_EQUAL(UDS_ERR_PARSE,
        uds_serialize_response(NULL, buf, &len));

    /* NULL buffer pointer */
    TEST_ASSERT_EQUAL(UDS_ERR_PARSE,
        uds_serialize_response(&rsp, NULL, &len));

    /* NULL len pointer */
    TEST_ASSERT_EQUAL(UDS_ERR_PARSE,
        uds_serialize_response(&rsp, buf, NULL));

    /* Negative response: NULL buffer */
    TEST_ASSERT_EQUAL(UDS_ERR_PARSE,
        uds_serialize_negative_response(0x10, NRC_GENERAL_REJECT, NULL, &len));

    /* Negative response: NULL len */
    TEST_ASSERT_EQUAL(UDS_ERR_PARSE,
        uds_serialize_negative_response(0x10, NRC_GENERAL_REJECT, buf, NULL));
}

/* ======================================================================== *
 * Serialize: buffer too small for output                                  *
 * ======================================================================== */
void test_serialize_buffer_too_small(void)
{
    uint8_t buf[2];
    uint16_t len = 2;
    uint8_t payload[] = {0xAA, 0xBB};
    uds_response_t rsp;

    rsp.sid          = 0x50;
    rsp.subfunc_echo = 0x01;
    rsp.data         = payload;
    rsp.data_len     = 2;   /* total needed = 1+1+2 = 4 > 2 */

    uds_status_t status = uds_serialize_response(&rsp, buf, &len);
    TEST_ASSERT_EQUAL(UDS_ERR_SERIALIZE, status);
}

/* ======================================================================== *
 * Test runner                                                              *
 * ======================================================================== */
int main(void)
{
    UNITY_BEGIN();

    /* Parse tests */
    RUN_TEST(test_parse_dsc_request);
    RUN_TEST(test_parse_suppress_rsp_bit);
    RUN_TEST(test_parse_sid_only);
    RUN_TEST(test_reject_empty);
    RUN_TEST(test_reject_null_parameters);

    /* Serialize tests */
    RUN_TEST(test_serialize_positive_response);
    RUN_TEST(test_serialize_positive_response_no_payload);
    RUN_TEST(test_serialize_negative_response);
    RUN_TEST(test_serialize_reject_null);
    RUN_TEST(test_serialize_buffer_too_small);

    /* Utility tests */
    RUN_TEST(test_sid_to_response_sid);
    RUN_TEST(test_is_positive_response);

    return UNITY_END();
}
