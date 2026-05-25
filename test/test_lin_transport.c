/*
 * test_lin_transport.c
 * LIN Transport Encode/Decode unit tests — Wave 2 Task 8
 */

#include "unity.h"
#include "uds/uds_lin_transport.h"
#include <string.h>

/* ======================================================================== *
 * Test fixtures                                                             *
 * ======================================================================== */

/** @brief Default NAD used in tests */
#define TEST_NAD   0x01

/** @brief An alternate NAD for mismatch testing */
#define ALT_NAD    0x7F

static lin_transport_ctx_t *g_ctx;

void setUp(void)
{
    g_ctx = lin_create_ctx(TEST_NAD);
}

void tearDown(void)
{
    lin_free_ctx(g_ctx);
    g_ctx = NULL;
}

/* ======================================================================== *
 * Helper: create a PDU with given NAD, data bytes, and length              *
 * ======================================================================== */
static lin_diag_pdu_t make_pdu(uint8_t nad, const uint8_t *data, uint8_t len)
{
    lin_diag_pdu_t pdu;
    pdu.nad      = nad;
    pdu.pci      = 0;   /* will be set by encoder */
    pdu.uds_data = (uint8_t *)data;
    pdu.data_len = len;
    return pdu;
}

/* ======================================================================== *
 * SF encode: UDS msg ≤ 6 bytes → 1 frame with correct PCI                 *
 * ======================================================================== */
void test_sf_encode(void)
{
    const uint8_t payload[] = {0x22, 0x12, 0x34, 0x56};
    lin_diag_pdu_t pdu = make_pdu(TEST_NAD, payload, sizeof(payload));
    lin_frame_t frames[8] = {{{0}}};
    uint8_t frame_count = 0;

    /* Test data_len ≤ LIN_SF_MAX_LEN (6) */
    lin_status_t status = lin_tx_encode_ctx(g_ctx, &pdu, frames, &frame_count, 8);
    TEST_ASSERT_EQUAL(LIN_OK, status);
    TEST_ASSERT_EQUAL_UINT8(1, frame_count);

    /* NAD in byte[0] */
    TEST_ASSERT_EQUAL_UINT8(TEST_NAD, frames[0].data[0]);

    /* PCI byte: SF type (0x00) | data_len (4) */
    TEST_ASSERT_EQUAL_UINT8(LIN_PCI_SF | 4, frames[0].data[1]);

    /* UDS payload bytes at offset 2..5 */
    TEST_ASSERT_EQUAL_UINT8(0x22, frames[0].data[2]);
    TEST_ASSERT_EQUAL_UINT8(0x12, frames[0].data[3]);
    TEST_ASSERT_EQUAL_UINT8(0x34, frames[0].data[4]);
    TEST_ASSERT_EQUAL_UINT8(0x56, frames[0].data[5]);

    /* Remaining bytes should be zero (no stale data) */
    TEST_ASSERT_EQUAL_UINT8(0, frames[0].data[6]);
    TEST_ASSERT_EQUAL_UINT8(0, frames[0].data[7]);
}

/* ======================================================================== *
 * SF encode: UDS msg of exactly 6 bytes (maximum SF payload)              *
 * ======================================================================== */
