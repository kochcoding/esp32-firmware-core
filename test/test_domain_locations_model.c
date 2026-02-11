#include <unity.h>
#include <string.h>
#include <stdio.h>

#include "test_api.h"

#include "locations_model.h"

static locations_model_t model;

/*
    helpers
*/

static void reset_model(void)
{
    model.count = 0U;
    (void)memset(model.items, 0, sizeof(model.items));
}

static location_t make_loc(const char *name, double lat, double lon, bool active)
{
    location_t loc;
    (void)memset(&loc, 0, sizeof(loc));

    (void)strncpy(loc.name, name, sizeof(loc.name) - 1U);
    loc.name[sizeof(loc.name) - 1U] = '\0';

    loc.latitude = lat;
    loc.longitude = lon;
    loc.is_active = active;

    return loc;
}

/*
    locations_model_add
*/

static void test_add_success(void)
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

static void test_add_duplicate_fails(void)
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

static void test_add_null_model_fails(void)
{
    location_t loc = {
        .name = "Berlin",
        .latitude = 52.52,
        .longitude = 13.405,
        .is_active = false};

    bool result = locations_model_add(NULL, &loc);

    TEST_ASSERT_FALSE(result);
}

static void test_add_null_location_fails(void)
{
    bool result = locations_model_add(&model, NULL);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_UINT32(0U, model.count);
}

static void test_add_when_full_fails(void)
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

static void test_add_duplicate_has_no_side_effects(void)
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

/*
    locations_model_remove
*/

static void test_remove_null_model_fails(void)
{
    bool result = locations_model_remove(NULL, "A");
    TEST_ASSERT_FALSE(result);
}

static void test_remove_null_name_fails(void)
{
    bool result = locations_model_remove(&model, NULL);
    TEST_ASSERT_FALSE(result);
}

static void test_remove_empty_name_fails(void)
{
    bool result = locations_model_remove(&model, "");
    TEST_ASSERT_FALSE(result);
}

static void test_remove_from_empty_model_fails(void)
{
    bool result = locations_model_remove(&model, "A");
    TEST_ASSERT_FALSE(result);
}

static void test_remove_unknown_name_has_no_side_effects(void)
{
    location_t a = make_loc("A", 1.0, 1.0, false);
    TEST_ASSERT_TRUE(locations_model_add(&model, &a));
    TEST_ASSERT_EQUAL_UINT32(1U, (uint32_t)model.count);

    location_t snapshot = model.items[0];

    bool result = locations_model_remove(&model, "X");
    TEST_ASSERT_FALSE(result);

    TEST_ASSERT_EQUAL_UINT32(1U, (uint32_t)model.count);
    TEST_ASSERT_EQUAL_STRING(snapshot.name, model.items[0].name);
}

static void test_remove_success_shifts_items(void)
{
    location_t a = make_loc("A", 1.0, 1.0, false);
    location_t b = make_loc("B", 2.0, 2.0, false);
    location_t c = make_loc("C", 3.0, 3.0, false);

    TEST_ASSERT_TRUE(locations_model_add(&model, &a));
    TEST_ASSERT_TRUE(locations_model_add(&model, &b));
    TEST_ASSERT_TRUE(locations_model_add(&model, &c));
    TEST_ASSERT_EQUAL_UINT32(3U, (uint32_t)model.count);

    bool result = locations_model_remove(&model, "B");
    TEST_ASSERT_TRUE(result);

    TEST_ASSERT_EQUAL_UINT32(2U, (uint32_t)model.count);
    TEST_ASSERT_EQUAL_STRING("A", model.items[0].name);
    TEST_ASSERT_EQUAL_STRING("C", model.items[1].name);
}

