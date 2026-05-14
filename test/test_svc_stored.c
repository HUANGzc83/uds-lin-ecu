/*
 * test_svc_stored.c
 * Stored Data Services — Unit Tests
 *
 * Tests 2 UDS service handlers: 0x14 (ClearDiagnosticInformation) and
 * 0x19 (ReadDTCInformation, 4 subfunctions).
 *
 * Wave 3c Task 11 — Stored Data Services
 */

#include "unity.h"
#include "uds/uds_core.h"
#include "uds/uds_svc_stored.h"
#include "uds/uds_dtc.h"
#include "uds/uds_session.h"
#include <string.h>

/* ======================================================================== *
 * Test Fixture                                                             *
 * ======================================================================== */

static uds_session_context_t session_ctx;
static bool                  unlocked;
static uds_svc_stored_ctx_t  stored_ctx;

void setUp(void)
{
    /* Initialise DTC engine */
    uds_dtc_init();

    /* Initialise session in defaultSession */
    uds_session_init(&session_ctx);

    /* Default: unlocked = false (locked) */
    unlocked = false;

    /* Setup stored data context */
    stored_ctx.sctx     = &session_ctx;
    stored_ctx.unlocked = &unlocked;
}

void tearDown(void)
{
    /* nothing to tear down */
}

/* ======================================================================== *
 * Helpers                                                                  *
 * ======================================================================== */

/**
 * @brief Parse raw request bytes and invoke a handler.
 * Returns the handler's bool result.
 */
static bool call_handler(const uint8_t *raw, uint16_t raw_len,
                         uds_response_t *rsp, void *context,
                         bool (*handler)(const uds_request_t *,
                                         uds_response_t *,
                                         void *))
{
    uds_request_t req;
    uds_status_t status = uds_parse_request(raw, raw_len, &req);
    TEST_ASSERT_EQUAL(UDS_OK, status);
    return handler(&req, rsp, context);
}

/**
 * @brief Assert that a response is a positive response.
 */
static void check_pos_rsp(const uds_response_t *rsp,
                          uint8_t expected_sid,
                          uint8_t expected_subfunc)
{
    TEST_ASSERT_EQUAL_UINT8(expected_sid,      rsp->sid);
    TEST_ASSERT_EQUAL_UINT8(expected_subfunc,  rsp->subfunc_echo);
}

/**
 * @brief Assert that a response is a negative response with expected NRC.
 */
static void check_neg_rsp(const uds_response_t *rsp,
                          uint8_t expected_req_sid,
                          uint8_t expected_nrc)
{
    TEST_ASSERT_EQUAL_UINT8(0x7F,                 rsp->sid);
    TEST_ASSERT_EQUAL_UINT8(expected_req_sid,     rsp->subfunc_echo);
    TEST_ASSERT_NOT_NULL(rsp->data);
    TEST_ASSERT_EQUAL_UINT8(1,                    rsp->data_len);
    TEST_ASSERT_EQUAL_UINT8(expected_nrc,         rsp->data[0]);
}

/**
 * @brief Helper: populate the DTC database with test DTCs.
 *
 * Creates 3 DTCs:
 *   0x010101 — powertrain (group 0x0), status = DTC_CONFIRMED | DTC_TEST_FAILED
 *   0x020202 — powertrain (group 0x0), status = DTC_PENDING
 *   0x230101 — chassis (group 0x2),   status = DTC_CONFIRMED
 * Also sets snapshot/extended data on the first DTC.
 *
 * DTC group is determined by the upper nibble (bits 23:20):
 *   0x0 = Powertrain, 0x2 = Chassis, 0x4 = Body, 0x6 = Network
 */
static void populate_test_dtcs(void)
{
    /* DTC 0x010101: powertrain, confirmed + testFailed */
    uds_dtc_set_status(0x010101, DTC_CONFIRMED | DTC_TEST_FAILED, true);
    {
        uint8_t snap[] = {0xAA, 0xBB, 0xCC, 0xDD};
        uds_dtc_set_snapshot(0x010101, snap, 4);
        uint8_t ext[] = {0x11, 0x22, 0x33};
        uds_dtc_set_extended(0x010101, ext, 3);
    }

    /* DTC 0x020202: powertrain, pending */
    uds_dtc_set_status(0x020202, DTC_PENDING, true);

    /* DTC 0x230101: chassis (upper nibble 0x2), confirmed */
    uds_dtc_set_status(0x230101, DTC_CONFIRMED, true);
}