void test_sf_encode_max_len(void)
{
    const uint8_t payload[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    lin_diag_pdu_t pdu = make_pdu(TEST_NAD, payload, sizeof(payload));
    lin_frame_t frames[8];
    uint8_t frame_count = 0;

    lin_status_t status = lin_tx_encode_ctx(g_ctx, &pdu, frames, &frame_count, 8);
    TEST_ASSERT_EQUAL(LIN_OK, status);
    TEST_ASSERT_EQUAL_UINT8(1, frame_count);
    TEST_ASSERT_EQUAL_UINT8(LIN_PCI_SF | 6, frames[0].data[1]);
    TEST_ASSERT_EQUAL_UINT8(0x06, frames[0].data[7]);  /* last payload byte */
}

/* ======================================================================== *
 * SF encode: 1-byte UDS payload (minimum non-zero)                        *
 * ======================================================================== */
void test_sf_encode_min_len(void)
{
    const uint8_t payload[] = {0x3E};  /* TesterPresent */
    lin_diag_pdu_t pdu = make_pdu(TEST_NAD, payload, sizeof(payload));
    lin_frame_t frames[8];
    uint8_t frame_count = 0;

    lin_status_t status = lin_tx_encode_ctx(g_ctx, &pdu, frames, &frame_count, 8);
    TEST_ASSERT_EQUAL(LIN_OK, status);
    TEST_ASSERT_EQUAL_UINT8(1, frame_count);
    TEST_ASSERT_EQUAL_UINT8(LIN_PCI_SF | 1, frames[0].data[1]);
    TEST_ASSERT_EQUAL_UINT8(0x3E, frames[0].data[2]);
}

/* ======================================================================== *
 * NAD mismatch → LIN_NAD_MISMATCH                                          *
 * ======================================================================== */
void test_nad_mismatch(void)
{
    lin_frame_t frame;
    (void)memset(frame.data, 0, sizeof(frame.data));
    frame.data[0] = ALT_NAD;            /* NAD != LIN_NAD_DEFAULT */
    frame.data[1] = LIN_PCI_SF | 2;     /* valid SF PCI, 2 bytes */
    frame.data[2] = 0x10;
    frame.data[3] = 0x03;

    lin_diag_pdu_t pdu;
    lin_status_t status = lin_rx_decode_ctx(g_ctx, &frame, &pdu);
    TEST_ASSERT_EQUAL(LIN_NAD_MISMATCH, status);
}

/* ======================================================================== *
 * Decode valid SF → correct NAD, PCI, UDS data extracted                  *
 * ======================================================================== */
void test_decode_valid_sf(void)
{
    lin_frame_t frame;
    (void)memset(frame.data, 0, sizeof(frame.data));
    frame.data[0] = TEST_NAD;                    /* NAD */
    frame.data[1] = LIN_PCI_SF | 3;              /* SF, 3 payload bytes */
    frame.data[2] = 0x22;                        /* UDS data byte 0 */
    frame.data[3] = 0x01;                        /* UDS data byte 1 */
    frame.data[4] = 0xAA;                        /* UDS data byte 2 */

    lin_diag_pdu_t pdu;
    lin_status_t status = lin_rx_decode_ctx(g_ctx, &frame, &pdu);
    TEST_ASSERT_EQUAL(LIN_OK, status);
    TEST_ASSERT_EQUAL_UINT8(TEST_NAD, pdu.nad);
    TEST_ASSERT_EQUAL_UINT8(LIN_PCI_SF | 3, pdu.pci);
    TEST_ASSERT_EQUAL_UINT8(3, pdu.data_len);
    TEST_ASSERT_NOT_NULL(pdu.uds_data);
    TEST_ASSERT_EQUAL_UINT8(0x22, pdu.uds_data[0]);
    TEST_ASSERT_EQUAL_UINT8(0x01, pdu.uds_data[1]);
    TEST_ASSERT_EQUAL_UINT8(0xAA, pdu.uds_data[2]);
}

/* ======================================================================== *
 * Decode SF with 0-length payload (empty but valid PCI)                    *
 * ======================================================================== */
void test_decode_sf_zero_len(void)
{
    lin_frame_t frame;
    (void)memset(frame.data, 0, sizeof(frame.data));
    frame.data[0] = TEST_NAD;
    frame.data[1] = LIN_PCI_SF | 0;   /* SF with 0-length payload */

    lin_diag_pdu_t pdu;
    lin_status_t status = lin_rx_decode_ctx(g_ctx, &frame, &pdu);
    TEST_ASSERT_EQUAL(LIN_OK, status);
    TEST_ASSERT_EQUAL_UINT8(0, pdu.data_len);
}

/* ======================================================================== *
 * Multi-frame FF+CF encode: UDS msg > 6 bytes → FF + CF(s)               *
 * ======================================================================== */
void test_multi_frame_encode(void)
{
    /* 10 bytes — needs FF (5 bytes) + 1 CF (5 bytes) = 2 frames */
    const uint8_t payload[] = {0x10, 0x11, 0x22, 0x33, 0x44, 0x55,
                               0x66, 0x77, 0x88, 0x99};
    lin_diag_pdu_t pdu = make_pdu(TEST_NAD, payload, sizeof(payload));
    lin_frame_t frames[8];
    uint8_t frame_count = 0;

    lin_status_t status = lin_tx_encode_ctx(g_ctx, &pdu, frames, &frame_count, 8);
    TEST_ASSERT_EQUAL(LIN_OK, status);
    TEST_ASSERT_EQUAL_UINT8(2, frame_count);

    /* --- First Frame (FF) --- */
    /* NAD */
    TEST_ASSERT_EQUAL_UINT8(TEST_NAD, frames[0].data[0]);

    /* PCI: FF type | high nibble of 12-bit length (10 = 0x00A) */
    TEST_ASSERT_EQUAL_UINT8(LIN_PCI_FF | 0x00, frames[0].data[1]);

    /* Length low byte */
    TEST_ASSERT_EQUAL_UINT8(0x0A, frames[0].data[2]);

    /* First 5 payload bytes at offset 3..7 */
    TEST_ASSERT_EQUAL_UINT8(0x10, frames[0].data[3]);
    TEST_ASSERT_EQUAL_UINT8(0x11, frames[0].data[4]);
    TEST_ASSERT_EQUAL_UINT8(0x22, frames[0].data[5]);
    TEST_ASSERT_EQUAL_UINT8(0x33, frames[0].data[6]);
    TEST_ASSERT_EQUAL_UINT8(0x44, frames[0].data[7]);

    /* --- Consecutive Frame (CF) --- */
    /* NAD */
    TEST_ASSERT_EQUAL_UINT8(TEST_NAD, frames[1].data[0]);

    /* PCI: CF type | sequence number (1 for first CF after FF) */
    TEST_ASSERT_EQUAL_UINT8(LIN_PCI_CF | 1, frames[1].data[1]);

    /* Remaining 5 payload bytes at offset 2..6 */
    TEST_ASSERT_EQUAL_UINT8(0x55, frames[1].data[2]);
    TEST_ASSERT_EQUAL_UINT8(0x66, frames[1].data[3]);
    TEST_ASSERT_EQUAL_UINT8(0x77, frames[1].data[4]);
    TEST_ASSERT_EQUAL_UINT8(0x88, frames[1].data[5]);
    TEST_ASSERT_EQUAL_UINT8(0x99, frames[1].data[6]);
}

/* ======================================================================== *
 * Multi-frame encode: 12 bytes → FF (5) + CF (6) + CF (1) = 3 frames      *
 * ======================================================================== */
void test_multi_frame_encode_three_frames(void)
{
    /* 12 bytes: FF carries 5, first CF carries 6, second CF carries 1 */
    const uint8_t payload[] = {0,1,2,3,4,5,6,7,8,9,10,11};
    lin_diag_pdu_t pdu = make_pdu(TEST_NAD, payload, sizeof(payload));
    lin_frame_t frames[8] = {{{0}}};  /* zero-initialize to avoid stale stack data */
    uint8_t frame_count = 0;

    lin_status_t status = lin_tx_encode_ctx(g_ctx, &pdu, frames, &frame_count, 8);
    TEST_ASSERT_EQUAL(LIN_OK, status);
    TEST_ASSERT_EQUAL_UINT8(3, frame_count);

    /* FF: PCI = 0x20 | 0x00 (high nibble of 0x00C = 0), length byte = 0x0C */
    TEST_ASSERT_EQUAL_UINT8(LIN_PCI_FF | 0x00, frames[0].data[1]);
    TEST_ASSERT_EQUAL_UINT8(0x0C, frames[0].data[2]);

    /* CF1: seq = 1 */
    TEST_ASSERT_EQUAL_UINT8(LIN_PCI_CF | 1, frames[1].data[1]);
    TEST_ASSERT_EQUAL_UINT8(5, frames[1].data[2]);  /* offset 5 */
    TEST_ASSERT_EQUAL_UINT8(10, frames[1].data[7]); /* offset 10, last of 6 bytes */

    /* CF2: seq = 2, carries 1 byte */
    TEST_ASSERT_EQUAL_UINT8(LIN_PCI_CF | 2, frames[2].data[1]);
    TEST_ASSERT_EQUAL_UINT8(11, frames[2].data[2]);
    TEST_ASSERT_EQUAL_UINT8(0, frames[2].data[3]);  /* no more data */
}

/* ======================================================================== *
 * Multi-frame FF+CF decode: receive FF then CF, reassemble correctly       *
 * ======================================================================== */
void test_multi_frame_decode(void)
{
    uint8_t expected[] = {0x10, 0x11, 0x22, 0x33, 0x44, 0x55,
                          0x66, 0x77, 0x88, 0x99};

    /* --- Send First Frame --- */
    lin_frame_t ff;
    (void)memset(ff.data, 0, sizeof(ff.data));
    ff.data[0] = TEST_NAD;
    ff.data[1] = LIN_PCI_FF | 0x00;          /* PCI + high nibble of 0x00A */
    ff.data[2] = 0x0A;                       /* length low byte (10 bytes) */
    ff.data[3] = 0x10;                       /* UDS data byte 0 */
    ff.data[4] = 0x11;                       /* UDS data byte 1 */
    ff.data[5] = 0x22;                       /* UDS data byte 2 */
    ff.data[6] = 0x33;                       /* UDS data byte 3 */
    ff.data[7] = 0x44;                       /* UDS data byte 4 */

    lin_diag_pdu_t pdu;
    lin_status_t status = lin_rx_decode_ctx(g_ctx, &ff, &pdu);

    /* FF accepted — partial data in PDU */
    TEST_ASSERT_EQUAL(LIN_OK, status);
    TEST_ASSERT_EQUAL_UINT8(TEST_NAD, pdu.nad);
    TEST_ASSERT_EQUAL_UINT8(5, pdu.data_len);  /* first 5 bytes */

    /* --- Send Consecutive Frame --- */
    lin_frame_t cf;
    (void)memset(cf.data, 0, sizeof(cf.data));
    cf.data[0] = TEST_NAD;
    cf.data[1] = LIN_PCI_CF | 1;             /* CF, seq=1 */
    cf.data[2] = 0x55;                       /* UDS data byte 5 */
    cf.data[3] = 0x66;                       /* UDS data byte 6 */
    cf.data[4] = 0x77;                       /* UDS data byte 7 */
    cf.data[5] = 0x88;                       /* UDS data byte 8 */
    cf.data[6] = 0x99;                       /* UDS data byte 9 */

    status = lin_rx_decode_ctx(g_ctx, &cf, &pdu);

    /* Complete multi-frame received */
    TEST_ASSERT_EQUAL(LIN_OK, status);
    TEST_ASSERT_EQUAL_UINT8(TEST_NAD, pdu.nad);
    TEST_ASSERT_EQUAL_UINT8(10, pdu.data_len);  /* full 10 bytes */
    TEST_ASSERT_NOT_NULL(pdu.uds_data);

    /* Verify all reassembled bytes */
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, pdu.uds_data, 10);
}

