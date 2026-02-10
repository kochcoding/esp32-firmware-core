#include <unity.h>

#include "test_api.h"

void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();

    run_test_domain_locations_model_add();

    return UNITY_END();
}
