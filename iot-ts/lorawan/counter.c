/** @file
 * @brief LoRaWAN Group
 * The counter-reuse audit: plant two uplinks that reuse a frame counter and
 * assert the audit reports lorawan.counter-reuse; a strictly-increasing pair
 * must stay clean.
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "lorawan/counter"

#include "te_config.h"
#include <string.h>
#include "tapi_test.h"
#include "te_string.h"
#include "te_vector.h"

#include "tapi_cybersec.h"
#include "tapi_lorawan.h"

/*
 * Pure known-bad control. LoRaWAN's whole replay defence is the frame
 * counter going up; a device that reuses or resets it lets anyone replay its
 * traffic and, because the keystream is derived from the counter, two frames
 * with the same counter are enciphered with the same keystream. There is no
 * audit function in tapi_lorawan, so the check runs here over the real
 * parse() primitive and records the finding in a tapi_cybersec_report.
 *
 * Engine-side only: build()/parse() run in this process, no agent, no radio.
 */

/** The stable identifier the audit flags a counter reuse with. */
#define COUNTER_CHECK   "lorawan.counter-reuse"

/** A fixed device address, in the order people write it. */
#define COUNTER_DEV_ADDR 0x26011F88u

/**
 * Audit a pair of uplinks for a counter that did not advance. The FCnt is
 * in the cleartext header, so this needs no keys. Appends a finding when the
 * same device sends a frame whose counter is not strictly greater than one
 * already seen.
 *
 * @return @c true when a reuse/reset was found and recorded.
 */
static bool
audit_counter_reuse(const tapi_lorawan_frame *a, const tapi_lorawan_frame *b,
                    tapi_cybersec_report *report)
{
    if (a->dev_addr == b->dev_addr && b->fcnt <= a->fcnt)
    {
        tapi_cybersec_report_add(report, TAPI_CYBERSEC_SEV_HIGH,
            COUNTER_CHECK, "DevAddr 0x26011F88 uplink stream",
            "frame counter did not advance: %u then %u - replayable, and the "
            "payload keystream repeats", a->fcnt, b->fcnt);
        return true;
    }
    return false;
}

/** Is there a finding with the given check id in the report? */
static bool
report_has_check(const tapi_cybersec_report *report, const char *check)
{
    const tapi_cybersec_finding *f;

    TE_VEC_FOREACH(&report->findings, f)
    {
        if (strcmp(f->check, check) == 0)
            return true;
    }
    return false;
}

/** Build an uplink carrying @p plain under @p keys at counter @p fcnt. */
static void
build_uplink(const tapi_lorawan_keys *keys, uint32_t fcnt,
             const uint8_t *plain, size_t len, te_string *wire)
{
    tapi_lorawan_frame frame = tapi_lorawan_default_frame;

    frame.mtype = TAPI_LORAWAN_UNCONFIRMED_UP;
    frame.dev_addr = COUNTER_DEV_ADDR;
    frame.fport = 1;
    frame.fcnt = fcnt;
    CHECK_RC(tapi_lorawan_build(&frame, plain, len, keys, wire));
}

