/** @file
 * @brief MDNS Group
 *
 * Exercise tsf-mdns against whatever the agent's link advertises. What
 * is out there depends on the LAN, so this asserts no service count: it
 * drives the raw probe, the avahi browse and the posture audit, checks
 * each call returns cleanly and the audit report is well-formed, and
 * logs what was seen. Zero services is a clean pass.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "mdns/discover"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_vector.h"

#include "tapi_cybersec.h"
#include "tapi_mdns.h"
#include "tapi_mdns_audit.h"
#include "tsapi_mdns.h"

int
main(int argc, char **argv)
{
    tsapi_mdns_session sess;
    te_vec services = TE_VEC_INIT(tapi_mdns_service);
    tapi_cybersec_report report;
    te_string detail = TE_STRING_INIT;
    const tapi_mdns_service *svc;
    bool report_ready = false;
    int responders = 0;
    int records = 0;
    te_errno rc;

    TEST_START;

    TEST_STEP("Open a session to the agent");
    CHECK_RC(tsapi_mdns_session_init(&sess, "pco_mdns_discover"));

    TEST_STEP("Raw mDNS probe (daemon-less) returns cleanly");
    /* The DNS-SD service-enumeration query; NULL asks for the default. */
    CHECK_RC(tapi_mdns_probe(sess.pco, NULL, 3, &responders, &records,
                             &detail));
    RING("raw mDNS probe: %d responder(s), %d record(s)\n%s",
         responders, records, detail.ptr != NULL ? detail.ptr : "");

    TEST_STEP("Browse advertised services (needs avahi-daemon)");
    /*
     * The daemon-less raw probe above already exercised mDNS. The avahi
     * browse and the posture audit both talk to avahi-daemon over D-Bus;
     * where it is not running (a stock container) there is nothing to
     * talk to, so skip cleanly rather than fail. Run natively with
     * avahi-daemon up to exercise browse and the audit.
     */
    rc = tapi_mdns_browse(sess.pco, 3, &services);
    if (rc != 0)
        TEST_SKIP("avahi-daemon not reachable on the agent; the raw mDNS "
                  "probe ran, browse and audit need the daemon");
    RING("browse found %u service(s)", (unsigned)te_vec_size(&services));
    TE_VEC_FOREACH(&services, svc)
        RING("  %s  %s  (%s)", svc->type, svc->name, svc->domain);

    TEST_STEP("Posture audit produces a well-formed report");
    tapi_cybersec_report_init(&report);
    report_ready = true;
    CHECK_RC(tapi_mdns_audit(sess.pco, NULL, &report));
    tapi_cybersec_report_log(&report);
    /*
     * The audit always adds either one mdns.service-advertised per
     * service or a single mdns.none, so an empty report means it did
     * not run. Do NOT fail on findings: what is advertised is the LAN's
     * business, not a defect of the agent.
     */
    if (tapi_cybersec_report_count(&report, TAPI_CYBERSEC_SEV_INFO) == 0)
        TEST_VERDICT("the mDNS audit produced no findings at all");

    TEST_SUCCESS;

cleanup:
    if (report_ready)
        tapi_cybersec_report_free(&report);
    tapi_mdns_services_free(&services);
    te_string_free(&detail);
    tsapi_mdns_session_fini(&sess);
    TEST_END;
}
