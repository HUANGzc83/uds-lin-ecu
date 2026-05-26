/*
 * test_svc_service.c
 * Master Service Dispatch — Unit Tests
 *
 * Tests the central dispatch engine (uds_service_dispatch) including
 * SID routing, session availability checks, functional-addressing NRC
 * suppression (ISO 14229-1 Tables 4-7), SPRMIB propagation, and stub
 * handler registration.
 *
 * Wave 3d Task 18 — Master Service Dispatch (UDS Request Router)
 */

#include "unity.h"
#include "uds/uds_core.h"
#include "uds/uds_service.h"
#include "uds/uds_svc_diagcomm.h"
#include "uds/uds_svc_data.h"
#include "uds/uds_svc_stored.h"
#include "uds/uds_svc_io.h"
#include "uds/uds_svc_routine.h"
#include "uds/uds_svc_upload.h"
#include "uds/uds_svc_auth.h"
#include "uds/uds_session.h"
#include <string.h>

/* ======================================================================== *
 * Test Fixture                                                             *
 * ======================================================================== */

/** @brief Static NRC buffer for test-stub negative responses */
static uint8_t g_test_nrc_buf[1];

/*
 * Test stub handlers
 */

/** @brief Simple handler that always returns a positive response echo. */
static bool stub_echo_handler(const uds_request_t *req,
                               uds_response_t      *rsp,
                               void                *context)
{
    (void)context;
    rsp->sid          = (uint8_t)(req->sid + 0x40);
    rsp->subfunc_echo = req->subfunction.value;
    rsp->data         = NULL;
    rsp->data_len     = 0;
    return true;
}

/** @brief Handler that always returns false (simulates SPRMIB suppression). */
static bool stub_suppress_handler(const uds_request_t *req,
                                   uds_response_t      *rsp,
                                   void                *context)
{
    (void)req;
    (void)rsp;
    (void)context;
    return false;
}

/** @brief Handler that returns a negative response with a given NRC. */
static bool stub_nrc_handler(const uds_request_t *req,
                              uds_response_t      *rsp,
                              void                *context)
{
    uint8_t nrc = (context != NULL) ? *(const uint8_t *)context : 0x10;
    g_test_nrc_buf[0] = nrc;
    rsp->sid          = 0x7F;
    rsp->subfunc_echo = req->sid;
    rsp->data         = g_test_nrc_buf;
    rsp->data_len     = 1;
    return true;
}

/* ======================================================================== *
 * Helpers                                                                  *
 * ======================================================================== */

/** @brief Parse raw bytes and dispatch in one call. */
static bool dispatch_raw(const uint8_t *raw, uint16_t raw_len,
                          uds_response_t *rsp,
                          uint8_t current_session,
                          uds_addressing_t ta_type,
                          void *context)
{
    uds_request_t req;
    uds_status_t status = uds_parse_request(raw, raw_len, &req);
    TEST_ASSERT_EQUAL(UDS_OK, status);
    return uds_service_dispatch(&req, rsp, current_session, ta_type, context);
}

/** @brief Assert positive response format. */
static void check_pos_rsp(const uds_response_t *rsp,
                           uint8_t expected_sid,
                           uint8_t expected_subfunc)
{
    TEST_ASSERT_EQUAL_UINT8(expected_sid,      rsp->sid);
    TEST_ASSERT_EQUAL_UINT8(expected_subfunc,  rsp->subfunc_echo);
}

/** @brief Assert negative response format with expected NRC. */
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

/* ======================================================================== *
 * setUp / tearDown                                                         *
 * ======================================================================== */

void setUp(void)
{
    uds_service_clear();
}

void tearDown(void)
{
    /* nothing to tear down */
}

/* ======================================================================== *
 * Registration Tests                                                       *
 * ======================================================================== */

