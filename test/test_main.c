#include <unity.h>

#include "test_api.h"

void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();

    run_test_domain_locations_model_add();
    run_test_domain_locations_model_remove();
    run_test_domain_locations_model_get_active();

    return UNITY_END();
}
