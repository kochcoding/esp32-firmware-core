#include <unity.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "test_api.h"

#include "locations_storage.h"
#include "locations_model.h"

/*
    helpers
*/

static void reset_model(locations_model_t *m)
{
    TEST_ASSERT_NOT_NULL(m);
    (void)memset(m, 0, sizeof(*m));
}

static size_t count_active(const locations_model_t *m)
{
    size_t active = 0U;

    if (m != NULL)
    {
        for (size_t i = 0U; i < m->count; i++)
        {
            if (m->items[i].is_active)
            {
                active++;
            }
        }
    }

    return active;
}

/*
    locations_storage_from_json
*/

static void test_from_json_success(void)
{
    const char *json =
        "{"
        "  \"locations\": ["
        "    {\"name\":\"Berlin\",\"latitude\":52.52,\"longitude\":13.405,\"is_active\":false},"
        "    {\"name\":\"Munich\",\"latitude\":48.137,\"longitude\":11.575,\"is_active\":true}"
        "  ]"
        "}";

    locations_model_t model;
    reset_model(&model);

    TEST_ASSERT_TRUE(locations_storage_from_json(json, &model));
    TEST_ASSERT_EQUAL_UINT32(2U, model.count);
    TEST_ASSERT_EQUAL_STRING("Berlin", model.items[0].name);
    TEST_ASSERT_EQUAL_STRING("Munich", model.items[1].name);
    TEST_ASSERT_TRUE(model.items[1].is_active);
}

static void test_from_json_invalid_json_fails(void)
{
    const char *json = "{ this is not json }";

    locations_model_t *model;
    reset_model(model);

    TEST_ASSERT_FALSE(locations_storage_from_json(json, model));
}

static void test_from_json_missing_locations_array_fails(void)
{
    const char *json = "{\"foo\":123}";

    locations_model_t *model;
    reset_model(model);

    TEST_ASSERT_FALSE(locations_storage_from_json(json, model));
}

static void test_from_json_multiple_active_keeps_first_only(void)
{
    const char *json =
        "{"
        "  \"locations\": ["
        "    {\"name\":\"A\",\"latitude\":1.0,\"longitude\":2.0,\"is_active\":true},"
        "    {\"name\":\"B\",\"latitude\":3.0,\"longitude\":4.0,\"is_active\":true},"
        "    {\"name\":\"C\",\"latitude\":5.0,\"longitude\":6.0,\"is_active\":true}"
        "  ]"
        "}";

    locations_model_t model;
    reset_model(&model);

    TEST_ASSERT_TRUE(locations_storage_from_json(json, &model));
    TEST_ASSERT_EQUAL_UINT32(3U, model.count);

    TEST_ASSERT_EQUAL_UINT32(1U, (uint32_t)count_active(&model));
    TEST_ASSERT_TRUE(model.items[0].is_active);
    TEST_ASSERT_FALSE(model.items[1].is_active);
    TEST_ASSERT_FALSE(model.items[2].is_active);
}

static void test_from_json_too_many_entries_fails(void)
{
    /* build JSON with LOCATIONS_MODEL_MAX_NUMBER + 1 entries */
    char json[4096];
    size_t pos = 0U;

    pos += (size_t)snprintf(&json[pos], sizeof(json) - pos, "{\"locations\":[");
    for (size_t i = 0U; i < ((size_t)LOCATIONS_MODEL_MAX_NUMBER + 1U); i++)
    {
        pos += (size_t)snprintf(
            &json[pos], sizeof(json) - pos,
            "{\"name\":\"L%u\",\"latitude\":1.0,\"longitude\":2.0,\"is_active\":false}%s",
            (unsigned)i,
            (i == ((size_t)LOCATIONS_MODEL_MAX_NUMBER)) ? "" : ",");
        if (pos >= sizeof(json))
        {
            TEST_FAIL_MESSAGE("Test JSON buffer too small");
        }
    }

    (void)snprintf(&json[pos], sizeof(json) - pos, "]}");

    locations_model_t model;
    reset_model(&model);

    TEST_ASSERT_FALSE(locations_storage_from_json(json, &model));
}

/*
    locations_storage_to_json
    locations_storage_measure_json
*/

static void test_measure_and_to_json_success(void)
{
    locations_model_t model;
    reset_model(&model);

    /* minimal model with 1 item */
    (void)snprintf(model.items[0].name, sizeof(model.items[0].name), "%s", "Berlin");
    model.items[0].latitude = 52.52;
    model.items[0].longitude = 13.405;
    model.items[0].is_active = false;
    model.count = 1U;

    const size_t needed = locations_storage_measure_json(&model);
    TEST_ASSERT_TRUE(needed > 0U);

    char *buf = (char *)malloc(needed);
    TEST_ASSERT_NOT_NULL(buf);

    TEST_ASSERT_TRUE(locations_storage_to_json(&model, buf, needed));
    TEST_ASSERT_TRUE(strstr(buf, "\"locations\"") != NULL);
    TEST_ASSERT_TRUE(strstr(buf, "Berlin") != NULL);

    free(buf);
}

static void test_to_json_buffer_too_small_fails(void)
{
    locations_model_t model;
    reset_model(&model);

    (void)snprintf(model.items[0].name, sizeof(model.items[0].name), "%s", "Berlin");
    model.items[0].latitude = 52.52;
    model.items[0].longitude = 13.405;
    model.items[0].is_active = false;
    model.count = 1U;

    char out[8];
    TEST_ASSERT_FALSE(locations_storage_to_json(&model, out, sizeof(out)));
}

/*
    test runners
*/

void run_test_storage_locations_storage_from_json(void)
{
    UnityPrint("=== storage/locations_storage : locations_storage_from_json() ===");
    UNITY_OUTPUT_CHAR('\n');
    UNITY_OUTPUT_CHAR('\n');

    RUN_TEST(test_from_json_success);
    RUN_TEST(test_from_json_invalid_json_fails);
    RUN_TEST(test_from_json_missing_locations_array_fails);
    RUN_TEST(test_from_json_multiple_active_keeps_first_only);
    RUN_TEST(test_from_json_too_many_entries_fails);

    UNITY_OUTPUT_CHAR('\n');
}

void run_test_storage_locations_storage_to_json_and_measure_json(void)
{
    UnityPrint("=== storage/locations_storage : locations_storage_to_json ===");
    UNITY_OUTPUT_CHAR('\n');
    UnityPrint("=== storage/locations_storage : locations_storage_measure_json ===");
    UNITY_OUTPUT_CHAR('\n');
    UNITY_OUTPUT_CHAR('\n');

    RUN_TEST(test_measure_and_to_json_success);
    RUN_TEST(test_to_json_buffer_too_small_fails);

    UNITY_OUTPUT_CHAR('\n');
}