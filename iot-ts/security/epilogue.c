/** @file
 * @brief Security Group
 * Security group epilogue.
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "security/epilogue"

#include "te_config.h"
#include "tapi_test.h"
#include "tsapi_evo.h"

int
main(int argc, char **argv)
{
    TEST_START;

    tsapi_evo_analysis_hint("Security group epilogue.");

    TEST_STEP("Finalize the security group");
    /* Nothing to undo: every test cleaned up its own session. */

    TEST_SUCCESS;

cleanup:

    TEST_END;
}
