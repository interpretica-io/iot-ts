/** @file
 * @brief MQTT Group
 *
 * The two-agent IoT topology: Agt_A is the gateway and stands up the MQTT
 * broker; Agt_B is a fleet of IoT device-clients that publish telemetry to
 * it. The test sweeps the fleet size, drives every device's publish from
 * Agt_B, confirms the gateway received every device's telemetry, and graphs
 * the publish throughput and the ingest time against the number of devices.
 *
 * It needs an MQTT broker and clients on the agents (mosquitto); where they
 * are absent it SKIPs cleanly. On localhost both agents run on the engine
 * host and meet over the loopback.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "mqtt/fleet"

#include "te_config.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tapi_test.h"
#include "te_string.h"
#include "te_vector.h"
#include "te_mi_log.h"

#include "rcf_rpc.h"
#include "tapi_job_factory_rpc.h"
#include "tapi_mqtt.h"
#include "tapi_mqtt_broker.h"

/** The broker Agt_A listens on; Agt_B's devices connect here over loopback. */
#define FLEET_BROKER_PORT   18831

/** Where the gateway is, as seen from the device agent. */
#define FLEET_BROKER_HOST   "127.0.0.1"

/** Topic prefix each emulated device publishes its telemetry under. */
#define FLEET_TOPIC_PREFIX  "fleet/telemetry/dev-"

/** Wildcard the gateway subscribes to collect the whole fleet. */
#define FLEET_TOPIC_WILD    "fleet/telemetry/#"

/** Fleet sizes to sweep (ascending; retained topics accumulate). */
static const int fleet_sizes[] = { 1, 4, 10, 25 };

/** The largest fleet, for the retained-topic cleanup at the end. */
#define FLEET_MAX           25

