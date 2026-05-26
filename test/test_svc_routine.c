/*
 * test_svc_routine.c
 * Routine Control Service (0x31) — Unit Tests
 *
 * Tests the RoutineControl handler, routine registry, and default
 * routines (eraseMemory 0xFF00, checkProgrammingIntegrity 0xFF01).
 *
 * Wave 3b Task 13 — Routine Control Service (0x31 RoutineControl)
 */

#include "unity.h"
#include "uds/uds_core.h"
#include "uds/uds_svc_routine.h"
#include <string.h>

/* ======================================================================== *
 * Test Fixture                                                             *
 * ======================================================================== */

static bool unlocked;

/** @brief Custom routines IDs used in tests */
#define CUSTOM_ROUTINE_ID_1  0x5000u
#define CUSTOM_ROUTINE_ID_2  0x5001u

/** @brief Scratch buffers for test callbacks */
static uint8_t g_test_start_resp[UDS_ROUTINE_RSP_MAX];
static uint8_t g_test_stop_resp[UDS_ROUTINE_RSP_MAX];
static uint8_t g_test_results_resp[UDS_ROUTINE_RSP_MAX];

/* ----------------------------------------------------------------------- *
 * Custom routine callbacks for testing                                    *
 * ----------------------------------------------------------------------- */

static bool custom_start_fn(const uint8_t *params, uint16_t params_len,
                            uint8_t *resp, uint16_t *resp_len)
{
    /* Echo params as response (up to buffer capacity) */
    uint16_t copy_len = (params_len < *resp_len) ? params_len : *resp_len;
    if (copy_len > 0u && params != NULL && resp != NULL)
    {
        memcpy(resp, params, copy_len);
    }
    *resp_len = copy_len;

    /* Store for test inspection */
    memcpy(g_test_start_resp, resp, *resp_len);
    return true;
}

static bool custom_stop_fn(const uint8_t *params, uint16_t params_len,
                           uint8_t *resp, uint16_t *resp_len)
{
    uint16_t copy_len = (params_len < *resp_len) ? params_len : *resp_len;
    if (copy_len > 0u && params != NULL && resp != NULL)
    {
        memcpy(resp, params, copy_len);
    }
    *resp_len = copy_len;
    memcpy(g_test_stop_resp, resp, *resp_len);
    return true;
}

static bool custom_results_fn(const uint8_t *params, uint16_t params_len,
                              uint8_t *resp, uint16_t *resp_len)
{
    /* Return a fixed status byte: 0x55 = in-progress */
    if (*resp_len >= 1u && resp != NULL)
    {
        resp[0] = 0x55u;
        *resp_len = 1u;
    }
    memcpy(g_test_results_resp, resp, *resp_len);
    return true;
}

static bool failing_fn(const uint8_t *params, uint16_t params_len,
                       uint8_t *resp, uint16_t *resp_len)
{
    (void)params;
    (void)params_len;
    (void)resp;
    (void)resp_len;
    return false; /* always fails */
}

/* ----------------------------------------------------------------------- *
 * setUp / tearDown                                                        *
 * ----------------------------------------------------------------------- */

