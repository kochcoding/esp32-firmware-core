#include <unity.h>
#include "test_api.h"

static void test_smoke_runs(void)
{
    TEST_ASSERT_TRUE(1);
}

void run_smoke_tests(void)
{
    RUN_TEST(test_smoke_runs);
}