/** A monotonic timestamp in seconds. */
static double
now_sec(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/** Publish one device's retained telemetry; @c true when the broker took it. */
static bool
device_publish(tapi_job_factory_t *fleet, const tapi_mqtt_conn *conn, int dev)
{
    char topic[64];
    char payload[64];
    tapi_mqtt_result result = TAPI_MQTT_REFUSED;
    te_errno rc;

    TE_SPRINTF(topic, "%s%d", FLEET_TOPIC_PREFIX, dev);
    TE_SPRINTF(payload, "{\"dev\":%d,\"temp\":%d.5,\"seq\":1}", dev, 20 + dev % 10);

    rc = tapi_mqtt_publish_retained(fleet, conn, topic, payload,
                                    TAPI_MQTT_TIMEOUT_MS, &result);
    return rc == 0 && result == TAPI_MQTT_ACCEPTED;
}

int
main(int argc, char **argv)
{
    rcf_rpc_server     *pco_gw = NULL;
    rcf_rpc_server     *pco_fleet = NULL;
    tapi_job_factory_t *gw = NULL;
    tapi_job_factory_t *fleet = NULL;
    tapi_mqtt_broker   *broker = NULL;
    tapi_mqtt_broker_opt bopt = tapi_mqtt_broker_default_opt;
    tapi_mqtt_conn      conn = tapi_mqtt_default_conn;
    te_mi_logger       *logger = NULL;
    unsigned int        i;
    int                 dev;

    TEST_START;

    TEST_STEP("Open an RPC server and job factory on each agent");
    CHECK_RC(rcf_rpc_server_create("Agt_A", "pco_mqtt_gw", &pco_gw));
    CHECK_RC(tapi_job_factory_rpc_create(pco_gw, &gw));
    CHECK_RC(rcf_rpc_server_create("Agt_B", "pco_mqtt_fleet", &pco_fleet));
    CHECK_RC(tapi_job_factory_rpc_create(pco_fleet, &fleet));

    TEST_STEP("SKIP unless both agents have an MQTT client");
    if (!tapi_mqtt_available(gw, TAPI_MQTT_TIMEOUT_MS) ||
        !tapi_mqtt_available(fleet, TAPI_MQTT_TIMEOUT_MS))
    {
        TEST_SKIP("No MQTT client on an agent (mosquitto_pub/sub absent)");
    }

    TEST_STEP("Stand the gateway broker up on Agt_A (anonymous for the fleet)");
    bopt.port = FLEET_BROKER_PORT;
    bopt.defects = TAPI_MQTT_DEFECT_ANONYMOUS;
    if (tapi_mqtt_broker_start(gw, &bopt, TAPI_MQTT_TIMEOUT_MS, &broker) != 0)
        TEST_SKIP("Could not start a mosquitto broker on Agt_A");

    conn.host = FLEET_BROKER_HOST;
    conn.port = FLEET_BROKER_PORT;
    conn.username = NULL;   /* anonymous: the fleet carries no credentials */
    conn.password = NULL;
    RING("gateway broker up on %s:%u; driving the fleet from Agt_B",
         conn.host, (unsigned int)conn.port);

    CHECK_RC(te_mi_logger_meas_create("mqtt-fleet", &logger));
    te_mi_logger_add_comment(logger, NULL, "topology", "Agt_A broker, Agt_B devices");

    TEST_STEP("Sweep the fleet size: publish from Agt_B, collect on the gateway");
    for (i = 0; i < TE_ARRAY_LEN(fleet_sizes); i++)
    {
        int     n = fleet_sizes[i];
        double   t0;
        double   ingest;
        double   rate;
        unsigned got;
        te_vec   messages;

        t0 = now_sec();
        for (dev = 0; dev < n; dev++)
        {
            if (!device_publish(fleet, &conn, dev))
                TEST_VERDICT("device %d could not publish to the gateway", dev);
        }
        ingest = now_sec() - t0;

        /* The gateway side collects every retained device topic. */
        CHECK_RC(tapi_mqtt_collect(gw, &conn, FLEET_TOPIC_WILD, n, n,
                                   TAPI_MQTT_TIMEOUT_MS, &messages));
        got = (unsigned)te_vec_size(&messages);
        tapi_mqtt_messages_free(&messages);

        if ((int)got < n)
            TEST_VERDICT("the gateway received %u of %d device telemetry "
                         "messages - the fleet lost data", got, n);

        rate = (ingest > 0.0) ? (double)n / ingest : 0.0;
        RING("fleet of %d: all %u telemetry messages reached the gateway; "
             "%.1f publishes/s, %.3f s ingest", n, got, rate, ingest);

        /* X: fleet size. Y: publish throughput and total ingest time. */
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "devices", TE_MI_MEAS_AGGR_SINGLE,
                              (double)n, TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_RPS,
                              "publish-rate", TE_MI_MEAS_AGGR_SINGLE,
                              rate, TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_TIME,
                              "ingest", TE_MI_MEAS_AGGR_SINGLE,
                              ingest, TE_MI_MEAS_MULTIPLIER_PLAIN);
    }

    TEST_STEP("Emit the fleet graph: throughput and ingest vs fleet size");
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "fleet", "MQTT fleet: throughput & ingest vs devices");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "fleet",
                               TE_MI_GRAPH_AXIS_X, "devices");
    te_mi_logger_meas_graph_axis_add(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "fleet",
                               TE_MI_GRAPH_AXIS_Y, TE_MI_MEAS_RPS, "publish-rate");
    te_mi_logger_meas_graph_axis_add(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "fleet",
                               TE_MI_GRAPH_AXIS_Y, TE_MI_MEAS_TIME, "ingest");
    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_SUCCESS;

cleanup:
    /* Clear every retained device topic the fleet left on the broker. */
    if (broker != NULL)
    {
        for (dev = 0; dev < FLEET_MAX; dev++)
        {
            char topic[64];

            TE_SPRINTF(topic, "%s%d", FLEET_TOPIC_PREFIX, dev);
            (void)tapi_mqtt_publish_retained(fleet, &conn, topic, "",
                                             TAPI_MQTT_TIMEOUT_MS, NULL);
        }
    }
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    if (broker != NULL)
        (void)tapi_mqtt_broker_stop(broker);
    if (gw != NULL)
        tapi_job_factory_destroy(gw);
    if (fleet != NULL)
        tapi_job_factory_destroy(fleet);
    if (pco_gw != NULL)
        rcf_rpc_server_destroy(pco_gw);
    if (pco_fleet != NULL)
        rcf_rpc_server_destroy(pco_fleet);
    TEST_END;
}
