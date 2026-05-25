/*
 * test_uds_integration.c
 * UDS Integration Test Runner
 *
 * Task 21 — PC Simulation Main + Integration Test Runner
 *
 * Tests the full UDS-over-LIN pipeline end-to-end:
 *   mock UART → LIN decode → UDS parse → service dispatch →
 *   response serialize → LIN encode → mock UART
 *
 * Supports multi-frame requests and responses via lin_tx_encode
 * for request injection and lin_rx_decode for response reassembly.
 */

#include "unity.h"

#include "uds/uds_core.h"
#include "uds/uds_session.h"
#include "uds/uds_security.h"
#include "uds/uds_dtc.h"
#include "uds/uds_data.h"
#include "uds/uds_service.h"
#include "uds/uds_lin_transport.h"
#include "uds/uds_svc_diagcomm.h"
#include "uds/uds_svc_data.h"
#include "uds/uds_svc_io.h"
#include "uds/uds_svc_routine.h"
#include "uds/uds_svc_upload.h"
#include "uds/uds_svc_auth.h"
#include "uds/uds_svc_stored.h"

#include "mock_all.h"

#include <string.h>

/* ======================================================================== *
 * CRC-8 Helper (matches default_key_validate in uds_security.c)            *
 * ======================================================================== */

/**
 * @brief Compute CRC-8 over a buffer (polynomial 0x07, init 0xFF).
 */
static uint8_t crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFFu;
    for (uint8_t i = 0u; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t j = 0u; j < 8u; j++)
        {
            if (crc & 0x80u)
            {
                crc = (uint8_t)((crc << 1u) ^ 0x07u);
            }
            else
            {
                crc <<= 1u;
            }
        }
    }
    return crc;
}

/* ======================================================================== *
 * Test Fixture — Global State                                              *
 * ======================================================================== */

typedef struct {
    uds_session_context_t  session;
    bool                   unlocked;
} sim_context_t;

static sim_context_t        g_sim_ctx;
static uds_svc_stored_ctx_t g_stored_ctx;
static lin_transport_ctx_t *g_lin_ctx;

/* ======================================================================== *
 * Helper — Select context per SID                                         *
 * ======================================================================== */

static void* get_dispatch_context(uint8_t sid)
{
    switch (sid)
    {
    case DIAGNOSTIC_SESSION_CONTROL:
    case REQUEST_DOWNLOAD:
    case REQUEST_UPLOAD:
    case REQUEST_FILE_TRANSFER:
    case AUTHENTICATION:
        return &g_sim_ctx.session;

    case SECURITY_ACCESS:
    case READ_DATA_BY_IDENTIFIER:
    case READ_SCALING_DATA_BY_ID:
    case WRITE_DATA_BY_IDENTIFIER:
    case WRITE_MEMORY_BY_ADDRESS:
    case INPUT_OUTPUT_CONTROL_BY_ID:
    case ROUTINE_CONTROL:
    case CONTROL_DTC_SETTING:
        return &g_sim_ctx.unlocked;

    case CLEAR_DIAGNOSTIC_INFO:
        g_stored_ctx.sctx     = &g_sim_ctx.session;
        g_stored_ctx.unlocked = &g_sim_ctx.unlocked;
        return &g_stored_ctx;

    default:
        return NULL;
    }
}

/* ======================================================================== *
 * Helper — Post-dispatch sync                                             *
 * ======================================================================== */

static void sync_after_dispatch(uint8_t sid, const uds_response_t *rsp)
{
    if (sid == DIAGNOSTIC_SESSION_CONTROL)
    {
        if (rsp != NULL && rsp->sid == DIAGNOSTIC_SESSION_CONTROL_RSP)
        {
            if (g_sim_ctx.session.security_locked)
            {
                uds_security_lock();
                g_sim_ctx.unlocked = false;
            }
        }
    }
    g_sim_ctx.unlocked = uds_security_is_unlocked();
}

/* ======================================================================== *
 * Round-Trip Helper — Full pipeline                                       *
 * ======================================================================== *
 * Injects a UDS request via LIN frames into mock UART, processes through
 * the full pipeline, and returns the serialised UDS response bytes.
 *
 * @param uds_req_data  Raw UDS request bytes (SID + parameters)
 * @param uds_req_len   Number of request bytes
 * @param rsp_buf       [out] Buffer for response UDS bytes (must hold 512)
 * @param rsp_len       [out] Number of response bytes written
 * @return >0 response length, 0 if suppressed, -1 on error
 */

