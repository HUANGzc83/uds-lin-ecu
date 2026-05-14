/*
 * test_svc_diagcomm.c
 * Diagnostic & Communication Management Services — Unit Tests
 *
 * Tests 8 UDS service handlers: 0x10, 0x11, 0x27, 0x28, 0x3E, 0x85,
 * 0x86, and 0x87.
 *
 * Wave 3a Task 9 — Diagnostic & Communication Management Services
 */

#include "unity.h"
#include "uds/uds_core.h"
#include "uds/uds_svc_diagcomm.h"
#include "uds/uds_session.h"
#include "uds/uds_security.h"
#include "mock_timer.h"
#include <string.h>

/* ======================================================================== *
 * Test Fixture                                                             *
 * ======================================================================== */

static uds_session_context_t session_ctx;
static bool                  unlocked;

void setUp(void)
{
    mock_timer_clear();
    uds_security_init();
    uds_session_init(&session_ctx);
    uds_svc_event_store_init();
    unlocked = false;
}

void tearDown(void)
{
    /* nothing to tear down */
}

/* ======================================================================== *
 * Helpers                                                                  *
 * ======================================================================== */

/**
 * @brief Parse raw request bytes and invoke the handler.
 * Returns the handler's bool result.
 */
static bool call_handler(const uint8_t *raw, uint16_t raw_len,
                         uds_response_t *rsp, void *context,
                         bool (*handler)(const uds_request_t *, uds_response_t *, void *))
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

/* ======================================================================== *
 * 0x10 — DiagnosticSessionControl Tests                                    *
 * ======================================================================== */

void test_dsc_valid_session_switch(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x10, 0x03};  /* default → extendedSession */

    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                               uds_svc_diagnostic_session_control);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, DIAGNOSTIC_SESSION_CONTROL_RSP, 0x03);

    /* Verify sessionParameterRecord (P2 + P2* for extended session) */
    TEST_ASSERT_EQUAL_UINT8(4, rsp.data_len);
    TEST_ASSERT_NOT_NULL(rsp.data);

    /* Extended: P2=5000(0x1388), P2*=5000(0x1388) */
    TEST_ASSERT_EQUAL_UINT8(0x13, rsp.data[0]);
    TEST_ASSERT_EQUAL_UINT8(0x88, rsp.data[1]);
    TEST_ASSERT_EQUAL_UINT8(0x13, rsp.data[2]);
    TEST_ASSERT_EQUAL_UINT8(0x88, rsp.data[3]);

    /* Session should actually be switched */
    TEST_ASSERT_EQUAL_UINT8(0x03, session_ctx.current_session);
}

void test_dsc_invalid_session_returns_nrc12(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x10, 0xFF};  /* unsupported session */

    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                               uds_svc_diagnostic_session_control);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x10, NRC_SUB_FUNCTION_NOT_SUPPORTED);

    /* Session unchanged */
    TEST_ASSERT_EQUAL_UINT8(UDS_DEFAULT_SESSION, session_ctx.current_session);
}

void test_dsc_imloif_extra_bytes(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x10, 0x03, 0xAA};  /* extra byte */

    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                               uds_svc_diagnostic_session_control);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x10, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

void test_dsc_sprmib_suppresses_response(void)
{
    uds_response_t rsp;
    /* 0x10 with suppress bit set on subfunction 0x03 → 0x83 */
    uint8_t raw[] = {0x10, 0x83};

    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                               uds_svc_diagnostic_session_control);

    /* Positive response suppressed */
    TEST_ASSERT_FALSE(result);

    /* Session should still have switched though */
    TEST_ASSERT_EQUAL_UINT8(0x03, session_ctx.current_session);
}

void test_dsc_null_context_returns_nrc22(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x10, 0x03};

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_diagnostic_session_control);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x10, NRC_CONDITIONS_NOT_CORRECT);
}

/* ======================================================================== *
 * 0x11 — ECUReset Tests                                                    *
 * ======================================================================== */

void test_ecu_reset_hard_reset(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x11, 0x01};  /* hardReset */

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_ecu_reset);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, ECU_RESET_RSP, 0x01);
    TEST_ASSERT_EQUAL_UINT8(1, rsp.data_len);
    TEST_ASSERT_NOT_NULL(rsp.data);
    TEST_ASSERT_EQUAL_UINT8(0x00, rsp.data[0]);  /* powerDownTime */
}

