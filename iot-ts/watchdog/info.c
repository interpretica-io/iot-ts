/** @file
 * @brief Watchdog Group
 *
 * Read the agent's hardware watchdog identity and status over the safe
 * info path, which leaves the device disarmed. What the host has is its
 * own business, so this asserts no particular watchdog: a host with no
 * /dev/watchdog (or no permission to read it) is a clean pass, and a
 * host that has one must report a non-empty identity.
 *
 * SAFETY: this test calls ONLY tapi_watchdog_info_get(). It must NEVER
 * set a timeout, pet, or otherwise arm the watchdog - arming it could
 * reboot the agent host.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "watchdog/info"

#include "te_config.h"
#include "tapi_test.h"
#include "te_errno.h"

#include "tapi_watchdog.h"
#include "tsapi_watchdog.h"

int
main(int argc, char **argv)
{
    tsapi_watchdog_session sess;
    tapi_watchdog_info info;
    bool info_ready = false;

    TEST_START;

    TEST_STEP("Open a session to the agent");
    CHECK_RC(tsapi_watchdog_session_init(&sess, "pco_watchdog_info"));

    TEST_STEP("Read the watchdog identity and status (safe, no arming)");
    rc = tapi_watchdog_info_get(sess.pco, NULL, &info);
    if (rc != 0)
    {
        /* No device, or no permission to read it: nothing to assess. */
        if (TE_RC_GET_ERROR(rc) == TE_ENOENT ||
            TE_RC_GET_ERROR(rc) == TE_ENODEV ||
            TE_RC_GET_ERROR(rc) == TE_EACCES)
        {
            TEST_SKIP("No readable /dev/watchdog on the agent (%r)", rc);
        }
        TEST_VERDICT("reading the watchdog failed: %r", rc);
    }
    info_ready = true;

    tapi_watchdog_info_log(&info);

    TEST_STEP("A present watchdog reports a non-empty identity");
    if (info.identity == NULL || info.identity[0] == '\0')
        TEST_VERDICT("the watchdog reported no identity string");

    RING("watchdog '%s': options [%s], timeout %d s, time left %d s, "
         "boot status %d, magic-close %s",
         info.identity, info.options_str != NULL ? info.options_str : "",
         info.timeout, info.timeleft, info.bootstatus,
         info.magic_close ? "yes" : "no");

    TEST_SUCCESS;

cleanup:
    if (info_ready)
        tapi_watchdog_info_free(&info);
    tsapi_watchdog_session_fini(&sess);
    TEST_END;
}
