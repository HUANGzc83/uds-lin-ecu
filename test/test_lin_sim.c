/*
 * test_lin_sim.c
 * LIN Master Simulator unit tests — Wave 3d Task 20
 *
 * Exercises lin_sim_init, lin_sim_send_request, lin_sim_receive_response,
 * and lin_sim_calc_frame_time_ms using mock_uart for frame I/O.
 */

#include "unity.h"
#include "mock_all.h"
#include "lin_sim/lin_master_sim.h"
#include <string.h>

/* ======================================================================== *
 * Test constants                                                            *
 * ======================================================================== */

/** @brief Default NAD used in most tests */
#define TEST_NAD    LIN_NAD_DEFAULT   /* 0x01 */

/** @brief Alternate NAD for mismatch testing */
#define ALT_NAD     0x7F

/** @brief Buffer for capturing UART-sent data */
static uint8_t g_sent[512];
static uint16_t g_sent_len;

/* ======================================================================== *
 * Fixtures                                                                  *
 * ======================================================================== */

void setUp(void)
{
    mock_uart_clear();
    mock_timer_clear();
    g_sent_len = 0;
}

void tearDown(void)
{
    /* nothing to tear down */
}

/* ======================================================================== *
 * Helper: check a sent LIN frame at a given index                          *
 * ======================================================================== */
static void verify_sent_frame(uint16_t *offset,
                              uint8_t exp_nad,
                              uint8_t exp_pci,
                              const uint8_t *exp_data,
                              uint8_t exp_data_len)
{
    uint8_t frame[LIN_FRAME_SIZE];

    /* Copy the frame bytes from the sent buffer starting at *offset */
    for (uint8_t i = 0u; i < LIN_FRAME_SIZE; i++)
    {
        frame[i] = g_sent[(*offset)++];
    }

    /* Verify NAD */
    TEST_ASSERT_EQUAL_UINT8(exp_nad, frame[0]);

    /* Verify PCI */
    TEST_ASSERT_EQUAL_UINT8(exp_pci, frame[1]);

    /* Verify UDS payload at offset 2..(2+exp_data_len-1) */
    if (exp_data != NULL && exp_data_len > 0u)
    {
        TEST_ASSERT_EQUAL_UINT8_ARRAY(exp_data, &frame[2], exp_data_len);
    }

    /* Verify remaining bytes are zero */
    for (uint8_t i = (uint8_t)(2u + exp_data_len); i < LIN_FRAME_SIZE; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(0, frame[i]);
    }
}

/* ======================================================================== *
 * lin_sim_init tests                                                        *
 * ======================================================================== */

void test_init_default(void)
{
    int rc = lin_sim_init(TEST_NAD, LIN_BAUDRATE);
    TEST_ASSERT_EQUAL_INT(0, rc);
}

void test_init_zero_baud(void)
{
    int rc = lin_sim_init(TEST_NAD, 0u);
    TEST_ASSERT_EQUAL_INT(-1, rc);
}

/* ======================================================================== *
 * lin_sim_send_request — Single Frame                                      *
 * ======================================================================== */

void test_send_sf(void)
{
    const uint8_t payload[] = {0x22, 0x01, 0x02, 0x03};  /* ReadDataByIdentifier */

    lin_sim_init(TEST_NAD, LIN_BAUDRATE);

    int frames_sent = lin_sim_send_request(TEST_NAD, payload, sizeof(payload));
    TEST_ASSERT_EQUAL_INT(1, frames_sent);

    /* Capture and verify the sent data */
    mock_uart_get_sent_data(g_sent, &g_sent_len);
    TEST_ASSERT_EQUAL_UINT16(LIN_FRAME_SIZE, g_sent_len);

    uint16_t off = 0u;
    verify_sent_frame(&off,
                      TEST_NAD,
                      LIN_PCI_SF | sizeof(payload),   /* SF, len=4 */
                      payload,
                      sizeof(payload));
}

