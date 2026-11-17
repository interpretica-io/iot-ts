/** @file
 * @brief Security Group
 * Security group prologue: report what cross-cutting tools the agent has.
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "security/prologue"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_time.h"
#include "tsapi_cybersec.h"
#include "tsapi_evo.h"

int
main(int argc, char **argv)
{
    tsapi_cybersec_session sess;
    bool have_sess = false;

    TEST_START;

    tsapi_evo_analysis_hint("Security group prologue: the hardening layer "
                            "fuzzes a parser and probes a fake clock. Each "
                            "test opens its own session and skips cleanly "
                            "when its tool is absent.");

    TEST_STEP("Open a session and report the tools the group may use");
    /*
     * The group needs nothing of its own - every test makes and cleans up
     * its own session. This prologue only writes down, once, what the
     * agent can and cannot do, so a run that is mostly SKIPs is easy to
     * read later.
     */
    if (tsapi_cybersec_session_init(&sess, "pco_sec_prologue") == 0)
    {
        have_sess = true;
        RING("clang for a sanitized fuzz target: %s",
             tsapi_cybersec_have(&sess, "clang") ? "present" : "absent");
        RING("libfaketime for a fake clock: %s",
             tapi_time_available(sess.factory, 10000) ? "present" : "absent");
    }
    else
    {
        RING("no session to the agent; every security test will skip");
    }

    TEST_SUCCESS;

cleanup:

    if (have_sess)
        tsapi_cybersec_session_fini(&sess);

    TEST_END;
}
