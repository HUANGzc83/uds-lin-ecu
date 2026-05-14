/*
 * test_svc_upload.c
 * Upload/Download Services — Unit Tests
 *
 * Tests 5 UDS service handlers: 0x34 (RequestDownload), 0x35 (RequestUpload),
 * 0x36 (TransferData), 0x37 (RequestTransferExit), 0x38 (RequestFileTransfer).
 *
 * Each test verifies positive response paths, negative response codes (NRC),
 * and edge cases per ISO 14229-1:2020.
 *
 * Wave 3c Task 14 — Upload/Download Services
 */

#include "unity.h"
#include "uds/uds_core.h"
#include "uds/uds_svc_upload.h"
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

/* Pre-defined test addresses */
#define TEST_REGION_START  0x00000000u
#define TEST_REGION_END    0x000003FFu
#define TEST_ADDR          0x00000100u
#define TEST_SIZE          64u

void setUp(void)
{
    mock_timer_clear();
    uds_security_init();
    uds_session_init(&session_ctx);
    uds_svc_transfer_init();
    unlocked = false;

    /* Register a test memory region (done after init so count resets) */
    uds_svc_transfer_register_memory_region(TEST_REGION_START, TEST_REGION_END);
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
 * @brief Assert that a response is a positive response (for services without
 *        a subfunction — subfunc_echo should be 0).
 */
static void check_pos_rsp(const uds_response_t *rsp,
                           uint8_t expected_sid)
{
    TEST_ASSERT_EQUAL_UINT8(expected_sid, rsp->sid);
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
 * @brief Switch to the given session type and unlock security.
 *
 * Uses the DiagnosticSessionControl and SecurityAccess handlers to
 * set up the environment for upload/download operations.
 */
static void prepare_session_and_security(uint8_t session_type)
{
    /* Step 1: Switch session via diagnostic session control */
    {
        uds_response_t rsp;
        uint8_t raw[] = {0x10, session_type};
        uds_request_t req;
        TEST_ASSERT_EQUAL(UDS_OK, uds_parse_request(raw, sizeof(raw), &req));
        bool result = uds_svc_diagnostic_session_control(&req, &rsp, &session_ctx);
        TEST_ASSERT_TRUE(result);
        TEST_ASSERT_EQUAL_UINT8(session_type, session_ctx.current_session);
    }

    /* Step 2: Request seed */
    uint8_t seed0;
    {
        uds_response_t rsp;
        uint8_t raw[] = {0x27, 0x01};
        uds_request_t req;
        TEST_ASSERT_EQUAL(UDS_OK, uds_parse_request(raw, sizeof(raw), &req));
        uds_svc_security_access(&req, &rsp, &unlocked);
        seed0 = rsp.data[0];
    }

    /* Step 3: Send valid key */
    {
        uint8_t raw[2 + SECURITY_SEED_SIZE];
        raw[0] = 0x27;
        raw[1] = 0x02;
        memset(raw + 2, 0xAA, SECURITY_SEED_SIZE);
        raw[2] = (uint8_t)(~seed0);

        uds_response_t rsp;
        uds_request_t req;
        TEST_ASSERT_EQUAL(UDS_OK, uds_parse_request(raw, sizeof(raw), &req));
        uds_svc_security_access(&req, &rsp, &unlocked);
        TEST_ASSERT_TRUE(unlocked);
    }
}

/**
 * @brief Build a RequestDownload/RequestUpload raw request with given
 *        dataFormatIdentifier, address, and size.
 */
static void build_transfer_request_raw(uint8_t *raw, uint8_t sid,
                                       uint8_t dfi,
                                       uint32_t addr, uint32_t size)
{
    raw[0] = sid;
    raw[1] = dfi;
    raw[2] = (uint8_t)((addr >> 24) & 0xFFu);
    raw[3] = (uint8_t)((addr >> 16) & 0xFFu);
    raw[4] = (uint8_t)((addr >>  8) & 0xFFu);
    raw[5] = (uint8_t)( addr        & 0xFFu);
    raw[6] = (uint8_t)((size >> 24) & 0xFFu);
    raw[7] = (uint8_t)((size >> 16) & 0xFFu);
    raw[8] = (uint8_t)((size >>  8) & 0xFFu);
    raw[9] = (uint8_t)( size        & 0xFFu);
}

/* ======================================================================== *
 * Transfer Context Management Tests                                       *
 * ======================================================================== */

void test_transfer_init_resets_context(void)
{
    /* First set some state */
    uds_svc_transfer_register_memory_region(0x1000, 0x1FFF);

    /* Re-init */
    uds_svc_transfer_init();

    /* Verify context is cleared */
    const uds_transfer_context_t *ctx = uds_svc_transfer_get_context();
    TEST_ASSERT_NOT_NULL(ctx);
    TEST_ASSERT_FALSE(ctx->active);
    TEST_ASSERT_EQUAL_UINT8(UDS_TRANSFER_NONE, (uint8_t)ctx->direction);
    TEST_ASSERT_EQUAL_UINT32(0, ctx->memory_addr);
    TEST_ASSERT_EQUAL_UINT32(0, ctx->remaining_size);
    TEST_ASSERT_EQUAL_UINT8(0, ctx->block_seq_counter);
    TEST_ASSERT_EQUAL_UINT16(0, ctx->buffer_length);
}

void test_transfer_register_region(void)
{
    uds_svc_transfer_init();
    uds_svc_transfer_register_memory_region(0x1000, 0x1FFF);
    TEST_ASSERT_EQUAL_UINT16(1, uds_svc_transfer_get_region_count());
}

void test_transfer_register_multiple_regions(void)
{
    uds_svc_transfer_init();
    uds_svc_transfer_register_memory_region(0x0000, 0x0FFF);
    uds_svc_transfer_register_memory_region(0x1000, 0x1FFF);
    uds_svc_transfer_register_memory_region(0x2000, 0x2FFF);
    TEST_ASSERT_EQUAL_UINT16(3, uds_svc_transfer_get_region_count());
}

void test_transfer_register_invalid_region_ignored(void)
{
    uds_svc_transfer_init();
    uds_svc_transfer_register_memory_region(0x2000, 0x1000); /* end < start */
    TEST_ASSERT_EQUAL_UINT16(0, uds_svc_transfer_get_region_count());
}

void test_transfer_get_buffer_returns_buffer(void)
{
    uint8_t *buf = uds_svc_transfer_get_buffer();
    TEST_ASSERT_NOT_NULL(buf);
    buf[0] = 0xAA;
    TEST_ASSERT_EQUAL_UINT8(0xAA, buf[0]);
}

void test_transfer_get_context_returns_valid_ptr(void)
{
    const uds_transfer_context_t *ctx = uds_svc_transfer_get_context();
    TEST_ASSERT_NOT_NULL(ctx);
    TEST_ASSERT_FALSE(ctx->active);
}

/* ======================================================================== *
 * 0x34 — RequestDownload Tests                                            *
 * ======================================================================== */

void test_req_download_valid_extended_session(void)
{
    prepare_session_and_security(UDS_EXTENDED_SESSION);

    uint8_t raw[10];
    build_transfer_request_raw(raw, REQUEST_DOWNLOAD, 0x00, TEST_ADDR, TEST_SIZE);

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                                uds_svc_request_download);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, REQUEST_DOWNLOAD_RSP);

    /* Response should contain maxBlockLength (2 bytes) */
    TEST_ASSERT_EQUAL_UINT8(2, rsp.data_len);
    TEST_ASSERT_NOT_NULL(rsp.data);

    /* Verify transfer context was set up */
    const uds_transfer_context_t *ctx = uds_svc_transfer_get_context();
    TEST_ASSERT_TRUE(ctx->active);
    TEST_ASSERT_EQUAL_UINT8(UDS_TRANSFER_DOWNLOAD, (uint8_t)ctx->direction);
    TEST_ASSERT_EQUAL_UINT32(TEST_ADDR, ctx->memory_addr);
    TEST_ASSERT_EQUAL_UINT32(TEST_SIZE, ctx->remaining_size);
    TEST_ASSERT_EQUAL_UINT8(0, ctx->block_seq_counter);
}

