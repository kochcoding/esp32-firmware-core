#include <unity.h>
#include <string.h>
#include <stdio.h>

#include "test_api.h"

#include "locations_model.h"

static locations_model_t model;

static void reset_model(void)
{
    model.count = 0U;
    (void)memset(model.items, 0, sizeof(model.items));
}

static void test_domain_locations_model_add_success(void)
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

static void test_domain_locations_model_add_duplicate_fails(void)
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

static void test_domain_locations_model_add_null_model_fails(void)
{
    location_t loc = {
        .name = "Berlin",
        .latitude = 52.52,
        .longitude = 13.405,
        .is_active = false};

    bool result = locations_model_add(NULL, &loc);

    TEST_ASSERT_FALSE(result);
}

static void test_domain_locations_model_add_null_location_fails(void)
{
    bool result = locations_model_add(&model, NULL);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_UINT32(0U, model.count);
}

static void test_domain_locations_model_add_when_full_fails(void)
{
    for (size_t i = 0U; i < (size_t)LOCATIONS_MODEL_MAX_NUMBER; i++)
    {
        location_t loc = {0};
        // create unique names: "L0", "L1", ...
        (void)snprintf(loc.name, sizeof(loc.name), "L%u", (unsigned)i);
        loc.latitude = (double)i;
        loc.longitude = (double)i;
        loc.is_active = false;

        TEST_ASSERT_TRUE(locations_model_add(&model, &loc));
    }

    TEST_ASSERT_EQUAL_UINT32((uint32_t)LOCATIONS_MODEL_MAX_NUMBER, (uint32_t)model.count);

    // one more must fails
    location_t extra = {
        .name = "Overflow",
        .latitude = 0.0,
        .longitude = 0.0,
        .is_active = false};

    bool result = locations_model_add(&model, &extra);
    TEST_ASSERT_FALSE(result);

    // count must not change
    TEST_ASSERT_EQUAL_UINT32((uint32_t)LOCATIONS_MODEL_MAX_NUMBER, (uint32_t)model.count);
}

static void test_domain_locations_model_add_duplicate_has_no_side_effects(void)
{
    location_t loc1 = {
        .name = "Berlin",
        .latitude = 52.52,
        .longitude = 13.405,
        .is_active = false};

    location_t loc2_same_name_different_data = {
        .name = "Berlin", /* same name => duplicate */
        .latitude = 1.0,  /* different values to detect overwrite */
        .longitude = 2.0,
        .is_active = true};

    TEST_ASSERT_TRUE(locations_model_add(&model, &loc1));
    TEST_ASSERT_EQUAL_UINT32(1U, model.count);

    // snapshot existing stored entry
    location_t snapshot = model.items[0];

    TEST_ASSERT_FALSE(locations_model_add(&model, &loc2_same_name_different_data));
    TEST_ASSERT_EQUAL_UINT32(1U, model.count);

    // ensure nothing got overwritten
    TEST_ASSERT_EQUAL_STRING(snapshot.name, model.items[0].name);
    TEST_ASSERT_EQUAL_FLOAT((float)snapshot.latitude, (float)model.items[0].latitude);
    TEST_ASSERT_EQUAL_FLOAT((float)snapshot.longitude, (float)model.items[0].longitude);
    TEST_ASSERT_EQUAL_UINT8(snapshot.is_active, model.items[0].is_active);
}

void run_test_domain_locations_model_add(void)
{
    UnityPrint("=== domain/locations_model : locations_model_add() ===");
    UNITY_OUTPUT_CHAR('\n');
    UNITY_OUTPUT_CHAR('\n');

    reset_model();
    RUN_TEST(test_domain_locations_model_add_success);

    reset_model();
    RUN_TEST(test_domain_locations_model_add_duplicate_fails);

    reset_model();
    RUN_TEST(test_domain_locations_model_add_null_model_fails);

    reset_model();
    RUN_TEST(test_domain_locations_model_add_null_location_fails);

    reset_model();
    RUN_TEST(test_domain_locations_model_add_when_full_fails);

    reset_model();
    RUN_TEST(test_domain_locations_model_add_duplicate_has_no_side_effects);

    UNITY_OUTPUT_CHAR('\n');
}

void run_test_domain_locations_model_remove(void)
{
    UnityPrint("=== domain/locations_model : locations_model_remove() ===");
    UNITY_OUTPUT_CHAR('\n');
    UNITY_OUTPUT_CHAR('\n');

    // TODO:
    // reset_model();
    // RUN_TEST(test_domain_locations_model_add_success);

    UNITY_OUTPUT_CHAR('\n');
}

void run_test_domain_locations_model_get_active(void)
{
    UnityPrint("=== domain/locations_model : locations_model_get_active() ===");
    UNITY_OUTPUT_CHAR('\n');
    UNITY_OUTPUT_CHAR('\n');

    // TODO:
    // reset_model();
    // RUN_TEST(test_domain_locations_model_add_success);

    UNITY_OUTPUT_CHAR('\n');
}