void test_register_and_find(void)
{
    /* Register a simple handler */
    bool ok = uds_service_register(0x3E, stub_echo_handler, UDS_SESSION_ALL);
    TEST_ASSERT_TRUE(ok);

    /* Find it */
    const uds_service_entry_t *entry = uds_service_find(0x3E);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT8(0x3E, entry->sid);
    TEST_ASSERT_EQUAL(stub_echo_handler, entry->handler);
    TEST_ASSERT_EQUAL_UINT8(UDS_SESSION_ALL, entry->session_mask);
    TEST_ASSERT_TRUE(entry->active);
}

void test_register_duplicate_sid_fails(void)
{
    uds_service_register(0x3E, stub_echo_handler, UDS_SESSION_ALL);
    bool ok = uds_service_register(0x3E, stub_echo_handler, UDS_SESSION_ALL);
    TEST_ASSERT_FALSE(ok);
}

void test_register_null_handler_fails(void)
{
    bool ok = uds_service_register(0x3E, NULL, UDS_SESSION_ALL);
    TEST_ASSERT_FALSE(ok);
}

void test_find_nonexistent_sid(void)
{
    const uds_service_entry_t *entry = uds_service_find(0xFF);
    TEST_ASSERT_NULL(entry);
}

void test_get_count(void)
{
    /* Initially zero */
    TEST_ASSERT_EQUAL_UINT8(0, uds_service_get_count());

    uds_service_register(0x10, stub_echo_handler, UDS_SESSION_ALL);
    TEST_ASSERT_EQUAL_UINT8(1, uds_service_get_count());

    uds_service_register(0x11, stub_echo_handler, UDS_SESSION_ALL);
    TEST_ASSERT_EQUAL_UINT8(2, uds_service_get_count());
}

void test_register_table_full_fails(void)
{
    /* Fill the entire dispatch table with unique SIDs */
    for (uint8_t i = 0; i < UDS_SERVICE_TABLE_MAX; i++)
    {
        bool ok = uds_service_register((uint8_t)(0x10 + i), stub_echo_handler, UDS_SESSION_ALL);
        TEST_ASSERT_TRUE(ok);
    }

    /* Verify count matches max */
    TEST_ASSERT_EQUAL_UINT8(UDS_SERVICE_TABLE_MAX, uds_service_get_count());

    /* Next registration must fail — table is full */
    bool ok = uds_service_register(0x99, stub_echo_handler, UDS_SESSION_ALL);
    TEST_ASSERT_FALSE(ok);
}

/* ======================================================================== *
 * Dispatch Routing Tests                                                   *
 * ======================================================================== */

void test_dispatch_routing_known_sid(void)
{
    uds_service_register(0x3E, stub_echo_handler, UDS_SESSION_ALL);

    uds_response_t rsp;
    uint8_t raw[] = {0x3E, 0x00};

    bool send = dispatch_raw(raw, sizeof(raw), &rsp,
                              UDS_DEFAULT_SESSION, UDS_PHYSICAL, NULL);

    TEST_ASSERT_TRUE(send);
    check_pos_rsp(&rsp, 0x7E, 0x00);
}

void test_dispatch_unknown_sid_physical_returns_nrc11(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x12, 0x00};  /* SID 0x12 is not registered */

    bool send = dispatch_raw(raw, sizeof(raw), &rsp,
                              UDS_DEFAULT_SESSION, UDS_PHYSICAL, NULL);

    TEST_ASSERT_TRUE(send);
    check_neg_rsp(&rsp, 0x12, NRC_SERVICE_NOT_SUPPORTED);
}

void test_dispatch_unknown_sid_functional_suppressed(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x12, 0x00};  /* SID 0x12 is not registered */

    bool send = dispatch_raw(raw, sizeof(raw), &rsp,
                              UDS_DEFAULT_SESSION, UDS_FUNCTIONAL, NULL);

    /* For functional addressing, unknown SID must be silently ignored */
    TEST_ASSERT_FALSE(send);
}

/* ======================================================================== *
 * Session Availability Tests                                                *
 * ======================================================================== */