int
main(int argc, char **argv)
{
    tapi_lorawan_keys keys;
    tapi_cybersec_report bad;
    tapi_cybersec_report good;
    bool bad_ready = false;
    bool good_ready = false;
    te_string wire_a = TE_STRING_INIT;
    te_string wire_b = TE_STRING_INIT;
    te_string wire_c = TE_STRING_INIT;
    tapi_lorawan_frame fa;
    tapi_lorawan_frame fb;
    tapi_lorawan_frame fc;
    /* Two different secrets of equal length, so the keystream reuse shows. */
    static const uint8_t p1[16] = "north-gate open ";
    static const uint8_t p2[16] = "south-gate shut!";

    TEST_START;

    TEST_STEP("Set up two session keys");
    CHECK_RC(tapi_lorawan_key_parse("2B7E151628AED2A6ABF7158809CF4F3C",
                                    keys.nwk_skey));
    CHECK_RC(tapi_lorawan_key_parse("000102030405060708090A0B0C0D0E0F",
                                    keys.app_skey));

    TEST_STEP("Plant the defect: two uplinks that reuse FCnt 7");
    build_uplink(&keys, 7, p1, sizeof(p1), &wire_a);
    build_uplink(&keys, 7, p2, sizeof(p2), &wire_b);
    /* A well-behaved third frame that advances the counter. */
    build_uplink(&keys, 8, p1, sizeof(p1), &wire_c);

    TEST_STEP("Parse the headers (no keys needed: FCnt is cleartext)");
    {
        /* build() emits hex strings; turn them back into bytes to parse. */
        uint8_t ra[TAPI_LORAWAN_MAX_PAYLOAD + 16];
        uint8_t rb[TAPI_LORAWAN_MAX_PAYLOAD + 16];
        uint8_t rcb[TAPI_LORAWAN_MAX_PAYLOAD + 16];
        size_t la = 0;
        size_t lb = 0;
        size_t lc = 0;

        CHECK_RC(tapi_lorawan_unhex(wire_a.ptr, ra, sizeof(ra), &la));
        CHECK_RC(tapi_lorawan_unhex(wire_b.ptr, rb, sizeof(rb), &lb));
        CHECK_RC(tapi_lorawan_unhex(wire_c.ptr, rcb, sizeof(rcb), &lc));
        CHECK_RC(tapi_lorawan_parse(ra, la, NULL, &fa));
        CHECK_RC(tapi_lorawan_parse(rb, lb, NULL, &fb));
        CHECK_RC(tapi_lorawan_parse(rcb, lc, NULL, &fc));
    }
    tapi_lorawan_frame_log("reuse A", &fa);
    tapi_lorawan_frame_log("reuse B", &fb);

    TEST_STEP("Show the keystream reuse the shared counter causes");
    if (fa.payload_len == fb.payload_len && fa.payload_len == sizeof(p1))
    {
        bool leaks = true;
        size_t i;

        /* Ciphertexts (keys were NULL, so payloads stayed encrypted):
         * C1 ^ C2 must equal P1 ^ P2 when the keystream repeats. */
        for (i = 0; i < sizeof(p1); i++)
        {
            if ((fa.payload[i] ^ fb.payload[i]) != (p1[i] ^ p2[i]))
            {
                leaks = false;
                break;
            }
        }
        RING("keystream reuse %s: C1^C2 %s P1^P2",
             leaks ? "confirmed" : "not observed",
             leaks ? "==" : "!=");
    }

    TEST_STEP("Audit the reused pair: it must flag " COUNTER_CHECK);
    tapi_cybersec_report_init(&bad);
    bad_ready = true;
    (void)audit_counter_reuse(&fa, &fb, &bad);
    tapi_cybersec_report_log(&bad);
    if (!report_has_check(&bad, COUNTER_CHECK))
        TEST_VERDICT("the counter audit missed the planted reuse of "
                     COUNTER_CHECK);
    RING("audit flagged %s as expected",  COUNTER_CHECK);

    TEST_STEP("Audit a well-behaved pair: it must stay clean");
    tapi_cybersec_report_init(&good);
    good_ready = true;
    (void)audit_counter_reuse(&fa, &fc, &good);
    tapi_cybersec_report_log(&good);
    if (report_has_check(&good, COUNTER_CHECK))
        TEST_VERDICT("the counter audit flagged a strictly-increasing pair");

    TEST_SUCCESS;

cleanup:
    te_string_free(&wire_a);
    te_string_free(&wire_b);
    te_string_free(&wire_c);
    if (bad_ready)
        tapi_cybersec_report_free(&bad);
    if (good_ready)
        tapi_cybersec_report_free(&good);
    TEST_END;
}