/* ======================================================================== *
 * Invalid PCI type → LIN_PCI_ERROR                                         *
 * ======================================================================== */
void test_invalid_pci_type(void)
{
    lin_frame_t frame;
    (void)memset(frame.data, 0, sizeof(frame.data));
    frame.data[0] = TEST_NAD;
    frame.data[1] = 0x80 | 2;   /* PCI type = 0x80 (invalid — only types 0-3 defined) */
    frame.data[2] = 0x10;

    lin_diag_pdu_t pdu;
    lin_status_t status = lin_rx_decode_ctx(g_ctx, &frame, &pdu);
    TEST_ASSERT_EQUAL(LIN_PCI_ERROR, status);
}

/* ======================================================================== *
 * Invalid SF length (PCI says 7 but SF max is 6) → LIN_PCI_ERROR           *
 * ======================================================================== */
void test_sf_invalid_length(void)
{
    lin_frame_t frame;
    (void)memset(frame.data, 0, sizeof(frame.data));
    frame.data[0] = TEST_NAD;
    frame.data[1] = LIN_PCI_SF | 7;   /* SF with len=7 > LIN_SF_MAX_LEN=6 */
    frame.data[2] = 0x10;

    lin_diag_pdu_t pdu;
    lin_status_t status = lin_rx_decode_ctx(g_ctx, &frame, &pdu);
    TEST_ASSERT_EQUAL(LIN_PCI_ERROR, status);
}