void setUp(void)
{
    memset(g_test_start_resp, 0, sizeof(g_test_start_resp));
    memset(g_test_stop_resp, 0, sizeof(g_test_stop_resp));
    memset(g_test_results_resp, 0, sizeof(g_test_results_resp));
    unlocked = false;
    uds_svc_routine_init();

    /* Register a custom test routine (available in default session, no security) */
    uds_routine_entry_t custom;
    memset(&custom, 0, sizeof(custom));
    custom.routine_id     = CUSTOM_ROUTINE_ID_1;
    custom.session_mask   = ROUTINE_AVAIL_DEFAULT;
    custom.security_level = 0u;
    custom.start_fn       = custom_start_fn;
    custom.stop_fn        = custom_stop_fn;
    custom.results_fn     = custom_results_fn;
    TEST_ASSERT_TRUE(uds_svc_routine_register(&custom));

    /* Register a secured custom routine */
    uds_routine_entry_t secured;
    memset(&secured, 0, sizeof(secured));
    secured.routine_id     = CUSTOM_ROUTINE_ID_2;
    secured.session_mask   = ROUTINE_AVAIL_DEFAULT;
    secured.security_level = 1u;
    secured.start_fn       = custom_start_fn;
    secured.stop_fn        = custom_stop_fn;
    secured.results_fn     = custom_results_fn;
    TEST_ASSERT_TRUE(uds_svc_routine_register(&secured));
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
                          uds_response_t *rsp, void *context)
{
    uds_request_t req;
    uds_status_t status = uds_parse_request(raw, raw_len, &req);
    TEST_ASSERT_EQUAL(UDS_OK, status);
    return uds_svc_routine_control(&req, rsp, context);
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
 *
 * Checks the uds_set_neg_rsp output layout:
 *   rsp->sid = 0x7F
 *   rsp->subfunc_echo = expected_subfunc
 *   rsp->data = {expected_sid, expected_nrc}  (2 bytes)
 */
static void check_neg_rsp(const uds_response_t *rsp,
                          uint8_t expected_sid,
                          uint8_t expected_nrc,
                          uint8_t expected_subfunc)
{
    TEST_ASSERT_EQUAL_UINT8(0x7F,               rsp->sid);
    TEST_ASSERT_EQUAL_UINT8(expected_subfunc,   rsp->subfunc_echo);
    TEST_ASSERT_NOT_NULL(rsp->data);
    TEST_ASSERT_EQUAL_UINT8(2,                  rsp->data_len);
    TEST_ASSERT_EQUAL_UINT8(expected_sid,       rsp->data[0]);
    TEST_ASSERT_EQUAL_UINT8(expected_nrc,       rsp->data[1]);
}

/**
 * @brief Build a RoutineControl raw request buffer.
 *
 * Format: [SID][subfn][routineID_hi][routineID_lo][ctrl...]
 *
 * @param[out] buf      Output buffer
 * @param[in]  subfn    Subfunction value (without suppress bit)
 * @param[in]  routine  Routine identifier
 * @param[in]  ctrl     ControlOptionRecord data (may be NULL)
 * @param[in]  ctrl_len Length of control data
 * @return Total length of the built message
 */
static uint16_t build_routine_req(uint8_t *buf, uint8_t subfn,
                                   uint16_t routine,
                                   const uint8_t *ctrl, uint16_t ctrl_len)
{
    buf[0] = ROUTINE_CONTROL;
    buf[1] = subfn;
    buf[2] = (uint8_t)((routine >> 8) & 0xFFu);
    buf[3] = (uint8_t)( routine        & 0xFFu);

    if (ctrl_len > 0u && ctrl != NULL)
    {
        memcpy(buf + 4, ctrl, ctrl_len);
    }

    return (uint16_t)(4u + ctrl_len);
}

/* ======================================================================== *
 * Routine Registry Tests                                                   *
 * ======================================================================== */

void test_registry_init_has_defaults(void)
{
    /* After init, two default routines should be registered */
    TEST_ASSERT_EQUAL_UINT8(4u, uds_svc_routine_get_count()); /* 2 defaults + 2 custom from setUp */

    /* Check default routines exist */
    const uds_routine_entry_t *e0 = uds_svc_routine_find(0xFF00u);
    TEST_ASSERT_NOT_NULL(e0);
    TEST_ASSERT_EQUAL_UINT16(0xFF00u, e0->routine_id);
    TEST_ASSERT_EQUAL_UINT8(ROUTINE_AVAIL_PROGRAMMING, e0->session_mask);
    TEST_ASSERT_EQUAL_UINT8(1u, e0->security_level);
    TEST_ASSERT_NOT_NULL(e0->start_fn);
    TEST_ASSERT_NULL(e0->stop_fn);
    TEST_ASSERT_NULL(e0->results_fn);

    const uds_routine_entry_t *e1 = uds_svc_routine_find(0xFF01u);
    TEST_ASSERT_NOT_NULL(e1);
    TEST_ASSERT_EQUAL_UINT16(0xFF01u, e1->routine_id);
    TEST_ASSERT_EQUAL_UINT8(ROUTINE_AVAIL_DEFAULT |
                             ROUTINE_AVAIL_PROGRAMMING |
                             ROUTINE_AVAIL_EXTENDED, e1->session_mask);
    TEST_ASSERT_EQUAL_UINT8(0u, e1->security_level);
    TEST_ASSERT_NOT_NULL(e1->start_fn);
    TEST_ASSERT_NULL(e1->stop_fn);
    TEST_ASSERT_NULL(e1->results_fn);
}

void test_registry_register_duplicate_fails(void)
{
    uds_routine_entry_t dup;
    memset(&dup, 0, sizeof(dup));
    dup.routine_id   = CUSTOM_ROUTINE_ID_1;
    dup.session_mask = ROUTINE_AVAIL_DEFAULT;
    dup.start_fn     = custom_start_fn;

    TEST_ASSERT_FALSE(uds_svc_routine_register(&dup));
}

void test_registry_find_nonexistent_returns_null(void)
{
    TEST_ASSERT_NULL(uds_svc_routine_find(0xFFFFu));
}

void test_registry_get_entry_by_index(void)
{
    /* Get entry at index 0 (first default) */
    const uds_routine_entry_t *e = uds_svc_routine_get_entry(0);
    TEST_ASSERT_NOT_NULL(e);

    /* Out-of-range index returns NULL */
    TEST_ASSERT_NULL(uds_svc_routine_get_entry(99u));
}

void test_registry_get_entry_ordered(void)
{
    /* Should be able to iterate all entries */
    uint8_t count = uds_svc_routine_get_count();
    TEST_ASSERT_TRUE(count > 0u);

    for (uint8_t i = 0u; i < count; i++)
    {
        const uds_routine_entry_t *e = uds_svc_routine_get_entry(i);
        TEST_ASSERT_NOT_NULL(e);
        TEST_ASSERT_TRUE(e->active);
    }

    /* Past end returns NULL */
    TEST_ASSERT_NULL(uds_svc_routine_get_entry(count));
}

void test_registry_register_null_entry(void)
{
    TEST_ASSERT_FALSE(uds_svc_routine_register(NULL));
}

/* ======================================================================== *
 * RoutineControl — Start (0x01) Tests                                     *
 * ======================================================================== */

void test_start_routine_valid(void)
{
    uds_response_t rsp;
    uint8_t raw[4];
    uint16_t len = build_routine_req(raw, ROUTINE_SUBFN_START, CUSTOM_ROUTINE_ID_1, NULL, 0);

    unlocked = true;
    bool result = call_handler(raw, len, &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, ROUTINE_CONTROL_RSP, ROUTINE_SUBFN_START);
    TEST_ASSERT_EQUAL_UINT8(0, rsp.data_len); /* no params, empty response */

    /* Routine should be marked as running */
    const uds_routine_entry_t *e = uds_svc_routine_find(CUSTOM_ROUTINE_ID_1);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_TRUE(e->is_running);
}

void test_start_routine_with_params(void)
{
    uds_response_t rsp;
    uint8_t ctrl_params[] = {0xAA, 0xBB, 0xCC};
    uint8_t raw[4 + sizeof(ctrl_params)];
    uint16_t len = build_routine_req(raw, ROUTINE_SUBFN_START, CUSTOM_ROUTINE_ID_1,
                                      ctrl_params, sizeof(ctrl_params));

    unlocked = true;
    bool result = call_handler(raw, len, &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, ROUTINE_CONTROL_RSP, ROUTINE_SUBFN_START);

    /* Response should echo the control params (custom_start_fn behavior) */
    TEST_ASSERT_EQUAL_UINT8(sizeof(ctrl_params), rsp.data_len);
    TEST_ASSERT_NOT_NULL(rsp.data);
    TEST_ASSERT_EQUAL_UINT8(ctrl_params[0], rsp.data[0]);
    TEST_ASSERT_EQUAL_UINT8(ctrl_params[1], rsp.data[1]);
    TEST_ASSERT_EQUAL_UINT8(ctrl_params[2], rsp.data[2]);
}

void test_start_routine_sprmib_suppresses(void)
{
    uds_response_t rsp;
    uint8_t raw[4];
    /* Set suppress bit on subfunction */
    build_routine_req(raw, ROUTINE_SUBFN_START | 0x80u, CUSTOM_ROUTINE_ID_1, NULL, 0);

    unlocked = true;
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked);

    TEST_ASSERT_FALSE(result);

    /* But routine should still be running */
    const uds_routine_entry_t *e = uds_svc_routine_find(CUSTOM_ROUTINE_ID_1);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_TRUE(e->is_running);
}

