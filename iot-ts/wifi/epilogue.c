/** @file
 * @brief WiFi Group
 * WiFi group epilogue.
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */
#define TE_TEST_NAME    "wifi/epilogue"
#include "te_config.h"
#include "tapi_test.h"
int
main(int argc, char **argv)
{
    TEST_START;
    TEST_STEP("WiFi group epilogue");
    TEST_SUCCESS;
cleanup:
    TEST_END;
}
