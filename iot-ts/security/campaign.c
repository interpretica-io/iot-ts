/** @file
 * @brief Security Group
 * Build a length-prefixed parser with a planted off-by-one, fuzz it, and
 * check the campaign both ran and found the defect.
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "security/campaign"

#include "te_config.h"
#include "tapi_test.h"
#include "tapi_file.h"
#include "te_string.h"
#include "te_vector.h"
#include "te_mi_log.h"

#include "tapi_cc.h"
#include "tapi_fuzz.h"
#include "tapi_fuzz_crash.h"
#include "tsapi_cybersec.h"
#include "tsapi_evo.h"

/** Seconds in one slice of the campaign. */
#define CAMPAIGN_SLICE_SECONDS  5
/** How many slices to run, charting progress after each. */
#define CAMPAIGN_SLICES         4

int
main(int argc, char **argv)
{
    tsapi_cybersec_session sess;
    tapi_cybersec_report report;
    tapi_fuzz_opt opt;
    tapi_fuzz_app *app = NULL;
    tapi_fuzz_stats stats;
    tapi_cc_opt cc_opt;
    tapi_cc_app *cc_app = NULL;
    te_mi_logger *logger = NULL;
    te_string scratch = TE_STRING_INIT;
    te_string source = TE_STRING_INIT;
    te_string target = TE_STRING_INIT;
    te_string corpus = TE_STRING_INIT;
    te_string artifacts = TE_STRING_INIT;
    const char *sources[1];
    const char *cflags[] = { "-g", "-fsanitize=fuzzer,address" };
    te_vec crashes;
    bool collected = false;
    bool reported = false;
    bool have_sess = false;
    int slice;

    TEST_START;

    tsapi_evo_analysis_hint("A campaign end to end: tapi_cc builds a "
                            "sanitized target, tapi_fuzz runs it in slices "
                            "so coverage is charted over time, and the crash "
                            "triage reduces what it found to one defect with "
                            "a stable signature.");

    TEST_STEP("Open a session to the agent");
    CHECK_RC(tsapi_cybersec_session_init(&sess, "pco_sec_campaign"));
    have_sess = true;

    TEST_STEP("Check the agent can build a fuzz target at all");
    if (!tsapi_cybersec_have(&sess, "clang"))
        TEST_SKIP("libFuzzer needs clang, and the agent has none");

    cc_opt = tapi_cc_default_opt;
    cc_opt.compiler = "clang";
    cc_opt.cflags = cflags;
    cc_opt.n_cflags = TE_ARRAY_LEN(cflags);

    rc = tapi_cc_check_snippet(sess.factory, &cc_opt,
                               TAPI_DEVTOOL_TIMEOUT_MS,
                               "int main(void) { return 0; }\n");
    if (rc != 0)
        TEST_SKIP("This clang cannot build with -fsanitize=fuzzer,address");

    TEST_STEP("Build a length-prefixed parser with a planted off-by-one");
    CHECK_RC(tsapi_cybersec_scratch(&sess, "security-fuzz", &scratch));
    te_string_append(&source, "%s/target.c", scratch.ptr);
    te_string_append(&target, "%s/target", scratch.ptr);
    te_string_append(&corpus, "%s/corpus", scratch.ptr);
    te_string_append(&artifacts, "%s/artifacts", scratch.ptr);

    CHECK_RC(tsapi_cybersec_mkdir(&sess, corpus.ptr));
    CHECK_RC(tsapi_cybersec_mkdir(&sess, artifacts.ptr));

    /*
     * A tiny frame parser: a two-byte magic ("Io"), then a one-byte
     * declared length, then the payload. The length guard uses <= where
     * it should use <, so a frame that declares exactly sizeof(frame)
     * writes its trailing NUL one byte past the end of the stack buffer.
     * Shallow enough to be found in seconds - the fuzzer only has to hit
     * the magic and the single boundary length - and deep enough that it
     * takes a fuzzer rather than luck.
     */
    CHECK_RC(tapi_file_create_ta(sess.ta, source.ptr,
        "#include <stdint.h>\n"
        "#include <stddef.h>\n"
        "int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)\n"
        "{\n"
        "    unsigned char frame[32];\n"
        "    size_t i, n;\n"
        "    if (size < 3)\n"
        "        return 0;\n"
        "    if (data[0] != 'I' || data[1] != 'o')\n"
        "        return 0;\n"
        "    n = data[2];\n"
        "    if (n <= sizeof(frame))\n"
        "    {\n"
        "        for (i = 0; i < n && 3 + i < size; i++)\n"
        "            frame[i] = data[3 + i];\n"
        "        frame[n] = 0;\n"
        "    }\n"
        "    return 0;\n"
        "}\n"));

    cc_opt.output = target.ptr;
    sources[0] = source.ptr;
    cc_opt.sources = sources;
    cc_opt.n_sources = 1;

    CHECK_RC(tapi_cc_do(sess.factory, &cc_opt, TAPI_DEVTOOL_TIMEOUT_MS,
                        &cc_app));

    TEST_STEP("Give the corpus a seed to start from");
    {
        te_string seed = TE_STRING_INIT;

        te_string_append(&seed, "%s/seed", corpus.ptr);
        /* The magic, so the fuzzer starts a mutation away from the guard. */
        CHECK_RC(tapi_file_create_ta(sess.ta, seed.ptr, "Io"));
        te_string_free(&seed);
    }

    TEST_STEP("Run the campaign in slices, charting coverage over time");
    CHECK_RC(te_mi_logger_meas_create("libFuzzer", &logger));
    te_mi_logger_add_meas_key(logger, NULL, "fuzzer", "%s",
                              tapi_fuzz_kind2str(TAPI_FUZZ_LIBFUZZER));

    opt = tapi_fuzz_default_opt;
    opt.kind = TAPI_FUZZ_LIBFUZZER;
    opt.target = target.ptr;
    opt.corpus_dir = corpus.ptr;
    opt.output_dir = artifacts.ptr;
    opt.max_total_time = CAMPAIGN_SLICE_SECONDS;
    opt.max_len = 64;

    memset(&stats, 0, sizeof(stats));

    for (slice = 1; slice <= CAMPAIGN_SLICES; slice++)
    {
        /*
         * libFuzzer treats the corpus directory as both input and output,
         * so each slice resumes where the last one left off: coverage and
         * corpus size accumulate across slices even though the per-slice
         * execution count restarts.
         */
        CHECK_RC(tapi_fuzz_do(sess.factory, &opt,
                              (CAMPAIGN_SLICE_SECONDS + 60) * 1000, &app));
        CHECK_RC(tapi_fuzz_get_stats(app, &stats));
        tapi_fuzz_stats_log(&stats);
        CHECK_RC(tapi_fuzz_destroy(app));
        app = NULL;

        if (stats.valid)
        {
            te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_TIME, "elapsed",
                                  TE_MI_MEAS_AGGR_SINGLE,
                                  (double)(slice * CAMPAIGN_SLICE_SECONDS),
                                  TE_MI_MEAS_MULTIPLIER_PLAIN);
            te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                                  "coverage", TE_MI_MEAS_AGGR_SINGLE,
                                  (double)stats.coverage,
                                  TE_MI_MEAS_MULTIPLIER_PLAIN);
            te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                                  "execs", TE_MI_MEAS_AGGR_SINGLE,
                                  (double)stats.execs,
                                  TE_MI_MEAS_MULTIPLIER_PLAIN);
        }
    }

    /* coverage(elapsed) and execs(elapsed): did the campaign make headway? */
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "progress", "Fuzz progress over time");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "progress",
                               TE_MI_GRAPH_AXIS_X, "elapsed");
    te_mi_logger_meas_graph_axis_add(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "progress",
                               TE_MI_GRAPH_AXIS_Y, TE_MI_MEAS_UNITLESS_VALUE,
                               "coverage");
    te_mi_logger_meas_graph_axis_add(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "progress",
                               TE_MI_GRAPH_AXIS_Y, TE_MI_MEAS_UNITLESS_VALUE,
                               "execs");
    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_STEP("Check the campaign actually fuzzed");
    tapi_cybersec_report_init(&report);
    reported = true;

    /*
     * The check that keeps this honest. A harness that dies on startup
     * finds no crashes, and a test that only looks for crashes would call
     * that a pass.
     */
    CHECK_RC(tapi_fuzz_check_progress(&stats, 1000, 1, &report));
    TSAPI_CYBERSEC_CHECK(&report, TAPI_CYBERSEC_SEV_HIGH);

    TEST_STEP("Collect and triage what it found");
    CHECK_RC(tapi_fuzz_crashes_collect(sess.factory, &opt, NULL, 30000,
                                       &crashes));
    collected = true;
    tapi_fuzz_crashes_log(&crashes);

    if (te_vec_size(&crashes) == 0)
        TEST_VERDICT("The campaign did not find the defect planted in the "
                     "target");

    TEST_STEP("Report the defects as findings");
    {
        tapi_cybersec_report crash_report;

        tapi_cybersec_report_init(&crash_report);
        CHECK_RC(tapi_fuzz_crashes_check(&crashes, &crash_report));
        tapi_cybersec_report_log(&crash_report);

        if (!tsapi_cybersec_report_has(&crash_report, "fuzz.crash"))
        {
            tapi_cybersec_report_free(&crash_report);
            TEST_VERDICT("The triage produced no finding for a crash it "
                         "collected");
        }
        tapi_cybersec_report_free(&crash_report);
    }

    TEST_SUCCESS;

cleanup:

    if (logger != NULL)
        te_mi_logger_destroy(logger);
    if (collected)
        tapi_fuzz_crashes_free(&crashes);
    if (reported)
        tapi_cybersec_report_free(&report);
    CLEANUP_CHECK_RC(tapi_fuzz_destroy(app));
    CLEANUP_CHECK_RC(tapi_cc_destroy(cc_app));
    if (have_sess)
        tsapi_cybersec_session_fini(&sess);
    te_string_free(&scratch);
    te_string_free(&source);
    te_string_free(&target);
    te_string_free(&corpus);
    te_string_free(&artifacts);

    TEST_END;
}
