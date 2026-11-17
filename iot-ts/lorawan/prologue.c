/** @file
 * @brief LoRaWAN Group
 * Prepare the LoRaWAN frame-codec group.
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "lorawan/prologue"

#include "te_config.h"
#include "tapi_test.h"

int
main(int argc, char **argv)
{
    TEST_START;
    TEST_STEP("LoRaWAN group prologue");
    TEST_SUCCESS;
cleanup:
    TEST_END;
}
