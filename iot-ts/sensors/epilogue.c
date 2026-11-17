/** @file
 * @brief Sensors Group
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */
#define TE_TEST_NAME    "sensors/epilogue"
#include "te_config.h"
#include "tapi_test.h"
int
main(int argc, char **argv)
{
    TEST_START;
    TEST_STEP("Sensors group epilogue");
    TEST_SUCCESS;
cleanup:
    TEST_END;
}