void test_req_download_valid_programming_session(void)
{
    prepare_session_and_security(UDS_PROGRAMMING_SESSION);

    uint8_t raw[10];
    build_transfer_request_raw(raw, REQUEST_DOWNLOAD, 0x00, TEST_ADDR, TEST_SIZE);

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                                uds_svc_request_download);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, REQUEST_DOWNLOAD_RSP);
}

void test_req_download_nrc22_null_context(void)
{
    uint8_t raw[10];
    build_transfer_request_raw(raw, REQUEST_DOWNLOAD, 0x00, TEST_ADDR, TEST_SIZE);

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                                uds_svc_request_download);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, REQUEST_DOWNLOAD, NRC_CONDITIONS_NOT_CORRECT);
}

void test_req_download_nrc22_default_session(void)
{
    /* Default session — not allowed for download */
    uint8_t raw[10];
    build_transfer_request_raw(raw, REQUEST_DOWNLOAD, 0x00, TEST_ADDR, TEST_SIZE);

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                                uds_svc_request_download);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, REQUEST_DOWNLOAD, NRC_CONDITIONS_NOT_CORRECT);
}

void test_req_download_nrc33_security_locked(void)
{
    /* Switch to extended session but don't unlock security */
    {
        uds_response_t rsp;
        uint8_t raw[] = {0x10, UDS_EXTENDED_SESSION};
        uds_request_t req;
        uds_parse_request(raw, sizeof(raw), &req);
        uds_svc_diagnostic_session_control(&req, &rsp, &session_ctx);
    }

    uint8_t raw[10];
    build_transfer_request_raw(raw, REQUEST_DOWNLOAD, 0x00, TEST_ADDR, TEST_SIZE);

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                                uds_svc_request_download);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, REQUEST_DOWNLOAD, NRC_SECURITY_ACCESS_DENIED);
}

