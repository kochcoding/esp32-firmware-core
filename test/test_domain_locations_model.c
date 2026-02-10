#include <unity.h>
#include <string.h>

#include "test_api.h"

#include "locations_model.h"

static locations_model_t model;

static void reset_model(void)
{
    model.count = 0U;
    (void)memset(model.items, 0, sizeof(model.items));
}

static void test_add_location_success(void)
{
    location_t loc = {
        .name = "Berlin",
        .latitude = 52.52,
        .longitude = 13.405,
        .is_active = false};

    bool result = locations_model_add(&model, &loc);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT32(1U, model.count);
    TEST_ASSERT_EQUAL_STRING("Berlin", model.items[0].name);
}

static void test_add_location_duplicate_fails(void)
{
    location_t loc = {
        .name = "Berlin",
        .latitude = 52.52,
        .longitude = 13.405,
        .is_active = false};

    TEST_ASSERT_TRUE(locations_model_add(&model, &loc));
    TEST_ASSERT_FALSE(locations_model_add(&model, &loc));
    TEST_ASSERT_EQUAL_UINT32(1, model.count);
}

void run_domain_locations_model_tests(void)
{
    reset_model();
    RUN_TEST(test_add_location_success);

    reset_model();
    RUN_TEST(test_add_location_duplicate_fails);
}