void test_start_routine_already_running_returns_nrc22(void)
{
    uds_response_t rsp;
    uint8_t raw[4];
    uint16_t len = build_routine_req(raw, ROUTINE_SUBFN_START, CUSTOM_ROUTINE_ID_1, NULL, 0);

    unlocked = true;
    call_handler(raw, len, &rsp, &unlocked); /* first start succeeds */

    /* Second start should fail */
    uds_response_t rsp2;
    bool result = call_handler(raw, len, &rsp2, &unlocked);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp2, ROUTINE_CONTROL, NRC_CONDITIONS_NOT_CORRECT, ROUTINE_SUBFN_START);
}

void test_start_routine_another_running_returns_nrc22(void)
{
    /* Start first routine */
    uds_response_t rsp;
    uint8_t raw1[4];
    uint16_t len1 = build_routine_req(raw1, ROUTINE_SUBFN_START, CUSTOM_ROUTINE_ID_1, NULL, 0);

    unlocked = true;
    call_handler(raw1, len1, &rsp, &unlocked);

    /* Register second routine and try to start it */
    uds_routine_entry_t another;
    memset(&another, 0, sizeof(another));
    another.routine_id     = 0x5002u;
    another.session_mask   = ROUTINE_AVAIL_DEFAULT;
    another.start_fn       = custom_start_fn;
    another.stop_fn        = custom_stop_fn;
    another.results_fn     = custom_results_fn;
    uds_svc_routine_register(&another);

    uds_response_t rsp2;
    uint8_t raw2[4];
    uint16_t len2 = build_routine_req(raw2, ROUTINE_SUBFN_START, 0x5002u, NULL, 0);

    bool result = call_handler(raw2, len2, &rsp2, &unlocked);
    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp2, ROUTINE_CONTROL, NRC_CONDITIONS_NOT_CORRECT, ROUTINE_SUBFN_START);
}

