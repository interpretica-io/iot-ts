/** @file
 * @brief Bluetooth Group
 * Finalize the Bluetooth/BLE group.
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */
#define TE_TEST_NAME    "ble/epilogue"
#include "te_config.h"
#include "tapi_test.h"
int
main(int argc, char **argv)
{
    TEST_START;
    TEST_STEP("Bluetooth group epilogue");
    TEST_SUCCESS;
cleanup:
    TEST_END;
}
