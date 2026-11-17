/** @file
 * @brief CoAP Group
 *
 * Read a CoAP endpoint's posture with tapi_coap_audit() and gate on it.
 * The endpoint is the test's to supply (env TSF_COAP_URI); with none
 * configured the test skips. It checks the audit runs and produces a
 * well-formed report, and fails only on a finding at least HIGH (an
 * unauthenticated write would be one - and that probe is off by default,
 * so a clean endpoint passes). Findings depend on the endpoint, so none
 * is asserted specifically.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "coap/audit"

#include "te_config.h"
#include <stdlib.h>
#include "tapi_test.h"
#include "te_string.h"

#include "tapi_cybersec.h"
#include "tapi_coap_audit.h"
#include "tsapi_coap.h"

int
main(int argc, char **argv)
{
    tsapi_coap_session sess = {0};
    tapi_cybersec_report report;
    te_string verdict = TE_STRING_INIT;
    const char *uri;
    bool report_ready = false;

    TEST_START;

    uri = getenv("TSF_COAP_URI");
    if (uri == NULL || uri[0] == '\0')
        TEST_SKIP("Set TSF_COAP_URI (e.g. coap://host) to point at an endpoint");

    TEST_STEP("Open a session to the agent");
    CHECK_RC(tsapi_coap_session_init(&sess, "pco_coap_audit"));

    TEST_STEP("Read the CoAP posture of %s into a report", uri);
    tapi_cybersec_report_init(&report);
    report_ready = true;
    /* Default policy: the write probe stays OFF (it changes the device). */
    CHECK_RC(tapi_coap_audit(sess.pco, uri, NULL, &report));
    tapi_cybersec_report_log(&report);

    TEST_STEP("The report is well-formed (at least one finding)");
    if (tapi_cybersec_report_count(&report, TAPI_CYBERSEC_SEV_INFO) == 0)
        TEST_VERDICT("the CoAP audit produced no findings at all");

    TEST_STEP("Gate: fail on anything at least HIGH");
    if (tapi_cybersec_report_verdict(&report, TAPI_CYBERSEC_SEV_HIGH,
                                     &verdict))
    {
        TEST_VERDICT("%s", verdict.ptr);
    }

    TEST_SUCCESS;

cleanup:
    te_string_free(&verdict);
    if (report_ready)
        tapi_cybersec_report_free(&report);
    tsapi_coap_session_fini(&sess);
    TEST_END;
}
