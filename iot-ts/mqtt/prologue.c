/** @file
 * @brief MQTT Group
 * Prepare the MQTT group; report whether an MQTT client is on the agent.
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "mqtt/prologue"

#include "te_config.h"
#include "tapi_test.h"

#include "rcf_rpc.h"
#include "tapi_job_factory_rpc.h"
#include "tapi_mqtt.h"

int
main(int argc, char **argv)
{
    rcf_rpc_server    *pco = NULL;
    tapi_job_factory_t *factory = NULL;

    TEST_START;

    TEST_STEP("Open an RPC server and a job factory on the agent");
    CHECK_RC(rcf_rpc_server_create("Agt_A", "pco_mqtt_prologue", &pco));
    CHECK_RC(tapi_job_factory_rpc_create(pco, &factory));

    TEST_STEP("Report whether an MQTT client is present (no SKIP here)");
    if (tapi_mqtt_available(factory, TAPI_MQTT_TIMEOUT_MS))
        RING("mosquitto client is present on the agent");
    else
        RING("no MQTT client on the agent; the pub/sub and audit tests "
             "will SKIP");

    TEST_SUCCESS;

cleanup:
    if (factory != NULL)
        tapi_job_factory_destroy(factory);
    if (pco != NULL)
        rcf_rpc_server_destroy(pco);
    TEST_END;
}