/* ======================================================================== *
 * Invalid FF total length (< SF max + 1) → LIN_PCI_ERROR                  *
 * ======================================================================== */
void test_ff_invalid_length(void)
{
    lin_frame_t frame;
    (void)memset(frame.data, 0, sizeof(frame.data));
    frame.data[0] = TEST_NAD;
    frame.data[1] = LIN_PCI_FF | 0x00;
    frame.data[2] = 0x05;   /* total_len = 5, which fits in SF → invalid for FF */

    lin_diag_pdu_t pdu;
    lin_status_t status = lin_rx_decode_ctx(g_ctx, &frame, &pdu);
    TEST_ASSERT_EQUAL(LIN_PCI_ERROR, status);
}

/* ======================================================================== *
 * CF received without prior FF → LIN_PCI_ERROR                             *
 * ======================================================================== */
void test_cf_without_ff(void)
{
    lin_frame_t frame;
    (void)memset(frame.data, 0, sizeof(frame.data));
    frame.data[0] = TEST_NAD;
    frame.data[1] = LIN_PCI_CF | 1;   /* CF seq=1, but no FF received */

    lin_diag_pdu_t pdu;
    lin_status_t status = lin_rx_decode_ctx(g_ctx, &frame, &pdu);
    TEST_ASSERT_EQUAL(LIN_PCI_ERROR, status);
}

/* ======================================================================== *
 * CF with wrong sequence number → LIN_PCI_ERROR                            *
 * ======================================================================== */
void test_cf_wrong_sequence(void)
{
    /* First send a valid FF */
    lin_frame_t ff;
    (void)memset(ff.data, 0, sizeof(ff.data));
    ff.data[0] = TEST_NAD;
    ff.data[1] = LIN_PCI_FF | 0x00;
    ff.data[2] = 0x0A;              /* 10 bytes total */
    ff.data[3] = 0x01;              /* payload bytes */
    ff.data[4] = 0x02;
    ff.data[5] = 0x03;
    ff.data[6] = 0x04;
    ff.data[7] = 0x05;

    lin_diag_pdu_t pdu;
    lin_rx_decode_ctx(g_ctx, &ff, &pdu);

    /* Now send CF with wrong sequence (expect 1, send 3) */
    lin_frame_t cf;
    (void)memset(cf.data, 0, sizeof(cf.data));
    cf.data[0] = TEST_NAD;
    cf.data[1] = LIN_PCI_CF | 3;   /* expecting seq=1, got seq=3 */
    cf.data[2] = 0x06;
    cf.data[3] = 0x07;
    cf.data[4] = 0x08;
    cf.data[5] = 0x09;
    cf.data[6] = 0x0A;

    lin_status_t status = lin_rx_decode_ctx(g_ctx, &cf, &pdu);
    TEST_ASSERT_EQUAL(LIN_PCI_ERROR, status);
}

