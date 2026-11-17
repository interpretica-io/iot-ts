/** @file
 * @brief Security Group
 * Run under a clock set far in the future and prove the fake clock took
 * effect, so an expiry/rollover probe cannot pass for the wrong reason.
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "security/clock"

#include "te_config.h"
#include "tapi_test.h"
#include "te_mi_log.h"

#include "tapi_time.h"
#include "tsapi_cybersec.h"
#include "tsapi_evo.h"

/** How far ahead to push the clock: over a year, into expiry territory. */
#define CLOCK_OFFSET        "+400d"
/** Seconds that offset is worth. */
#define CLOCK_OFFSET_S      ((int64_t)400 * 24 * 3600)
/** Least skew we will believe means the shim really loaded. */
#define CLOCK_MIN_SKEW_S    ((int64_t)390 * 24 * 3600)

int
main(int argc, char **argv)
{
    tsapi_cybersec_session sess;
    tapi_time_opt opt = tapi_time_default_opt;
    te_mi_logger *logger = NULL;
    int64_t real_epoch = 0;
    int64_t skew = 0;
    bool have_sess = false;

    TEST_START;

    tsapi_evo_analysis_hint("A fake-clock probe. libfaketime is an LD_PRELOAD "
                            "shim that there are several ordinary ways to not "
                            "load - a static binary, a missing library - and "
                            "in every one the program sees the real time and "
                            "the test passes for the wrong reason. The skew "
                            "check is the point.");

    TEST_STEP("Open a session to the agent");
    CHECK_RC(tsapi_cybersec_session_init(&sess, "pco_sec_clock"));
    have_sess = true;

    TEST_STEP("Skip unless the agent can run under a fake clock");
    if (!tapi_time_available(sess.factory, 10000))
        TEST_SKIP("libfaketime is not on the agent; cannot fake the clock");

    opt.offset = CLOCK_OFFSET;

    TEST_STEP("Prove the fake clock takes effect");
    CHECK_RC(tapi_time_now(sess.factory, 10000, &real_epoch));
    /*
     * tapi_time_check() runs 'date' under the environment the options
     * describe and returns how far its clock actually moved. A shim that
     * silently failed to load reports a skew near zero, which is exactly
     * the pass-for-the-wrong-reason this test exists to catch.
     */
    CHECK_RC(tapi_time_check(sess.factory, &opt, 10000, &skew));
    RING("asked the clock for %s (%lld s); it moved %lld s",
         CLOCK_OFFSET, (long long)CLOCK_OFFSET_S, (long long)skew);

    if (skew < CLOCK_MIN_SKEW_S)
        TEST_VERDICT("the fake clock did not take effect: the shim was "
                     "found but the observed skew is too small");

    TEST_STEP("Report the observed vs real time and chart the offset");
    RING("real time %lld; apparent time under the fake clock %lld",
         (long long)real_epoch, (long long)(real_epoch + skew));

    CHECK_RC(te_mi_logger_meas_create("libfaketime", &logger));
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_TIME, "clock_offset",
                          TE_MI_MEAS_AGGR_SINGLE, (double)skew,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "offset", "Apparent clock offset");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "offset",
                               TE_MI_GRAPH_AXIS_X, TE_MI_GRAPH_AUTO_SEQNO);
    te_mi_logger_meas_graph_axis_add(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "offset",
                               TE_MI_GRAPH_AXIS_Y, TE_MI_MEAS_TIME,
                               "clock_offset");
    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_SUCCESS;

cleanup:

    if (logger != NULL)
        te_mi_logger_destroy(logger);
    if (have_sess)
        tsapi_cybersec_session_fini(&sess);

    TEST_END;
}