/* ======================================================================== *
 * Switch to Extended Session Helper                                        *
 * ======================================================================== */

/** @brief Helper: switch to extendedDiagnosticSession for tests that need it */
static void switch_to_extended_session(void)
{
    uds_nrc_t nrc = NRC_GENERAL_REJECT;
    uds_session_switch(&session_ctx, UDS_EXTENDED_SESSION, &nrc);
    TEST_ASSERT_EQUAL_UINT8(UDS_EXTENDED_SESSION, session_ctx.current_session);
}

/* ======================================================================== *
 * 0x14 — ClearDiagnosticInformation Tests                                  *
 * ======================================================================== */

void test_clear_all_dtcs(void)
{
    populate_test_dtcs();
    switch_to_extended_session();
    unlocked = true;

    uds_response_t rsp;
    /* 0x14 with groupOfDTC = 0xFFFFFF (clear all) */
    uint8_t raw[] = {0x14, 0xFF, 0xFF, 0xFF};

    bool result = call_handler(raw, sizeof(raw), &rsp, &stored_ctx,
                               uds_svc_clear_diagnostic_info);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, CLEAR_DIAGNOSTIC_INFO_RSP, 0);

    /* Verify all DTCs cleared */
    uint32_t dtc_list[UDS_DTC_MAX];
    uint16_t remaining = uds_dtc_get_by_status_mask(0xFF, dtc_list, UDS_DTC_MAX);
    TEST_ASSERT_EQUAL_UINT16(0, remaining);
}

void test_clear_by_group(void)
{
    populate_test_dtcs();
    switch_to_extended_session();
    unlocked = true;

    uds_response_t rsp;
    /* Clear powertrain group (upper nibble 0x0) */
    uint8_t raw[] = {0x14, 0x00, 0x00, 0x00};  /* group = 0x000000 */

    bool result = call_handler(raw, sizeof(raw), &rsp, &stored_ctx,
                               uds_svc_clear_diagnostic_info);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, CLEAR_DIAGNOSTIC_INFO_RSP, 0);

    /* Powertrain DTCs (0x010101, 0x020202) should be gone */
    TEST_ASSERT_EQUAL_UINT8(0, uds_dtc_get_status(0x010101));
    TEST_ASSERT_EQUAL_UINT8(0, uds_dtc_get_status(0x020202));

    /* Chassis DTC (0x230101) should still exist */
    TEST_ASSERT_NOT_EQUAL(0, uds_dtc_get_status(0x230101));
}

void test_clear_null_context_returns_nrc22(void)
{
    populate_test_dtcs();
    switch_to_extended_session();
    unlocked = true;

    uds_response_t rsp;
    uint8_t raw[] = {0x14, 0xFF, 0xFF, 0xFF};

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_clear_diagnostic_info);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x14, NRC_CONDITIONS_NOT_CORRECT);
}

void test_clear_default_session_returns_nrc22(void)
{
    /* Keep session in defaultSession */
    populate_test_dtcs();
    unlocked = true;

    uds_response_t rsp;
    uint8_t raw[] = {0x14, 0xFF, 0xFF, 0xFF};

    bool result = call_handler(raw, sizeof(raw), &rsp, &stored_ctx,
                               uds_svc_clear_diagnostic_info);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x14, NRC_CONDITIONS_NOT_CORRECT);
}

void test_clear_locked_security_returns_nrc33(void)
{
    populate_test_dtcs();
    switch_to_extended_session();
    unlocked = false;  /* security locked */

    uds_response_t rsp;
    uint8_t raw[] = {0x14, 0xFF, 0xFF, 0xFF};

    bool result = call_handler(raw, sizeof(raw), &rsp, &stored_ctx,
                               uds_svc_clear_diagnostic_info);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x14, NRC_SECURITY_ACCESS_DENIED);
}