/* ======================================================================== *
 * NULL/zero-length input → appropriate error                               *
 * ======================================================================== */
void test_null_inputs(void)
{
    lin_frame_t frames[4];
    uint8_t frame_count = 0;
    uint8_t dummy_data[] = {0x10, 0x03};
    lin_diag_pdu_t pdu = make_pdu(TEST_NAD, dummy_data, sizeof(dummy_data));

    /* NULL pdu pointer */
    TEST_ASSERT_EQUAL(LIN_PCI_ERROR,
        lin_tx_encode_ctx(g_ctx, NULL, frames, &frame_count, 4));

    /* NULL frames pointer */
    TEST_ASSERT_EQUAL(LIN_PCI_ERROR,
        lin_tx_encode_ctx(g_ctx, &pdu, NULL, &frame_count, 4));

    /* NULL frame_count pointer */
    TEST_ASSERT_EQUAL(LIN_PCI_ERROR,
        lin_tx_encode_ctx(g_ctx, &pdu, frames, NULL, 4));

    /* NULL frame pointer (rx) */
    TEST_ASSERT_EQUAL(LIN_PCI_ERROR,
        lin_rx_decode_ctx(g_ctx, NULL, &pdu));

    /* NULL pdu pointer (rx) */
    lin_frame_t frame;
    (void)memset(frame.data, 0, sizeof(frame.data));
    TEST_ASSERT_EQUAL(LIN_PCI_ERROR,
        lin_rx_decode_ctx(g_ctx, &frame, NULL));
}

/* ======================================================================== *
 * Zero-length UDS data → LIN_PCI_ERROR                                     *
 * ======================================================================== */
void test_zero_length_encode(void)
{
    lin_diag_pdu_t pdu;
    pdu.nad      = TEST_NAD;
    pdu.pci      = 0;
    pdu.uds_data = NULL;
    pdu.data_len = 0;

    lin_frame_t frames[4];
    uint8_t frame_count = 0;

    lin_status_t status = lin_tx_encode_ctx(g_ctx, &pdu, frames, &frame_count, 4);
    TEST_ASSERT_EQUAL(LIN_PCI_ERROR, status);
}

/* ======================================================================== *
 * lin_transport_reset() clears state                                       *
 * ======================================================================== */
void test_reset_clears_state(void)
{
    /* Set up some state by doing a partial multi-frame receive */
    lin_frame_t ff;
    (void)memset(ff.data, 0, sizeof(ff.data));
    ff.data[0] = TEST_NAD;
    ff.data[1] = LIN_PCI_FF | 0x00;
    ff.data[2] = 0x0A;
    ff.data[3] = 0x01;
    ff.data[4] = 0x02;
    ff.data[5] = 0x03;
    ff.data[6] = 0x04;
    ff.data[7] = 0x05;

    lin_diag_pdu_t pdu;
    lin_rx_decode_ctx(g_ctx, &ff, &pdu);  /* Should transition to RX_CF */

    /* Now reset */
    lin_transport_reset_ctx(g_ctx);

    /* CF without FF (after reset) should now fail */
    lin_frame_t cf;
    (void)memset(cf.data, 0, sizeof(cf.data));
    cf.data[0] = TEST_NAD;
    cf.data[1] = LIN_PCI_CF | 1;
    cf.data[2] = 0x06;

    lin_status_t status = lin_rx_decode_ctx(g_ctx, &cf, &pdu);
    TEST_ASSERT_EQUAL(LIN_PCI_ERROR, status);  /* no FF state to match */

    /* TX encode should still work normally after reset */
    const uint8_t payload[] = {0x22, 0x12, 0x34};
    lin_diag_pdu_t pdu2 = make_pdu(TEST_NAD, payload, sizeof(payload));
    lin_frame_t frames[4];
    uint8_t frame_count = 0;
    status = lin_tx_encode_ctx(g_ctx, &pdu2, frames, &frame_count, 4);
    TEST_ASSERT_EQUAL(LIN_OK, status);
    TEST_ASSERT_EQUAL_UINT8(1, frame_count);
}

/* ======================================================================== *
 * FF exceeding max payload length (4095) → LIN_PCI_ERROR                   *
 * ======================================================================== */
