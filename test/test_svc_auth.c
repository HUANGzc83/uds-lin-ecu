/*
 * test_svc_auth.c
 * Authentication Service (0x29) — Unit Tests
 *
 * Tests the 0x29 Authentication handler including:
 *   - Session check (must be non-default session)
 *   - Subfunction dispatch: authConfig, deAuthenticate, PKI stubs
 *   - NRC: 0x12 (SFNS), 0x13 (IMLOIF), 0x22 (CNC), 0x34 (AR)
 *   - SPRMIB suppression
 *   - Authentication state transitions
 *
 * Wave 3c Task 19 — Authentication Service 0x29 Framework
 */

#include "unity.h"
#include "uds/uds_core.h"
#include "uds/uds_svc_auth.h"
#include "uds/uds_session.h"
#include <string.h>

/* ======================================================================== *
 * Test Fixture                                                             *
 * ======================================================================== */

static uds_session_context_t session_ctx;

void setUp(void)
{
    uds_session_init(&session_ctx);
    uds_svc_auth_init();
}

void tearDown(void)
{
    /* nothing to tear down */
}

/* ======================================================================== *
 * Helpers                                                                  *
 * ======================================================================== */

/**
 * @brief Parse raw request bytes and invoke the Authentication handler.
 * Returns the handler's bool result.
 */
static bool call_handler(const uint8_t *raw, uint16_t raw_len,
                         uds_response_t *rsp, void *context)
{
    uds_request_t req;
    uds_status_t status = uds_parse_request(raw, raw_len, &req);
    TEST_ASSERT_EQUAL(UDS_OK, status);
    return uds_svc_authentication(&req, rsp, context);
}

/**
 * @brief Assert that a response is a positive response for the Authentication
 *        service with the expected subfunction echo.
 */
static void check_pos_rsp(const uds_response_t *rsp, uint8_t expected_subfunc)
{
    TEST_ASSERT_EQUAL_UINT8(AUTHENTICATION_RSP, rsp->sid);
    TEST_ASSERT_EQUAL_UINT8(expected_subfunc,   rsp->subfunc_echo);
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
 * @brief Switch to a non-default session for testing.
 */
static void switch_to_extended_session(void)
{
    uds_nrc_t nrc = NRC_GENERAL_REJECT;
    uds_status_t status = uds_session_switch(&session_ctx,
                                              UDS_EXTENDED_SESSION, &nrc);
    TEST_ASSERT_EQUAL(UDS_OK, status);
    TEST_ASSERT_EQUAL(UDS_EXTENDED_SESSION, session_ctx.current_session);
}

/* ======================================================================== *
 * Session Check Tests                                                      *
 * ======================================================================== */

void test_auth_rejected_in_default_session(void)
{
    /* Handler should return NRC 0x22 when in default session */
    uds_response_t rsp;
    uint8_t raw[] = {0x29, 0x01};  /* authenticationConfiguration */

    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, AUTHENTICATION, NRC_CONDITIONS_NOT_CORRECT);
}

void test_auth_null_context_returns_cnc(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x29, 0x01};

    /* Context == NULL should also return NRC 0x22 */
    bool result = call_handler(raw, sizeof(raw), &rsp, NULL);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, AUTHENTICATION, NRC_CONDITIONS_NOT_CORRECT);
}

void test_auth_allowed_in_non_default_session(void)
{
    switch_to_extended_session();

    uds_response_t rsp;
    uint8_t raw[] = {0x29, 0x01};

    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx);

    /* Should succeed in non-default session */
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT8(AUTHENTICATION_RSP, rsp.sid);
}

/* ======================================================================== *
 * SubFunctionNotSupported (NRC 0x12) Tests                                *
 * ======================================================================== */

void test_auth_invalid_subfn_returns_nrc12(void)
{
    switch_to_extended_session();

    uds_response_t rsp;
    uint8_t raw[] = {0x29, 0xFF};  /* unknown subfunction */

    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, AUTHENTICATION, NRC_SUB_FUNCTION_NOT_SUPPORTED);
}