void test_session_mismatch_returns_nrc7f(void)
{
    /* Register service only available in programming session */
    uds_service_register(0x22, stub_echo_handler, UDS_SESSION_PROGRAMMING);

    uds_response_t rsp;
    uint8_t raw[] = {0x22, 0x01, 0x02};

    /* Dispatch from default session — should fail with NRC 0x7F */
    bool send = dispatch_raw(raw, sizeof(raw), &rsp,
                              UDS_DEFAULT_SESSION, UDS_PHYSICAL, NULL);

    TEST_ASSERT_TRUE(send);
    check_neg_rsp(&rsp, 0x22, NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION);
}

void test_session_availability_all_sessions(void)
{
    /* Register with ALL sessions mask */
    uds_service_register(0x22, stub_echo_handler, UDS_SESSION_ALL);

    uds_response_t rsp;
    uint8_t raw[] = {0x22, 0x01, 0x02};

    /* Should succeed in all sessions */
    bool send_def = dispatch_raw(raw, sizeof(raw), &rsp,
                                  UDS_DEFAULT_SESSION, UDS_PHYSICAL, NULL);
    TEST_ASSERT_TRUE(send_def);

    bool send_prog = dispatch_raw(raw, sizeof(raw), &rsp,
                                   UDS_PROGRAMMING_SESSION, UDS_PHYSICAL, NULL);
    TEST_ASSERT_TRUE(send_prog);

    bool send_ext = dispatch_raw(raw, sizeof(raw), &rsp,
                                  UDS_EXTENDED_SESSION, UDS_PHYSICAL, NULL);
    TEST_ASSERT_TRUE(send_ext);
}

void test_session_availability_non_default(void)
{
    uds_service_register(0x34, stub_echo_handler, UDS_SESSION_NON_DEFAULT);

    uds_response_t rsp;
    uint8_t raw[] = {0x34, 0x00};

    /* Should fail in default */
    bool send_def = dispatch_raw(raw, sizeof(raw), &rsp,
                                  UDS_DEFAULT_SESSION, UDS_PHYSICAL, NULL);
    TEST_ASSERT_TRUE(send_def);
    check_neg_rsp(&rsp, 0x34, NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION);

    /* Should succeed in extended */
    bool send_ext = dispatch_raw(raw, sizeof(raw), &rsp,
                                  UDS_EXTENDED_SESSION, UDS_PHYSICAL, NULL);
    TEST_ASSERT_TRUE(send_ext);
    check_pos_rsp(&rsp, 0x74, 0x00);

    /* Should succeed in programming */
    bool send_prog = dispatch_raw(raw, sizeof(raw), &rsp,
                                   UDS_PROGRAMMING_SESSION, UDS_PHYSICAL, NULL);
    TEST_ASSERT_TRUE(send_prog);
    check_pos_rsp(&rsp, 0x74, 0x00);
}

void test_dispatch_invalid_session_zero(void)
{
    /* Register a handler available in all sessions */
    uds_service_register(0x22, stub_echo_handler, UDS_SESSION_ALL);

    uds_response_t rsp;
    uint8_t raw[] = {0x22, 0x01, 0x02};

    /* current_session == 0 must not cause UB, must return NRC 0x7F */
    bool send = dispatch_raw(raw, sizeof(raw), &rsp, 0, UDS_PHYSICAL, NULL);

    TEST_ASSERT_TRUE(send);
    TEST_ASSERT_EQUAL_UINT8(0x7F, rsp.sid);
    TEST_ASSERT_EQUAL_UINT8(NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION,
                            rsp.data[0]);
}

/* ======================================================================== *
 * SPRMIB Propagation Tests                                                 *
 * ======================================================================== */

void test_dispatch_propagates_handler_false(void)
{
    /* Register a handler that always suppresses */
    uds_service_register(0x3E, stub_suppress_handler, UDS_SESSION_ALL);

    uds_response_t rsp;
    uint8_t raw[] = {0x3E, 0x00};

    bool send = dispatch_raw(raw, sizeof(raw), &rsp,
                              UDS_DEFAULT_SESSION, UDS_PHYSICAL, NULL);

    /* The suppress handler returns false, dispatch should propagate it */
    TEST_ASSERT_FALSE(send);
}

