/** @file
 * @brief MDNS Group
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */
#define TE_TEST_NAME    "mdns/prologue"
#include "te_config.h"
#include "tapi_test.h"
int
main(int argc, char **argv)
{
    TEST_START;
    TEST_STEP("mDNS group prologue");
    TEST_SUCCESS;
cleanup:
    TEST_END;
}