void test_send_sf_max_len(void)
{
    /* Exactly 6 bytes — maximum for Single Frame */
    const uint8_t payload[] = {0x10, 0x01, 0x02, 0x03, 0x04, 0x05};

    lin_sim_init(TEST_NAD, LIN_BAUDRATE);

    int frames_sent = lin_sim_send_request(TEST_NAD, payload, sizeof(payload));
    TEST_ASSERT_EQUAL_INT(1, frames_sent);

    mock_uart_get_sent_data(g_sent, &g_sent_len);
    TEST_ASSERT_EQUAL_UINT16(LIN_FRAME_SIZE, g_sent_len);

    uint16_t off = 0u;
    verify_sent_frame(&off,
                      TEST_NAD,
                      LIN_PCI_SF | 6,
                      payload,
                      6);
}

void test_send_sf_min_len(void)
{
    /* 1 byte — minimum non-zero */
    const uint8_t payload[] = {0x3E};  /* TesterPresent */

    lin_sim_init(TEST_NAD, LIN_BAUDRATE);

    int frames_sent = lin_sim_send_request(TEST_NAD, payload, sizeof(payload));
    TEST_ASSERT_EQUAL_INT(1, frames_sent);

    mock_uart_get_sent_data(g_sent, &g_sent_len);
    TEST_ASSERT_EQUAL_UINT16(LIN_FRAME_SIZE, g_sent_len);

    uint16_t off = 0u;
    verify_sent_frame(&off,
                      TEST_NAD,
                      LIN_PCI_SF | 1,
                      payload,
                      1);
}

/* ======================================================================== *
 * lin_sim_send_request — Multi-Frame (FF + CF)                             *
 * ======================================================================== */

void test_send_multi_frame(void)
{
    /* 10 bytes — requires FF (5 bytes) + 1 CF (5 bytes) */
    const uint8_t payload[] = {0x10, 0x11, 0x22, 0x33, 0x44, 0x55,
                               0x66, 0x77, 0x88, 0x99};

    lin_sim_init(TEST_NAD, LIN_BAUDRATE);

    int frames_sent = lin_sim_send_request(TEST_NAD, payload, sizeof(payload));
    TEST_ASSERT_EQUAL_INT(2, frames_sent);

    /* Capture sent data: 2 frames × 8 bytes = 16 bytes */
    mock_uart_get_sent_data(g_sent, &g_sent_len);
    TEST_ASSERT_EQUAL_UINT16(2u * LIN_FRAME_SIZE, g_sent_len);

    uint16_t off = 0u;

    /* --- First Frame (FF) — layout: NAD | PCI | LEN | UDS[0..4] --- */
    TEST_ASSERT_EQUAL_UINT8(TEST_NAD, g_sent[off]);                   /* NAD */
    TEST_ASSERT_EQUAL_UINT8(LIN_PCI_FF | 0x00, g_sent[off + 1]);     /* PCI */
    TEST_ASSERT_EQUAL_UINT8(0x0A, g_sent[off + 2]);                   /* length = 10 */
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, &g_sent[off + 3], 5);     /* UDS 0..4 */
    off += LIN_FRAME_SIZE;

    /* --- Consecutive Frame (CF) — layout: NAD | PCI | UDS[0..4] --- */
    TEST_ASSERT_EQUAL_UINT8(TEST_NAD, g_sent[off]);                   /* NAD */
    TEST_ASSERT_EQUAL_UINT8(LIN_PCI_CF | 1, g_sent[off + 1]);        /* PCI seq=1 */
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload + 5, &g_sent[off + 2], 5); /* UDS 5..9 */
    /* Remaining byte (offset 7) should be zero */
    TEST_ASSERT_EQUAL_UINT8(0, g_sent[off + 7]);
}