void test_dispatch_propagates_handler_true(void)
{
    uds_service_register(0x3E, stub_echo_handler, UDS_SESSION_ALL);

    uds_response_t rsp;
    uint8_t raw[] = {0x3E, 0x00};

    bool send = dispatch_raw(raw, sizeof(raw), &rsp,
                              UDS_DEFAULT_SESSION, UDS_PHYSICAL, NULL);

    TEST_ASSERT_TRUE(send);
}

/* ======================================================================== *
 * Local NRC Buffer Tests — no global g_nrc_buf dependency                 *
 * ======================================================================== */

void test_local_nrc_buf_unknown_sid_returns_nrc11(void)
{
    uds_service_clear();

    uds_response_t rsp;
    uint8_t raw[] = {0x99, 0x00};

    bool send = dispatch_raw(raw, sizeof(raw), &rsp,
                              UDS_DEFAULT_SESSION, UDS_PHYSICAL, NULL);

    TEST_ASSERT_TRUE(send);
    check_neg_rsp(&rsp, 0x99, NRC_SERVICE_NOT_SUPPORTED);
}

void test_local_nrc_buf_session_mismatch_returns_nrc7f(void)
{
    uds_service_clear();
    uds_service_register(0x22, stub_echo_handler, UDS_SESSION_PROGRAMMING);

    uds_response_t rsp;
    uint8_t raw[] = {0x22, 0x01, 0x02};

    bool send = dispatch_raw(raw, sizeof(raw), &rsp,
                              UDS_DEFAULT_SESSION, UDS_PHYSICAL, NULL);

    TEST_ASSERT_TRUE(send);
    check_neg_rsp(&rsp, 0x22, NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION);
}

void test_local_nrc_buf_independent_calls(void)
{
    uds_service_clear();
    uds_service_register(0x22, stub_echo_handler, UDS_SESSION_PROGRAMMING);

    {
        uds_response_t rsp1;
        uint8_t raw1[] = {0x99, 0x00};
        bool send1 = dispatch_raw(raw1, sizeof(raw1), &rsp1,
                                   UDS_DEFAULT_SESSION, UDS_PHYSICAL, NULL);
        TEST_ASSERT_TRUE(send1);
        check_neg_rsp(&rsp1, 0x99, NRC_SERVICE_NOT_SUPPORTED);
    }

    {
        uds_response_t rsp2;
        uint8_t raw2[] = {0x22, 0x01, 0x02};
        bool send2 = dispatch_raw(raw2, sizeof(raw2), &rsp2,
                                   UDS_DEFAULT_SESSION, UDS_PHYSICAL, NULL);
        TEST_ASSERT_TRUE(send2);
        check_neg_rsp(&rsp2, 0x22, NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION);
    }
}

/* ======================================================================== *
 * Functional-Addressing NRC Suppression Tests (Tables 4-7)                 *
 * ======================================================================== */

void test_fa_suppresses_nrc11(void)
{
    /* Unknown SID via functional → NRC 0x11 would be generated, must suppress */
    uds_response_t rsp;
    uint8_t raw[] = {0x12, 0x00};

    bool send = dispatch_raw(raw, sizeof(raw), &rsp,
                              UDS_DEFAULT_SESSION, UDS_FUNCTIONAL, NULL);

    TEST_ASSERT_FALSE(send);
}

void test_fa_suppresses_nrc12(void)
{
    /* Register a handler that returns NRC 0x12 */
    uds_service_register(0x3E, stub_nrc_handler, UDS_SESSION_ALL);

    uds_response_t rsp;
    uint8_t raw[] = {0x3E, 0x00};

    uint8_t nrc_12 = NRC_SUB_FUNCTION_NOT_SUPPORTED;
    bool send = dispatch_raw(raw, sizeof(raw), &rsp,
                              UDS_DEFAULT_SESSION, UDS_FUNCTIONAL, &nrc_12);

    /* NRC 0x12 via functional addressing must be suppressed */
    TEST_ASSERT_FALSE(send);
}