void test_req_download_nrc13_imloif_short_data(void)
{
    prepare_session_and_security(UDS_EXTENDED_SESSION);

    /* Only provide 4 bytes instead of the required 8 (addr+size) */
    uint8_t raw[6] = {REQUEST_DOWNLOAD, 0x00, 0x00, 0x00, 0x01, 0x00};

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                                uds_svc_request_download);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, REQUEST_DOWNLOAD, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

void test_req_download_nrc31_roor_bad_address(void)
{
    prepare_session_and_security(UDS_EXTENDED_SESSION);

    /* Address 0xFFFF0000 is not in any registered region */
    uint8_t raw[10];
    build_transfer_request_raw(raw, REQUEST_DOWNLOAD, 0x00, 0xFFFF0000u, TEST_SIZE);

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                                uds_svc_request_download);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, REQUEST_DOWNLOAD, NRC_REQUEST_OUT_OF_RANGE);
}

void test_req_download_nrc31_roor_zero_size(void)
{
    prepare_session_and_security(UDS_EXTENDED_SESSION);

    /* Zero size is invalid */
    uint8_t raw[10];
    build_transfer_request_raw(raw, REQUEST_DOWNLOAD, 0x00, TEST_ADDR, 0);

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                                uds_svc_request_download);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, REQUEST_DOWNLOAD, NRC_REQUEST_OUT_OF_RANGE);
}

void test_req_download_nrc70_size_exceeds_buffer(void)
{
    prepare_session_and_security(UDS_EXTENDED_SESSION);

    /* Size larger than UDS_TRANSFER_BUFFER_SIZE */
    uint8_t raw[10];
    build_transfer_request_raw(raw, REQUEST_DOWNLOAD, 0x00, TEST_ADDR,
                               UDS_TRANSFER_BUFFER_SIZE + 1u);

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                                uds_svc_request_download);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, REQUEST_DOWNLOAD, NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED);
}

void test_req_download_nrc70_already_active(void)
{
    prepare_session_and_security(UDS_EXTENDED_SESSION);

    /* First download: should succeed */
    {
        uint8_t raw[10];
        build_transfer_request_raw(raw, REQUEST_DOWNLOAD, 0x00, TEST_ADDR, TEST_SIZE);

        uds_response_t rsp;
        call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                     uds_svc_request_download);
    }

    /* Second download while first is still active: should fail */
    {
        uint8_t raw[10];
        build_transfer_request_raw(raw, REQUEST_DOWNLOAD, 0x00, TEST_ADDR, TEST_SIZE);

        uds_response_t rsp;
        bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                                    uds_svc_request_download);

        TEST_ASSERT_TRUE(result);
        check_neg_rsp(&rsp, REQUEST_DOWNLOAD, NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED);
    }
}

void test_req_download_response_max_block_length(void)
{
    prepare_session_and_security(UDS_EXTENDED_SESSION);

    uint8_t raw[10];
    build_transfer_request_raw(raw, REQUEST_DOWNLOAD, 0x00, TEST_ADDR, TEST_SIZE);

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                                uds_svc_request_download);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT8(2, rsp.data_len);

    /* Verify maxBlockLength is UDS_DEFAULT_MAX_BLOCK_LENGTH */
    uint16_t max_blk = (uint16_t)((uint16_t)rsp.data[0] << 8) | rsp.data[1];
    TEST_ASSERT_EQUAL_UINT16(UDS_DEFAULT_MAX_BLOCK_LENGTH, max_blk);
}

/* ======================================================================== *
 * 0x35 — RequestUpload Tests                                              *
 * ======================================================================== */

void test_req_upload_valid_extended_session(void)
{
    prepare_session_and_security(UDS_EXTENDED_SESSION);

    uint8_t raw[10];
    build_transfer_request_raw(raw, REQUEST_UPLOAD, 0x00, TEST_ADDR, TEST_SIZE);

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                                uds_svc_request_upload);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, REQUEST_UPLOAD_RSP);
    TEST_ASSERT_EQUAL_UINT8(2, rsp.data_len);

    /* Verify transfer context was set up for upload */
    const uds_transfer_context_t *ctx = uds_svc_transfer_get_context();
    TEST_ASSERT_TRUE(ctx->active);
    TEST_ASSERT_EQUAL_UINT8(UDS_TRANSFER_UPLOAD, (uint8_t)ctx->direction);
    TEST_ASSERT_EQUAL_UINT32(TEST_SIZE, ctx->remaining_size);
}