void test_send_multi_frame_three_frames(void)
{
    /* 12 bytes — FF (5) + CF1 (6) + CF2 (1) */
    const uint8_t payload[] = {0,1,2,3,4,5,6,7,8,9,10,11};

    lin_sim_init(TEST_NAD, LIN_BAUDRATE);

    int frames_sent = lin_sim_send_request(TEST_NAD, payload, sizeof(payload));
    TEST_ASSERT_EQUAL_INT(3, frames_sent);

    mock_uart_get_sent_data(g_sent, &g_sent_len);
    TEST_ASSERT_EQUAL_UINT16(3u * LIN_FRAME_SIZE, g_sent_len);

    uint16_t off = 0u;

    /* --- FF: 12 bytes total --- */
    TEST_ASSERT_EQUAL_UINT8(TEST_NAD, g_sent[off]);
    TEST_ASSERT_EQUAL_UINT8(LIN_PCI_FF | 0x00, g_sent[off + 1]);
    TEST_ASSERT_EQUAL_UINT8(0x0C, g_sent[off + 2]);   /* length = 12 */
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, &g_sent[off + 3], 5);
    off += LIN_FRAME_SIZE;

    /* --- CF1: seq=1, 6 bytes of UDS --- */
    TEST_ASSERT_EQUAL_UINT8(TEST_NAD, g_sent[off]);
    TEST_ASSERT_EQUAL_UINT8(LIN_PCI_CF | 1, g_sent[off + 1]);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload + 5, &g_sent[off + 2], 6);
    off += LIN_FRAME_SIZE;

    /* --- CF2: seq=2, 1 byte of UDS, rest zero --- */
    TEST_ASSERT_EQUAL_UINT8(TEST_NAD, g_sent[off]);
    TEST_ASSERT_EQUAL_UINT8(LIN_PCI_CF | 2, g_sent[off + 1]);
    TEST_ASSERT_EQUAL_UINT8(11, g_sent[off + 2]);
    TEST_ASSERT_EQUAL_UINT8(0, g_sent[off + 3]);
    TEST_ASSERT_EQUAL_UINT8(0, g_sent[off + 4]);
    TEST_ASSERT_EQUAL_UINT8(0, g_sent[off + 5]);
    TEST_ASSERT_EQUAL_UINT8(0, g_sent[off + 6]);
    TEST_ASSERT_EQUAL_UINT8(0, g_sent[off + 7]);
}

/* ======================================================================== *
 * lin_sim_send_request — Error cases                                       *
 * ======================================================================== */

void test_send_not_init(void)
{
    const uint8_t payload[] = {0x10, 0x03};
    /* Do NOT call lin_sim_init — should fail */
    int rc = lin_sim_send_request(TEST_NAD, payload, sizeof(payload));
    TEST_ASSERT_EQUAL_INT(-1, rc);
}

void test_send_null_data(void)
{
    lin_sim_init(TEST_NAD, LIN_BAUDRATE);
    int rc = lin_sim_send_request(TEST_NAD, NULL, 4u);
    TEST_ASSERT_EQUAL_INT(-1, rc);
}

void test_send_zero_len(void)
{
    uint8_t dummy = 0x10;
    lin_sim_init(TEST_NAD, LIN_BAUDRATE);
    int rc = lin_sim_send_request(TEST_NAD, &dummy, 0u);
    TEST_ASSERT_EQUAL_INT(-1, rc);
}

/* ======================================================================== *
 * lin_sim_receive_response — Single Frame                                  *
 * ======================================================================== */

void test_receive_sf(void)
{
    /* Inject a valid SF response into the UART RX buffer */
    uint8_t rx_frame[LIN_FRAME_SIZE] = {0};
    rx_frame[0] = TEST_NAD;
    rx_frame[1] = LIN_PCI_SF | 3;                    /* SF, 3 UDS bytes */
    rx_frame[2] = 0x50;                              /* UDS response: PositiveResponse */
    rx_frame[3] = 0x22;                              /* SID = ReadDataByIdentifier */
    rx_frame[4] = 0x01;                              /* Data byte */

    mock_uart_inject_rx_data(rx_frame, LIN_FRAME_SIZE);

    lin_sim_init(TEST_NAD, LIN_BAUDRATE);

    uint8_t buf[64];
    int received = lin_sim_receive_response(buf, sizeof(buf), 1000u);

    TEST_ASSERT_EQUAL_INT(3, received);
    TEST_ASSERT_EQUAL_UINT8(0x50, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0x22, buf[1]);
    TEST_ASSERT_EQUAL_UINT8(0x01, buf[2]);
}

