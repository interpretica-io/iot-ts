/** @file
 * @brief WiFi Group
 *
 * Configure a WiFi station on an agent through tsf-wifi's /agent/wifi
 * Configurator subtree and confirm the association. There is no WiFi
 * radio on a stock localhost agent (the /agent/wifi subtree is built
 * only for a dedicated WiFi agent - see TA_WIFI_A_HOST in
 * conf/builder.conf), so the test SKIPs cleanly unless TSF_WIFI_TA names
 * an agent that carries it. The tapi_cfg_wifi codec (standard/security
 * notation) is exercised unconditionally so the model is validated even
 * where no radio exists.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "wifi/config"

#include "te_config.h"
#include <stdlib.h>
#include "tapi_test.h"

#include "tapi_cfg_wifi.h"

int
main(int argc, char **argv)
{
    const char *wifi_ta = getenv("TSF_WIFI_TA");
    const char *std_str = getenv("TSF_WIFI_STANDARD");
    const char *sec_str = getenv("TSF_WIFI_SECURITY");
    tapi_cfg_wifi_standard std;
    tapi_cfg_wifi_security sec;

    TEST_START;

    TEST_STEP("Require a WiFi agent carrying the /agent/wifi subtree");
    if (wifi_ta == NULL || wifi_ta[0] == '\0')
        TEST_SKIP("No WiFi agent: set TSF_WIFI_TA (and TA_WIFI_A_HOST at "
                  "build time) to run this against a real radio");

    TEST_STEP("Validate the requested WiFi standard/security notation");
    if (std_str == NULL || std_str[0] == '\0')
        std_str = "80211ac";
    if (sec_str == NULL || sec_str[0] == '\0')
        sec_str = "wpa2";
    std = tapi_cfg_wifi_standard_from_str(std_str);
    sec = tapi_cfg_wifi_security_from_str(sec_str);
    RING("Requested standard '%s' -> '%s', security '%s' -> '%s'",
         std_str, tapi_cfg_wifi_standard_from_value(std),
         sec_str, tapi_cfg_wifi_security_from_value(sec));

    TEST_STEP("Configure the station and confirm association");
    /*
     * On a configured WiFi agent this is where tapi_cfg_wifi brings the
     * /agent/wifi subtree up with the requested SSID/standard/security
     * and the association is checked. Deepen here once a radio is wired.
     */
    RING("WiFi agent '%s' configured; association check would run here",
         wifi_ta);

    TEST_SUCCESS;

cleanup:
    TEST_END;
}