void test_req_upload_nrc22_default_session(void)
{
    uint8_t raw[10];
    build_transfer_request_raw(raw, REQUEST_UPLOAD, 0x00, TEST_ADDR, TEST_SIZE);

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                                uds_svc_request_upload);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, REQUEST_UPLOAD, NRC_CONDITIONS_NOT_CORRECT);
}

void test_req_upload_nrc33_security_locked(void)
{
    {
        uds_response_t rsp;
        uint8_t raw[] = {0x10, UDS_EXTENDED_SESSION};
        uds_request_t req;
        uds_parse_request(raw, sizeof(raw), &req);
        uds_svc_diagnostic_session_control(&req, &rsp, &session_ctx);
    }

    uint8_t raw[10];
    build_transfer_request_raw(raw, REQUEST_UPLOAD, 0x00, TEST_ADDR, TEST_SIZE);

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                                uds_svc_request_upload);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, REQUEST_UPLOAD, NRC_SECURITY_ACCESS_DENIED);
}

void test_req_upload_nrc13_imloif_short_data(void)
{
    prepare_session_and_security(UDS_EXTENDED_SESSION);

    uint8_t raw[4] = {REQUEST_UPLOAD, 0x00, 0x00, 0x00};

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                                uds_svc_request_upload);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, REQUEST_UPLOAD, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

void test_req_upload_nrc31_roor_bad_address(void)
{
    prepare_session_and_security(UDS_EXTENDED_SESSION);

    uint8_t raw[10];
    build_transfer_request_raw(raw, REQUEST_UPLOAD, 0x00, 0xFFFF0000u, TEST_SIZE);

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                                uds_svc_request_upload);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, REQUEST_UPLOAD, NRC_REQUEST_OUT_OF_RANGE);
}

void test_req_upload_nrc70_already_active(void)
{
    prepare_session_and_security(UDS_EXTENDED_SESSION);

    /* First upload */
    {
        uint8_t raw[10];
        build_transfer_request_raw(raw, REQUEST_UPLOAD, 0x00, TEST_ADDR, TEST_SIZE);
        uds_response_t rsp;
        call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                     uds_svc_request_upload);
    }

    /* Second upload while still active → UDNA */
    {
        uint8_t raw[10];
        build_transfer_request_raw(raw, REQUEST_UPLOAD, 0x00, TEST_ADDR, TEST_SIZE);
        uds_response_t rsp;
        bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                                    uds_svc_request_upload);
        TEST_ASSERT_TRUE(result);
        check_neg_rsp(&rsp, REQUEST_UPLOAD, NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED);
    }
}

/* ======================================================================== *
 * 0x36 — TransferData Tests                                               *
 * ======================================================================== */

void test_transfer_data_download_valid(void)
{
    prepare_session_and_security(UDS_EXTENDED_SESSION);

    /* Step 1: Start a download transfer */
    {
        uint8_t raw[10];
        build_transfer_request_raw(raw, REQUEST_DOWNLOAD, 0x00, TEST_ADDR, TEST_SIZE);
        uds_response_t rsp;
        call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                     uds_svc_request_download);
    }

    /* Step 2: Transfer data block (BSC=1) */
    uint8_t data_block[] = {0x36, 0x01, 0xAA, 0xBB, 0xCC, 0xDD};

    uds_response_t rsp;
    bool result = call_handler(data_block, sizeof(data_block), &rsp, NULL,
                                uds_svc_transfer_data);

    TEST_ASSERT_TRUE(result);
    /* Response: [0x76][bsc] — bsc echoed in subfunc_echo */
    TEST_ASSERT_EQUAL_UINT8(TRANSFER_DATA_RSP, rsp.sid);
    TEST_ASSERT_EQUAL_UINT8(0x01, rsp.subfunc_echo);
    TEST_ASSERT_EQUAL_UINT8(0, rsp.data_len);

    /* Verify context updated */
    const uds_transfer_context_t *ctx = uds_svc_transfer_get_context();
    TEST_ASSERT_EQUAL_UINT8(0x01, ctx->block_seq_counter);
    TEST_ASSERT_EQUAL_UINT32(TEST_SIZE - 4u, ctx->remaining_size);
    TEST_ASSERT_EQUAL_UINT16(4, ctx->buffer_length);

    /* Verify data in buffer */
    TEST_ASSERT_EQUAL_UINT8(0xAA, ctx->buffer[0]);
    TEST_ASSERT_EQUAL_UINT8(0xBB, ctx->buffer[1]);
    TEST_ASSERT_EQUAL_UINT8(0xCC, ctx->buffer[2]);
    TEST_ASSERT_EQUAL_UINT8(0xDD, ctx->buffer[3]);
}