void test_clear_imloif_wrong_length(void)
{
    switch_to_extended_session();
    unlocked = true;

    uds_response_t rsp;
    /* Only 2 bytes of groupOfDTC instead of 3 */
    uint8_t raw[] = {0x14, 0xFF, 0xFF};

    bool result = call_handler(raw, sizeof(raw), &rsp, &stored_ctx,
                               uds_svc_clear_diagnostic_info);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x14, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

/* ======================================================================== *
 * 0x19 — ReadDTCInformation Tests — Subfunction 0x01                       *
 * reportNumberOfDTCByStatusMask                                            *
 * ======================================================================== */

void test_read_number_of_dtc_by_mask(void)
{
    populate_test_dtcs();

    uds_response_t rsp;
    /* 0x19, subfn=0x01, statusMask = DTC_CONFIRMED (0x08) */
    uint8_t raw[] = {0x19, 0x01, 0x08};

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_read_dtc_info);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, READ_DTC_INFO_RSP, 0x01);
    TEST_ASSERT_EQUAL_UINT8(4, rsp.data_len);
    TEST_ASSERT_NOT_NULL(rsp.data);

    /* data[0] = statusMask echo */
    TEST_ASSERT_EQUAL_UINT8(0x08, rsp.data[0]);
    /* data[1] = DTCStatusAvailabilityMask */
    /* DTCs 0x010101 (0x08|0x01=0x09) and 0x230101 (0x08) → OR = 0x09 */
    TEST_ASSERT_EQUAL_UINT8(0x09, rsp.data[1]);
    /* data[2..3] = count = 2 (0x010101 and 0x230101 are confirmed) */
    TEST_ASSERT_EQUAL_UINT8(0x00, rsp.data[2]);
    TEST_ASSERT_EQUAL_UINT8(0x02, rsp.data[3]);
}

void test_read_number_of_dtc_by_mask_all_status(void)
{
    populate_test_dtcs();

    uds_response_t rsp;
    /* statusMask = 0xFF (all status bits) */
    uint8_t raw[] = {0x19, 0x01, 0xFF};

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_read_dtc_info);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, READ_DTC_INFO_RSP, 0x01);

    /* All 3 DTCs should match */
    TEST_ASSERT_EQUAL_UINT8(0x03, rsp.data[3]);  /* count = 3 */
}

void test_read_number_of_dtc_empty_mask(void)
{
    uds_dtc_init();  /* Ensure empty database */
    switch_to_extended_session();

    uds_response_t rsp;
    uint8_t raw[] = {0x19, 0x01, 0x08};

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_read_dtc_info);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, READ_DTC_INFO_RSP, 0x01);

    /* Count should be 0 */
    TEST_ASSERT_EQUAL_UINT8(0x00, rsp.data[3]);
}

void test_read_number_of_dtc_imloif_no_data(void)
{
    uds_response_t rsp;
    /* Missing statusMask */
    uint8_t raw[] = {0x19, 0x01};

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_read_dtc_info);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x19, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

/* ======================================================================== *
 * 0x19 — ReadDTCInformation Tests — Subfunction 0x02                       *
 * reportDTCByStatusMask                                                    *
 * ======================================================================== */

void test_report_dtc_by_mask(void)
{
    populate_test_dtcs();

    uds_response_t rsp;
    /* statusMask = DTC_CONFIRMED (0x08) */
    uint8_t raw[] = {0x19, 0x02, 0x08};

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_read_dtc_info);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, READ_DTC_INFO_RSP, 0x02);
    TEST_ASSERT_NOT_NULL(rsp.data);

    /* data[0] = statusMask echo */
    TEST_ASSERT_EQUAL_UINT8(0x08, rsp.data[0]);
    /* data[1] = DTCStatusAvailabilityMask */
    TEST_ASSERT_EQUAL_UINT8(0x09, rsp.data[1]);

    /* Expect 2 DTC records: 0x010101 and 0x230101
     * Each record is 4 bytes: 3-byte DTC + 1-byte status */
    uint16_t expected_size = 2 + (2 * 4);  /* header + 2 records */
    TEST_ASSERT_EQUAL_UINT16(expected_size, rsp.data_len);

    /* Verify first DTC record: 0x010101 with status 0x09 */
    TEST_ASSERT_EQUAL_UINT8(0x01, rsp.data[2]);
    TEST_ASSERT_EQUAL_UINT8(0x01, rsp.data[3]);
    TEST_ASSERT_EQUAL_UINT8(0x01, rsp.data[4]);
    TEST_ASSERT_EQUAL_UINT8(0x09, rsp.data[5]);  /* DTC_CONFIRMED | DTC_TEST_FAILED */

    /* Verify second DTC record: 0x230101 with status 0x08 */
    TEST_ASSERT_EQUAL_UINT8(0x23, rsp.data[6]);
    TEST_ASSERT_EQUAL_UINT8(0x01, rsp.data[7]);
    TEST_ASSERT_EQUAL_UINT8(0x01, rsp.data[8]);
    TEST_ASSERT_EQUAL_UINT8(0x08, rsp.data[9]);  /* DTC_CONFIRMED */
}