void test_ecu_reset_soft_reset(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x11, 0x03};  /* softReset */

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_ecu_reset);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, ECU_RESET_RSP, 0x03);
    TEST_ASSERT_EQUAL_UINT8(1, rsp.data_len);
    TEST_ASSERT_EQUAL_UINT8(0x00, rsp.data[0]);
}

void test_ecu_reset_unsupported_type_returns_nrc12(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x11, 0x00};  /* invalid reset type */

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_ecu_reset);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x11, NRC_SUB_FUNCTION_NOT_SUPPORTED);
}

void test_ecu_reset_imloif_extra_bytes(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x11, 0x01, 0x00};  /* extra byte */

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_ecu_reset);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x11, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

void test_ecu_reset_sprmib_suppresses(void)
{
    uds_response_t rsp;
    /* 0x11 with suppress bit → 0x81 */
    uint8_t raw[] = {0x11, 0x81};

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_ecu_reset);

    TEST_ASSERT_FALSE(result);
}

/* ======================================================================== *
 * 0x27 — SecurityAccess Tests                                              *
 * ======================================================================== */

void test_security_request_seed_returns_nonzero_seed(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x27, 0x01};  /* requestSeed level 1 */

    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked,
                               uds_svc_security_access);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, SECURITY_ACCESS_RSP, 0x01);
    TEST_ASSERT_EQUAL_UINT8(SECURITY_SEED_SIZE, rsp.data_len);
    TEST_ASSERT_NOT_NULL(rsp.data);

    /* Seed should not be all zeros */
    bool all_zero = true;
    for (uint8_t i = 0; i < SECURITY_SEED_SIZE; i++)
    {
        if (rsp.data[i] != 0u)
        {
            all_zero = false;
            break;
        }
    }
    TEST_ASSERT_FALSE(all_zero);

    /* Should still be locked */
    TEST_ASSERT_FALSE(unlocked);
}

void test_security_send_key_valid_unlocks(void)
{
    /* Step 1: requestSeed */
    {
        uds_response_t rsp;
        uint8_t raw[] = {0x27, 0x01};
        call_handler(raw, sizeof(raw), &rsp, &unlocked,
                     uds_svc_security_access);
        TEST_ASSERT_EQUAL_UINT8(SECURITY_SEED_SIZE, rsp.data_len);

        /* Step 2: build valid key (key[0] = ~seed[0]) */
        uint8_t seed0 = rsp.data[0];

        uint8_t raw2[2 + SECURITY_SEED_SIZE];
        raw2[0] = 0x27;
        raw2[1] = 0x02;  /* sendKey level 1 */
        memset(raw2 + 2, 0xAA, SECURITY_SEED_SIZE);
        raw2[2] = (uint8_t)(~seed0);

        uds_response_t rsp2;
        bool result = call_handler(raw2, sizeof(raw2), &rsp2, &unlocked,
                                   uds_svc_security_access);

        TEST_ASSERT_TRUE(result);
        check_pos_rsp(&rsp2, SECURITY_ACCESS_RSP, 0x02);
        TEST_ASSERT_EQUAL_UINT8(0, rsp2.data_len);
        TEST_ASSERT_TRUE(unlocked);
    }
}

void test_security_send_key_invalid_returns_nrc35(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x27, 0x01};
    call_handler(raw, sizeof(raw), &rsp, &unlocked,
                 uds_svc_security_access);
    uint8_t seed0 = rsp.data[0];

    /* Build invalid key: key[0] == seed[0] (should be ~seed[0]) */
    uint8_t raw2[2 + SECURITY_SEED_SIZE];
    raw2[0] = 0x27;
    raw2[1] = 0x02;
    memset(raw2 + 2, 0xBB, SECURITY_SEED_SIZE);
    raw2[2] = seed0;  /* intentionally wrong */

    uds_response_t rsp2;
    bool result = call_handler(raw2, sizeof(raw2), &rsp2, &unlocked,
                               uds_svc_security_access);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp2, 0x27, NRC_INVALID_KEY);
    TEST_ASSERT_FALSE(unlocked);
}

void test_security_send_key_without_request_returns_nrc24(void)
{
    /* Attempt sendKey directly without requestSeed */
    uint8_t key_buf[SECURITY_KEY_SIZE];
    memset(key_buf, 0xCC, SECURITY_KEY_SIZE);

    uint8_t raw[2 + SECURITY_KEY_SIZE];
    raw[0] = 0x27;
    raw[1] = 0x02;
    memcpy(raw + 2, key_buf, SECURITY_KEY_SIZE);

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked,
                               uds_svc_security_access);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x27, NRC_REQUEST_SEQUENCE_ERROR);
    TEST_ASSERT_FALSE(unlocked);
}

