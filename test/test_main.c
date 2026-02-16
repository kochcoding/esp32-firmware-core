#include <unity.h>

#include "test_api.h"

void setUp(void) {}
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

    return UNITY_END();
}