static int round_trip_uds(const uint8_t *uds_req_data, uint16_t uds_req_len,
                           uint8_t *rsp_buf, uint16_t *rsp_len)
{
    lin_diag_pdu_t  rx_pdu;
    uds_request_t   uds_req;
    uds_response_t  uds_rsp;
    uint8_t         serialised[512];
    uint16_t        serialised_len;
    uds_status_t    st;
    lin_frame_t     rx_frame;

    /* ---- 1. Encode and inject request as LIN frame(s) ---- */
    lin_diag_pdu_t tx_req_pdu;
    tx_req_pdu.nad      = LIN_NAD_DEFAULT;
    tx_req_pdu.pci      = 0;
    tx_req_pdu.uds_data = (uint8_t *)uds_req_data;
    tx_req_pdu.data_len = (uint8_t)uds_req_len;

    uint8_t     tx_req_count = 0;
    lin_frame_t tx_req_frames[8];
    lin_status_t lin_st = lin_tx_encode_ctx(g_lin_ctx, &tx_req_pdu, tx_req_frames,
                                         &tx_req_count, 8);
    TEST_ASSERT_EQUAL(LIN_OK, lin_st);
    TEST_ASSERT(tx_req_count >= 1);

    for (uint8_t i = 0; i < tx_req_count; i++)
    {
        mock_uart_inject_rx_data(tx_req_frames[i].data, LIN_FRAME_SIZE);
    }

    /* ---- 2. Receive and decode request frame(s) ---- */
    for (uint8_t i = 0; i < tx_req_count; i++)
    {
        hal_status_t rx_st = hal_uart_receive(rx_frame.data,
                                               LIN_FRAME_SIZE, 0);
        TEST_ASSERT_EQUAL(HAL_OK, rx_st);

        lin_st = lin_rx_decode_ctx(g_lin_ctx, &rx_frame, &rx_pdu);
        TEST_ASSERT_EQUAL(LIN_OK, lin_st);
    }

    TEST_ASSERT_NOT_NULL(rx_pdu.uds_data);
    TEST_ASSERT(rx_pdu.data_len >= 1);

    /* ---- 3. Parse UDS request ---- */
    st = uds_parse_request(rx_pdu.uds_data, rx_pdu.data_len, &uds_req);
    TEST_ASSERT_EQUAL(UDS_OK, st);

    /* ---- 4. Select context and dispatch ---- */
    void *context = get_dispatch_context(uds_req.sid);
    memset(&uds_rsp, 0, sizeof(uds_rsp));

    bool send_rsp = uds_service_dispatch(
        &uds_req, &uds_rsp,
        g_sim_ctx.session.current_session,
        UDS_PHYSICAL,
        context);

    /* ---- 5. Post-dispatch sync ---- */
    sync_after_dispatch(uds_req.sid, &uds_rsp);

    if (!send_rsp)
    {
        return 0;
    }

    /* ---- 6. Serialise response ---- */
    serialised_len = sizeof(serialised);
    if (uds_rsp.sid == 0x7F)
    {
        uint8_t nrc = (uds_rsp.data != NULL) ? uds_rsp.data[0] : 0x10;
        st = uds_serialize_negative_response(
                uds_rsp.subfunc_echo, (uds_nrc_t)nrc,
                serialised, &serialised_len);
    }
    else
    {
        st = uds_serialize_response(&uds_rsp, serialised, &serialised_len);
    }
    TEST_ASSERT_EQUAL(UDS_OK, st);
    TEST_ASSERT(serialised_len > 0);

    if (rsp_buf != NULL && rsp_len != NULL)
    {
        memcpy(rsp_buf, serialised, serialised_len);
        *rsp_len = serialised_len;
    }

    return (int)serialised_len;
}

/* ======================================================================== *
 * Extended round-trip with addressing type                                 *
 * ======================================================================== */

static int round_trip_uds_ex(const uint8_t *uds_req_data, uint16_t uds_req_len,
                              uint8_t *rsp_buf, uint16_t *rsp_len,
                              uds_addressing_t ta_type)
{
    lin_diag_pdu_t  rx_pdu;
    uds_request_t   uds_req;
    uds_response_t  uds_rsp;
    uint8_t         serialised[512];
    uint16_t        serialised_len;
    uds_status_t    st;
    lin_frame_t     rx_frame;

    /* ---- 1. Encode and inject request as LIN frame(s) ---- */
    lin_diag_pdu_t tx_req_pdu;
    tx_req_pdu.nad      = LIN_NAD_DEFAULT;
    tx_req_pdu.pci      = 0;
    tx_req_pdu.uds_data = (uint8_t *)uds_req_data;
    tx_req_pdu.data_len = (uint8_t)uds_req_len;

    uint8_t     tx_req_count = 0;
    lin_frame_t tx_req_frames[8];
    lin_status_t lin_st = lin_tx_encode_ctx(g_lin_ctx, &tx_req_pdu, tx_req_frames,
                                         &tx_req_count, 8);
    TEST_ASSERT_EQUAL(LIN_OK, lin_st);
    TEST_ASSERT(tx_req_count >= 1);

    for (uint8_t i = 0; i < tx_req_count; i++)
    {
        mock_uart_inject_rx_data(tx_req_frames[i].data, LIN_FRAME_SIZE);
    }

    /* ---- 2. Receive and decode request frame(s) ---- */
    for (uint8_t i = 0; i < tx_req_count; i++)
    {
        hal_status_t rx_st = hal_uart_receive(rx_frame.data,
                                               LIN_FRAME_SIZE, 0);
        TEST_ASSERT_EQUAL(HAL_OK, rx_st);

        lin_st = lin_rx_decode_ctx(g_lin_ctx, &rx_frame, &rx_pdu);
        TEST_ASSERT_EQUAL(LIN_OK, lin_st);
    }

    TEST_ASSERT_NOT_NULL(rx_pdu.uds_data);
    TEST_ASSERT(rx_pdu.data_len >= 1);

    /* ---- 3. Parse UDS request ---- */
    st = uds_parse_request(rx_pdu.uds_data, rx_pdu.data_len, &uds_req);
    TEST_ASSERT_EQUAL(UDS_OK, st);

    /* ---- 4. Select context and dispatch ---- */
    void *context = get_dispatch_context(uds_req.sid);
    memset(&uds_rsp, 0, sizeof(uds_rsp));

    bool send_rsp = uds_service_dispatch(
        &uds_req, &uds_rsp,
        g_sim_ctx.session.current_session,
        ta_type,
        context);

    /* ---- 5. Post-dispatch sync ---- */
    sync_after_dispatch(uds_req.sid, &uds_rsp);

    if (!send_rsp)
    {
        return 0;
    }

    /* ---- 6. Serialise response ---- */
    serialised_len = sizeof(serialised);
    if (uds_rsp.sid == 0x7F)
    {
        uint8_t nrc = (uds_rsp.data != NULL) ? uds_rsp.data[0] : 0x10;
        st = uds_serialize_negative_response(
                uds_rsp.subfunc_echo, (uds_nrc_t)nrc,
                serialised, &serialised_len);
    }
    else
    {
        st = uds_serialize_response(&uds_rsp, serialised, &serialised_len);
    }
    TEST_ASSERT_EQUAL(UDS_OK, st);
    TEST_ASSERT(serialised_len > 0);

    if (rsp_buf != NULL && rsp_len != NULL)
    {
        memcpy(rsp_buf, serialised, serialised_len);
        *rsp_len = serialised_len;
    }

    return (int)serialised_len;
}

