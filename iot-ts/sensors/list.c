/** @file
 * @brief Sensors Group
 *
 * Snapshot the agent's hardware-monitoring readings and check them.
 * How many sensors there are is the host's business, so this does not
 * assert a count: a host with no hwmon sensors is a clean pass (the RPC
 * and libsensors path still ran). Where readings exist, each must parse
 * and no temperature may sit at or above its own critical limit.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "sensors/list"

#include "te_config.h"
#include "tapi_test.h"
#include "te_vector.h"

#include "tapi_sensors.h"
#include "tsapi_sensors.h"

int
main(int argc, char **argv)
{
    tsapi_sensors_session sess;
    te_vec readings = TE_VEC_INIT(tapi_sensors_reading);
    const tapi_sensors_reading *r;
    unsigned int n;

    TEST_START;

    TEST_STEP("Open a session to the agent");
    CHECK_RC(tsapi_sensors_session_init(&sess, "pco_sensors_list"));

    TEST_STEP("Snapshot the hardware-monitoring readings");
    CHECK_RC(tapi_sensors_list(sess.pco, &readings));
    n = te_vec_size(&readings);
    RING("the agent reports %u sensor reading(s)", n);

    if (n == 0)
    {
        TEST_STEP("No hwmon sensors present - nothing more to check");
        TEST_SUCCESS;
    }

    TEST_STEP("Every reading is well-formed; no temperature is over critical");
    TE_VEC_FOREACH(&readings, r)
    {
        tapi_sensors_log(r);

        if (r->kind == NULL || r->label == NULL)
            TEST_VERDICT("a reading has no kind/label");

        /* A temperature at or above its own critical limit is a real
         * fault the host is reporting - fail on it. */
        if (r->kind != NULL && strcmp(r->kind, "temp") == 0 &&
            r->has_value && r->has_crit && r->value >= r->crit)
        {
            TEST_VERDICT("%s is at/above critical (%.1f >= %.1f C)",
                         r->label, r->value, r->crit);
        }
    }

    TEST_SUCCESS;

cleanup:
    tapi_sensors_list_free(&readings);
    tsapi_sensors_session_fini(&sess);
    TEST_END;
}