void test_fa_suppresses_nrc31(void)
{
    /* Register a handler that returns NRC 0x31 */
    uds_service_register(0x3E, stub_nrc_handler, UDS_SESSION_ALL);

    uds_response_t rsp;
    uint8_t raw[] = {0x3E, 0x00};

    uint8_t nrc_31 = NRC_REQUEST_OUT_OF_RANGE;
    bool send = dispatch_raw(raw, sizeof(raw), &rsp,
                              UDS_DEFAULT_SESSION, UDS_FUNCTIONAL, &nrc_31);

    /* NRC 0x31 via functional addressing must be suppressed */
    TEST_ASSERT_FALSE(send);
}

void test_fa_does_not_suppress_other_nrcs(void)
{
    /* Register a handler that returns NRC 0x22 */
    uds_service_register(0x3E, stub_nrc_handler, UDS_SESSION_ALL);

    uds_response_t rsp;
    uint8_t raw[] = {0x3E, 0x00};

    uint8_t nrc_22 = NRC_CONDITIONS_NOT_CORRECT;
    bool send = dispatch_raw(raw, sizeof(raw), &rsp,
                              UDS_DEFAULT_SESSION, UDS_FUNCTIONAL, &nrc_22);

    /* NRC 0x22 should NOT be suppressed for functional addressing */
    TEST_ASSERT_TRUE(send);
    check_neg_rsp(&rsp, 0x3E, NRC_CONDITIONS_NOT_CORRECT);
}

/* ======================================================================== *
 * Built-in Service Registration Tests                                      *
 * ======================================================================== */

void test_service_init_registers_all(void)
{
    uds_service_init();

    /* We should have 26 registered services */
    TEST_ASSERT_EQUAL_UINT8(26, uds_service_get_count());
}

void test_service_init_diagcomm_handlers(void)
{
    uds_service_init();

    /* Verify a few key services are registered */
    TEST_ASSERT_NOT_NULL(uds_service_find(DIAGNOSTIC_SESSION_CONTROL));
    TEST_ASSERT_NOT_NULL(uds_service_find(ECU_RESET));
    TEST_ASSERT_NOT_NULL(uds_service_find(SECURITY_ACCESS));
    TEST_ASSERT_NOT_NULL(uds_service_find(COMMUNICATION_CONTROL));
    TEST_ASSERT_NOT_NULL(uds_service_find(TESTER_PRESENT));
    TEST_ASSERT_NOT_NULL(uds_service_find(CONTROL_DTC_SETTING));
    TEST_ASSERT_NOT_NULL(uds_service_find(RESPONSE_ON_EVENT));
    TEST_ASSERT_NOT_NULL(uds_service_find(LINK_CONTROL));
}

void test_service_init_data_handlers(void)
{
    uds_service_init();

    TEST_ASSERT_NOT_NULL(uds_service_find(READ_DATA_BY_IDENTIFIER));
    TEST_ASSERT_NOT_NULL(uds_service_find(READ_MEMORY_BY_ADDRESS));
    TEST_ASSERT_NOT_NULL(uds_service_find(READ_SCALING_DATA_BY_ID));
    TEST_ASSERT_NOT_NULL(uds_service_find(READ_DATA_BY_PERIODIC_ID));
    TEST_ASSERT_NOT_NULL(uds_service_find(DYNAMICALLY_DEFINE_DATA_ID));
    TEST_ASSERT_NOT_NULL(uds_service_find(WRITE_DATA_BY_IDENTIFIER));
    TEST_ASSERT_NOT_NULL(uds_service_find(WRITE_MEMORY_BY_ADDRESS));
}

void test_service_init_stored_handlers(void)
{
    uds_service_init();

    TEST_ASSERT_NOT_NULL(uds_service_find(CLEAR_DIAGNOSTIC_INFO));
    TEST_ASSERT_NOT_NULL(uds_service_find(READ_DTC_INFO));
}

