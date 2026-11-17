/** @file
 * @brief LoRaWAN Group
 * Build, sign and encrypt a LoRaWAN frame, parse it back, verify the MIC
 * and decrypt the payload, and assert the round-trip is exact.
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "lorawan/frames"

#include "te_config.h"
#include <string.h>
#include <time.h>
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_lorawan.h"

/*
 * Pure engine-side codec: build()/parse()/verify()/crypt() all run in this
 * process over AES-128 and need no agent, no radio and no network. It runs
 * on a plain localhost.
 */

/** A fixed, well-known device address, in the order people write it. */
#define FRAMES_DEV_ADDR 0x26011F88u

/** How many build+parse iterations to time per payload size. */
#define FRAMES_ITERS    2000

/** The payload sizes swept; each must be <= TAPI_LORAWAN_MAX_PAYLOAD. */
static const size_t frames_sizes[] = { 1, 4, 16, 32, 64, 128, 222, 242 };

/** Monotonic clock in nanoseconds. */
static uint64_t
now_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/**
 * Build one frame, parse it back with the keys, verify the MIC and check
 * the decrypted payload matches. Fails the test on any mismatch.
 */
static void
roundtrip_one(const tapi_lorawan_keys *keys, tapi_lorawan_mtype mtype,
              bool downlink, uint32_t fcnt, const uint8_t *plain, size_t len)
{
    tapi_lorawan_frame frame = tapi_lorawan_default_frame;
    tapi_lorawan_frame parsed;
    te_string wire = TE_STRING_INIT;
    uint8_t raw[TAPI_LORAWAN_MAX_PAYLOAD + 16];
    size_t raw_len = 0;

    frame.mtype = mtype;
    frame.downlink = downlink;
    frame.dev_addr = FRAMES_DEV_ADDR;
    frame.fport = 1;
    frame.fcnt = fcnt;

    CHECK_RC(tapi_lorawan_build(&frame, plain, len, keys, &wire));

    /* build() emits the frame as a hex string; turn it back into bytes. */
    CHECK_RC(tapi_lorawan_unhex(wire.ptr, raw, sizeof(raw), &raw_len));

    if (!tapi_lorawan_verify(raw, raw_len, keys))
        TEST_VERDICT("the MIC does not verify on a frame we just built");

    CHECK_RC(tapi_lorawan_parse(raw, raw_len, keys, &parsed));

    if (parsed.payload_len != len)
        TEST_VERDICT("round-trip changed the payload length: built %zu, "
                     "parsed %zu", len, parsed.payload_len);
    if (len != 0 && memcmp(parsed.payload, plain, len) != 0)
        TEST_VERDICT("round-trip did not reproduce the %s payload",
                     downlink ? "downlink" : "uplink");
    if (parsed.fcnt != fcnt)
        TEST_VERDICT("round-trip changed the frame counter: %u -> %u",
                     fcnt, parsed.fcnt);

    te_string_free(&wire);
}

int
main(int argc, char **argv)
{
    tapi_lorawan_keys keys;
    te_mi_logger *logger = NULL;
    uint8_t plain[TAPI_LORAWAN_MAX_PAYLOAD];
    size_t i;

    TEST_START;

    TEST_STEP("Set up two session keys for the codec");
    CHECK_RC(tapi_lorawan_key_parse("2B7E151628AED2A6ABF7158809CF4F3C",
                                    keys.nwk_skey));
    CHECK_RC(tapi_lorawan_key_parse("000102030405060708090A0B0C0D0E0F",
                                    keys.app_skey));

    for (i = 0; i < sizeof(plain); i++)
        plain[i] = (uint8_t)(i * 7 + 1);

    TEST_STEP("Start an MI logger for the codec-time curve");
    CHECK_RC(te_mi_logger_meas_create("tapi_lorawan", &logger));
    te_mi_logger_add_comment(logger, NULL, "op", "build+parse+verify");

    TEST_STEP("Sweep uplink payload sizes: round-trip each and time it");
    for (i = 0; i < TE_ARRAY_LEN(frames_sizes); i++)
    {
        size_t   len = frames_sizes[i];
        uint64_t t0;
        uint64_t t1;
        double   us_per_op;
        unsigned int k;

        /* Correctness: one exact round-trip at this size. */
        roundtrip_one(&keys, TAPI_LORAWAN_UNCONFIRMED_UP, false,
                      10 + (uint32_t)i, plain, len);

        /* Timing: the same build+parse+verify over many iterations. */
        t0 = now_ns();
        for (k = 0; k < FRAMES_ITERS; k++)
        {
            tapi_lorawan_frame frame = tapi_lorawan_default_frame;
            tapi_lorawan_frame parsed;
            te_string wire = TE_STRING_INIT;
            uint8_t raw[TAPI_LORAWAN_MAX_PAYLOAD + 16];
            size_t raw_len = 0;

            frame.dev_addr = FRAMES_DEV_ADDR;
            frame.fport = 1;
            frame.fcnt = 10 + (uint32_t)i;
            CHECK_RC(tapi_lorawan_build(&frame, plain, len, &keys, &wire));
            CHECK_RC(tapi_lorawan_unhex(wire.ptr, raw, sizeof(raw), &raw_len));
            (void)tapi_lorawan_verify(raw, raw_len, &keys);
            CHECK_RC(tapi_lorawan_parse(raw, raw_len, &keys, &parsed));
            te_string_free(&wire);
        }
        t1 = now_ns();
        us_per_op = (double)(t1 - t0) / FRAMES_ITERS / 1000.0;
        RING("payload %3zu B: %.3f us per build+parse+verify", len, us_per_op);

        /* X: the independent parameter. */
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "payload", TE_MI_MEAS_AGGR_SINGLE,
                              (double)len, TE_MI_MEAS_MULTIPLIER_PLAIN);
        /* Y: the codec time, stored in seconds via the micro multiplier. */
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_LATENCY,
                              "codec-time", TE_MI_MEAS_AGGR_MEAN,
                              us_per_op, TE_MI_MEAS_MULTIPLIER_MICRO);
    }

    TEST_STEP("Emit the line-graph: codec time vs payload size");
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "roundtrip",
                               "LoRaWAN codec time vs. payload size");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "roundtrip",
                               TE_MI_GRAPH_AXIS_X, "payload");
    te_mi_logger_meas_graph_axis_add(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "roundtrip",
                               TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_LATENCY, "codec-time");
    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_STEP("Round-trip a downlink frame as well");
    {
        static const uint8_t down[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02 };

        roundtrip_one(&keys, TAPI_LORAWAN_UNCONFIRMED_DOWN, true, 42,
                      down, sizeof(down));
    }

    TEST_SUCCESS;

cleanup:
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    TEST_END;
}
