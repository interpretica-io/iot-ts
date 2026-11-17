/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Suite helpers
 *
 * @author Maxim Menshikov <maxim.menshikov@interpretica.io>
 */

#define TE_LGR_USER "TSAPI COAP"

#include "te_config.h"

#include <string.h>

#include "logger_api.h"
#include "te_alloc.h"

#include "tsapi_coap.h"

/* See description in tsapi_coap.h */
te_errno
tsapi_coap_session_init(tsapi_coap_session *session, const char *name)
{
    te_errno rc;

    memset(session, 0, sizeof(*session));
    session->ta = TSAPI_COAP_TA;

    rc = rcf_rpc_server_create(session->ta, name, &session->pco);
    if (rc != 0)
    {
        ERROR("Cannot create the RPC server '%s' on %s: %r", name,
              session->ta, rc);
        return rc;
    }

    return 0;
}

/* See description in tsapi_coap.h */
void
tsapi_coap_session_fini(tsapi_coap_session *session)
{
    if (session->pco != NULL)
    {
        rcf_rpc_server_destroy(session->pco);
        session->pco = NULL;
    }
}
