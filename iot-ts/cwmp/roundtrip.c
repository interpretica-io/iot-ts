/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Codec round-trip
 *
 * Exercises the CPE-direction codec with no network: the agent builds the
 * message a CPE sends (its Inform), and for each RPC an ACS can issue it
 * builds the request (the ACS half, already in the library), dispatches it
 * against a CPE model (the new half), and parses the response the CPE
 * produced (the ACS half again). Every new build and parse function is on
 * the path, and the oracle is that what goes in comes back out.
 */

#define TE_TEST_NAME    "cwmp/roundtrip"

#include "te_config.h"
#include <string.h>
#include "tapi_test.h"
#include "te_string.h"
#include "te_vector.h"

#include "tapi_cwmp.h"
#include "tapi_cwmp_msg.h"
#include "tapi_cwmp_cpe.h"

/** Find a parameter value by name in a vector of tapi_cwmp_param. */
static const char *
find_param(const te_vec *params, const char *name)
{
    const tapi_cwmp_param *p;

    TE_VEC_FOREACH(params, p)
    {
        if (strcmp(p->name, name) == 0)
            return p->value;
    }

    return NULL;
}

/** Dispatch one ACS request against @p cpe and parse the CPE's response. */
static void
dispatch_and_parse(tapi_cwmp_cpe *cpe, const char *request,
                   tapi_cwmp_rpc expect_rpc, tapi_cwmp_result *result)
{
    te_string out = TE_STRING_INIT;
    tapi_cwmp_rpc issued = TAPI_CWMP_RPC_UNKNOWN;
    bool is_fault = false;
    bool more;

    more = tapi_cwmp_cpe_dispatch(cpe, request, &out, &issued, &is_fault);
    if (!more)
        TEST_FAIL("the CPE did not answer a %s", tapi_cwmp_rpc2str(expect_rpc));
    if (issued != expect_rpc)
        TEST_VERDICT("dispatch named the request %s, expected %s",
                     tapi_cwmp_rpc2str(issued), tapi_cwmp_rpc2str(expect_rpc));
    if (is_fault)
        TEST_VERDICT("the CPE answered %s with a fault",
                     tapi_cwmp_rpc2str(expect_rpc));

    tapi_cwmp_result_init(result);
    CHECK_RC(tapi_cwmp_parse_response(out.ptr, expect_rpc, result));
    if (!result->ok)
        TEST_VERDICT("the CPE's %s response parsed as a fault",
                     tapi_cwmp_rpc2str(expect_rpc));
    te_string_free(&out);
}