void test_service_init_io_handler(void)
{
    uds_service_init();

    TEST_ASSERT_NOT_NULL(uds_service_find(INPUT_OUTPUT_CONTROL_BY_ID));
}

void test_service_init_routine_handler(void)
{
    uds_service_init();

    TEST_ASSERT_NOT_NULL(uds_service_find(ROUTINE_CONTROL));
}

void test_service_init_upload_handlers(void)
{
    uds_service_init();

    TEST_ASSERT_NOT_NULL(uds_service_find(REQUEST_DOWNLOAD));
    TEST_ASSERT_NOT_NULL(uds_service_find(REQUEST_UPLOAD));
    TEST_ASSERT_NOT_NULL(uds_service_find(TRANSFER_DATA));
    TEST_ASSERT_NOT_NULL(uds_service_find(REQUEST_TRANSFER_EXIT));
    TEST_ASSERT_NOT_NULL(uds_service_find(REQUEST_FILE_TRANSFER));
}

void test_service_init_auth_handler(void)
{
    uds_service_init();

    TEST_ASSERT_NOT_NULL(uds_service_find(AUTHENTICATION));
}

void test_service_init_secured_data_handler(void)
{
    uds_service_init();

    const uds_service_entry_t *entry = uds_service_find(SECURED_DATA_TRANSMISSION);
    TEST_ASSERT_NOT_NULL(entry);
}

/* ======================================================================== *
 * Session Mask Verification for Built-in Services                          *
 * ======================================================================== */

void test_upload_services_session_mask(void)
{
    uds_service_init();

    /* Upload/download services should be UDS_SESSION_PROGRAMMING only */
    const uds_service_entry_t *dl = uds_service_find(REQUEST_DOWNLOAD);
    TEST_ASSERT_NOT_NULL(dl);
    TEST_ASSERT_EQUAL_UINT8(UDS_SESSION_PROGRAMMING, dl->session_mask);

    const uds_service_entry_t *ul = uds_service_find(REQUEST_UPLOAD);
    TEST_ASSERT_NOT_NULL(ul);
    TEST_ASSERT_EQUAL_UINT8(UDS_SESSION_PROGRAMMING, ul->session_mask);
}

void test_diagcomm_session_masks(void)
{
    uds_service_init();

    /* DSC, ECUReset, SecurityAccess should be ALL sessions */
    TEST_ASSERT_EQUAL_UINT8(UDS_SESSION_ALL,
        uds_service_find(DIAGNOSTIC_SESSION_CONTROL)->session_mask);
    TEST_ASSERT_EQUAL_UINT8(UDS_SESSION_ALL,
        uds_service_find(ECU_RESET)->session_mask);

    /* ControlDTCSetting, ResponseOnEvent, LinkControl should be non-default */
    TEST_ASSERT_EQUAL_UINT8(UDS_SESSION_NON_DEFAULT,
        uds_service_find(CONTROL_DTC_SETTING)->session_mask);
    TEST_ASSERT_EQUAL_UINT8(UDS_SESSION_NON_DEFAULT,
        uds_service_find(RESPONSE_ON_EVENT)->session_mask);
    TEST_ASSERT_EQUAL_UINT8(UDS_SESSION_NON_DEFAULT,
        uds_service_find(LINK_CONTROL)->session_mask);
}

/* ======================================================================== *
 * Dispatch with Real Handlers — Smoke Tests                                *
 * ======================================================================== */

void test_dispatch_tester_present_with_real_handler(void)
{
    uds_service_init();

    uds_response_t rsp;
    uint8_t raw[] = {0x3E, 0x00};

    bool send = dispatch_raw(raw, sizeof(raw), &rsp,
                              UDS_DEFAULT_SESSION, UDS_PHYSICAL, NULL);

    TEST_ASSERT_TRUE(send);
    check_pos_rsp(&rsp, TESTER_PRESENT_RSP, 0x00);
}

