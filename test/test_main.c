/**
 * @file test_main.c
 * @brief Native test entry point — runs all Unity test suites.
 */

//------------------------------------------------------------------------------
// includes
//------------------------------------------------------------------------------

#include "test_api.h"

#include <unity.h>

//------------------------------------------------------------------------------
// functions (implementation)
//------------------------------------------------------------------------------

/** @brief Unity required setup hook — unused. */
void setUp(void) {}

/** @brief Unity required teardown hook — unused. */
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();

    /* domain/locations_model */
    run_test_domain_locations_model_add();
    run_test_domain_locations_model_remove();
    run_test_domain_locations_model_get_active();
    run_test_domain_locations_model_invariants();

    /* storage/locations_storage */
    run_test_storage_locations_storage_from_json();
    run_test_storage_locations_storage_to_json_and_measure_json();

    /* storage/settings_storage */
    run_test_storage_settings_storage_wifi_from_json();
    run_test_storage_settings_storage_wifi_to_json_and_measure_json();

    /* storage/weather_storage */
    run_test_storage_weather_storage_validate_json();
    run_test_storage_weather_storage_compact_json_and_measure_json();

    return UNITY_END();
}
