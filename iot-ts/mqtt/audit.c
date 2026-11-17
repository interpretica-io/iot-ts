/** @file
 * @brief MQTT Group
 * Find out what a broker lets a stranger do, and gate on the posture.
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

/*
 * The broker is the test's to supply: env TSF_MQTT_HOST (and optional
 * TSF_MQTT_PORT). With none configured, or with no MQTT client on the
 * agent, the test SKIPs.
 *
 * tapi_mqtt_audit() connects as a stranger (no credentials) and asks what
 * the broker would allow: anonymous access, subscribing to everything,
 * plaintext (no TLS), and whether $SYS / retained messages are readable.
 * The default policy expects none of that, so a finding means the broker
 * permits something it should not. The audit only reads - no scratch topic
 * is named, so nothing is published. The gate fails on a finding at least
 * HIGH; the exact findings depend on the broker, so none is asserted.
 */

#define TE_TEST_NAME    "mqtt/audit"

#include "te_config.h"
#include <stdlib.h>

#include "tapi_test.h"
#include "te_string.h"

#include "rcf_rpc.h"
#include "tapi_job_factory_rpc.h"
#include "tapi_cybersec.h"
#include "tapi_mqtt.h"
#include "tapi_mqtt_audit.h"

int
main(int argc, char **argv)
{
    rcf_rpc_server     *pco = NULL;
    tapi_job_factory_t *factory = NULL;
    tapi_mqtt_conn      conn = tapi_mqtt_default_conn;
    tapi_mqtt_policy    policy = tapi_mqtt_default_policy;
    tapi_cybersec_report report;
    te_string           verdict = TE_STRING_INIT;
    bool                report_ready = false;
    const char         *host = getenv("TSF_MQTT_HOST");
    const char         *port = getenv("TSF_MQTT_PORT");

    TEST_START;

    if (host == NULL || host[0] == '\0')
        TEST_SKIP("No broker: set TSF_MQTT_HOST (and optional TSF_MQTT_PORT)");

    TEST_STEP("Open an RPC server and a job factory on the agent");
    CHECK_RC(rcf_rpc_server_create("Agt_A", "pco_mqtt_audit", &pco));
    CHECK_RC(tapi_job_factory_rpc_create(pco, &factory));

    TEST_STEP("SKIP cleanly when there is no MQTT client on the agent");
    if (!tapi_mqtt_available(factory, TAPI_MQTT_TIMEOUT_MS))
        TEST_SKIP("No MQTT client on the agent (mosquitto_pub absent)");

    conn.host = host;
    conn.port = (port != NULL && port[0] != '\0') ? (uint16_t)atoi(port) : 0;
    RING("auditing broker %s:%u as a stranger", conn.host,
         (unsigned int)(conn.port != 0 ? conn.port : TAPI_MQTT_PORT));

    TEST_STEP("Read the broker posture into a report (read-only, no publish)");
    tapi_cybersec_report_init(&report);
    report_ready = true;
    /* Default policy: anonymous/wildcard/plaintext/$SYS all disallowed, and
     * scratch_topic NULL so the audit never writes. */
    CHECK_RC(tapi_mqtt_audit(factory, &conn, &policy, TAPI_MQTT_TIMEOUT_MS,
                             &report));
    tapi_cybersec_report_log(&report);

    TEST_STEP("The report is well-formed (at least one finding)");
    if (tapi_cybersec_report_count(&report, TAPI_CYBERSEC_SEV_INFO) == 0)
        TEST_VERDICT("the MQTT audit produced no findings at all");

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
    if (factory != NULL)
        tapi_job_factory_destroy(factory);
    if (pco != NULL)
        rcf_rpc_server_destroy(pco);
    TEST_END;
}