void test_transfer_data_download_multiple_blocks(void)
{
    prepare_session_and_security(UDS_EXTENDED_SESSION);

    /* Start download (size = 8 bytes) */
    {
        uint8_t raw[10];
        build_transfer_request_raw(raw, REQUEST_DOWNLOAD, 0x00, TEST_ADDR, 8);
        uds_response_t rsp;
        call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                     uds_svc_request_download);
    }

    /* Block 1: BSC=1 */
    {
        uint8_t blk[] = {0x36, 0x01, 0x01, 0x02, 0x03, 0x04};
        uds_response_t rsp;
        call_handler(blk, sizeof(blk), &rsp, NULL, uds_svc_transfer_data);
    }

    /* Block 2: BSC=2 */
    {
        uint8_t blk[] = {0x36, 0x02, 0x05, 0x06, 0x07, 0x08};
        uds_response_t rsp;
        bool result = call_handler(blk, sizeof(blk), &rsp, NULL,
                                    uds_svc_transfer_data);
        TEST_ASSERT_TRUE(result);
        TEST_ASSERT_EQUAL_UINT8(TRANSFER_DATA_RSP, rsp.sid);
        TEST_ASSERT_EQUAL_UINT8(0x02, rsp.subfunc_echo);
    }

    /* Verify all 8 bytes received */
    const uds_transfer_context_t *ctx = uds_svc_transfer_get_context();
    TEST_ASSERT_EQUAL_UINT16(8, ctx->buffer_length);
    TEST_ASSERT_EQUAL_UINT8(0x01, ctx->buffer[0]);
    TEST_ASSERT_EQUAL_UINT8(0x05, ctx->buffer[4]);
}

void test_transfer_data_upload_valid(void)
{
    prepare_session_and_security(UDS_EXTENDED_SESSION);

    /* Step 1: Start an upload transfer */
    {
        uint8_t raw[10];
        build_transfer_request_raw(raw, REQUEST_UPLOAD, 0x00, TEST_ADDR, TEST_SIZE);
        uds_response_t rsp;
        call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                     uds_svc_request_upload);
    }

    /* Step 2: Pre-fill buffer with upload data */
    uint8_t *buf = uds_svc_transfer_get_buffer();
    buf[0] = 0x11;
    buf[1] = 0x22;
    buf[2] = 0x33;
    buf[3] = 0x44;

    /* Update context to reflect pre-filled data */
    uds_transfer_context_t *ctx = (uds_transfer_context_t *)uds_svc_transfer_get_context();
    /* We need to set buffer_length since pre-filled data needs to be tracked.
     * Cast away const since this is a test helper. */
    {
        /* Use memcpy through non-const pointer obtained via buffer accessor.
         * Set buffer_length by casting const away (acceptable in test code). */
        uds_transfer_context_t *nc_ctx = (uds_transfer_context_t *)ctx;
        nc_ctx->buffer_length = 4u;
        nc_ctx->remaining_size = 4u;
    }

    /* Step 3: TransferData (BSC=1, no request data for upload) */
    uint8_t raw[] = {0x36, 0x01};

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                                uds_svc_transfer_data);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT8(TRANSFER_DATA_RSP, rsp.sid);
    TEST_ASSERT_EQUAL_UINT8(0x01, rsp.subfunc_echo);

    /* Response should contain the buffered data */
    TEST_ASSERT_NOT_NULL(rsp.data);
    TEST_ASSERT(rsp.data_len > 0);
    TEST_ASSERT_EQUAL_UINT8(0x11, rsp.data[0]);
    TEST_ASSERT_EQUAL_UINT8(0x22, rsp.data[1]);
    TEST_ASSERT_EQUAL_UINT8(0x33, rsp.data[2]);
    TEST_ASSERT_EQUAL_UINT8(0x44, rsp.data[3]);
}

void test_transfer_data_nrc22_no_active_transfer(void)
{
    uint8_t raw[] = {0x36, 0x01};

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                                uds_svc_transfer_data);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, TRANSFER_DATA, NRC_CONDITIONS_NOT_CORRECT);
}

void test_transfer_data_nrc73_wrong_bsc(void)
{
    prepare_session_and_security(UDS_EXTENDED_SESSION);

    /* Start download */
    {
        uint8_t raw[10];
        build_transfer_request_raw(raw, REQUEST_DOWNLOAD, 0x00, TEST_ADDR, TEST_SIZE);
        uds_response_t rsp;
        call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                     uds_svc_request_download);
    }

    /* Transfer with wrong BSC (expect 1, send 2) */
    uint8_t blk[] = {0x36, 0x02, 0xAA};

    uds_response_t rsp;
    bool result = call_handler(blk, sizeof(blk), &rsp, NULL,
                                uds_svc_transfer_data);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, TRANSFER_DATA, NRC_WRONG_BLOCK_SEQUENCE_COUNTER);
}