void test_ff_exceeds_max_length(void)
{
    lin_frame_t frame;
    (void)memset(frame.data, 0, sizeof(frame.data));
    frame.data[0] = TEST_NAD;
    frame.data[1] = LIN_PCI_FF | 0x0F;   /* high nibble = 0x0F */
    frame.data[2] = 0xFF;                 /* low byte = 0xFF → total = 4095 + 0xFF = wrong? */

    /* Actually: 0x0F << 8 | 0xFF = 0x0FFF = 4095 = LIN_FF_MAX_LEN, so this is valid */
    /* Let's test 4096 which is > LIN_FF_MAX_LEN */
    frame.data[1] = LIN_PCI_FF | 0x10;   /* 0x10 << 8 = 4096 → exceeds max */
    frame.data[2] = 0x00;                 /* 0x1000 = 4096 */

    lin_diag_pdu_t pdu;
    lin_status_t status = lin_rx_decode_ctx(g_ctx, &frame, &pdu);
    TEST_ASSERT_EQUAL(LIN_PCI_ERROR, status);
}

/* ======================================================================== *
 * SF max 6 bytes — border of single-frame, encode + decode                 *
 * ======================================================================== */
void test_multi_frame_sf_max_6_bytes(void)
{
    /* --- Encode: 6 bytes should produce SF, not FF --- */
    const uint8_t payload[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    lin_diag_pdu_t pdu = make_pdu(TEST_NAD, payload, sizeof(payload));
    lin_frame_t frames[4];
    uint8_t frame_count = 0;

    lin_status_t status = lin_tx_encode_ctx(g_ctx, &pdu, frames, &frame_count, 4);
    TEST_ASSERT_EQUAL(LIN_OK, status);
    TEST_ASSERT_EQUAL_UINT8(1, frame_count);
    TEST_ASSERT_EQUAL_UINT8(LIN_PCI_SF | 6, frames[0].data[1]);
    TEST_ASSERT_EQUAL_UINT8(0xFF, frames[0].data[7]);

    /* --- Decode: SF with exactly 6 bytes --- */
    lin_frame_t sf;
    (void)memset(sf.data, 0, sizeof(sf.data));
    sf.data[0] = TEST_NAD;
    sf.data[1] = LIN_PCI_SF | 6;
    sf.data[2] = 0x11;
    sf.data[3] = 0x22;
    sf.data[4] = 0x33;
    sf.data[5] = 0x44;
    sf.data[6] = 0x55;
    sf.data[7] = 0x66;

    lin_diag_pdu_t decoded;
    status = lin_rx_decode_ctx(g_ctx, &sf, &decoded);
    TEST_ASSERT_EQUAL(LIN_OK, status);
    TEST_ASSERT_EQUAL_UINT8(6, decoded.data_len);
    TEST_ASSERT_EQUAL_UINT8(0x66, decoded.uds_data[5]);
}

/* ======================================================================== *
 * FF min 7 bytes — minimum multi-frame boundary                            *
 * ======================================================================== */
void test_multi_frame_ff_min_7_bytes(void)
{
    /* Exactly 7 bytes: FF carries 5, CF carries 2 → 2 frames */
    const uint8_t payload[] = {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6};
    lin_diag_pdu_t pdu = make_pdu(TEST_NAD, payload, sizeof(payload));
    lin_frame_t frames[4] = {{{0}}};
    uint8_t frame_count = 0;

    lin_status_t status = lin_tx_encode_ctx(g_ctx, &pdu, frames, &frame_count, 4);
    TEST_ASSERT_EQUAL(LIN_OK, status);
    TEST_ASSERT_EQUAL_UINT8(2, frame_count);

    /* FF: PCI with total_len=7 */
    TEST_ASSERT_EQUAL_UINT8(LIN_PCI_FF | 0x00, frames[0].data[1]);
    TEST_ASSERT_EQUAL_UINT8(0x07, frames[0].data[2]);
    TEST_ASSERT_EQUAL_UINT8(0xA0, frames[0].data[3]);
    TEST_ASSERT_EQUAL_UINT8(0xA4, frames[0].data[7]);

    /* CF: seq=1, carries remaining 2 bytes */
    TEST_ASSERT_EQUAL_UINT8(LIN_PCI_CF | 1, frames[1].data[1]);
    TEST_ASSERT_EQUAL_UINT8(0xA5, frames[1].data[2]);
    TEST_ASSERT_EQUAL_UINT8(0xA6, frames[1].data[3]);
    TEST_ASSERT_EQUAL_UINT8(0,    frames[1].data[4]);  /* unused */
}

/* ======================================================================== *
 * FF exact 5 bytes — decode: FF with exactly 5-byte payload (total=7)      *
 * ======================================================================== */
void test_multi_frame_ff_exact_5_bytes(void)
{
    uint8_t expected[] = {0x10, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    /* --- Send First Frame with total_len=7 (minimum valid FF) --- */
    lin_frame_t ff;
    (void)memset(ff.data, 0, sizeof(ff.data));
    ff.data[0] = TEST_NAD;
    ff.data[1] = LIN_PCI_FF | 0x00;
    ff.data[2] = 0x07;       /* total_len = 7 */
    ff.data[3] = 0x10;
    ff.data[4] = 0x11;
    ff.data[5] = 0x22;
    ff.data[6] = 0x33;
    ff.data[7] = 0x44;

    lin_diag_pdu_t pdu;
    lin_status_t status = lin_rx_decode_ctx(g_ctx, &ff, &pdu);
    TEST_ASSERT_EQUAL(LIN_OK, status);
    TEST_ASSERT_EQUAL_UINT8(5, pdu.data_len);  /* FF carries 5 bytes */

    /* --- Send Consecutive Frame with remaining 2 bytes --- */
    lin_frame_t cf;
    (void)memset(cf.data, 0, sizeof(cf.data));
    cf.data[0] = TEST_NAD;
    cf.data[1] = LIN_PCI_CF | 1;
    cf.data[2] = 0x55;
    cf.data[3] = 0x66;

    status = lin_rx_decode_ctx(g_ctx, &cf, &pdu);
    TEST_ASSERT_EQUAL(LIN_OK, status);
    TEST_ASSERT_EQUAL_UINT8(7, pdu.data_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, pdu.uds_data, 7);
}

/* ======================================================================== *
 * CF sequence wrap — 100-byte payload → 16 CFs, seq 1..15→1               *
 * ======================================================================== */
void test_multi_frame_cf_seq_wrap(void)
{
    /* 100 bytes: FF(5) + CF×16(15×6 + 1×5) = 17 frames */
    static uint8_t payload[100];
    uint8_t i;
    for (i = 0; i < 100; i++) {
        payload[i] = i;
    }

    lin_diag_pdu_t pdu = make_pdu(TEST_NAD, payload, 100);
    static lin_frame_t frames[20];
    uint8_t frame_count = 0;

    lin_status_t status = lin_tx_encode_ctx(g_ctx, &pdu, frames, &frame_count, 20);
    TEST_ASSERT_EQUAL(LIN_OK, status);
    TEST_ASSERT_EQUAL_UINT8(17, frame_count);

    /* FF: total_len = 100 = 0x0064 */
    TEST_ASSERT_EQUAL_UINT8(LIN_PCI_FF | 0x00, frames[0].data[1]);
    TEST_ASSERT_EQUAL_UINT8(0x64, frames[0].data[2]);

    /* CF1..CF15: seq 1..15, each carries 6 bytes */
    for (i = 0; i < 15; i++) {
        uint8_t expected_seq = (uint8_t)((i % 15) + 1);
        TEST_ASSERT_EQUAL_UINT8(LIN_PCI_CF | expected_seq, frames[1 + i].data[1]);
    }

    /* CF16: seq wraps to 1, carries last 5 bytes */
    TEST_ASSERT_EQUAL_UINT8(LIN_PCI_CF | 1, frames[16].data[1]);
    TEST_ASSERT_EQUAL_UINT8(95,  frames[16].data[2]); /* offset 95 */
    TEST_ASSERT_EQUAL_UINT8(99,  frames[16].data[6]); /* offset 99, last byte */
}

/* ======================================================================== *
 * Max payload 4095 bytes — verify boundary and buffer management           *
 * ======================================================================== */
void test_multi_frame_max_4095_bytes(void)
{
    lin_status_t status;

    /* Boundary: Encode must reject data_len=4096 */
    {
        static uint8_t buf[8];
        lin_diag_pdu_t big;
        big.nad      = TEST_NAD;
        big.pci      = 0;
        big.uds_data = buf;
        big.data_len = 4096;
        lin_frame_t frames[4];
        uint8_t frame_count = 0;
        status = lin_tx_encode_ctx(g_ctx, &big, frames, &frame_count, 4);
        TEST_ASSERT_EQUAL(LIN_PCI_ERROR, status);
    }

    /* Boundary: FF decode total_len=4096 rejected */
    {
        lin_frame_t frame;
        lin_diag_pdu_t pdu;
        (void)memset(frame.data, 0, sizeof(frame.data));
        frame.data[0] = TEST_NAD;
        frame.data[1] = LIN_PCI_FF | 0x10;
        frame.data[2] = 0x00;
        frame.data[3] = 0x01;
        status = lin_rx_decode_ctx(g_ctx, &frame, &pdu);
        TEST_ASSERT_EQUAL(LIN_PCI_ERROR, status);
    }

    /* Boundary: FF decode total_len=4095 accepted → verifies rx_buffer size */
    {
        lin_frame_t frame;
        lin_diag_pdu_t pdu;
        (void)memset(frame.data, 0, sizeof(frame.data));
        frame.data[0] = TEST_NAD;
        frame.data[1] = LIN_PCI_FF | 0x0F;
        frame.data[2] = 0xFF;
        frame.data[3] = 0x01;
        frame.data[4] = 0x02;
        frame.data[5] = 0x03;
        frame.data[6] = 0x04;
        frame.data[7] = 0x05;
        status = lin_rx_decode_ctx(g_ctx, &frame, &pdu);
        TEST_ASSERT_EQUAL(LIN_OK, status);
        TEST_ASSERT_EQUAL_UINT8(5, pdu.data_len);
    }

    /* Scale: encode 260-byte payload → 44 frames (safe range, avoids uint8_t wraparound) */
    {
        static uint8_t large_payload[260];
        uint16_t i;
        for (i = 0; i < 260; i++) {
            large_payload[i] = (uint8_t)(i & 0xFF);
        }
        lin_diag_pdu_t big;
        big.nad      = TEST_NAD;
        big.pci      = 0;
        big.uds_data = large_payload;
        big.data_len = 260;
        static lin_frame_t frames[48];
        uint8_t frame_count = 0;

        status = lin_tx_encode_ctx(g_ctx, &big, frames, &frame_count, 48);
        TEST_ASSERT_EQUAL(LIN_OK, status);
        /* FF(1) + CFs: 255 bytes / 6 = 42 rem 3 → 43 CFs → 44 total */
        TEST_ASSERT_EQUAL_UINT8(44, frame_count);

        /* FF: 260 = 0x0104 → PCI high nibble = 0x01, low byte = 0x04 */
        TEST_ASSERT_EQUAL_UINT8(LIN_PCI_FF | 0x01, frames[0].data[1]);
        TEST_ASSERT_EQUAL_UINT8(0x04, frames[0].data[2]);

        /* Last CF (index 43): seq = (42%15)+1 = 13, carries last 3 bytes */
        TEST_ASSERT_EQUAL_UINT8(LIN_PCI_CF | 13, frames[43].data[1]);
        TEST_ASSERT_EQUAL_UINT8(0x01, frames[43].data[2]); /* payload[257] = 257 & 0xFF = 1 */
        TEST_ASSERT_EQUAL_UINT8(0x03, frames[43].data[4]); /* payload[259] = 259 & 0xFF = 3 */
    }
}

/* ======================================================================== *
 * Invalid PCI type — reject all undefined upper PCI types                  *
 * ======================================================================== */
void test_lin_rx_decode_invalid_pci(void)
{
    /* Valid types: 0x00 (SF), 0x20 (FF), 0x40 (CF), 0x60 (FC)
     * Invalid types: 0x80, 0xA0, 0xC0, 0xE0 */
    static const uint8_t invalid_types[] = {0x80, 0xA0, 0xC0, 0xE0};
    uint8_t i;

    for (i = 0; i < sizeof(invalid_types); i++) {
        lin_frame_t frame;
        (void)memset(frame.data, 0, sizeof(frame.data));
        frame.data[0] = TEST_NAD;
        frame.data[1] = invalid_types[i] | 2;
        frame.data[2] = 0x10;

        lin_diag_pdu_t pdu;
        lin_status_t status = lin_rx_decode_ctx(g_ctx, &frame, &pdu);
        TEST_ASSERT_EQUAL(LIN_PCI_ERROR, status);
    }
}

/* ======================================================================== *
 * Test runner                                                               *
 * ======================================================================== */
int main(void)
{
    UNITY_BEGIN();

    /* --- Single Frame encode --- */
    RUN_TEST(test_sf_encode);
    RUN_TEST(test_sf_encode_max_len);
    RUN_TEST(test_sf_encode_min_len);

    /* --- NAD filtering --- */
    RUN_TEST(test_nad_mismatch);

    /* --- Single Frame decode --- */
    RUN_TEST(test_decode_valid_sf);
    RUN_TEST(test_decode_sf_zero_len);

    /* --- Multi-frame encode --- */
    RUN_TEST(test_multi_frame_encode);
    RUN_TEST(test_multi_frame_encode_three_frames);

    /* --- Multi-frame decode --- */
    RUN_TEST(test_multi_frame_decode);

    /* --- Error cases --- */
    RUN_TEST(test_invalid_pci_type);
    RUN_TEST(test_sf_invalid_length);
    RUN_TEST(test_ff_invalid_length);
    RUN_TEST(test_cf_without_ff);
    RUN_TEST(test_cf_wrong_sequence);
    RUN_TEST(test_ff_exceeds_max_length);
    RUN_TEST(test_null_inputs);
    RUN_TEST(test_zero_length_encode);

    /* --- State management --- */
    RUN_TEST(test_reset_clears_state);

    /* --- Boundary / edge-case tests (Task 16) --- */
    RUN_TEST(test_multi_frame_sf_max_6_bytes);
    RUN_TEST(test_multi_frame_ff_min_7_bytes);
    RUN_TEST(test_multi_frame_ff_exact_5_bytes);
    RUN_TEST(test_multi_frame_cf_seq_wrap);
    RUN_TEST(test_multi_frame_max_4095_bytes);
    RUN_TEST(test_lin_rx_decode_invalid_pci);

    return UNITY_END();
}