void test_security_request_seed_invalid_level_returns_nrc12(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x27, 0x00};  /* level 0 is invalid */

    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked,
                               uds_svc_security_access);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x27, NRC_SUB_FUNCTION_NOT_SUPPORTED);
}

void test_security_send_key_imloif_no_key_data(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x27, 0x02};  /* sendKey with no key data */

    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked,
                               uds_svc_security_access);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x27, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

void test_security_request_seed_sprmib_suppresses(void)
{
    uds_response_t rsp;
    /* 0x27 with suppress bit set on subfunction 0x01 → 0x81 */
    uint8_t raw[] = {0x27, 0x81};

    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked,
                               uds_svc_security_access);

    TEST_ASSERT_FALSE(result);
}

/* ======================================================================== *
 * 0x28 — CommunicationControl Tests                                        *
 * ======================================================================== */

void test_comm_control_valid(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x28, 0x00, 0x01};  /* enableRxAndTx, normalComm */

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_communication_control);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, COMMUNICATION_CONTROL_RSP, 0x00);
    TEST_ASSERT_EQUAL_UINT8(0, rsp.data_len);
}

void test_comm_control_imloif_no_data(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x28, 0x00};  /* missing communicationType */

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_communication_control);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x28, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

void test_comm_control_invalid_control_type_returns_nrc12(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x28, 0x05, 0x01};  /* invalid controlType */

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_communication_control);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x28, NRC_SUB_FUNCTION_NOT_SUPPORTED);
}

void test_comm_control_invalid_comm_type_returns_nrc31(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x28, 0x00, 0xFF};  /* invalid comm type */

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_communication_control);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x28, NRC_REQUEST_OUT_OF_RANGE);
}

void test_comm_control_sprmib_suppresses(void)
{
    uds_response_t rsp;
    /* 0x28 with suppress bit (0x80) on controlType 0x00 */
    uint8_t raw[] = {0x28, 0x80, 0x01};

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_communication_control);

    TEST_ASSERT_FALSE(result);
}

/* ======================================================================== *
 * 0x3E — TesterPresent Tests                                              *
 * ======================================================================== */

void test_tester_present_positive_response(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x3E, 0x00};  /* subfunction 0x00 */

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_tester_present);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, TESTER_PRESENT_RSP, 0x00);
    TEST_ASSERT_EQUAL_UINT8(0, rsp.data_len);
}

void test_tester_present_sprmib_suppresses(void)
{
    uds_response_t rsp;
    /* subfunction 0x80 = 0x00 with suppress bit */
    uint8_t raw[] = {0x3E, 0x80};

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_tester_present);

    TEST_ASSERT_FALSE(result);
}

void test_tester_present_imloif_extra_bytes(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x3E, 0x00, 0x01};  /* extra byte */

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_tester_present);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x3E, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

void test_tester_present_subfunction_echoed(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x3E, 0x55};  /* arbitrary subfunction 0x55 */

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_tester_present);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, TESTER_PRESENT_RSP, 0x55);
}

/* ======================================================================== *
 * 0x85 — ControlDTCSetting Tests                                           *
 * ======================================================================== */

void test_dtc_setting_on_with_unlock(void)
{
    uds_response_t rsp;
    unlocked = true;
    uint8_t raw[] = {0x85, 0x01};  /* dtcSettingOn */

    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked,
                               uds_svc_control_dtc_setting);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, CONTROL_DTC_SETTING_RSP, 0x01);
    TEST_ASSERT_EQUAL_UINT8(0, rsp.data_len);
}

void test_dtc_setting_off_with_unlock(void)
{
    uds_response_t rsp;
    unlocked = true;
    uint8_t raw[] = {0x85, 0x02};  /* dtcSettingOff */

    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked,
                               uds_svc_control_dtc_setting);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, CONTROL_DTC_SETTING_RSP, 0x02);
}

void test_dtc_setting_without_unlock_returns_nrc33(void)
{
    uds_response_t rsp;
    unlocked = false;
    uint8_t raw[] = {0x85, 0x01};

    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked,
                               uds_svc_control_dtc_setting);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x85, NRC_SECURITY_ACCESS_DENIED);
}