void test_receive_sf_zero_len(void)
{
    /* Inject SF with 0-length UDS payload (valid per ISO 17987-3) */
    uint8_t rx_frame[LIN_FRAME_SIZE] = {0};
    rx_frame[0] = TEST_NAD;
    rx_frame[1] = LIN_PCI_SF | 0;    /* SF, 0 UDS bytes */

    mock_uart_inject_rx_data(rx_frame, LIN_FRAME_SIZE);

    lin_sim_init(TEST_NAD, LIN_BAUDRATE);

    uint8_t buf[64];
    int received = lin_sim_receive_response(buf, sizeof(buf), 1000u);

    TEST_ASSERT_EQUAL_INT(0, received);
}

/* ======================================================================== *
 * lin_sim_receive_response — Multi-Frame (FF + CF)                         *
 * ======================================================================== */

void test_receive_multi_frame(void)
{
    /* Build a 10-byte response: first frame (FF) + consecutive frame (CF) */
    uint8_t expected[] = {0x50, 0x22, 0x01, 0x02, 0x03, 0x04,
                          0x05, 0x06, 0x07, 0x08};

    /* --- First Frame (FF) --- */
    uint8_t ff[LIN_FRAME_SIZE] = {0};
    ff[0] = TEST_NAD;
    ff[1] = LIN_PCI_FF | 0x00;           /* FF, high nibble of 0x000A */
    ff[2] = 0x0A;                        /* length low byte = 10 */
    ff[3] = 0x50;                        /* UDS bytes 0-4 */
    ff[4] = 0x22;
    ff[5] = 0x01;
    ff[6] = 0x02;
    ff[7] = 0x03;

    /* --- Consecutive Frame (CF) --- */
    uint8_t cf[LIN_FRAME_SIZE] = {0};
    cf[0] = TEST_NAD;
    cf[1] = LIN_PCI_CF | 1;              /* CF, seq = 1 */
    cf[2] = 0x04;                        /* UDS bytes 5-9 */
    cf[3] = 0x05;
    cf[4] = 0x06;
    cf[5] = 0x07;
    cf[6] = 0x08;

    /* Inject both frames */
    mock_uart_inject_rx_data(ff, LIN_FRAME_SIZE);
    mock_uart_inject_rx_data(cf, LIN_FRAME_SIZE);

    lin_sim_init(TEST_NAD, LIN_BAUDRATE);

    uint8_t buf[64];
    int received = lin_sim_receive_response(buf, sizeof(buf), 1000u);

    TEST_ASSERT_EQUAL_INT(10, received);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buf, 10);
}

/* ======================================================================== *
 * lin_sim_receive_response — Error cases                                   *
 * ======================================================================== */

void test_receive_timeout(void)
{
    /* No data injected — should time out */
    lin_sim_init(TEST_NAD, LIN_BAUDRATE);

    uint8_t buf[64];
    int received = lin_sim_receive_response(buf, sizeof(buf), 10u);

    TEST_ASSERT_EQUAL_INT(-1, received);
}

void test_receive_nad_mismatch(void)
{
    /* Inject frame with ALT_NAD — should be filtered out */
    uint8_t rx_frame[LIN_FRAME_SIZE] = {0};
    rx_frame[0] = ALT_NAD;                    /* NAD != TEST_NAD */
    rx_frame[1] = LIN_PCI_SF | 2;
    rx_frame[2] = 0x50;
    rx_frame[3] = 0x22;

    mock_uart_inject_rx_data(rx_frame, LIN_FRAME_SIZE);

    lin_sim_init(TEST_NAD, LIN_BAUDRATE);

    uint8_t buf[64];
    int received = lin_sim_receive_response(buf, sizeof(buf), 1000u);

    TEST_ASSERT_EQUAL_INT(-1, received);
}