void test_dispatch_dsc_with_real_handler(void)
{
    uds_service_init();

    /* Setup: session context */
    uds_session_context_t sctx;
    uds_session_init(&sctx);

    uds_response_t rsp;
    uint8_t raw[] = {0x10, 0x03};  /* default → extended */

    bool send = dispatch_raw(raw, sizeof(raw), &rsp,
                              UDS_DEFAULT_SESSION, UDS_PHYSICAL, &sctx);

    TEST_ASSERT_TRUE(send);
    check_pos_rsp(&rsp, DIAGNOSTIC_SESSION_CONTROL_RSP, 0x03);
}

void test_dispatch_programming_only_service_from_default(void)
{
    uds_service_init();

    /* 0x34 (RequestDownload) is registered with UDS_SESSION_PROGRAMMING */
    uds_response_t rsp;
    uint8_t raw[] = {0x34, 0x00, 0x01, 0x00, 0x00, 0x00, 0x10,
                     0x00, 0x00, 0x00, 0x64};

    bool send = dispatch_raw(raw, sizeof(raw), &rsp,
                              UDS_DEFAULT_SESSION, UDS_PHYSICAL, NULL);

    /* Should fail with NRC 0x7F since we're in default session */
    TEST_ASSERT_TRUE(send);
    check_neg_rsp(&rsp, 0x34, NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION);
}

/* ======================================================================== *
 * Test Runner                                                              *
 * ======================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* Registration Tests */
    RUN_TEST(test_register_and_find);
    RUN_TEST(test_register_duplicate_sid_fails);
    RUN_TEST(test_register_null_handler_fails);
    RUN_TEST(test_find_nonexistent_sid);
    RUN_TEST(test_get_count);
    RUN_TEST(test_register_table_full_fails);

    /* Dispatch Routing */
    RUN_TEST(test_dispatch_routing_known_sid);
    RUN_TEST(test_dispatch_unknown_sid_physical_returns_nrc11);
    RUN_TEST(test_dispatch_unknown_sid_functional_suppressed);

    /* Session Availability */
    RUN_TEST(test_session_mismatch_returns_nrc7f);
    RUN_TEST(test_session_availability_all_sessions);
    RUN_TEST(test_session_availability_non_default);
    RUN_TEST(test_dispatch_invalid_session_zero);

    /* SPRMIB Propagation */
    RUN_TEST(test_dispatch_propagates_handler_false);
    RUN_TEST(test_dispatch_propagates_handler_true);

    /* Local NRC Buffer — no global dependency */
    RUN_TEST(test_local_nrc_buf_unknown_sid_returns_nrc11);
    RUN_TEST(test_local_nrc_buf_session_mismatch_returns_nrc7f);
    RUN_TEST(test_local_nrc_buf_independent_calls);

    /* Functional Addressing NRC Suppression */
    RUN_TEST(test_fa_suppresses_nrc11);
    RUN_TEST(test_fa_suppresses_nrc12);
    RUN_TEST(test_fa_suppresses_nrc31);
    RUN_TEST(test_fa_does_not_suppress_other_nrcs);

    /* Built-in Service Registration */
    RUN_TEST(test_service_init_registers_all);
    RUN_TEST(test_service_init_diagcomm_handlers);
    RUN_TEST(test_service_init_data_handlers);
    RUN_TEST(test_service_init_stored_handlers);
    RUN_TEST(test_service_init_io_handler);
    RUN_TEST(test_service_init_routine_handler);
    RUN_TEST(test_service_init_upload_handlers);
    RUN_TEST(test_service_init_auth_handler);
    RUN_TEST(test_service_init_secured_data_handler);

    /* Session Mask Verification */
    RUN_TEST(test_upload_services_session_mask);
    RUN_TEST(test_diagcomm_session_masks);

    /* Smoke Tests with Real Handlers */
    RUN_TEST(test_dispatch_tester_present_with_real_handler);
    RUN_TEST(test_dispatch_dsc_with_real_handler);
    RUN_TEST(test_dispatch_programming_only_service_from_default);

    return UNITY_END();
}