void test_start_routine_unknown_returns_nrc31(void)
{
    uds_response_t rsp;
    uint8_t raw[4];
    uint16_t len = build_routine_req(raw, ROUTINE_SUBFN_START, 0x9999u, NULL, 0);

    unlocked = true;
    bool result = call_handler(raw, len, &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, ROUTINE_CONTROL, NRC_REQUEST_OUT_OF_RANGE, ROUTINE_SUBFN_START);
}

void test_start_routine_missing_routineid_returns_nrc13(void)
{
    uds_response_t rsp;
    /* Only SID + subfunction, no routineID bytes */
    uint8_t raw[] = {ROUTINE_CONTROL, ROUTINE_SUBFN_START};

    unlocked = true;
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, ROUTINE_CONTROL, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT, ROUTINE_SUBFN_START);
}

void test_start_routine_secured_without_unlock_returns_nrc33(void)
{
    uds_response_t rsp;
    uint8_t raw[4];
    uint16_t len = build_routine_req(raw, ROUTINE_SUBFN_START, CUSTOM_ROUTINE_ID_2, NULL, 0);

    unlocked = false; /* not unlocked */
    bool result = call_handler(raw, len, &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, ROUTINE_CONTROL, NRC_SECURITY_ACCESS_DENIED, ROUTINE_SUBFN_START);
}

void test_start_routine_secured_with_unlock_succeeds(void)
{
    uds_response_t rsp;
    uint8_t raw[4];
    uint16_t len = build_routine_req(raw, ROUTINE_SUBFN_START, CUSTOM_ROUTINE_ID_2, NULL, 0);

    unlocked = true;
    bool result = call_handler(raw, len, &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, ROUTINE_CONTROL_RSP, ROUTINE_SUBFN_START);
}