void test_report_dtc_by_mask_imloif_no_data(void)
{
    uds_response_t rsp;
    /* Missing statusMask */
    uint8_t raw[] = {0x19, 0x02};

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_read_dtc_info);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x19, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

/* ======================================================================== *
 * 0x19 — ReadDTCInformation Tests — Subfunction 0x06                       *
 * reportDTCExtendedDataRecordByDTCNumber                                   *
 * ======================================================================== */

void test_report_extended_snapshot(void)
{
    populate_test_dtcs();

    uds_response_t rsp;
    /* DTC = 0x010101, recordNumber = 0x01 (snapshot) */
    uint8_t raw[] = {0x19, 0x06, 0x01, 0x01, 0x01, 0x01};

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_read_dtc_info);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, READ_DTC_INFO_RSP, 0x06);
    TEST_ASSERT_NOT_NULL(rsp.data);

    /* data[0..2] = DTC 0x010101 */
    TEST_ASSERT_EQUAL_UINT8(0x01, rsp.data[0]);
    TEST_ASSERT_EQUAL_UINT8(0x01, rsp.data[1]);
    TEST_ASSERT_EQUAL_UINT8(0x01, rsp.data[2]);
    /* data[3] = recordNumber */
    TEST_ASSERT_EQUAL_UINT8(0x01, rsp.data[3]);
    /* data[4..7] = snapshot data: 0xAA, 0xBB, 0xCC, 0xDD */
    TEST_ASSERT_EQUAL_UINT8(0xAA, rsp.data[4]);
    TEST_ASSERT_EQUAL_UINT8(0xBB, rsp.data[5]);
    TEST_ASSERT_EQUAL_UINT8(0xCC, rsp.data[6]);
    TEST_ASSERT_EQUAL_UINT8(0xDD, rsp.data[7]);
    /* Total: 4 (header) + 4 (snapshot) = 8 bytes */
    TEST_ASSERT_EQUAL_UINT16(8, rsp.data_len);
}

void test_report_extended_data_record(void)
{
    populate_test_dtcs();

    uds_response_t rsp;
    /* DTC = 0x010101, recordNumber = 0x02 (extended data) */
    uint8_t raw[] = {0x19, 0x06, 0x01, 0x01, 0x01, 0x02};

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_read_dtc_info);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, READ_DTC_INFO_RSP, 0x06);

    /* data[0..2] = DTC 0x010101 */
    TEST_ASSERT_EQUAL_UINT8(0x01, rsp.data[0]);
    TEST_ASSERT_EQUAL_UINT8(0x01, rsp.data[1]);
    TEST_ASSERT_EQUAL_UINT8(0x01, rsp.data[2]);
    /* data[3] = recordNumber */
    TEST_ASSERT_EQUAL_UINT8(0x02, rsp.data[3]);
    /* data[4..6] = extended data: 0x11, 0x22, 0x33 */
    TEST_ASSERT_EQUAL_UINT8(0x11, rsp.data[4]);
    TEST_ASSERT_EQUAL_UINT8(0x22, rsp.data[5]);
    TEST_ASSERT_EQUAL_UINT8(0x33, rsp.data[6]);
    /* Total: 4 (header) + 3 (extended) = 7 bytes */
    TEST_ASSERT_EQUAL_UINT16(7, rsp.data_len);
}

void test_report_extended_unsupported_record_returns_nrc31(void)
{
    populate_test_dtcs();

    uds_response_t rsp;
    /* DTC = 0x010101, recordNumber = 0x00 (not supported) */
    uint8_t raw[] = {0x19, 0x06, 0x01, 0x01, 0x01, 0x00};

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_read_dtc_info);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x19, NRC_REQUEST_OUT_OF_RANGE);
}

void test_report_extended_dtc_not_found_returns_nrc31(void)
{
    uds_dtc_init();  /* Empty database */

    uds_response_t rsp;
    /* DTC = 0x999999 (doesn't exist), recordNumber = 0x01 */
    uint8_t raw[] = {0x19, 0x06, 0x99, 0x99, 0x99, 0x01};

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_read_dtc_info);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x19, NRC_REQUEST_OUT_OF_RANGE);
}

void test_report_extended_imloif_short_data(void)
{
    uds_response_t rsp;
    /* Only 2 bytes of data instead of 4 */
    uint8_t raw[] = {0x19, 0x06, 0x01, 0x01};

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_read_dtc_info);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x19, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

/* ======================================================================== *
 * 0x19 — ReadDTCInformation Tests — Subfunction 0x0A                       *
 * reportSupportedDTC                                                       *
 * ======================================================================== */