void test_auth_subfn_zero_returns_nrc12(void)
{
    switch_to_extended_session();

    uds_response_t rsp;
    uint8_t raw[] = {0x29, 0x00};  /* subfunction 0 is not defined for Auth */

    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, AUTHENTICATION, NRC_SUB_FUNCTION_NOT_SUPPORTED);
}

/* ======================================================================== *
 * authenticationConfiguration (0x01) Tests                                *
 * ======================================================================== */

void test_auth_config_returns_config_data(void)
{
    switch_to_extended_session();

    uds_response_t rsp;
    uint8_t raw[] = {0x29, 0x01};

    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, AUTH_SF_AUTHENTICATION_CONFIGURATION);
    TEST_ASSERT_NOT_NULL(rsp.data);
    TEST_ASSERT_EQUAL_UINT8(2, rsp.data_len);
    TEST_ASSERT_EQUAL_UINT8(0x01, rsp.data[0]);  /* 1 supported concept */
    TEST_ASSERT_EQUAL_UINT8(0x01, rsp.data[1]);  /* concept: certificate-based */
}

void test_auth_config_with_extra_data_returns_nrc13(void)
{
    switch_to_extended_session();

    uds_response_t rsp;
    uint8_t raw[] = {0x29, 0x01, 0xAA};  /* unexpected data byte */

    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, AUTHENTICATION, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

void test_auth_config_suppressed_via_sprmib(void)
{
    switch_to_extended_session();

    uds_response_t rsp;
    uint8_t raw[] = {0x29, 0x81};  /* subfunction 0x01 with SPRMIB set */

    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx);

    /* Response should be suppressed (false) */
    TEST_ASSERT_FALSE(result);
}

/* ======================================================================== *
 * deAuthenticate (0x02) Tests                                             *
 * ======================================================================== */

void test_deauth_clears_auth_state(void)
{
    switch_to_extended_session();

    /* First verify initial state is NONE */
    TEST_ASSERT_EQUAL_UINT8(AUTH_STATE_NONE, uds_svc_auth_get_state());

    uds_response_t rsp;
    uint8_t raw[] = {0x29, 0x02};

    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, AUTH_SF_DE_AUTHENTICATE);
    TEST_ASSERT_EQUAL_UINT8(0, rsp.data_len);  /* no additional data */

    /* State remains NONE after deauth */
    TEST_ASSERT_EQUAL_UINT8(AUTH_STATE_NONE, uds_svc_auth_get_state());
}

void test_deauth_with_extra_data_returns_nrc13(void)
{
    switch_to_extended_session();

    uds_response_t rsp;
    uint8_t raw[] = {0x29, 0x02, 0xBB};  /* unexpected data byte */

    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, AUTHENTICATION, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

void test_deauth_suppressed_via_sprmib(void)
{
    switch_to_extended_session();

    uds_response_t rsp;
    uint8_t raw[] = {0x29, 0x82};  /* subfunction 0x02 with SPRMIB set */

    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx);

    TEST_ASSERT_FALSE(result);
}

/* ======================================================================== *
 * PKI Stub Tests — All Return NRC 0x34                                    *
 * ======================================================================== */

static void check_pki_stub_returns_nrc34(uint8_t subfn)
{
    switch_to_extended_session();

    uds_response_t rsp;
    uint8_t raw[] = {0x29, subfn};

    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, AUTHENTICATION, NRC_AUTHENTICATION_REQUIRED);
}

void test_verify_cert_uni_returns_nrc34(void)
{
    check_pki_stub_returns_nrc34(0x11);
}

void test_verify_cert_bi_returns_nrc34(void)
{
    check_pki_stub_returns_nrc34(0x12);
}

void test_proof_of_ownership_returns_nrc34(void)
{
    check_pki_stub_returns_nrc34(0x13);
}

void test_transmit_cert_returns_nrc34(void)
{
    check_pki_stub_returns_nrc34(0x14);
}

void test_pki_stub_with_data_still_returns_nrc34(void)
{
    /* Even with extra data, PKI stubs return NRC 0x34 (no length check) */
    switch_to_extended_session();

    uds_response_t rsp;
    uint8_t raw[] = {0x29, 0x11, 0x01, 0x02, 0x03, 0x04};

    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, AUTHENTICATION, NRC_AUTHENTICATION_REQUIRED);
}

