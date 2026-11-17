/** @file
 * @brief IEEE 802.15.4 Group
 * Plant a frame-counter reuse across a capture and assert the tapi_802154
 * audit reports it as ieee802154.counter-reuse.
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

/*
 * A known-bad control that runs on a plain localhost: no radio is
 * needed, the frames are crafted in-process. The counter is what makes
 * the AES-CCM* nonce unique, so two secured frames from one device that
 * carry the same counter were encrypted with the same keystream - the
 * classic reuse. We craft exactly that, round-trip the frames through
 * the codec so the audit sees real parsed frames, run tapi_802154_audit
 * over them, and assert it finds the planted defect. A clean capture is
 * audited too, to show the detector does not cry wolf.
 */

#define TE_TEST_NAME    "ieee802154/audit"

#include "te_config.h"

#include <string.h>

#include "tapi_test.h"
#include "te_string.h"
#include "te_vector.h"

#include "tapi_cybersec.h"
#include "tapi_802154.h"
#include "tapi_802154_audit.h"

/** The sender whose counter we abuse. */
#define AUDIT_SRC_ADDR 0x00124B0001020304ULL
/** The finding the audit must raise. */
#define AUDIT_CHECK_REUSE "ieee802154.counter-reuse"

/** Build one secured ENC_MIC128 frame and parse it back into @p out. */
static void
audit_make_frame(uint32_t counter, tapi_802154_frame *out)
{
    tapi_802154_frame f = tapi_802154_default_frame;
    uint8_t raw[TAPI_802154_MAX_FRAME];
    size_t raw_len = 0;
    te_string wire = TE_STRING_INIT;
    static const uint8_t body[] = { 0xDE, 0xAD, 0xBE, 0xEF };

    f.type = TAPI_802154_DATA;
    f.seq = (uint8_t)counter;
    f.dest_mode = TAPI_802154_ADDR_SHORT;
    f.dest_panid = 0x1234;
    f.dest_addr = 0x0001;
    f.src_mode = TAPI_802154_ADDR_EXTENDED;
    f.panid_compression = true;
    f.src_addr = AUDIT_SRC_ADDR;
    f.secured = true;
    f.sec_level = TAPI_802154_SEC_ENC_MIC128;
    f.frame_counter = counter;
    f.key_index = 1;

    CHECK_RC(tapi_802154_build(&f, body, sizeof(body), &wire));
    CHECK_RC(tapi_802154_unhex(wire.ptr, raw, sizeof(raw), &raw_len));
    CHECK_RC(tapi_802154_parse(raw, raw_len, out));
    te_string_free(&wire);
}

/** Does @p report hold a finding whose check id is @p check? */
static bool
audit_has_finding(const tapi_cybersec_report *report, const char *check)
{
    const tapi_cybersec_finding *f;

    TE_VEC_FOREACH(&report->findings, f)
    {
        if (f->check != NULL && strcmp(f->check, check) == 0)
            return true;
    }

    return false;
}

int
main(int argc, char **argv)
{
    tapi_cybersec_report report;
    te_string verdict = TE_STRING_INIT;
    tapi_802154_frame frames[3];
    bool report_ready = false;

    TEST_START;

    TEST_STEP("Craft a capture that reuses a frame counter (1, 2, 2)");
    audit_make_frame(1, &frames[0]);
    audit_make_frame(2, &frames[1]);
    audit_make_frame(2, &frames[2]); /* the planted reuse */
    tapi_802154_frame_log("reused", &frames[2]);

    TEST_STEP("Run the audit over the crafted capture");
    tapi_cybersec_report_init(&report);
    report_ready = true;
    tapi_802154_audit(frames, TE_ARRAY_LEN(frames), NULL, &report);
    tapi_cybersec_report_log(&report);

    TEST_STEP("The audit reports the planted " AUDIT_CHECK_REUSE);
    if (!audit_has_finding(&report, AUDIT_CHECK_REUSE))
        TEST_VERDICT("the audit missed the planted counter reuse");
    if (tapi_cybersec_report_worst(&report) != TAPI_CYBERSEC_SEV_CRITICAL)
        TEST_VERDICT("counter reuse was not rated CRITICAL");
    if (!tapi_cybersec_report_verdict(&report, TAPI_CYBERSEC_SEV_CRITICAL,
                                      &verdict))
        TEST_VERDICT("the report did not yield a CRITICAL verdict");
    RING("verdict: %s", verdict.ptr);

    tapi_cybersec_report_free(&report);
    report_ready = false;

    TEST_STEP("A clean capture (1, 2, 3) raises no reuse finding");
    audit_make_frame(1, &frames[0]);
    audit_make_frame(2, &frames[1]);
    audit_make_frame(3, &frames[2]);
    tapi_cybersec_report_init(&report);
    report_ready = true;
    tapi_802154_audit(frames, TE_ARRAY_LEN(frames), NULL, &report);
    tapi_cybersec_report_log(&report);
    if (audit_has_finding(&report, AUDIT_CHECK_REUSE))
        TEST_VERDICT("the audit cried reuse over a clean capture");

    TEST_SUCCESS;

cleanup:
    te_string_free(&verdict);
    if (report_ready)
        tapi_cybersec_report_free(&report);
    TEST_END;
}
