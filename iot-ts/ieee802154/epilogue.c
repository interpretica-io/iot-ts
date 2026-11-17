/** @file
 * @brief IEEE 802.15.4 Group
 * Finalize the IEEE 802.15.4 codec group.
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */
#define TE_TEST_NAME    "ieee802154/epilogue"
#include "te_config.h"
#include "tapi_test.h"
int
main(int argc, char **argv)
{
    TEST_START;
    TEST_STEP("IEEE 802.15.4 group epilogue");
    TEST_SUCCESS;
cleanup:
    TEST_END;
}