void test_receive_buf_overflow(void)
{
    /* Inject a valid SF response with data larger than caller's buffer */
    uint8_t rx_frame[LIN_FRAME_SIZE] = {0};
    rx_frame[0] = TEST_NAD;
    rx_frame[1] = LIN_PCI_SF | 3;    /* 3 bytes of UDS data */
    rx_frame[2] = 0x50;
    rx_frame[3] = 0x22;
    rx_frame[4] = 0x01;

    mock_uart_inject_rx_data(rx_frame, LIN_FRAME_SIZE);

    lin_sim_init(TEST_NAD, LIN_BAUDRATE);

    /* Provide a buffer too small to hold 3 bytes */
    uint8_t tiny_buf[1];
    int received = lin_sim_receive_response(tiny_buf, 0u, 1000u);

    TEST_ASSERT_EQUAL_INT(-1, received);
}

void test_receive_not_init(void)
{
    /* Call receive without initialising */
    uint8_t buf[64];
    int received = lin_sim_receive_response(buf, sizeof(buf), 10u);
    TEST_ASSERT_EQUAL_INT(-1, received);
}

void test_receive_null_buf(void)
{
    lin_sim_init(TEST_NAD, LIN_BAUDRATE);
    int received = lin_sim_receive_response(NULL, 64u, 10u);
    TEST_ASSERT_EQUAL_INT(-1, received);
}

/* ======================================================================== *
 * lin_sim_calc_frame_time_ms                                               *
 * ======================================================================== */

void test_frame_time_calc(void)
{
    /* At 19200 baud, 8N1, 8 bytes:
     *   bits = 8 * 10 = 80
     *   time = 80 * 1000 / 19200 = 4.166... → rounded up to 5
     */
    uint32_t t = lin_sim_calc_frame_time_ms(19200u);
    TEST_ASSERT_EQUAL_UINT32(5u, t);

    /* At 9600 baud:
     *   time = 80 * 1000 / 9600 = 8.333... → rounded up to 9
     */
    t = lin_sim_calc_frame_time_ms(9600u);
    TEST_ASSERT_EQUAL_UINT32(9u, t);

    /* At 0 baud — should return 0 */
    t = lin_sim_calc_frame_time_ms(0u);
    TEST_ASSERT_EQUAL_UINT32(0u, t);
}

/* ======================================================================== *
 * Test runner                                                               *
 * ======================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* --- Init --- */
    RUN_TEST(test_init_default);
    RUN_TEST(test_init_zero_baud);

    /* --- Send SF --- */
    RUN_TEST(test_send_sf);
    RUN_TEST(test_send_sf_max_len);
    RUN_TEST(test_send_sf_min_len);

    /* --- Send multi-frame --- */
    RUN_TEST(test_send_multi_frame);
    RUN_TEST(test_send_multi_frame_three_frames);

    /* --- Send error cases --- */
    RUN_TEST(test_send_not_init);
    RUN_TEST(test_send_null_data);
    RUN_TEST(test_send_zero_len);

    /* --- Receive SF --- */
    RUN_TEST(test_receive_sf);
    RUN_TEST(test_receive_sf_zero_len);

    /* --- Receive multi-frame --- */
    RUN_TEST(test_receive_multi_frame);

    /* --- Receive error cases --- */
    RUN_TEST(test_receive_timeout);
    RUN_TEST(test_receive_nad_mismatch);
    RUN_TEST(test_receive_buf_overflow);
    RUN_TEST(test_receive_not_init);
    RUN_TEST(test_receive_null_buf);

    /* --- Utility --- */
    RUN_TEST(test_frame_time_calc);

    return UNITY_END();
}