void test_transfer_data_nrc13_imloif_download_no_data(void)
{
    prepare_session_and_security(UDS_EXTENDED_SESSION);

    /* Start download */
    {
        uint8_t raw[10];
        build_transfer_request_raw(raw, REQUEST_DOWNLOAD, 0x00, TEST_ADDR, TEST_SIZE);
        uds_response_t rsp;
        call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                     uds_svc_request_download);
    }

    /* TransferData with no data bytes (just BSC) — invalid for download */
    uint8_t raw[] = {0x36, 0x01};

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                                uds_svc_transfer_data);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, TRANSFER_DATA, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

void test_transfer_data_bsc_sequential_increment(void)
{
    prepare_session_and_security(UDS_EXTENDED_SESSION);

    /* Start download (size = 16 bytes) */
    {
        uint8_t raw[10];
        build_transfer_request_raw(raw, REQUEST_DOWNLOAD, 0x00, TEST_ADDR, 16);
        uds_response_t rsp;
        call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                     uds_svc_request_download);
    }

    /* Send 4 blocks with BSC 1, 2, 3, 4 */
    for (uint8_t bsc = 1; bsc <= 4; bsc++)
    {
        uint8_t blk[3] = {0x36, bsc, (uint8_t)(0xA0 + bsc)};
        uds_response_t rsp;
        bool result = call_handler(blk, sizeof(blk), &rsp, NULL,
                                    uds_svc_transfer_data);
        TEST_ASSERT_TRUE(result);
        TEST_ASSERT_EQUAL_UINT8(TRANSFER_DATA_RSP, rsp.sid);
        TEST_ASSERT_EQUAL_UINT8(bsc, rsp.subfunc_echo);
    }

    const uds_transfer_context_t *ctx = uds_svc_transfer_get_context();
    TEST_ASSERT_EQUAL_UINT8(4, ctx->block_seq_counter);
    TEST_ASSERT_EQUAL_UINT16(4, ctx->buffer_length);
}

void test_transfer_data_nrc73_wrong_bsc_after_valid(void)
{
    prepare_session_and_security(UDS_EXTENDED_SESSION);

    /* Start download */
    {
        uint8_t raw[10];
        build_transfer_request_raw(raw, REQUEST_DOWNLOAD, 0x00, TEST_ADDR, 12);
        uds_response_t rsp;
        call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                     uds_svc_request_download);
    }

    /* BSC=1: valid */
    {
        uint8_t blk[] = {0x36, 0x01, 0xAA};
        uds_response_t rsp;
        call_handler(blk, sizeof(blk), &rsp, NULL, uds_svc_transfer_data);
    }

    /* BSC=3: skip 2 → wrong */
    {
        uint8_t blk[] = {0x36, 0x03, 0xBB};
        uds_response_t rsp;
        bool result = call_handler(blk, sizeof(blk), &rsp, NULL,
                                    uds_svc_transfer_data);
        TEST_ASSERT_TRUE(result);
        check_neg_rsp(&rsp, TRANSFER_DATA, NRC_WRONG_BLOCK_SEQUENCE_COUNTER);
    }
}

/* ======================================================================== *
 * 0x37 — RequestTransferExit Tests                                        *
 * ======================================================================== */

void test_req_transfer_exit_valid(void)
{
    prepare_session_and_security(UDS_EXTENDED_SESSION);

    /* Start a download transfer */
    {
        uint8_t raw[10];
        build_transfer_request_raw(raw, REQUEST_DOWNLOAD, 0x00, TEST_ADDR, TEST_SIZE);
        uds_response_t rsp;
        call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                     uds_svc_request_download);
    }

    /* Verify it's active */
    TEST_ASSERT_TRUE(uds_svc_transfer_get_context()->active);

    /* RequestTransferExit */
    uint8_t raw[] = {REQUEST_TRANSFER_EXIT};

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                                uds_svc_request_transfer_exit);

    TEST_ASSERT_TRUE(result);
    check_pos_rsp(&rsp, REQUEST_TRANSFER_EXIT_RSP);

    /* Verify context is cleaned up */
    const uds_transfer_context_t *ctx = uds_svc_transfer_get_context();
    TEST_ASSERT_FALSE(ctx->active);
    TEST_ASSERT_EQUAL_UINT8(UDS_TRANSFER_NONE, (uint8_t)ctx->direction);
    TEST_ASSERT_EQUAL_UINT32(0, ctx->remaining_size);
}

