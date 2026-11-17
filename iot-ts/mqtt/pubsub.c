/** @file
 * @brief MQTT Group
 * Round-trip a sweep of payload sizes through a broker and graph the latency.
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

/*
 * The broker is the test's to supply: env TSF_MQTT_HOST (and optional
 * TSF_MQTT_PORT). With none configured, or with no MQTT client on the
 * agent, the test SKIPs - there is no broker on a stock host.
 *
 * MQTT delivers only to subscribers that were already connected, and the
 * publisher and subscriber here are two separate processes. So each point
 * is measured through a retained publish: the broker keeps the message and
 * hands it to the subscriber that connects just after, which is a real
 * publish->broker->receive round trip. The scratch topic is cleared at the
 * end. The curve of that round-trip latency against payload size is emitted
 * as an MI line-graph.
 */

#define TE_TEST_NAME    "mqtt/pubsub"

#include "te_config.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tapi_test.h"
#include "te_alloc.h"
#include "te_string.h"
#include "te_vector.h"
#include "te_mi_log.h"

#include "rcf_rpc.h"
#include "tapi_job_factory_rpc.h"
#include "tapi_mqtt.h"

/** Topic the installation does not use; cleared when the test ends. */
#define MQTT_SCRATCH_TOPIC  "tapi/iot-mqtt/pubsub"

/** Payload sizes to sweep, bytes. */
static const int payload_sizes[] = { 16, 64, 256, 1024, 4096, 16384 };

/** A monotonic timestamp in seconds. */
static double
now_sec(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int
main(int argc, char **argv)
{
    rcf_rpc_server     *pco = NULL;
    tapi_job_factory_t *factory = NULL;
    te_mi_logger       *logger = NULL;
    tapi_mqtt_conn      conn = tapi_mqtt_default_conn;
    char               *payload = NULL;
    size_t              max_size = 0;
    const char         *host = getenv("TSF_MQTT_HOST");
    const char         *port = getenv("TSF_MQTT_PORT");
    unsigned int        i;

    TEST_START;

    if (host == NULL || host[0] == '\0')
        TEST_SKIP("No broker: set TSF_MQTT_HOST (and optional TSF_MQTT_PORT)");

    conn.host = host;
    conn.port = (port != NULL && port[0] != '\0') ? (uint16_t)atoi(port) : 0;

    TEST_STEP("Open an RPC server and a job factory on the agent");
    CHECK_RC(rcf_rpc_server_create("Agt_A", "pco_mqtt_pubsub", &pco));
    CHECK_RC(tapi_job_factory_rpc_create(pco, &factory));

    TEST_STEP("SKIP cleanly when there is no MQTT client on the agent");
    if (!tapi_mqtt_available(factory, TAPI_MQTT_TIMEOUT_MS))
        TEST_SKIP("No MQTT client on the agent (mosquitto_pub absent)");

    RING("broker %s:%u", conn.host,
         (unsigned int)(conn.port != 0 ? conn.port : TAPI_MQTT_PORT));

    /* One buffer big enough for the largest payload; 'x' repeated. */
    for (i = 0; i < TE_ARRAY_LEN(payload_sizes); i++)
        if ((size_t)payload_sizes[i] > max_size)
            max_size = payload_sizes[i];
    payload = TE_ALLOC(max_size + 1);
    memset(payload, 'x', max_size);
    payload[max_size] = '\0';

    CHECK_RC(te_mi_logger_meas_create("mqtt", &logger));
    te_mi_logger_add_meas_key(logger, NULL, "broker", "%s", host);
    te_mi_logger_add_comment(logger, NULL, "mode", "retained round-trip");

    TEST_STEP("Round-trip each payload size and record the latency");
    for (i = 0; i < TE_ARRAY_LEN(payload_sizes); i++)
    {
        int                 size = payload_sizes[i];
        te_vec              messages;
        double              t0;
        double              rtt;
        char                saved;
        tapi_mqtt_result    result = TAPI_MQTT_REFUSED;
        te_errno            rc;

        /* Terminate the shared buffer at this size for this iteration. */
        saved = payload[size];
        payload[size] = '\0';

        t0 = now_sec();
        rc = tapi_mqtt_publish_retained(factory, &conn, MQTT_SCRATCH_TOPIC,
                                        payload, TAPI_MQTT_TIMEOUT_MS,
                                        &result);
        payload[size] = saved; /* restore for the next, larger size */
        if (rc != 0 || result != TAPI_MQTT_ACCEPTED)
        {
            RING("%d B: publish not accepted (%r, %s); point skipped", size,
                 rc, tapi_mqtt_result2str(result));
            continue;
        }

        rc = tapi_mqtt_collect(factory, &conn, MQTT_SCRATCH_TOPIC, 5, 1,
                               TAPI_MQTT_TIMEOUT_MS, &messages);
        rtt = now_sec() - t0;
        if (rc != 0)
        {
            RING("%d B: collect failed (%r); point skipped", size, rc);
            continue;
        }

        if (tapi_mqtt_message_find(&messages, MQTT_SCRATCH_TOPIC) == NULL)
        {
            RING("%d B: nothing came back; point skipped", size);
            tapi_mqtt_messages_free(&messages);
            continue;
        }
        tapi_mqtt_messages_free(&messages);

        RING("%d B round-trip: %.3f ms", size, rtt * 1e3);

        /* X: payload size. Y: the round-trip latency, in seconds. */
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "payload_bytes", TE_MI_MEAS_AGGR_SINGLE,
                              (double)size, TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_LATENCY,
                              "rtt", TE_MI_MEAS_AGGR_SINGLE,
                              rtt, TE_MI_MEAS_MULTIPLIER_PLAIN);
    }

    TEST_STEP("Emit the latency-vs-payload line graph");
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "rtt", "Round-trip latency vs. payload size");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "rtt",
                               TE_MI_GRAPH_AXIS_X, "payload_bytes");
    te_mi_logger_meas_graph_axis_add(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "rtt",
                               TE_MI_GRAPH_AXIS_Y, TE_MI_MEAS_LATENCY, "rtt");

    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_SUCCESS;

cleanup:
    /* Clear the retained scratch message (empty payload clears it). */
    if (factory != NULL && host != NULL && host[0] != '\0')
        (void)tapi_mqtt_publish_retained(factory, &conn, MQTT_SCRATCH_TOPIC,
                                         "", TAPI_MQTT_TIMEOUT_MS, NULL);
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    free(payload);
    if (factory != NULL)
        tapi_job_factory_destroy(factory);
    if (pco != NULL)
        rcf_rpc_server_destroy(pco);
    TEST_END;
}
