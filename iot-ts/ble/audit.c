/** @file
 * @brief Bluetooth Group
 * Read the agent's Bluetooth security posture and gate on it.
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "ble/audit"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"

#include "rcf_rpc.h"
#include "tapi_job_factory_rpc.h"

#include "tapi_cybersec.h"
#include "tapi_bt.h"
#include "tapi_bt_audit.h"

int
main(int argc, char **argv)
{
    rcf_rpc_server     *pco = NULL;
    tapi_job_factory_t *factory = NULL;
    tapi_bt_backend     backend = TAPI_BT_AUTO;
    tapi_cybersec_report report;
    te_string           verdict = TE_STRING_INIT;
    bool                report_ready = false;

    TEST_START;

    TEST_STEP("Create an RPC job factory on the agent");
    CHECK_RC(rcf_rpc_server_create("Agt_A", "pco_bt_audit", &pco));
    CHECK_RC(tapi_job_factory_rpc_create(pco, &factory));

    TEST_STEP("Skip cleanly when the agent has no Bluetooth adapter");
    if (!tapi_bt_available(factory, TAPI_BT_AUTO, TAPI_BT_TIMEOUT_MS))
        TEST_SKIP("No Bluetooth adapter on the agent");

    TEST_STEP("Learn which backend the agent has");
    CHECK_RC(tapi_bt_detect(factory, TAPI_BT_TIMEOUT_MS, &backend));
    RING("Bluetooth backend: %s", tapi_bt_backend2str(backend));

    TEST_STEP("Skip when the backend cannot read the adapter posture");
    if (!tapi_bt_supports(backend, TAPI_BT_FEAT_ADAPTER))
        TEST_SKIP("Backend %s exposes no adapter to audit",
                  tapi_bt_backend2str(backend));

    TEST_STEP("Read the Bluetooth posture into a report (default policy)");
    tapi_cybersec_report_init(&report);
    report_ready = true;
    CHECK_RC(tapi_bt_audit(factory, backend, NULL, TAPI_BT_TIMEOUT_MS,
                           &report));
    tapi_cybersec_report_log(&report);

    TEST_STEP("The report is well-formed: at least one finding");
    if (tapi_cybersec_report_count(&report, TAPI_CYBERSEC_SEV_INFO) == 0)
        TEST_VERDICT("the Bluetooth audit produced no findings at all");

    TEST_STEP("Gate: fail on a HIGH finding (exposure that should not be)");
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
    tapi_job_factory_destroy(factory);
    TEST_END;
}
