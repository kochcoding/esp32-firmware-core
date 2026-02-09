#include <unity.h>

#include "test_smoke.c"

// forward declaration of your tests
void test_smoke_runs(void);

void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_smoke_runs);
    return UNITY_END();
}