void test_start_routine_no_start_fn_returns_nrc12(void)
{
    /* Use default routine 0xFF01 which has no stop/results fn but check start */
    /* Actually 0xFF01 DOES have start_fn. Let's register one without start_fn. */
    uds_routine_entry_t no_start;
    memset(&no_start, 0, sizeof(no_start));
    no_start.routine_id   = 0x6000u;
    no_start.session_mask = ROUTINE_AVAIL_DEFAULT;
    no_start.start_fn     = NULL; /* no start callback */
    no_start.stop_fn      = custom_stop_fn;
    TEST_ASSERT_TRUE(uds_svc_routine_register(&no_start));

    uds_response_t rsp;
    uint8_t raw[4];
    uint16_t len = build_routine_req(raw, ROUTINE_SUBFN_START, 0x6000u, NULL, 0);

    unlocked = true;
    bool result = call_handler(raw, len, &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, ROUTINE_CONTROL, NRC_SUB_FUNCTION_NOT_SUPPORTED, ROUTINE_SUBFN_START);
}

void test_start_routine_callback_fails_returns_nrc72(void)
{
    /* Register routine with failing callback */
    uds_routine_entry_t fail_routine;
    memset(&fail_routine, 0, sizeof(fail_routine));
    fail_routine.routine_id   = 0x7000u;
    fail_routine.session_mask = ROUTINE_AVAIL_DEFAULT;
    fail_routine.start_fn     = failing_fn;
    uds_svc_routine_register(&fail_routine);

    uds_response_t rsp;
    uint8_t raw[4];
    uint16_t len = build_routine_req(raw, ROUTINE_SUBFN_START, 0x7000u, NULL, 0);

    unlocked = true;
    bool result = call_handler(raw, len, &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, ROUTINE_CONTROL, NRC_GENERAL_PROGRAMMING_FAILURE, ROUTINE_SUBFN_START);
}

/* ======================================================================== *
 * RoutineControl — Stop (0x02) Tests                                      *
 * ======================================================================== */

void test_stop_routine_valid(void)
{
    /* First start the routine */
    uds_response_t rsp_start;
    uint8_t raw_start[4];
    uint16_t len = build_routine_req(raw_start, ROUTINE_SUBFN_START, CUSTOM_ROUTINE_ID_1, NULL, 0);
    unlocked = true;
    call_handler(raw_start, len, &rsp_start, &unlocked);

    /* Now stop it */
    uds_response_t rsp_stop;
    uint8_t raw_stop[4];
    uint16_t len_stop = build_routine_req(raw_stop, ROUTINE_SUBFN_STOP, CUSTOM_ROUTINE_ID_1, NULL, 0);

    bool result = call_handler(raw_stop, len_stop, &rsp_stop, &unlocked);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp_stop, ROUTINE_CONTROL_RSP, ROUTINE_SUBFN_STOP);

    /* Routine should no longer be running */
    const uds_routine_entry_t *e = uds_svc_routine_find(CUSTOM_ROUTINE_ID_1);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_FALSE(e->is_running);
}

void test_stop_routine_not_running_returns_nrc22(void)
{
    uds_response_t rsp;
    uint8_t raw[4];
    uint16_t len = build_routine_req(raw, ROUTINE_SUBFN_STOP, CUSTOM_ROUTINE_ID_1, NULL, 0);

    unlocked = true;
    bool result = call_handler(raw, len, &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, ROUTINE_CONTROL, NRC_CONDITIONS_NOT_CORRECT, ROUTINE_SUBFN_STOP);
}

void test_stop_routine_sprmib_suppresses(void)
{
    /* Start first */
    uds_response_t rsp_start;
    uint8_t raw_start[4];
    uint16_t len_start = build_routine_req(raw_start, ROUTINE_SUBFN_START, CUSTOM_ROUTINE_ID_1, NULL, 0);
    unlocked = true;
    call_handler(raw_start, len_start, &rsp_start, &unlocked);

    /* Stop with suppress bit */
    uds_response_t rsp_stop;
    uint8_t raw_stop[4];
    build_routine_req(raw_stop, ROUTINE_SUBFN_STOP | 0x80u, CUSTOM_ROUTINE_ID_1, NULL, 0);

    bool result = call_handler(raw_stop, sizeof(raw_stop), &rsp_stop, &unlocked);

    TEST_ASSERT_FALSE(result);

    /* Routine should be stopped despite suppression */
    const uds_routine_entry_t *e = uds_svc_routine_find(CUSTOM_ROUTINE_ID_1);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_FALSE(e->is_running);
}