void test_req_transfer_exit_nrc22_no_active(void)
{
    uint8_t raw[] = {REQUEST_TRANSFER_EXIT};

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                                uds_svc_request_transfer_exit);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, REQUEST_TRANSFER_EXIT, NRC_CONDITIONS_NOT_CORRECT);
}

void test_req_transfer_exit_nrc13_imloif_extra_bytes(void)
{
    prepare_session_and_security(UDS_EXTENDED_SESSION);

    /* Start a download transfer */
    {
        uint8_t raw[10];
        build_transfer_request_raw(raw, REQUEST_DOWNLOAD, 0x00, TEST_ADDR, TEST_SIZE);
        uds_response_t rsp;
        call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                     uds_svc_request_download);
    }

    /* RequestTransferExit with extra byte (parser consumes byte[1] as
     * subfunction, so data_len becomes 0 for 2-byte messages; we need
     * a 3-byte message for IMLOIF: [SID][0x00][extra_byte]) */
    uint8_t raw[] = {REQUEST_TRANSFER_EXIT, 0x00, 0xFF};

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                                uds_svc_request_transfer_exit);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, REQUEST_TRANSFER_EXIT, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

void test_req_transfer_exit_cleans_up_context(void)
{
    prepare_session_and_security(UDS_EXTENDED_SESSION);

    /* Start download and send some data */
    {
        uint8_t raw[10];
        build_transfer_request_raw(raw, REQUEST_DOWNLOAD, 0x00, TEST_ADDR, TEST_SIZE);
        uds_response_t rsp;
        call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                     uds_svc_request_download);
    }

    {
        uint8_t blk[] = {0x36, 0x01, 0x11, 0x22};
        uds_response_t rsp;
        call_handler(blk, sizeof(blk), &rsp, NULL, uds_svc_transfer_data);
    }

    /* Exit */
    {
        uint8_t raw[] = {REQUEST_TRANSFER_EXIT};
        uds_response_t rsp;
        call_handler(raw, sizeof(raw), &rsp, NULL, uds_svc_request_transfer_exit);
    }

    /* All fields should be cleared */
    const uds_transfer_context_t *ctx = uds_svc_transfer_get_context();
    TEST_ASSERT_FALSE(ctx->active);
    TEST_ASSERT_EQUAL_UINT32(0, ctx->memory_addr);
    TEST_ASSERT_EQUAL_UINT32(0, ctx->remaining_size);
    TEST_ASSERT_EQUAL_UINT8(0, ctx->block_seq_counter);
    TEST_ASSERT_EQUAL_UINT16(0, ctx->buffer_length);
}

/* ======================================================================== *
 * 0x38 — RequestFileTransfer Tests (Stub)                                 *
 * ======================================================================== */

void test_req_file_transfer_stub_returns_nrc31(void)
{
    prepare_session_and_security(UDS_EXTENDED_SESSION);

    /* addFile (subfn=0x01), filePathLength=5, path="hello" */
    uint8_t raw[] = {REQUEST_FILE_TRANSFER, 0x01, 0x05, 'h', 'e', 'l', 'l', 'o'};

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                                uds_svc_request_file_transfer);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, REQUEST_FILE_TRANSFER, NRC_REQUEST_OUT_OF_RANGE);
}

void test_req_file_transfer_nrc22_null_context(void)
{
    uint8_t raw[] = {REQUEST_FILE_TRANSFER, 0x01, 0x00};

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, NULL,
                                uds_svc_request_file_transfer);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, REQUEST_FILE_TRANSFER, NRC_CONDITIONS_NOT_CORRECT);
}

void test_req_file_transfer_nrc22_default_session(void)
{
    uint8_t raw[] = {REQUEST_FILE_TRANSFER, 0x01, 0x00};

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                                uds_svc_request_file_transfer);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, REQUEST_FILE_TRANSFER, NRC_CONDITIONS_NOT_CORRECT);
}

void test_req_file_transfer_nrc33_security_locked(void)
{
    /* Switch to extended session but don't unlock */
    {
        uds_response_t rsp;
        uint8_t raw[] = {0x10, UDS_EXTENDED_SESSION};
        uds_request_t req;
        uds_parse_request(raw, sizeof(raw), &req);
        uds_svc_diagnostic_session_control(&req, &rsp, &session_ctx);
    }

    uint8_t raw[] = {REQUEST_FILE_TRANSFER, 0x01, 0x00};

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                                uds_svc_request_file_transfer);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, REQUEST_FILE_TRANSFER, NRC_SECURITY_ACCESS_DENIED);
}

void test_req_file_transfer_nrc13_imloif_no_data(void)
{
    prepare_session_and_security(UDS_EXTENDED_SESSION);

    uint8_t raw[] = {REQUEST_FILE_TRANSFER, 0x01};  /* missing filePathLength */

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                                uds_svc_request_file_transfer);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, REQUEST_FILE_TRANSFER, NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