/* ======================================================================== *
 * Authentication State Tests                                               *
 * ======================================================================== */

void test_auth_state_initially_none(void)
{
    TEST_ASSERT_EQUAL_UINT8(AUTH_STATE_NONE, uds_svc_auth_get_state());
}

void test_auth_state_remains_persistent_across_calls(void)
{
    /* The state is static and should persist across handler calls.
     * For now, since only deAuth affects state, call deAuth multiple
     * times and verify state stays NONE. */

    switch_to_extended_session();

    uds_response_t rsp1;
    uint8_t raw1[] = {0x29, 0x02};
    call_handler(raw1, sizeof(raw1), &rsp1, &session_ctx);
    TEST_ASSERT_EQUAL_UINT8(AUTH_STATE_NONE, uds_svc_auth_get_state());

    uds_response_t rsp2;
    uint8_t raw2[] = {0x29, 0x02};
    call_handler(raw2, sizeof(raw2), &rsp2, &session_ctx);
    TEST_ASSERT_EQUAL_UINT8(AUTH_STATE_NONE, uds_svc_auth_get_state());
}

/* ======================================================================== *
 * Edge Cases                                                               *
 * ======================================================================== */

void test_auth_sid_0x29_used_for_request(void)
{
    /* Verify the handler receives the correct SID */
    switch_to_extended_session();

    uds_request_t req;
    uint8_t raw[] = {0x29, 0x01};
    uds_status_t status = uds_parse_request(raw, sizeof(raw), &req);
    TEST_ASSERT_EQUAL(UDS_OK, status);
    TEST_ASSERT_EQUAL_UINT8(AUTHENTICATION, req.sid);
}

void test_auth_in_non_default_session_only(void)
{
    /* Test in programming session as well */
    uds_nrc_t nrc = NRC_GENERAL_REJECT;
    uds_status_t status = uds_session_switch(&session_ctx,
                                              UDS_PROGRAMMING_SESSION, &nrc);
    TEST_ASSERT_EQUAL(UDS_OK, status);

    uds_response_t rsp;
    uint8_t raw[] = {0x29, 0x01};

    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, AUTH_SF_AUTHENTICATION_CONFIGURATION);
}

/* ======================================================================== *
 * Test runner                                                              *
 * ======================================================================== */
int main(void)
{
    UNITY_BEGIN();

    /* Session check tests */
    RUN_TEST(test_auth_rejected_in_default_session);
    RUN_TEST(test_auth_null_context_returns_cnc);
    RUN_TEST(test_auth_allowed_in_non_default_session);

    /* SubFunctionNotSupported tests */
    RUN_TEST(test_auth_invalid_subfn_returns_nrc12);
    RUN_TEST(test_auth_subfn_zero_returns_nrc12);

    /* authenticationConfiguration tests */
    RUN_TEST(test_auth_config_returns_config_data);
    RUN_TEST(test_auth_config_with_extra_data_returns_nrc13);
    RUN_TEST(test_auth_config_suppressed_via_sprmib);

    /* deAuthenticate tests */
    RUN_TEST(test_deauth_clears_auth_state);
    RUN_TEST(test_deauth_with_extra_data_returns_nrc13);
    RUN_TEST(test_deauth_suppressed_via_sprmib);

    /* PKI stub tests */
    RUN_TEST(test_verify_cert_uni_returns_nrc34);
    RUN_TEST(test_verify_cert_bi_returns_nrc34);
    RUN_TEST(test_proof_of_ownership_returns_nrc34);
    RUN_TEST(test_transmit_cert_returns_nrc34);
    RUN_TEST(test_pki_stub_with_data_still_returns_nrc34);

    /* Authentication state tests */
    RUN_TEST(test_auth_state_initially_none);
    RUN_TEST(test_auth_state_remains_persistent_across_calls);

    /* Edge case tests */
    RUN_TEST(test_auth_sid_0x29_used_for_request);
    RUN_TEST(test_auth_in_non_default_session_only);

    return UNITY_END();
}