void test_stop_routine_no_stop_fn_returns_nrc12(void)
{
    /* Try to stop a default routine that has no stop_fn */
    /* First start it */
    uds_response_t rsp_start;
    uint8_t raw_start[4];
    uint16_t len_start = build_routine_req(raw_start, ROUTINE_SUBFN_START, 0xFF00u, NULL, 0);
    unlocked = true;
    call_handler(raw_start, len_start, &rsp_start, &unlocked);

    /* Now try to stop (0xFF00 has no stop_fn) */
    uds_response_t rsp_stop;
    uint8_t raw_stop[4];
    uint16_t len_stop = build_routine_req(raw_stop, ROUTINE_SUBFN_STOP, 0xFF00u, NULL, 0);

    bool result = call_handler(raw_stop, len_stop, &rsp_stop, &unlocked);
    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp_stop, ROUTINE_CONTROL, NRC_SUB_FUNCTION_NOT_SUPPORTED, ROUTINE_SUBFN_STOP);
}

/* ======================================================================== *
 * RoutineControl — RequestResults (0x03) Tests                            *
 * ======================================================================== */

void test_request_results_valid(void)
{
    /* RequestResults does not require the routine to be running */
    uds_response_t rsp;
    uint8_t raw[4];
    uint16_t len = build_routine_req(raw, ROUTINE_SUBFN_REQUEST_RESULTS, CUSTOM_ROUTINE_ID_1, NULL, 0);

    unlocked = true;
    bool result = call_handler(raw, len, &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, ROUTINE_CONTROL_RSP, ROUTINE_SUBFN_REQUEST_RESULTS);
    TEST_ASSERT_EQUAL_UINT8(1, rsp.data_len);
    TEST_ASSERT_NOT_NULL(rsp.data);
    TEST_ASSERT_EQUAL_UINT8(0x55, rsp.data[0]); /* fixed status from custom_results_fn */
}

void test_request_results_sprmib_suppresses(void)
{
    uds_response_t rsp;
    uint8_t raw[4];
    build_routine_req(raw, ROUTINE_SUBFN_REQUEST_RESULTS | 0x80u, CUSTOM_ROUTINE_ID_1, NULL, 0);

    unlocked = true;
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked);

    TEST_ASSERT_FALSE(result);
}

void test_request_results_no_results_fn_returns_nrc12(void)
{
    /* Default routine 0xFF00 has no results_fn */
    uds_response_t rsp;
    uint8_t raw[4];
    uint16_t len = build_routine_req(raw, ROUTINE_SUBFN_REQUEST_RESULTS, 0xFF00u, NULL, 0);

    unlocked = true;
    bool result = call_handler(raw, len, &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, ROUTINE_CONTROL, NRC_SUB_FUNCTION_NOT_SUPPORTED, ROUTINE_SUBFN_REQUEST_RESULTS);
}

/* ======================================================================== *
 * RoutineControl — Invalid Subfunction Tests                              *
 * ======================================================================== */

void test_invalid_subfunction_returns_nrc12(void)
{
    uds_response_t rsp;
    /* Subfunction 0xFF is invalid */
    uint8_t raw[] = {ROUTINE_CONTROL, 0xFF, 0x50, 0x00};

    unlocked = true;
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, ROUTINE_CONTROL, NRC_SUB_FUNCTION_NOT_SUPPORTED, 0x7F);
}

void test_subfunction_zero_returns_nrc12(void)
{
    uds_response_t rsp;
    uint8_t raw[] = {ROUTINE_CONTROL, 0x00, 0x50, 0x00};

    unlocked = true;
    bool result = call_handler(raw, sizeof(raw), &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, ROUTINE_CONTROL, NRC_SUB_FUNCTION_NOT_SUPPORTED, 0x00);
}

