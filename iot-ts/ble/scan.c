/** @file
 * @brief Bluetooth Group
 * Run a BLE device-discovery scan and chart devices found vs scan window.
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "ble/scan"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_vector.h"
#include "te_mi_log.h"

#include "rcf_rpc.h"
#include "tapi_job_factory_rpc.h"

#include "tapi_bt.h"
#include "tapi_bt_device.h"

/*
 * The scan windows to sweep, in seconds. A longer inquiry finds more of
 * what is willing to be found, so devices-discovered as a function of
 * the window is the curve worth plotting.
 */
static const unsigned int scan_windows[] = { 2, 5, 10 };

int
main(int argc, char **argv)
{
    rcf_rpc_server    *pco = NULL;
    tapi_job_factory_t *factory = NULL;
    te_mi_logger      *logger = NULL;
    tapi_bt_backend    backend = TAPI_BT_AUTO;
    tapi_bt_adapter    adapter;
    bool               adapter_ready = false;
    unsigned int       i;

    TEST_START;

    TEST_STEP("Create an RPC job factory on the agent");
    CHECK_RC(rcf_rpc_server_create("Agt_A", "pco_bt_scan", &pco));
    CHECK_RC(tapi_job_factory_rpc_create(pco, &factory));

    TEST_STEP("Skip cleanly when the agent has no Bluetooth adapter");
    if (!tapi_bt_available(factory, TAPI_BT_AUTO, TAPI_BT_TIMEOUT_MS))
        TEST_SKIP("No Bluetooth adapter on the agent");

    TEST_STEP("Learn which backend the agent has");
    CHECK_RC(tapi_bt_detect(factory, TAPI_BT_TIMEOUT_MS, &backend));
    RING("Bluetooth backend: %s", tapi_bt_backend2str(backend));

    TEST_STEP("Skip when the backend cannot scan for devices");
    if (!tapi_bt_supports(backend, TAPI_BT_FEAT_SCAN))
        TEST_SKIP("Backend %s does not support device discovery",
                  tapi_bt_backend2str(backend));

    TEST_STEP("Enumerate the adapter");
    CHECK_RC(tapi_bt_adapter_get(factory, backend, TAPI_BT_TIMEOUT_MS,
                                 &adapter));
    adapter_ready = true;
    tapi_bt_adapter_log(&adapter);

    TEST_STEP("Sweep the scan window and count devices discovered");
    CHECK_RC(te_mi_logger_meas_create("tapi_bt", &logger));
    te_mi_logger_add_meas_key(logger, NULL, "backend", "%s",
                              tapi_bt_backend2str(backend));
    te_mi_logger_add_meas_key(logger, NULL, "adapter", "%s",
                              adapter.address);

    for (i = 0; i < TE_ARRAY_LEN(scan_windows); i++)
    {
        te_vec       devices = TE_VEC_INIT(tapi_bt_device);
        unsigned int window = scan_windows[i];
        unsigned int found;

        /* Give the job more wall time than the on-air window. */
        CHECK_RC(tapi_bt_scan(factory, backend, window,
                              (int)(window + 10) * 1000, &devices));
        found = (unsigned int)te_vec_size(&devices);
        RING("scan window %u s -> %u device(s)", window, found);
        tapi_bt_devices_log("in range", &devices);
        tapi_bt_devices_free(&devices);

        /* X: the independent parameter, the inquiry window in seconds. */
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_TIME, "window",
                              TE_MI_MEAS_AGGR_SINGLE, window,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        /* Y: how many devices answered within that window. */
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "devices", TE_MI_MEAS_AGGR_SINGLE, found,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
    }

    TEST_STEP("Emit a line-graph of devices discovered vs scan window");
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "scan", "Devices discovered vs. scan window");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "scan",
                               TE_MI_GRAPH_AXIS_X, "window");
    te_mi_logger_meas_graph_axis_add(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "scan",
                               TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "devices");

    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_SUCCESS;

cleanup:
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    if (adapter_ready)
        tapi_bt_adapter_free(&adapter);
    tapi_job_factory_destroy(factory);
    TEST_END;
}