/* ======================================================================== *
 * Helper — Register a test writable DID for 0x2E/0x2F tests               *
 * ======================================================================== */

static void register_test_writable_did(void)
{
    if (uds_did_find(0x1234) != NULL)
        return;

    static uint8_t test_did_data[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    uds_did_entry_t entry;
    entry.did    = 0x1234;
    entry.len    = 4;
    entry.access = DID_READ_WRITE;
    entry.data   = test_did_data;
    entry.on_read  = NULL;
    entry.on_write = NULL;
    (void)uds_did_register(&entry);
}

/* ======================================================================== *
 * Helper — Assert response header                                         *
 * ======================================================================== */

static void check_rsp_header(const uint8_t *rsp, uint16_t rsp_len,
                              uint8_t expected_sid, uint8_t expected_subfunc)
{
    TEST_ASSERT_NOT_NULL(rsp);
    TEST_ASSERT(rsp_len >= 2);
    TEST_ASSERT_EQUAL_UINT8(expected_sid,      rsp[0]);
    TEST_ASSERT_EQUAL_UINT8(expected_subfunc,  rsp[1]);
}

/* ======================================================================== *
 * setUp / tearDown                                                         *
 * ======================================================================== */

void setUp(void)
{
    mock_uart_clear();
    mock_timer_clear();
    mock_nvm_clear();

    (void)hal_uart_init(19200);
    hal_timer_init();

    g_lin_ctx = lin_create_ctx(LIN_NAD_DEFAULT);
    uds_session_init(&g_sim_ctx.session);
    uds_security_init();
    uds_dtc_init();
    uds_did_init();
    uds_svc_data_init();
    uds_svc_io_init();
    uds_svc_routine_init();
    uds_svc_transfer_init();
    uds_svc_auth_init();
    uds_svc_event_store_init();
    uds_service_init();

    g_sim_ctx.unlocked = false;
}

void tearDown(void)
{
    lin_free_ctx(g_lin_ctx);
    g_lin_ctx = NULL;
}

/* ======================================================================== *
 * Test Cases                                                              *
 * ======================================================================== */

void test_all_modules_initialised(void)
{
    TEST_ASSERT_EQUAL_UINT8(UDS_DEFAULT_SESSION,
                            g_sim_ctx.session.current_session);
    TEST_ASSERT_FALSE(g_sim_ctx.unlocked);
    uint8_t count = uds_service_get_count();
    TEST_ASSERT(count >= 25);
    const uds_did_entry_t *vin = uds_did_find(0xF190);
    TEST_ASSERT_NOT_NULL(vin);
    TEST_ASSERT_EQUAL_UINT16(17, vin->len);
}

/* ---- DSC ---- */

void test_dsc_default_to_extended(void)
{
    uint8_t req[] = {0x10, 0x03};
    uint8_t rsp[512];
    uint16_t rsp_len = 0;
    int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
    TEST_ASSERT(result > 0);
    check_rsp_header(rsp, rsp_len, 0x50, 0x03);
    TEST_ASSERT_EQUAL_UINT8(UDS_EXTENDED_SESSION,
                            g_sim_ctx.session.current_session);
}

void test_dsc_default_to_programming(void)
{
    uint8_t req[] = {0x10, 0x02};
    uint8_t rsp[512];
    uint16_t rsp_len = 0;
    int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
    TEST_ASSERT(result > 0);
    check_rsp_header(rsp, rsp_len, 0x50, 0x02);
    TEST_ASSERT_EQUAL_UINT8(UDS_PROGRAMMING_SESSION,
                            g_sim_ctx.session.current_session);
}

void test_dsc_unsupported_session(void)
{
    uint8_t req[] = {0x10, 0x04};
    uint8_t rsp[512];
    uint16_t rsp_len = 0;
    int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
    TEST_ASSERT(result > 0);
    TEST_ASSERT_EQUAL_UINT8(0x7F, rsp[0]);
    TEST_ASSERT_EQUAL_UINT8(0x10, rsp[1]);
    TEST_ASSERT(rsp_len >= 3);
    TEST_ASSERT_EQUAL_UINT8(NRC_SUB_FUNCTION_NOT_SUPPORTED, rsp[2]);
}

/* ---- TesterPresent ---- */

void test_tester_present(void)
{
    uint8_t req[] = {0x3E, 0x00};
    uint8_t rsp[512];
    uint16_t rsp_len = 0;
    int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
    TEST_ASSERT(result > 0);
    check_rsp_header(rsp, rsp_len, 0x7E, 0x00);
}

void test_tester_present_suppressed(void)
{
    uint8_t req[] = {0x3E, 0x80};
    uint8_t rsp[512];
    uint16_t rsp_len = 0;
    int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
    TEST_ASSERT_EQUAL(0, result);
}

/* ---- SecurityAccess ---- */

void test_security_request_seed(void)
{
    uint8_t req[] = {0x27, 0x01};
    uint8_t rsp[512];
    uint16_t rsp_len = 0;
    int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
    TEST_ASSERT(result > 0);
    check_rsp_header(rsp, rsp_len, 0x67, 0x01);
    TEST_ASSERT(rsp_len >= 3);
    TEST_ASSERT_FALSE(g_sim_ctx.unlocked);
}

void test_security_send_key_invalid(void)
{
    uint8_t seed_req[] = {0x27, 0x01};
    uint8_t tmp[512];
    uint16_t tmp_len = 0;
    (void)round_trip_uds(seed_req, sizeof(seed_req), tmp, &tmp_len);

    /* Use a short (1-byte) invalid key to fit in SF */
    uint8_t key_req[] = {0x27, 0x02, 0xAA};
    uint8_t rsp[512];
    uint16_t rsp_len = 0;
    int result = round_trip_uds(key_req, sizeof(key_req), rsp, &rsp_len);
    TEST_ASSERT(result > 0);
    TEST_ASSERT_EQUAL_UINT8(0x7F, rsp[0]);
    TEST_ASSERT_EQUAL_UINT8(0x27, rsp[1]);
    TEST_ASSERT(rsp_len >= 3);
    TEST_ASSERT_EQUAL_UINT8(NRC_INVALID_KEY, rsp[2]);
    TEST_ASSERT_FALSE(g_sim_ctx.unlocked);
}

/* ---- ECU Reset ---- */

void test_ecu_reset(void)
{
    uint8_t req[] = {0x11, 0x01};
    uint8_t rsp[512];
    uint16_t rsp_len = 0;
    int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
    TEST_ASSERT(result > 0);
    check_rsp_header(rsp, rsp_len, 0x51, 0x01);
}

/* ---- Service not supported ---- */

void test_service_not_supported(void)
{
    uint8_t req[] = {0x12, 0x00};
    uint8_t rsp[512];
    uint16_t rsp_len = 0;
    int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
    TEST_ASSERT(result > 0);
    TEST_ASSERT_EQUAL_UINT8(0x7F, rsp[0]);
    TEST_ASSERT_EQUAL_UINT8(0x12, rsp[1]);
    TEST_ASSERT(rsp_len >= 3);
    TEST_ASSERT_EQUAL_UINT8(NRC_SERVICE_NOT_SUPPORTED, rsp[2]);
}

/* ---- Read VIN ---- */

void test_read_vin(void)
{
    uint8_t req[] = {0x22, 0xF1, 0x90};
    uint8_t rsp[512];
    uint16_t rsp_len = 0;
    int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
    TEST_ASSERT(result > 0);
    if (rsp[0] == 0x7F)
    {
        TEST_FAIL_MESSAGE("Read VIN returned negative response");
    }
    else
    {
        TEST_ASSERT_EQUAL_UINT8(0x62, rsp[0]);
        TEST_ASSERT_EQUAL_UINT8(0xF1, rsp[2]);
        TEST_ASSERT_EQUAL_UINT8(0x90, rsp[3]);
        TEST_ASSERT(rsp_len >= 4);
    }
}

/* ---- Session-restricted service ---- */

void test_request_download_not_available(void)
{
    uint8_t req[] = {0x34, 0x00, 0x01, 0x00, 0x00, 0x00, 0x10,
                     0x00, 0x00, 0x00, 0x64};
    uint8_t rsp[512];
    uint16_t rsp_len = 0;
    int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
    TEST_ASSERT(result > 0);
    TEST_ASSERT_EQUAL_UINT8(0x7F, rsp[0]);
    TEST_ASSERT_EQUAL_UINT8(0x34, rsp[1]);
    TEST_ASSERT(rsp_len >= 3);
    TEST_ASSERT_EQUAL_UINT8(NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION,
                            rsp[2]);
}

/* ---- DSC + DID ---- */

void test_switch_to_extended_then_read_did(void)
{
    uint8_t dsc_req[] = {0x10, 0x03};
    uint8_t tmp[512];
    uint16_t tmp_len = 0;
    (void)round_trip_uds(dsc_req, sizeof(dsc_req), tmp, &tmp_len);
    TEST_ASSERT_EQUAL_UINT8(UDS_EXTENDED_SESSION,
                            g_sim_ctx.session.current_session);

    uint8_t vin_req[] = {0x22, 0xF1, 0x90};
    uint8_t rsp[512];
    uint16_t rsp_len = 0;
    (void)round_trip_uds(vin_req, sizeof(vin_req), rsp, &rsp_len);
    TEST_ASSERT_EQUAL_UINT8(0x62, rsp[0]);
}

/* ---- Security re-lock ---- */

void test_security_relock_on_session_switch(void)
{
    uint8_t seed_req[] = {0x27, 0x01};
    uint8_t tmp[512];
    uint16_t tmp_len = 0;
    (void)round_trip_uds(seed_req, sizeof(seed_req), tmp, &tmp_len);

    uint8_t dsc_req[] = {0x10, 0x03};
    (void)round_trip_uds(dsc_req, sizeof(dsc_req), tmp, &tmp_len);
    TEST_ASSERT_TRUE(g_sim_ctx.session.events_paused);

    uint8_t dsc_default[] = {0x10, 0x01};
    (void)round_trip_uds(dsc_default, sizeof(dsc_default), tmp, &tmp_len);
    TEST_ASSERT_FALSE(g_sim_ctx.session.events_paused);
    TEST_ASSERT_TRUE(g_sim_ctx.session.security_locked);
}

/* ---- CommunicationControl ---- */

void test_communication_control(void)
{
    uint8_t req[] = {0x28, 0x00, 0x01};
    uint8_t rsp[512];
    uint16_t rsp_len = 0;
    int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
    TEST_ASSERT(result > 0);
    check_rsp_header(rsp, rsp_len, 0x68, 0x00);
}

/* ======================================================================== *
 * 1. Full Diagnostic Session Lifecycle                                    *
 * ======================================================================== *
 * 0x10 -> 0x27 (unlock) -> 0x22 (read DID) -> 0x2E (write DID)           *
 * -> 0x3E (tester present) -> 0x10 (back to default)                     *
 * ======================================================================== */

void test_full_diag_session_lifecycle(void)
{
    uint8_t rsp[512];
    uint16_t rsp_len = 0;

    /* ---- Step 1: 0x10 0x03 -> extended session ---- */
    {
        uint8_t req[] = {0x10, 0x03};
        int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
        TEST_ASSERT(result > 0);
        check_rsp_header(rsp, rsp_len, 0x50, 0x03);
        TEST_ASSERT_EQUAL_UINT8(UDS_EXTENDED_SESSION,
                                g_sim_ctx.session.current_session);
    }

    /* ---- Step 2: 0x27 0x01 -> request seed ---- */
    {
        uint8_t req[] = {0x27, 0x01};
        int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
        TEST_ASSERT(result > 0);
        check_rsp_header(rsp, rsp_len, 0x67, 0x01);
        TEST_ASSERT(rsp_len >= 3);
    }

    /* ---- Step 3: 0x27 0x02 <key> -> send key, unlock ---- */
    {
        /* key = CRC-8 of seed; seed bytes start at rsp[2] */
        uint8_t key = crc8(rsp + 2, SECURITY_SEED_SIZE);
        uint8_t req[] = {0x27, 0x02, key};
        int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
        TEST_ASSERT(result > 0);
        check_rsp_header(rsp, rsp_len, 0x67, 0x02);
        TEST_ASSERT_TRUE(g_sim_ctx.unlocked);
    }

    /* ---- Step 4: 0x22 0xF1 0x90 -> read VIN ---- */
    {
        uint8_t req[] = {0x22, 0xF1, 0x90};
        int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
        TEST_ASSERT(result > 0);
        TEST_ASSERT_EQUAL_UINT8(0x62, rsp[0]);
        TEST_ASSERT_EQUAL_UINT8(0xF1, rsp[2]);
        TEST_ASSERT_EQUAL_UINT8(0x90, rsp[3]);
    }

    /* ---- Step 5: 0x2E <DID> <data> -> write data to DID ---- */
    {
        register_test_writable_did();
        uint8_t req[] = {0x2E, 0x12, 0x34, 0xAA, 0xBB};
        int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
        TEST_ASSERT(result > 0);
        /* Response: [0x6E][subfunc_echo=0x00][DID_hi][DID_lo] */
        TEST_ASSERT_EQUAL_UINT8(0x6E, rsp[0]);
        TEST_ASSERT_EQUAL_UINT8(0x00, rsp[1]);
        TEST_ASSERT(rsp_len >= 4);
        TEST_ASSERT_EQUAL_UINT8(0x12, rsp[2]);
        TEST_ASSERT_EQUAL_UINT8(0x34, rsp[3]);
    }

    /* ---- Step 6: 0x3E 0x00 -> tester present ---- */
    {
        uint8_t req[] = {0x3E, 0x00};
        int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
        TEST_ASSERT(result > 0);
        check_rsp_header(rsp, rsp_len, 0x7E, 0x00);
    }

    /* ---- Step 7: 0x10 0x01 -> back to default, security re-locked ---- */
    {
        uint8_t req[] = {0x10, 0x01};
        int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
        TEST_ASSERT(result > 0);
        check_rsp_header(rsp, rsp_len, 0x50, 0x01);
        TEST_ASSERT_EQUAL_UINT8(UDS_DEFAULT_SESSION,
                                g_sim_ctx.session.current_session);
        TEST_ASSERT_TRUE(g_sim_ctx.session.security_locked);
    }
}

/* ======================================================================== *
 * 2. DTC Lifecycle                                                        *
 * ======================================================================== *
 * Set fault via DTC engine -> read with 0x19 -> clear with 0x14           *
 * -> verify cleared                                                       *
 * ======================================================================== */

void test_dtc_lifecycle(void)
{
    uint8_t rsp[512];
    uint16_t rsp_len = 0;

    /* ---- Preparation: extended session + unlock ---- */
    {
        uint8_t req[] = {0x10, 0x03};
        (void)round_trip_uds(req, sizeof(req), rsp, &rsp_len);
    }
    {
        uint8_t req[] = {0x27, 0x01};
        int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
        TEST_ASSERT(result > 0);
        uint8_t key = crc8(rsp + 2, SECURITY_SEED_SIZE);
        uint8_t key_req[] = {0x27, 0x02, key};
        (void)round_trip_uds(key_req, sizeof(key_req), rsp, &rsp_len);
        TEST_ASSERT_TRUE(g_sim_ctx.unlocked);
    }

    /* ---- Inject a DTC fault directly via engine API ---- */
    uds_dtc_set_status(0x123456,
                       DTC_TEST_FAILED | DTC_CONFIRMED | DTC_PENDING,
                       true);

    /* ---- Step 1: 0x19 0x02 0x0A -> reportDTCByStatusMask (confirmed) ---- */
    {
        uint8_t req[] = {0x19, 0x02, 0x0A};
        int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
        TEST_ASSERT(result > 0);
        /* Response: [0x59][0x02][statusMask][availMask][dtc_records...] */
        TEST_ASSERT_EQUAL_UINT8(0x59, rsp[0]);
        TEST_ASSERT_EQUAL_UINT8(0x02, rsp[1]);
        TEST_ASSERT(rsp_len >= 4);
        /* Search for DTC 0x123456 in the response records */
        uint8_t dtc_found = 0;
        for (uint16_t i = 4; i + 3 < rsp_len; i += 4)
        {
            uint32_t dtc = ((uint32_t)rsp[i] << 16) |
                           ((uint32_t)rsp[i+1] << 8) | rsp[i+2];
            if (dtc == 0x123456)
            {
                dtc_found = 1;
                break;
            }
        }
        if (!dtc_found) { TEST_FAIL_MESSAGE("DTC 0x123456 not found in report"); }
    }

    /* ---- Step 2: 0x14 0xFF 0xFF 0xFF -> clear all DTCs ---- */
    {
        uint8_t req[] = {0x14, 0xFF, 0xFF, 0xFF};
        int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
        TEST_ASSERT(result > 0);
        TEST_ASSERT_EQUAL_UINT8(0x54, rsp[0]);
    }

    /* ---- Step 3: 0x19 0x01 0x0A -> reportNumberOfDTCByStatusMask (verify 0) ---- */
    {
        uint8_t req[] = {0x19, 0x01, 0x0A};
        int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
        TEST_ASSERT(result > 0);
        /* Response: [0x59][0x01][statusMask][availMask][count_hi][count_lo] */
        TEST_ASSERT_EQUAL_UINT8(0x59, rsp[0]);
        TEST_ASSERT_EQUAL_UINT8(0x01, rsp[1]);
        TEST_ASSERT(rsp_len >= 6);
        uint16_t count = (uint16_t)((rsp[4] << 8) | rsp[5]);
        TEST_ASSERT_EQUAL_UINT16(0, count);
    }
}

/* ======================================================================== *
 * 3. Upload/Download Full Cycle                                            *
 * ======================================================================== *
 * Programming session + unlock -> 0x34 (RequestDownload) -> 0x36 (Transfer)*
 * -> 0x37 (TransferExit)                                                  *
 * ======================================================================== */

void test_upload_download_cycle(void)
{
    uint8_t rsp[512];
    uint16_t rsp_len = 0;

    /* ---- Preparation: programming session + unlock ---- */
    {
        uint8_t req[] = {0x10, 0x02};
        int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
        TEST_ASSERT(result > 0);
        check_rsp_header(rsp, rsp_len, 0x50, 0x02);
    }
    {
        uint8_t req[] = {0x27, 0x01};
        int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
        TEST_ASSERT(result > 0);
        uint8_t key = crc8(rsp + 2, SECURITY_SEED_SIZE);
        uint8_t key_req[] = {0x27, 0x02, key};
        (void)round_trip_uds(key_req, sizeof(key_req), rsp, &rsp_len);
        TEST_ASSERT_TRUE(g_sim_ctx.unlocked);
    }

    /* ---- Register a valid memory region for transfer ---- */
    uds_svc_transfer_register_memory_region(0x00000000, 0x000003FF);

    /* ---- Step 1: 0x34 -> RequestDownload ---- */
    {
        uint8_t req[] = {0x34, 0x00,
                         0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x01, 0x00};
        int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
        TEST_ASSERT(result > 0);
        TEST_ASSERT_EQUAL_UINT8(0x74, rsp[0]);
        /* Response: [0x74][subfunc_echo=0][maxBlk_hi][maxBlk_lo] */
        TEST_ASSERT(rsp_len >= 3);
        uint16_t max_blk = (uint16_t)((rsp[1] << 8) | rsp[2]);
        TEST_ASSERT(max_blk > 0);
    }

    /* ---- Step 2: 0x36 -> TransferData (download direction) ---- */
    {
        uint8_t req[] = {0x36, 0x01, 0xAA, 0xBB, 0xCC, 0xDD};
        int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
        TEST_ASSERT(result > 0);
        /* Response: [0x76][bsc_echo=0x01] */
        TEST_ASSERT_EQUAL_UINT8(0x76, rsp[0]);
        TEST_ASSERT_EQUAL_UINT8(0x01, rsp[1]);
    }

    /* ---- Step 3: 0x37 -> RequestTransferExit ---- */
    {
        uint8_t req[] = {0x37};
        int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
        TEST_ASSERT(result > 0);
        TEST_ASSERT_EQUAL_UINT8(0x77, rsp[0]);
    }
}

/* ======================================================================== *
 * 4. Routine Control                                                      *
 * ======================================================================== *
 * 0x31 start checkProgrammingIntegrity (0xFF01) -> verify response        *
 * ======================================================================== */

void test_routine_control(void)
{
    uint8_t rsp[512];
    uint16_t rsp_len = 0;

    /* checkProgrammingIntegrity: no security, available in all sessions.
     * Request: [0x31][start=0x01][routineID_hi=0xFF][routineID_lo=0x01] */
    uint8_t req[] = {0x31, 0x01, 0xFF, 0x01};
    int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
    TEST_ASSERT(result > 0);
    /* Response: [0x71][0x01][routineStatusRecord...] */
    TEST_ASSERT_EQUAL_UINT8(0x71, rsp[0]);
    TEST_ASSERT_EQUAL_UINT8(0x01, rsp[1]);
    /* Routine returns [0x00] status byte */
    TEST_ASSERT(rsp_len >= 3);
    TEST_ASSERT_EQUAL_UINT8(0x00, rsp[2]);
}

/* ======================================================================== *
 * 5. Security Rejection                                                   *
 * ======================================================================== *
 * Request service needing unlock without unlocking -> expect NRC 0x33     *
 * ======================================================================== */

void test_security_rejection_without_unlock(void)
{
    uint8_t rsp[512];
    uint16_t rsp_len = 0;

    /* ---- Switch to extended session first (needed by ControlDTCSetting) ---- */
    {
        uint8_t req[] = {0x10, 0x03};
        (void)round_trip_uds(req, sizeof(req), rsp, &rsp_len);
    }

    /* ---- 0x85 0x01 (ControlDTCSetting on) without unlock -> NRC 0x33 ---- */
    {
        uint8_t req[] = {0x85, 0x01};
        int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
        TEST_ASSERT(result > 0);
        TEST_ASSERT_EQUAL_UINT8(0x7F, rsp[0]);
        TEST_ASSERT_EQUAL_UINT8(0x85, rsp[1]);
        TEST_ASSERT(rsp_len >= 3);
        TEST_ASSERT_EQUAL_UINT8(NRC_SECURITY_ACCESS_DENIED, rsp[2]);
    }
}

/* ======================================================================== *
 * 6. Functional Addressing                                                *
 * ======================================================================== *
 * Broadcast request with functional addressing: unsupported SID suppressed*
 * ======================================================================== */

void test_functional_addressing(void)
{
    uint8_t rsp[512];
    uint16_t rsp_len = 0;

    /* ---- Unsupported SID with functional addressing -> suppressed ---- */
    {
        uint8_t req[] = {0x12, 0x00};
        int result = round_trip_uds_ex(req, sizeof(req), rsp, &rsp_len,
                                        UDS_FUNCTIONAL);
        TEST_ASSERT_EQUAL(0, result);
    }

    /* ---- Valid SID with functional addressing -> normal response ---- */
    {
        uint8_t req[] = {0x10, 0x01};
        int result = round_trip_uds_ex(req, sizeof(req), rsp, &rsp_len,
                                        UDS_FUNCTIONAL);
        TEST_ASSERT(result > 0);
        check_rsp_header(rsp, rsp_len, 0x50, 0x01);
    }
}

/* ======================================================================== *
 * 7. Error Handling                                                       *
 * ======================================================================== *
 * Wrong length, invalid params                                            *
 * ======================================================================== */

void test_error_wrong_length(void)
{
    uint8_t rsp[512];
    uint16_t rsp_len = 0;

    /* 0x10 with extra data -> NRC 0x13 (IMLOIF) */
    uint8_t req[] = {0x10, 0x03, 0xAA};
    int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
    TEST_ASSERT(result > 0);
    TEST_ASSERT_EQUAL_UINT8(0x7F, rsp[0]);
    TEST_ASSERT_EQUAL_UINT8(0x10, rsp[1]);
    TEST_ASSERT(rsp_len >= 3);
    TEST_ASSERT_EQUAL_UINT8(NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT,
                            rsp[2]);
}

void test_error_invalid_params(void)
{
    uint8_t rsp[512];
    uint16_t rsp_len = 0;

    /* 0x28 0x00 0x04 -> CommunicationControl with invalid commType -> NRC 0x31 */
    uint8_t req[] = {0x28, 0x00, 0x04};
    int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
    TEST_ASSERT(result > 0);
    TEST_ASSERT_EQUAL_UINT8(0x7F, rsp[0]);
    TEST_ASSERT_EQUAL_UINT8(0x28, rsp[1]);
    TEST_ASSERT(rsp_len >= 3);
    TEST_ASSERT_EQUAL_UINT8(NRC_REQUEST_OUT_OF_RANGE, rsp[2]);
}

/* ======================================================================== *
 * 8. LIN Multi-Frame (FF/CF)                                              *
 * ======================================================================== *
 * Long request (>6 bytes UDS) triggers multi-frame encoding              *
 * ======================================================================== */

void test_lin_multiframe_request(void)
{
    uint8_t rsp[512];
    uint16_t rsp_len = 0;

    /* Switch to extended session (needed by 0x2C) */
    {
        uint8_t req[] = {0x10, 0x03};
        int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
        TEST_ASSERT(result > 0);
    }

    /* Use 0x2C DynamicallyDefineDataIdentifier with a long request.
     * Request: [0x2C][0x01][targetDID_h][targetDID_l]
     *          [sourceDID_h][sourceDID_l][offset_h][offset_l][len_h][len_l]
     * Total = 1(SID) + 1(subfn) + 2(target) + 6(source) = 10 bytes > LIN_SF_MAX_LEN(6)
     * This triggers multi-frame FF+CF encoding. */
    uint8_t req[] = {0x2C, 0x01,
                     0x12, 0x34,           /* target DID = 0x1234 */
                     0xF1, 0x90,           /* source DID = VIN (0xF190) */
                     0x00, 0x00,           /* offset = 0 */
                     0x00, 0x05};          /* length = 5 */
    int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
    TEST_ASSERT(result > 0);
    /* Positive response: [0x6C][subfunc_echo=0][targetDID_h][targetDID_l] */
    TEST_ASSERT_EQUAL_UINT8(0x6C, rsp[0]);
    TEST_ASSERT_EQUAL_UINT8(0x00, rsp[1]);
    TEST_ASSERT(rsp_len >= 4);
    TEST_ASSERT_EQUAL_UINT8(0x12, rsp[2]);
    TEST_ASSERT_EQUAL_UINT8(0x34, rsp[3]);
}

/* ======================================================================== *
 * 9. Session Timeout / DSC Timing Params                                  *
 * ======================================================================== *
 * Verify P2/P2* returned in DSC response match expected values           *
 * ======================================================================== */

void test_session_timing_params(void)
{
    uint8_t rsp[512];
    uint16_t rsp_len = 0;

    /* Default session: P2=50 (0x0032), P2*=5000 (0x1388) */
    {
        uint8_t req[] = {0x10, 0x01};
        int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
        TEST_ASSERT(result > 0);
        check_rsp_header(rsp, rsp_len, 0x50, 0x01);
        TEST_ASSERT(rsp_len >= 6);
        uint16_t p2  = (uint16_t)((rsp[2] << 8) | rsp[3]);
        uint16_t p2s = (uint16_t)((rsp[4] << 8) | rsp[5]);
        TEST_ASSERT_EQUAL_UINT16(50,   p2);
        TEST_ASSERT_EQUAL_UINT16(5000, p2s);
    }

    /* Extended session: P2=5000 (0x1388), P2*=5000 (0x1388) */
    {
        uint8_t req[] = {0x10, 0x03};
        int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
        TEST_ASSERT(result > 0);
        check_rsp_header(rsp, rsp_len, 0x50, 0x03);
        TEST_ASSERT(rsp_len >= 6);
        uint16_t p2  = (uint16_t)((rsp[2] << 8) | rsp[3]);
        uint16_t p2s = (uint16_t)((rsp[4] << 8) | rsp[5]);
        TEST_ASSERT_EQUAL_UINT16(5000, p2);
        TEST_ASSERT_EQUAL_UINT16(5000, p2s);
    }
}

/* ======================================================================== *
 * 10. SecuredDataTransmission (0x84) Stub                                 *
 * ======================================================================== *
 * 0x84 should always return NRC 0x33                                      *
 * ======================================================================== */

void test_secured_data_transmission_stub(void)
{
    uint8_t rsp[512];
    uint16_t rsp_len = 0;

    uint8_t req[] = {0x84, 0x01};
    int result = round_trip_uds(req, sizeof(req), rsp, &rsp_len);
    TEST_ASSERT(result > 0);
    TEST_ASSERT_EQUAL_UINT8(0x7F, rsp[0]);
    TEST_ASSERT_EQUAL_UINT8(0x84, rsp[1]);
    TEST_ASSERT(rsp_len >= 3);
    TEST_ASSERT_EQUAL_UINT8(NRC_SECURITY_ACCESS_DENIED, rsp[2]);
}

/* ======================================================================== *
 * Test Runner                                                              *
 * ======================================================================== */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_all_modules_initialised);
    RUN_TEST(test_dsc_default_to_extended);
    RUN_TEST(test_dsc_default_to_programming);
    RUN_TEST(test_dsc_unsupported_session);
    RUN_TEST(test_tester_present);
    RUN_TEST(test_tester_present_suppressed);
    RUN_TEST(test_security_request_seed);
    RUN_TEST(test_security_send_key_invalid);
    RUN_TEST(test_ecu_reset);
    RUN_TEST(test_service_not_supported);
    RUN_TEST(test_read_vin);
    RUN_TEST(test_request_download_not_available);
    RUN_TEST(test_switch_to_extended_then_read_did);
    RUN_TEST(test_security_relock_on_session_switch);
    RUN_TEST(test_communication_control);
    /* ---- Integration E2E Scenarios ---- */
    RUN_TEST(test_full_diag_session_lifecycle);
    RUN_TEST(test_dtc_lifecycle);
    RUN_TEST(test_upload_download_cycle);
    RUN_TEST(test_routine_control);
    RUN_TEST(test_security_rejection_without_unlock);
    RUN_TEST(test_functional_addressing);
    RUN_TEST(test_error_wrong_length);
    RUN_TEST(test_error_invalid_params);
    RUN_TEST(test_lin_multiframe_request);
    RUN_TEST(test_session_timing_params);
    RUN_TEST(test_secured_data_transmission_stub);
    return UNITY_END();
}