/* ======================================================================== *
 * Default Routine Tests                                                    *
 * ======================================================================== */

void test_default_erase_memory_start_succeeds(void)
{
    uds_response_t rsp;
    uint8_t raw[4];
    uint16_t len = build_routine_req(raw, ROUTINE_SUBFN_START, 0xFF00u, NULL, 0);

    unlocked = true; /* eraseMemory requires security */
    bool result = call_handler(raw, len, &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, ROUTINE_CONTROL_RSP, ROUTINE_SUBFN_START);
    TEST_ASSERT_EQUAL_UINT8(1, rsp.data_len);
    TEST_ASSERT_NOT_NULL(rsp.data);
    TEST_ASSERT_EQUAL_UINT8(0x00, rsp.data[0]); /* success status */
}

void test_default_erase_memory_secured(void)
{
    uds_response_t rsp;
    uint8_t raw[4];
    uint16_t len = build_routine_req(raw, ROUTINE_SUBFN_START, 0xFF00u, NULL, 0);

    unlocked = false; /* not unlocked */
    bool result = call_handler(raw, len, &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, ROUTINE_CONTROL, NRC_SECURITY_ACCESS_DENIED, ROUTINE_SUBFN_START);
}

void test_default_erase_memory_stop_not_supported(void)
{
    /* First start */
    uds_response_t rsp_start;
    uint8_t raw_start[4];
    uint16_t len_start = build_routine_req(raw_start, ROUTINE_SUBFN_START, 0xFF00u, NULL, 0);
    unlocked = true;
    call_handler(raw_start, len_start, &rsp_start, &unlocked);

    /* Stop should return SFNS (no stop_fn) */
    uds_response_t rsp_stop;
    uint8_t raw_stop[4];
    uint16_t len_stop = build_routine_req(raw_stop, ROUTINE_SUBFN_STOP, 0xFF00u, NULL, 0);

    bool result = call_handler(raw_stop, len_stop, &rsp_stop, &unlocked);
    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp_stop, ROUTINE_CONTROL, NRC_SUB_FUNCTION_NOT_SUPPORTED, ROUTINE_SUBFN_STOP);
}

void test_default_check_integrity_start_succeeds(void)
{
    uds_response_t rsp;
    uint8_t raw[4];
    uint16_t len = build_routine_req(raw, ROUTINE_SUBFN_START, 0xFF01u, NULL, 0);

    unlocked = false; /* no security needed */
    bool result = call_handler(raw, len, &rsp, &unlocked);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, ROUTINE_CONTROL_RSP, ROUTINE_SUBFN_START);
    TEST_ASSERT_EQUAL_UINT8(1, rsp.data_len);
    TEST_ASSERT_EQUAL_UINT8(0x00, rsp.data[0]); /* integrity OK */
}

void test_default_check_integrity_no_security_needed(void)
{
    uds_response_t rsp;
    uint8_t raw[4];
    uint16_t len = build_routine_req(raw, ROUTINE_SUBFN_START, 0xFF01u, NULL, 0);

    /* NULL context (no unlocked flag) should still work for non-secured routines */
    bool result = call_handler(raw, len, &rsp, NULL);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, ROUTINE_CONTROL_RSP, ROUTINE_SUBFN_START);
}

/* ======================================================================== *
 * Custom Routine — Full Lifecycle                                         *
 * ======================================================================== */