int
main(int argc, char **argv)
{
    tapi_cwmp_cpe *cpe = NULL;
    tapi_cwmp_cpe_opts opts = tapi_cwmp_cpe_default_opts;
    const char *version = "9.9.9-codec";
    const char *name = "Device.DeviceInfo.SoftwareVersion";

    TEST_START;

    /* The ACS URL is only parsed here; no session is opened. */
    opts.acs_url = "http://127.0.0.1:7547/";
    CHECK_RC(tapi_cwmp_cpe_create(NULL, &opts, &cpe));
    tapi_cwmp_cpe_set_param(cpe, name, version, TAPI_CWMP_STRING, true);
    tapi_cwmp_cpe_set_param(cpe, "Device.DeviceInfo.Manufacturer",
                            "Interpretica", TAPI_CWMP_STRING, false);

    TEST_STEP("Inform round-trips: build it, parse it back");
    {
        te_string env = TE_STRING_INIT;
        tapi_cwmp_inform inform;
        tapi_cwmp_param ip = { .name = (char *)name, .value = (char *)version,
                               .type = TAPI_CWMP_STRING };
        static const char *events[] = { TAPI_CWMP_EVENT_BOOT };
        const char *got;

        tapi_cwmp_build_inform("CPE_1", &opts.device_id, events,
                               TE_ARRAY_LEN(events), &ip, 1, 0, NULL, &env);
        CHECK_RC(tapi_cwmp_parse_inform(env.ptr, &inform));

        if (strcmp(inform.device_id.serial_number,
                   opts.device_id.serial_number) != 0)
            TEST_VERDICT("Inform carried serial '%s'",
                         inform.device_id.serial_number);
        if (!tapi_cwmp_inform_has_event(&inform, TAPI_CWMP_EVENT_BOOT))
            TEST_VERDICT("Inform lost its BOOT event");
        got = find_param(&inform.params, name);
        if (got == NULL || strcmp(got, version) != 0)
            TEST_VERDICT("Inform parameter came back as '%s'",
                         got != NULL ? got : "(absent)");
        tapi_cwmp_inform_free(&inform);
        te_string_free(&env);
    }

    TEST_STEP("GetParameterValues: the CPE returns the model value");
    {
        te_string req = TE_STRING_INIT;
        tapi_cwmp_result r;
        const char *got;
        const char *names[] = { name };

        tapi_cwmp_build_gpv("ACS_1", names, TE_ARRAY_LEN(names), &req);
        dispatch_and_parse(cpe, req.ptr, TAPI_CWMP_RPC_GET_PARAMETER_VALUES,
                           &r);
        got = tapi_cwmp_result_value(&r, name);
        if (got == NULL || strcmp(got, version) != 0)
            TEST_VERDICT("GPV returned '%s', expected '%s'",
                         got != NULL ? got : "(absent)", version);
        tapi_cwmp_result_free(&r);
        te_string_free(&req);
    }

    TEST_STEP("SetParameterValues: the CPE applies it to the model");
    {
        te_string req = TE_STRING_INIT;
        tapi_cwmp_result r;
        const char *newv = "1.2.3-set";
        tapi_cwmp_param sp = { .name = (char *)name, .value = (char *)newv,
                               .type = TAPI_CWMP_STRING };
        const char *got;

        tapi_cwmp_build_spv("ACS_2", &sp, 1, "key-42", &req);
        dispatch_and_parse(cpe, req.ptr, TAPI_CWMP_RPC_SET_PARAMETER_VALUES,
                           &r);
        if (r.status != 0)
            TEST_VERDICT("SPV status was %d, expected 0", r.status);
        got = tapi_cwmp_cpe_value(cpe, name);
        if (got == NULL || strcmp(got, newv) != 0)
            TEST_VERDICT("SPV did not update the model (now '%s')",
                         got != NULL ? got : "(absent)");
        tapi_cwmp_result_free(&r);
        te_string_free(&req);
    }

    TEST_STEP("GetParameterNames: the CPE lists its model");
    {
        te_string req = TE_STRING_INIT;
        tapi_cwmp_result r;

        tapi_cwmp_build_gpn("ACS_3", "Device.", false, &req);
        dispatch_and_parse(cpe, req.ptr, TAPI_CWMP_RPC_GET_PARAMETER_NAMES, &r);
        if (find_param(&r.params, name) == NULL)
            TEST_VERDICT("GPN did not list %s", name);
        tapi_cwmp_result_free(&r);
        te_string_free(&req);
    }

    TEST_STEP("AddObject: the CPE creates an instance");
    {
        te_string req = TE_STRING_INIT;
        tapi_cwmp_result r;
        size_t before = tapi_cwmp_cpe_param_count(cpe);

        tapi_cwmp_build_add_object("ACS_4", "Device.Test.", "k", &req);
        dispatch_and_parse(cpe, req.ptr, TAPI_CWMP_RPC_ADD_OBJECT, &r);
        if (r.instance < 1)
            TEST_VERDICT("AddObject returned instance %d", r.instance);
        if (tapi_cwmp_cpe_param_count(cpe) <= before)
            TEST_VERDICT("AddObject did not grow the model");
        tapi_cwmp_result_free(&r);
        te_string_free(&req);
    }

    TEST_STEP("Reboot: the CPE acknowledges");
    {
        te_string req = TE_STRING_INIT;
        tapi_cwmp_result r;

        tapi_cwmp_build_reboot("ACS_5", "boot-key", &req);
        dispatch_and_parse(cpe, req.ptr, TAPI_CWMP_RPC_REBOOT, &r);
        tapi_cwmp_result_free(&r);
        te_string_free(&req);
    }

    TEST_SUCCESS;

cleanup:
    tapi_cwmp_cpe_destroy(cpe);
    TEST_END;
}
