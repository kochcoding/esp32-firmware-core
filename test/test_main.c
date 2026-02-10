#include <unity.h>

#include "test_api.h"

void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();

    run_domain_locations_model_tests();

    return UNITY_END();
}
