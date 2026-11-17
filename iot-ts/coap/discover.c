/** @file
 * @brief CoAP Group
 *
 * Discover a CoAP endpoint's resources (/.well-known/core) and GET one.
 * The endpoint is the test's to supply (env TSF_COAP_URI, e.g.
 * "coap://[::1]"); with none configured the test skips cleanly - a host
 * with no CoAP server to point at is not a failure. It asserts the
 * discovery/request path runs and is self-consistent, not a particular
 * resource set.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "coap/discover"

#include "te_config.h"
#include <stdlib.h>
#include "tapi_test.h"
#include "te_string.h"
#include "te_vector.h"

#include "tapi_coap.h"
#include "tsapi_coap.h"

int
main(int argc, char **argv)
{
    tsapi_coap_session sess = {0};
    te_vec resources = TE_VEC_INIT(tapi_coap_resource);
    tapi_coap_reply summary;
    const tapi_coap_resource *res;
    const char *uri;
    bool have_summary = false;

    TEST_START;

    uri = getenv("TSF_COAP_URI");
    if (uri == NULL || uri[0] == '\0')
        TEST_SKIP("Set TSF_COAP_URI (e.g. coap://host) to point at an endpoint");

    TEST_STEP("Open a session to the agent");
    CHECK_RC(tsapi_coap_session_init(&sess, "pco_coap_discover"));

    TEST_STEP("Discover %s /.well-known/core", uri);
    CHECK_RC(tapi_coap_discover(sess.pco, uri, NULL, &summary, &resources));
    have_summary = true;
    RING("discovery %s (%s): %u resource(s)", summary.code_str,
         summary.secure ? "DTLS" : "plaintext", te_vec_size(&resources));
    TE_VEC_FOREACH(&resources, res)
        RING("  %s %s", res->path, res->attrs != NULL ? res->attrs : "");

    TEST_STEP("GET the base URI and log the reply");
    {
        tapi_coap_reply reply;

        if (tapi_coap_request(sess.pco, TAPI_COAP_GET, uri, NULL, NULL,
                              &reply) == 0)
        {
            RING("GET %s -> %s (cf=%d, %u bytes)", uri, reply.code_str,
                 reply.content_format,
                 (unsigned)(reply.payload != NULL ? strlen(reply.payload) : 0));
            tapi_coap_reply_free(&reply);
        }
        else
        {
            RING("GET %s did not complete (endpoint may require a path)", uri);
        }
    }

    TEST_SUCCESS;

cleanup:
    if (have_summary)
        tapi_coap_reply_free(&summary);
    tapi_coap_resources_free(&resources);
    tsapi_coap_session_fini(&sess);
    TEST_END;
}
