/*
 * fuzz_uds_parser.c
 * Standalone fuzz harness for UDS PDU Parser/Serializer
 *
 * Fuzz targets:
 *   - uds_parse_request()         — parse arbitrary byte sequences
 *   - uds_serialize_response()    — serialize random response structs
 *   - uds_serialize_negative_response() — serialize NRC with random params
 *
 * Build: gcc -o fuzz_uds_parser fuzz_uds_parser.c -I../../inc -L../.. -luds-core
 * Usage:  for i in $(seq 10000); do
 *           echo $RANDOM | xxd > /tmp/fuzz_input.bin
 *           ./fuzz_uds_parser /tmp/fuzz_input.bin
 *         done
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "uds/uds_core.h"

/* ----------------------------------------------------------------------- *
 * LLVMFuzzerTestOneInput — called once per fuzz iteration                 *
 * ----------------------------------------------------------------------- */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* =================================================================== *
     * 1. Fuzz uds_parse_request                                           *
     *    Try every length from 0 to min(size, 64) to hit boundary cases. *
     * =================================================================== */
    {
        uint16_t max_len = (size < 64) ? (uint16_t)size : 64;
        for (uint16_t len = 0; len <= max_len; len++)
        {
            uds_request_t req;
            (void)uds_parse_request((const uint8_t *)data, len, &req);
        }

        /* NULL / edge-case inputs */
        uds_request_t req;
        (void)uds_parse_request(NULL, 0, &req);
        (void)uds_parse_request(NULL, 1, &req);
        (void)uds_parse_request(NULL, 65535, &req);
    }

    /* =================================================================== *
     * 2. Fuzz uds_serialize_response                                      *
     *    Craft a response struct from random bytes and try to serialize.  *
     * =================================================================== */
    {
        uds_response_t rsp;
        memset(&rsp, 0, sizeof(rsp));

        rsp.sid          = (size > 0) ? data[0] : 0;
        rsp.subfunc_echo = (size > 1) ? data[1] : 0;

        /* Bound data_len to prevent internal uint16 overflow (unlikely
         * in practice, keep fuzz conservative) and buf overruns. */
        uint16_t avail = (size > 2) ? (uint16_t)(size - 2) : 0;
        rsp.data_len   = (avail > 1024) ? 1024 : avail;
        rsp.data       = (rsp.data_len > 0) ? (const uint8_t *)(data + 2) : NULL;

        /* Normal serialization */
        uint8_t  buf[2048];
        uint16_t len = sizeof(buf);
        (void)uds_serialize_response(&rsp, buf, &len);

        /* Tiny buffer (capacity < needed) */
        len = 1;
        (void)uds_serialize_response(&rsp, buf, &len);
        len = 2;
        (void)uds_serialize_response(&rsp, buf, &len);

        /* NULL-pointer safety checks */
        (void)uds_serialize_response(NULL, buf, &len);
        (void)uds_serialize_response(&rsp, NULL, &len);

        /* NULL data pointer (explicit) */
        rsp.data     = NULL;
        rsp.data_len = 0;
        len = sizeof(buf);
        (void)uds_serialize_response(&rsp, buf, &len);
    }

    /* =================================================================== *
     * 3. Fuzz uds_serialize_negative_response                             *
     *    [0x7F][SID][NRC] — try many SID/NRC combinations.              *
     * =================================================================== */
    {
        uint8_t   sid = (size > 0) ? data[0] : 0;
        uds_nrc_t nrc = (uds_nrc_t)((size > 1) ? data[1] : 0);

        uint8_t  nrc_buf[4];
        uint16_t len;

        /* Normal path */
        len = sizeof(nrc_buf);
        (void)uds_serialize_negative_response(sid, nrc, nrc_buf, &len);

        /* Buffer too small for 3-byte NRC PDU */
        len = 2;
        (void)uds_serialize_negative_response(sid, nrc, nrc_buf, &len);
        len = 1;
        (void)uds_serialize_negative_response(sid, nrc, nrc_buf, &len);
        len = 0;
        (void)uds_serialize_negative_response(sid, nrc, nrc_buf, &len);

        /* NULL-pointer safety checks */
        (void)uds_serialize_negative_response(sid, nrc, NULL, &len);
        (void)uds_serialize_negative_response(sid, nrc, nrc_buf, NULL);

        /* Exhaustive NRC range test */
        for (int nrc_val = 0; nrc_val <= 255; nrc_val++)
        {
            len = sizeof(nrc_buf);
            (void)uds_serialize_negative_response(sid, (uds_nrc_t)nrc_val, nrc_buf, &len);
        }
    }

    /* =================================================================== *
     * 4. Fuzz helper functions                                             *
     * =================================================================== */
    {
        /* uds_is_positive_response */
        (void)uds_is_positive_response(NULL);
        if (size > 0)
            (void)uds_is_positive_response((const uint8_t *)data);

        /* uds_sid_to_response_sid */
        for (size_t i = 0; i < size && i < 256; i++)
            (void)uds_sid_to_response_sid(data[i]);
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