void test_routine_full_lifecycle(void)
{
    unlocked = true;

    /* 1. Start */
    uds_response_t rsp_start;
    uint8_t raw_start[4];
    uint16_t len_start = build_routine_req(raw_start, ROUTINE_SUBFN_START, CUSTOM_ROUTINE_ID_1, NULL, 0);
    TEST_ASSERT_TRUE(call_handler(raw_start, len_start, &rsp_start, &unlocked));
    check_pos_rsp(&rsp_start, ROUTINE_CONTROL_RSP, ROUTINE_SUBFN_START);
    TEST_ASSERT_TRUE(uds_svc_routine_find(CUSTOM_ROUTINE_ID_1)->is_running);

    /* 2. Request results while running */
    uds_response_t rsp_results;
    uint8_t raw_results[4];
    uint16_t len_results = build_routine_req(raw_results, ROUTINE_SUBFN_REQUEST_RESULTS, CUSTOM_ROUTINE_ID_1, NULL, 0);
    TEST_ASSERT_TRUE(call_handler(raw_results, len_results, &rsp_results, &unlocked));
    check_pos_rsp(&rsp_results, ROUTINE_CONTROL_RSP, ROUTINE_SUBFN_REQUEST_RESULTS);
    TEST_ASSERT_EQUAL_UINT8(0x55, rsp_results.data[0]);

    /* 3. Stop */
    uds_response_t rsp_stop;
    uint8_t raw_stop[4];
    uint16_t len_stop = build_routine_req(raw_stop, ROUTINE_SUBFN_STOP, CUSTOM_ROUTINE_ID_1, NULL, 0);
    TEST_ASSERT_TRUE(call_handler(raw_stop, len_stop, &rsp_stop, &unlocked));
    check_pos_rsp(&rsp_stop, ROUTINE_CONTROL_RSP, ROUTINE_SUBFN_STOP);
    TEST_ASSERT_FALSE(uds_svc_routine_find(CUSTOM_ROUTINE_ID_1)->is_running);

    /* 4. Can start again after stop */
    TEST_ASSERT_TRUE(call_handler(raw_start, len_start, &rsp_start, &unlocked));
    check_pos_rsp(&rsp_start, ROUTINE_CONTROL_RSP, ROUTINE_SUBFN_START);
    TEST_ASSERT_TRUE(uds_svc_routine_find(CUSTOM_ROUTINE_ID_1)->is_running);
}

/* ======================================================================== *
 * Test Runner                                                              *
 * ======================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* Routine Registry */
    RUN_TEST(test_registry_init_has_defaults);
    RUN_TEST(test_registry_register_duplicate_fails);
    RUN_TEST(test_registry_find_nonexistent_returns_null);
    RUN_TEST(test_registry_get_entry_by_index);
    RUN_TEST(test_registry_get_entry_ordered);
    RUN_TEST(test_registry_register_null_entry);

    /* 0x31 — Start (0x01) */
    RUN_TEST(test_start_routine_valid);
    RUN_TEST(test_start_routine_with_params);
    RUN_TEST(test_start_routine_sprmib_suppresses);
    RUN_TEST(test_start_routine_already_running_returns_nrc22);
    RUN_TEST(test_start_routine_another_running_returns_nrc22);
    RUN_TEST(test_start_routine_unknown_returns_nrc31);
    RUN_TEST(test_start_routine_missing_routineid_returns_nrc13);
    RUN_TEST(test_start_routine_secured_without_unlock_returns_nrc33);
    RUN_TEST(test_start_routine_secured_with_unlock_succeeds);
    RUN_TEST(test_start_routine_no_start_fn_returns_nrc12);
    RUN_TEST(test_start_routine_callback_fails_returns_nrc72);

    /* 0x31 — Stop (0x02) */
    RUN_TEST(test_stop_routine_valid);
    RUN_TEST(test_stop_routine_not_running_returns_nrc22);
    RUN_TEST(test_stop_routine_sprmib_suppresses);
    RUN_TEST(test_stop_routine_no_stop_fn_returns_nrc12);

    /* 0x31 — RequestResults (0x03) */
    RUN_TEST(test_request_results_valid);
    RUN_TEST(test_request_results_sprmib_suppresses);
    RUN_TEST(test_request_results_no_results_fn_returns_nrc12);

    /* 0x31 — Invalid Subfunction */
    RUN_TEST(test_invalid_subfunction_returns_nrc12);
    RUN_TEST(test_subfunction_zero_returns_nrc12);

    /* Default Routines */
    RUN_TEST(test_default_erase_memory_start_succeeds);
    RUN_TEST(test_default_erase_memory_secured);
    RUN_TEST(test_default_erase_memory_stop_not_supported);
    RUN_TEST(test_default_check_integrity_start_succeeds);
    RUN_TEST(test_default_check_integrity_no_security_needed);

    /* Full Lifecycle */
    RUN_TEST(test_routine_full_lifecycle);

    return UNITY_END();
}