void test_report_supported_dtc(void)
{
    populate_test_dtcs();

    uds_response_t rsp;
    uint8_t raw[] = {0x19, 0x0A};

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_read_dtc_info);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, READ_DTC_INFO_RSP, 0x0A);
    TEST_ASSERT_NOT_NULL(rsp.data);

    /* data[0] = statusAvailabilityMask = 0xFF */
    TEST_ASSERT_EQUAL_UINT8(0xFF, rsp.data[0]);
    /* data[1] = DTCStatusAvailabilityMask (OR of all 3 DTCs) */
    /* 0x010101: 0x09, 0x020202: 0x04, 0x230101: 0x08 → OR = 0x0D */
    TEST_ASSERT_EQUAL_UINT8(0x0D, rsp.data[1]);

    /* Expect 3 DTC records = 2 + (3 * 4) = 14 bytes */
    TEST_ASSERT_EQUAL_UINT16(14, rsp.data_len);
}

void test_report_supported_dtc_imloif_extra_data(void)
{
    uds_response_t rsp;
    /* Unexpected additional data byte */
    uint8_t raw[] = {0x19, 0x0A, 0x01};

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_read_dtc_info);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x19, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

/* ======================================================================== *
 * 0x19 — ReadDTCInformation Tests — Invalid Subfunction                    *
 * ======================================================================== */

void test_read_dtc_invalid_subfunction_returns_nrc12(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x19, 0xFF, 0x01};  /* subfunction 0xFF is invalid */

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_read_dtc_info);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x19, NRC_SUB_FUNCTION_NOT_SUPPORTED);
}

/* ======================================================================== *
 * 0x19 — SPRMIB Suppression Tests                                         *
 * ======================================================================== */

void test_read_number_of_dtc_sprmib_suppresses(void)
{
    populate_test_dtcs();

    uds_response_t rsp;
    /* subfunction 0x01 with suppress bit = 0x81 */
    uint8_t raw[] = {0x19, 0x81, 0x08};

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_read_dtc_info);

    TEST_ASSERT_FALSE(result);
}

void test_report_dtc_by_mask_sprmib_suppresses(void)
{
    populate_test_dtcs();

    uds_response_t rsp;
    /* subfunction 0x02 with suppress bit = 0x82 */
    uint8_t raw[] = {0x19, 0x82, 0x08};

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_read_dtc_info);

    TEST_ASSERT_FALSE(result);
}

/* ======================================================================== *
 * Test Runner                                                              *
 * ======================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* 0x14 — ClearDiagnosticInformation */
    RUN_TEST(test_clear_all_dtcs);
    RUN_TEST(test_clear_by_group);
    RUN_TEST(test_clear_null_context_returns_nrc22);
    RUN_TEST(test_clear_default_session_returns_nrc22);
    RUN_TEST(test_clear_locked_security_returns_nrc33);
    RUN_TEST(test_clear_imloif_wrong_length);

    /* 0x19 — Subfunction 0x01: reportNumberOfDTCByStatusMask */
    RUN_TEST(test_read_number_of_dtc_by_mask);
    RUN_TEST(test_read_number_of_dtc_by_mask_all_status);
    RUN_TEST(test_read_number_of_dtc_empty_mask);
    RUN_TEST(test_read_number_of_dtc_imloif_no_data);

    /* 0x19 — Subfunction 0x02: reportDTCByStatusMask */
    RUN_TEST(test_report_dtc_by_mask);
    RUN_TEST(test_report_dtc_by_mask_imloif_no_data);

    /* 0x19 — Subfunction 0x06: reportDTCExtendedDataRecordByDTCNumber */
    RUN_TEST(test_report_extended_snapshot);
    RUN_TEST(test_report_extended_data_record);
    RUN_TEST(test_report_extended_unsupported_record_returns_nrc31);
    RUN_TEST(test_report_extended_dtc_not_found_returns_nrc31);
    RUN_TEST(test_report_extended_imloif_short_data);

    /* 0x19 — Subfunction 0x0A: reportSupportedDTC */
    RUN_TEST(test_report_supported_dtc);
    RUN_TEST(test_report_supported_dtc_imloif_extra_data);

    /* 0x19 — Invalid subfunction and SPRMIB */
    RUN_TEST(test_read_dtc_invalid_subfunction_returns_nrc12);
    RUN_TEST(test_read_number_of_dtc_sprmib_suppresses);
    RUN_TEST(test_report_dtc_by_mask_sprmib_suppresses);

    return UNITY_END();
}