void test_req_file_transfer_nrc12_bad_subfn(void)
{
    prepare_session_and_security(UDS_EXTENDED_SESSION);

    /* Subfunction 0x05 is not supported (valid: 0x01-0x04) */
    uint8_t raw[] = {REQUEST_FILE_TRANSFER, 0x05, 0x00};

    uds_response_t rsp;
    bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                                uds_svc_request_file_transfer);

    TEST_ASSERT_TRUE(result);
    check_neg_rsp(&rsp, REQUEST_FILE_TRANSFER, NRC_SUB_FUNCTION_NOT_SUPPORTED);
}

void test_req_file_transfer_all_subfn_return_nrc31(void)
{
    prepare_session_and_security(UDS_EXTENDED_SESSION);

    /* Test all 4 supported subfunctions, all should return NRC 0x31 (stub) */
    for (uint8_t subfn = 0x01; subfn <= 0x04; subfn++)
    {
        uint8_t raw[3] = {REQUEST_FILE_TRANSFER, subfn, 0x00};

        uds_response_t rsp;
        bool result = call_handler(raw, sizeof(raw), &rsp, &session_ctx,
                                    uds_svc_request_file_transfer);

        TEST_ASSERT_TRUE(result);
        check_neg_rsp(&rsp, REQUEST_FILE_TRANSFER, NRC_REQUEST_OUT_OF_RANGE);
    }
}

/* ======================================================================== *
 * Test Runner                                                              *
 * ======================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* Transfer Context Management */
    RUN_TEST(test_transfer_init_resets_context);
    RUN_TEST(test_transfer_register_region);
    RUN_TEST(test_transfer_register_multiple_regions);
    RUN_TEST(test_transfer_register_invalid_region_ignored);
    RUN_TEST(test_transfer_get_buffer_returns_buffer);
    RUN_TEST(test_transfer_get_context_returns_valid_ptr);

    /* 0x34 — RequestDownload */
    RUN_TEST(test_req_download_valid_extended_session);
    RUN_TEST(test_req_download_valid_programming_session);
    RUN_TEST(test_req_download_nrc22_null_context);
    RUN_TEST(test_req_download_nrc22_default_session);
    RUN_TEST(test_req_download_nrc33_security_locked);
    RUN_TEST(test_req_download_nrc13_imloif_short_data);
    RUN_TEST(test_req_download_nrc31_roor_bad_address);
    RUN_TEST(test_req_download_nrc31_roor_zero_size);
    RUN_TEST(test_req_download_nrc70_size_exceeds_buffer);
    RUN_TEST(test_req_download_nrc70_already_active);
    RUN_TEST(test_req_download_response_max_block_length);

    /* 0x35 — RequestUpload */
    RUN_TEST(test_req_upload_valid_extended_session);
    RUN_TEST(test_req_upload_nrc22_default_session);
    RUN_TEST(test_req_upload_nrc33_security_locked);
    RUN_TEST(test_req_upload_nrc13_imloif_short_data);
    RUN_TEST(test_req_upload_nrc31_roor_bad_address);
    RUN_TEST(test_req_upload_nrc70_already_active);

    /* 0x36 — TransferData */
    RUN_TEST(test_transfer_data_download_valid);
    RUN_TEST(test_transfer_data_download_multiple_blocks);
    RUN_TEST(test_transfer_data_upload_valid);
    RUN_TEST(test_transfer_data_nrc22_no_active_transfer);
    RUN_TEST(test_transfer_data_nrc73_wrong_bsc);
    RUN_TEST(test_transfer_data_nrc13_imloif_download_no_data);
    RUN_TEST(test_transfer_data_bsc_sequential_increment);
    RUN_TEST(test_transfer_data_nrc73_wrong_bsc_after_valid);

    /* 0x37 — RequestTransferExit */
    RUN_TEST(test_req_transfer_exit_valid);
    RUN_TEST(test_req_transfer_exit_nrc22_no_active);
    RUN_TEST(test_req_transfer_exit_nrc13_imloif_extra_bytes);
    RUN_TEST(test_req_transfer_exit_cleans_up_context);

    /* 0x38 — RequestFileTransfer */
    RUN_TEST(test_req_file_transfer_stub_returns_nrc31);
    RUN_TEST(test_req_file_transfer_nrc22_null_context);
    RUN_TEST(test_req_file_transfer_nrc22_default_session);
    RUN_TEST(test_req_file_transfer_nrc33_security_locked);
    RUN_TEST(test_req_file_transfer_nrc13_imloif_no_data);
    RUN_TEST(test_req_file_transfer_nrc12_bad_subfn);
    RUN_TEST(test_req_file_transfer_all_subfn_return_nrc31);

    return UNITY_END();
}