void test_dtc_setting_invalid_type_returns_nrc12(void)
{
    uds_response_t rsp;
    unlocked = true;
    uint8_t raw[] = {0x85, 0x00};  /* invalid setting type */

    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked,
                               uds_svc_control_dtc_setting);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x85, NRC_SUB_FUNCTION_NOT_SUPPORTED);
}

void test_dtc_setting_null_context_returns_nrc22(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x85, 0x01};

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_control_dtc_setting);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x85, NRC_CONDITIONS_NOT_CORRECT);
}

void test_dtc_setting_sprmib_suppresses(void)
{
    uds_response_t rsp;
    unlocked = true;
    /* 0x85 with suppress bit (0x81) on subfunction 0x01 */
    uint8_t raw[] = {0x85, 0x81};

    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked,
                               uds_svc_control_dtc_setting);

    TEST_ASSERT_FALSE(result);
}

/* ======================================================================== *
 * 0x86 — ResponseOnEvent Tests                                             *
 * ======================================================================== */

void test_response_on_event_start(void)
{
    uds_response_t rsp;
    /* startResponseOnEvent(0x01), eventType=0xAA */
    uint8_t raw[] = {0x86, 0x01, 0xAA};

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_response_on_event);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, RESPONSE_ON_EVENT_RSP, 0x01);
    TEST_ASSERT_EQUAL_UINT8(0, rsp.data_len);

    /* Verify event was stored */
    uint8_t count;
    const uds_event_record_t *store = uds_svc_event_store_get(&count);
    TEST_ASSERT_NOT_NULL(store);
    TEST_ASSERT_TRUE(store[0].active);
    TEST_ASSERT_EQUAL_UINT8(0xAA, store[0].event_type);
}

void test_response_on_event_stop_clears(void)
{
    /* First store an event */
    {
        uds_response_t rsp;
        uint8_t raw[] = {0x86, 0x01, 0xBB};
        call_handler(raw, sizeof(raw), &rsp, NULL,
                     uds_svc_response_on_event);
    }

    /* Now stop */
    {
        uds_response_t rsp;
        uint8_t raw[] = {0x86, 0x00, 0x00};  /* stopResponseOnEvent */
        bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                                   uds_svc_response_on_event);

        TEST_ASSERT_TRUE(result);
        check_pos_rsp(&rsp, RESPONSE_ON_EVENT_RSP, 0x00);
    }

    /* Event store should be cleared */
    const uds_event_record_t *store = uds_svc_event_store_get(NULL);
    TEST_ASSERT_NOT_NULL(store);
    TEST_ASSERT_FALSE(store[0].active);
}

void test_response_on_event_clear(void)
{
    /* First store an event */
    {
        uds_response_t rsp;
        uint8_t raw[] = {0x86, 0x01, 0xCC};
        call_handler(raw, sizeof(raw), &rsp, NULL,
                     uds_svc_response_on_event);
    }

    /* Clear */
    {
        uds_response_t rsp;
        uint8_t raw[] = {0x86, 0x02, 0x00};  /* clearResponseOnEvent */
        call_handler(raw, sizeof(raw), &rsp, NULL,
                     uds_svc_response_on_event);
    }

    const uds_event_record_t *store = uds_svc_event_store_get(NULL);
    TEST_ASSERT_FALSE(store[0].active);
}

void test_response_on_event_invalid_subfn_returns_nrc12(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x86, 0xFF, 0x01};  /* invalid subfunction */

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_response_on_event);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x86, NRC_SUB_FUNCTION_NOT_SUPPORTED);
}

void test_response_on_event_imloif_no_data(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x86, 0x01};  /* missing eventType */

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_response_on_event);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x86, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

/* ======================================================================== *
 * 0x87 — LinkControl Tests                                                 *
 * ======================================================================== */

void test_link_control_verify_baudrate(void)
{
    uds_response_t rsp;
    /* verifyBaudrateTransition(0x01) + 3-byte baudrate ID */
    uint8_t raw[] = {0x87, 0x01, 0x01, 0x02, 0x03};

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_link_control);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, LINK_CONTROL_RSP, 0x01);
    TEST_ASSERT_EQUAL_UINT8(0, rsp.data_len);
}

void test_link_control_transition_baudrate(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x87, 0x02};  /* transitionBaudrate */

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_link_control);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, LINK_CONTROL_RSP, 0x02);
}

void test_link_control_unsupported_subfn_returns_nrc12(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {0x87, 0x00};  /* invalid subfunction */

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_link_control);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x87, NRC_SUB_FUNCTION_NOT_SUPPORTED);
}