static void test_remove_active_clears_all_active_flags(void)
{
    location_t a = make_loc("A", 1.0, 1.0, false);
    location_t b = make_loc("B", 2.0, 2.0, false);

    TEST_ASSERT_TRUE(locations_model_add(&model, &a));
    TEST_ASSERT_TRUE(locations_model_add(&model, &b));

    /* mark B active */
    model.items[1].is_active = true;

    /* remove ative item */
    TEST_ASSERT_TRUE(locations_model_remove(&model, "B"));

    /* by design: if active removed, all remaining must be inactive */
    TEST_ASSERT_EQUAL_UINT32(1U, (uint32_t)model.count);
    TEST_ASSERT_FALSE(model.items[0].is_active);

    const location_t *active = locations_model_get_active(&model);
    TEST_ASSERT_NULL(active);
}

/*
    locations_model_get_active
*/

static void test_get_active_null_model_returns_null(void)
{
    const location_t *active = locations_model_get_active(NULL);
    TEST_ASSERT_NULL(active);
}

static void test_get_active_empty_model_returns_null(void)
{
    const location_t *active = locations_model_get_active(&model);
    TEST_ASSERT_NULL(active);
}

static void test_get_active_none_active_returns_null(void)
{
    location_t a = make_loc("A", 1.0, 1.0, false);
    TEST_ASSERT_TRUE(locations_model_add(&model, &a));

    const location_t *active = locations_model_get_active(&model);
    TEST_ASSERT_NULL(active);
}

static void test_get_active_returns_first_active(void)
{
    location_t a = make_loc("A", 1.0, 1.0, true);
    location_t b = make_loc("B", 2.0, 2.0, true);

    TEST_ASSERT_TRUE(locations_model_add(&model, &a));
    TEST_ASSERT_TRUE(locations_model_add(&model, &b));

    const location_t *active = locations_model_get_active(&model);

    TEST_ASSERT_NOT_NULL(active);
    TEST_ASSERT_EQUAL_STRING("A", active->name);
}

/*
    test runners
*/

void run_test_domain_locations_model_add(void)
{
    UnityPrint("=== domain/locations_model : locations_model_add() ===");
    UNITY_OUTPUT_CHAR('\n');
    UNITY_OUTPUT_CHAR('\n');

    reset_model();
    RUN_TEST(test_add_success);

    reset_model();
    RUN_TEST(test_add_duplicate_fails);

    reset_model();
    RUN_TEST(test_add_null_model_fails);

    reset_model();
    RUN_TEST(test_add_null_location_fails);

    reset_model();
    RUN_TEST(test_add_when_full_fails);

    reset_model();
    RUN_TEST(test_add_duplicate_has_no_side_effects);

    UNITY_OUTPUT_CHAR('\n');
}

void run_test_domain_locations_model_remove(void)
{
    UnityPrint("=== domain/locations_model : locations_model_remove() ===");
    UNITY_OUTPUT_CHAR('\n');
    UNITY_OUTPUT_CHAR('\n');

    reset_model();
    RUN_TEST(test_remove_null_model_fails);

    reset_model();
    RUN_TEST(test_remove_null_name_fails);

    reset_model();
    RUN_TEST(test_remove_empty_name_fails);

    reset_model();
    RUN_TEST(test_remove_from_empty_model_fails);

    reset_model();
    RUN_TEST(test_remove_unknown_name_has_no_side_effects);

    reset_model();
    RUN_TEST(test_remove_success_shifts_items);

    reset_model();
    RUN_TEST(test_remove_active_clears_all_active_flags);

    UNITY_OUTPUT_CHAR('\n');
}

void run_test_domain_locations_model_get_active(void)
{
    UnityPrint("=== domain/locations_model : locations_model_get_active() ===");
    UNITY_OUTPUT_CHAR('\n');
    UNITY_OUTPUT_CHAR('\n');

    reset_model();
    RUN_TEST(test_get_active_null_model_returns_null);

    reset_model();
    RUN_TEST(test_get_active_empty_model_returns_null);

    reset_model();
    RUN_TEST(test_get_active_none_active_returns_null);

    reset_model();
    RUN_TEST(test_get_active_returns_first_active);

    UNITY_OUTPUT_CHAR('\n');
}