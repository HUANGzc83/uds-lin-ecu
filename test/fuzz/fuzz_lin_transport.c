/*
 * fuzz_lin_transport.c
 * Standalone fuzz harness for LIN Transport encode/decode
 *
 * Fuzz targets:
 *   - lin_rx_decode()  — decode arbitrary 8-byte LIN frames
 *   - lin_tx_encode()  — encode arbitrary diagnostic PDUs
 *
 * Build: gcc -o fuzz_lin_transport fuzz_lin_transport.c -I../../inc -L../.. -luds-core
 * Usage:  for i in $(seq 10000); do
 *           echo $RANDOM | xxd > /tmp/fuzz_input.bin
 *           ./fuzz_lin_transport /tmp/fuzz_input.bin
 *         done
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "uds/uds_lin_transport.h"

/* ----------------------------------------------------------------------- *
 * LLVMFuzzerTestOneInput — called once per fuzz iteration                 *
 * ----------------------------------------------------------------------- */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    size_t offset = 0;

    /* =================================================================== *
     * 1. Fuzz lin_rx_decode — consume 8-byte LIN frames                   *
     *    Reset state before each frame to avoid cross-frame dependency    *
     *    that would make every frame after the first depend on the one    *
     *    before it.  (Multi-frame sequences are tested separately via     *
     *    a single FF→CF simulation below.)                                *
     * =================================================================== */
    while (offset + LIN_FRAME_SIZE <= size)
    {
        lin_transport_reset();
        /* Use first byte of the frame as the expected NAD — intentionally
         * random so some frames pass NAD check and some fail. */
        lin_transport_set_nad(data[offset]);

        lin_frame_t frame;
        memcpy(frame.data, data + offset, LIN_FRAME_SIZE);

        lin_diag_pdu_t pdu;
        (void)lin_rx_decode(&frame, &pdu);

        offset += LIN_FRAME_SIZE;
    }

    /* =================================================================== *
     * 2. Fuzz multi-frame FF→CF sequence                                  *
     *    If >= 16 bytes remain, simulate a First Frame + one Consecutive  *
     *    Frame to exercise the multi-frame reassembly path.               *
     * =================================================================== */
    if (offset + 16 <= size)
    {
        lin_transport_reset();
        lin_transport_set_nad(data[offset]);

        /* First Frame */
        lin_frame_t ff;
        memcpy(ff.data, data + offset, LIN_FRAME_SIZE);
        lin_diag_pdu_t pdu;
        (void)lin_rx_decode(&ff, &pdu);

        /* Consecutive Frame */
        lin_frame_t cf;
        memcpy(cf.data, data + offset + 8, LIN_FRAME_SIZE);
        (void)lin_rx_decode(&cf, &pdu);

        /* One more CF (seq_num should be advanced) */
        memcpy(cf.data, data + offset + 16, (size - offset - 16 < LIN_FRAME_SIZE)
                   ? (size - offset - 16) : LIN_FRAME_SIZE);
        (void)lin_rx_decode(&cf, &pdu);
    }

    /* =================================================================== *
     * 3. Fuzz lin_tx_encode — encode random PDUs                          *
     * =================================================================== */
    {
        lin_diag_pdu_t pdu;
        lin_frame_t    frames[32];
        uint8_t        frame_count = 0;

        /* Use remaining bytes (or restart from 0 if consumed entirely) */
        size_t tx_off = (offset < size) ? offset : 0;
        size_t tx_avail = size - tx_off;

        if (tx_avail > 0)
        {
            pdu.nad = data[tx_off];

            uint16_t payload_len = (tx_avail > 1) ? (uint16_t)(tx_avail - 1) : 0;
            /* Bound to keep things reasonable and avoid overflow */
            if (payload_len > 512)
                payload_len = 512;

            uint8_t payload[512];
            if (payload_len > 0)
            {
                size_t to_copy = (payload_len < (tx_avail - 1))
                                     ? payload_len
                                     : (tx_avail - 1);
                memcpy(payload, data + tx_off + 1, to_copy);
            }
            pdu.uds_data = (payload_len > 0) ? payload : NULL;
            pdu.data_len = payload_len;

            /* Normal encode */
            memset(frames, 0, sizeof(frames));
            (void)lin_tx_encode(&pdu, frames, &frame_count, 32);

            /* Boundary: max_frames = 0 */
            (void)lin_tx_encode(&pdu, frames, &frame_count, 0);

            /* Boundary: max_frames = 1 */
            (void)lin_tx_encode(&pdu, frames, &frame_count, 1);

            /* Boundary: zero-length PDU */
            pdu.data_len = 0;
            pdu.uds_data = NULL;
            (void)lin_tx_encode(&pdu, frames, &frame_count, 32);

            /* NULL-pointer safety */
            (void)lin_tx_encode(NULL, frames, &frame_count, 32);
            (void)lin_tx_encode(&pdu, NULL, &frame_count, 32);
            (void)lin_tx_encode(&pdu, frames, NULL, 32);
        }

        /* Purely NULL tests even with no data */
        (void)lin_tx_encode(NULL, frames, &frame_count, 32);
    }

    /* =================================================================== *
     * 4. Fuzz utility functions                                            *
     * =================================================================== */
    {
        /* lin_rx_decode with NULL inputs */
        lin_diag_pdu_t pdu;
        (void)lin_rx_decode(NULL, &pdu);
        (void)lin_rx_decode((const lin_frame_t *)data, NULL);
        (void)lin_rx_decode(NULL, NULL);
    }

    return 0;  /* Non-zero signals a crash — always return 0 */
}

/* ----------------------------------------------------------------------- *
 * main — standalone driver: reads a file and feeds it to the fuzz target *
 * ----------------------------------------------------------------------- */
int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    if (f == NULL)
    {
        fprintf(stderr, "Error: cannot open '%s'\n", argv[1]);
        return 1;
    }

    uint8_t buf[65536];
    size_t  nread = fread(buf, 1, sizeof(buf), f);
    fclose(f);

    return LLVMFuzzerTestOneInput(buf, nread);
}
