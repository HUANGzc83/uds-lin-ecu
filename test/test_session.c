/*
 * test_session.c
 * UDS Session State Machine unit tests — Wave 2 Task 7
 */

#include "unity.h"
#include "uds/uds_session.h"

void setUp(void)
{
    /* Called before each test */
}

void tearDown(void)
{
    /* Called after each test */
}

/* ======================================================================== *
 * Init → current_session == UDS_DEFAULT_SESSION                            *
 * ======================================================================== */
void test_init_sets_default_session(void)
{
    uds_session_context_t ctx;
    uds_session_init(&ctx);

    TEST_ASSERT_EQUAL(UDS_DEFAULT_SESSION, ctx.current_session);
    TEST_ASSERT_FALSE(ctx.events_paused);
    TEST_ASSERT_TRUE(ctx.security_locked);
    TEST_ASSERT_EQUAL(50,   ctx.params.p2_server_max);
    TEST_ASSERT_EQUAL(5000, ctx.params.p2_star_server_max);
}

/* ======================================================================== *
 * default → programming → session changes, params returned                *
 * ======================================================================== */
void test_default_to_programming_ok(void)
{
    uds_session_context_t ctx;
    uds_session_init(&ctx);

    uds_nrc_t nrc = 0;
    uds_status_t status = uds_session_switch(&ctx, UDS_PROGRAMMING_SESSION, &nrc);

    TEST_ASSERT_EQUAL(UDS_OK, status);
    TEST_ASSERT_EQUAL(UDS_PROGRAMMING_SESSION, ctx.current_session);
    TEST_ASSERT_TRUE(ctx.events_paused);
    TEST_ASSERT_TRUE(ctx.security_locked);   /* unchanged by default→non-default */

    /* Params should now reflect programming session */
    const uds_std_return_t *params = uds_session_get_params(&ctx);
    TEST_ASSERT_NOT_NULL(params);
    TEST_ASSERT_EQUAL(5000, params->p2_server_max);
    TEST_ASSERT_EQUAL(5000, params->p2_star_server_max);
}

/* ======================================================================== *
 * Unsupported session (0xFF) → NRC 0x12 (SFNS)                            *
 * ======================================================================== */
void test_unsupported_session_returns_nrc_sfns(void)
{
    uds_session_context_t ctx;
    uds_session_init(&ctx);

    uds_nrc_t nrc = 0;
    uds_status_t status = uds_session_switch(&ctx, 0xFF, &nrc);

    TEST_ASSERT_NOT_EQUAL(UDS_OK, status);
    TEST_ASSERT_EQUAL(NRC_SUB_FUNCTION_NOT_SUPPORTED, nrc);

    /* Session should remain unchanged on failure */
    TEST_ASSERT_EQUAL(UDS_DEFAULT_SESSION, ctx.current_session);
}

/* ======================================================================== *
 * Transition default→programming→default → security re-locked flag set    *
 * ======================================================================== */
void test_default_to_prog_to_default_relocks_security(void)
{
    uds_session_context_t ctx;
    uds_session_init(&ctx);

    uds_nrc_t nrc = 0;

    /* Switch to programming (non-default) */
    uds_session_switch(&ctx, UDS_PROGRAMMING_SESSION, &nrc);
    TEST_ASSERT_EQUAL(UDS_PROGRAMMING_SESSION, ctx.current_session);

    /* Simulate security being unlocked by SecurityAccess service */
    ctx.security_locked = false;

    /* Switch back to default — should re-lock security */
    uds_session_switch(&ctx, UDS_DEFAULT_SESSION, &nrc);
    TEST_ASSERT_EQUAL(UDS_DEFAULT_SESSION, ctx.current_session);
    TEST_ASSERT_TRUE(ctx.security_locked);
    TEST_ASSERT_FALSE(ctx.events_paused);  /* events resumed when returning to default */
}

/* ======================================================================== *
 * Transition non-default→default→extended → events paused flag set        *
 * ======================================================================== */
void test_non_default_to_default_to_extended_pauses_events(void)
{
    uds_session_context_t ctx;
    uds_session_init(&ctx);

    uds_nrc_t nrc = 0;

    /* default → programming (entering non-default → events pause) */
    uds_session_switch(&ctx, UDS_PROGRAMMING_SESSION, &nrc);
    TEST_ASSERT_TRUE(ctx.events_paused);

    /* programming → default (returning to default → events resume) */
    uds_session_switch(&ctx, UDS_DEFAULT_SESSION, &nrc);
    TEST_ASSERT_FALSE(ctx.events_paused);

    /* default → extended (entering non-default again → events pause) */
    uds_session_switch(&ctx, UDS_EXTENDED_SESSION, &nrc);
    TEST_ASSERT_TRUE(ctx.events_paused);
}

/* ======================================================================== *
 * Session parameter record correct per session type                        *
 * ======================================================================== */
void test_session_params_correct_per_type(void)
{
    uds_session_context_t ctx;
    uds_session_init(&ctx);
    uds_nrc_t nrc = 0;

    /* Default session */
    TEST_ASSERT_EQUAL(50,   ctx.params.p2_server_max);
    TEST_ASSERT_EQUAL(5000, ctx.params.p2_star_server_max);

    /* Programming session */
    uds_session_switch(&ctx, UDS_PROGRAMMING_SESSION, &nrc);
    TEST_ASSERT_EQUAL(5000, ctx.params.p2_server_max);
    TEST_ASSERT_EQUAL(5000, ctx.params.p2_star_server_max);

    /* Extended session */
    uds_session_switch(&ctx, UDS_EXTENDED_SESSION, &nrc);
    TEST_ASSERT_EQUAL(5000, ctx.params.p2_server_max);
    TEST_ASSERT_EQUAL(5000, ctx.params.p2_star_server_max);
}

/* ======================================================================== *
 * uds_session_is_supported returns correct for known/unknown sessions      *
 * ======================================================================== */
void test_is_supported(void)
{
    TEST_ASSERT_TRUE(uds_session_is_supported(UDS_DEFAULT_SESSION));
    TEST_ASSERT_TRUE(uds_session_is_supported(UDS_PROGRAMMING_SESSION));
    TEST_ASSERT_TRUE(uds_session_is_supported(UDS_EXTENDED_SESSION));

    TEST_ASSERT_FALSE(uds_session_is_supported(0x00));
    TEST_ASSERT_FALSE(uds_session_is_supported(0xFF));
    TEST_ASSERT_FALSE(uds_session_is_supported(0x04));
    TEST_ASSERT_FALSE(uds_session_is_supported(0xAA));
}

/* ======================================================================== *
 * Test runner                                                               *
 * ======================================================================== */
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_sets_default_session);
    RUN_TEST(test_default_to_programming_ok);
    RUN_TEST(test_unsupported_session_returns_nrc_sfns);
    RUN_TEST(test_default_to_prog_to_default_relocks_security);
    RUN_TEST(test_non_default_to_default_to_extended_pauses_events);
    RUN_TEST(test_session_params_correct_per_type);
    RUN_TEST(test_is_supported);

    return UNITY_END();
}