void test_link_control_verify_imloif_short_data(void)
{
    uds_response_t rsp;
    /* verify needs 3 bytes, only provide 2 */
    uint8_t raw[] = {0x87, 0x01, 0x01, 0x02};

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_link_control);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x87, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

void test_link_control_transition_imloif_extra_data(void)
{
    uds_response_t rsp;
    /* transition expects no data, but we provide a byte */
    uint8_t raw[] = {0x87, 0x02, 0xAA};

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_link_control);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, 0x87, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

void test_link_control_sprmib_suppresses(void)
{
    uds_response_t rsp;
    /* 0x87 with suppress bit (0x81) on subfunction 0x01 + baudrate */
    uint8_t raw[] = {0x87, 0x81, 0x01, 0x02, 0x03};

    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                               uds_svc_link_control);

    TEST_ASSERT_FALSE(result);
}

/* ======================================================================== *
 * Test Runner                                                              *
 * ======================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* 0x10 — DiagnosticSessionControl */
    RUN_TEST(test_dsc_valid_session_switch);
    RUN_TEST(test_dsc_invalid_session_returns_nrc12);
    RUN_TEST(test_dsc_imloif_extra_bytes);
    RUN_TEST(test_dsc_sprmib_suppresses_response);
    RUN_TEST(test_dsc_null_context_returns_nrc22);

    /* 0x11 — ECUReset */
    RUN_TEST(test_ecu_reset_hard_reset);
    RUN_TEST(test_ecu_reset_soft_reset);
    RUN_TEST(test_ecu_reset_unsupported_type_returns_nrc12);
    RUN_TEST(test_ecu_reset_imloif_extra_bytes);
    RUN_TEST(test_ecu_reset_sprmib_suppresses);

    /* 0x27 — SecurityAccess */
    RUN_TEST(test_security_request_seed_returns_nonzero_seed);
    RUN_TEST(test_security_send_key_valid_unlocks);
    RUN_TEST(test_security_send_key_invalid_returns_nrc35);
    RUN_TEST(test_security_send_key_without_request_returns_nrc24);
    RUN_TEST(test_security_request_seed_invalid_level_returns_nrc12);
    RUN_TEST(test_security_send_key_imloif_no_key_data);
    RUN_TEST(test_security_request_seed_sprmib_suppresses);

    /* 0x28 — CommunicationControl */
    RUN_TEST(test_comm_control_valid);
    RUN_TEST(test_comm_control_imloif_no_data);
    RUN_TEST(test_comm_control_invalid_control_type_returns_nrc12);
    RUN_TEST(test_comm_control_invalid_comm_type_returns_nrc31);
    RUN_TEST(test_comm_control_sprmib_suppresses);

    /* 0x3E — TesterPresent */
    RUN_TEST(test_tester_present_positive_response);
    RUN_TEST(test_tester_present_sprmib_suppresses);
    RUN_TEST(test_tester_present_imloif_extra_bytes);
    RUN_TEST(test_tester_present_subfunction_echoed);

    /* 0x85 — ControlDTCSetting */
    RUN_TEST(test_dtc_setting_on_with_unlock);
    RUN_TEST(test_dtc_setting_off_with_unlock);
    RUN_TEST(test_dtc_setting_without_unlock_returns_nrc33);
    RUN_TEST(test_dtc_setting_invalid_type_returns_nrc12);
    RUN_TEST(test_dtc_setting_null_context_returns_nrc22);
    RUN_TEST(test_dtc_setting_sprmib_suppresses);

    /* 0x86 — ResponseOnEvent */
    RUN_TEST(test_response_on_event_start);
    RUN_TEST(test_response_on_event_stop_clears);
    RUN_TEST(test_response_on_event_clear);
    RUN_TEST(test_response_on_event_invalid_subfn_returns_nrc12);
    RUN_TEST(test_response_on_event_imloif_no_data);

    /* 0x87 — LinkControl */
    RUN_TEST(test_link_control_verify_baudrate);
    RUN_TEST(test_link_control_transition_baudrate);
    RUN_TEST(test_link_control_unsupported_subfn_returns_nrc12);
    RUN_TEST(test_link_control_verify_imloif_short_data);
    RUN_TEST(test_link_control_transition_imloif_extra_data);
    RUN_TEST(test_link_control_sprmib_suppresses);

    return UNITY_END();
}
