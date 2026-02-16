#include <unity.h>

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "test_api.h"

#include "weather_storage.h"

/*
    weather_storage_validate_json
*/

static void test_validate_json_object_success(void)
{
    TEST_ASSERT_TRUE(weather_storage_validate_json("{\"a\":1}"));
}

static void test_validate_json_array_success(void)
{
    TEST_ASSERT_TRUE(weather_storage_validate_json("[1,2,3]"));
}

static void test_validate_json_number_success(void)
{
    TEST_ASSERT_TRUE(weather_storage_validate_json("123"));
}

static void test_validate_json_invalid_fails(void)
{
    TEST_ASSERT_FALSE(weather_storage_validate_json("{ this is not json }"));
}

static void test_validate_json_null_fails(void)
{
    TEST_ASSERT_FALSE(weather_storage_validate_json(NULL));
}

/*
    weather_storage_compact_json
    weather_storage_measure_compact_json
*/

static void test_measure_and_compact_json_success(void)
{
    const char *pretty = "{\n  \"a\" : 1,\n  \"b\" : [ 2, 3 ]\n}\n";

    const size_t needed = weather_storage_measure_compact_json(pretty);
    TEST_ASSERT_TRUE(needed > 0U);

    char *buf = (char *)malloc(needed);
    TEST_ASSERT_NOT_NULL(buf);

    TEST_ASSERT_TRUE(weather_storage_compact_json(pretty, buf, needed));

    TEST_ASSERT_TRUE(strchr(buf, '\n') == NULL);
    TEST_ASSERT_TRUE(strstr(buf, "\"a\":1") != NULL);
    TEST_ASSERT_TRUE(strstr(buf, "\"b\":[2,3]") != NULL);

    free(buf);
}

static void test_compact_json_buffer_too_small_fails(void)
{
    const char *pretty = "{\"a\":1}";
    char out[4];
    TEST_ASSERT_FALSE(weather_storage_compact_json(pretty, out, sizeof(out)));
}

static void test_compact_json_invalid_input_fails(void)
{
    char out[32];
    TEST_ASSERT_FALSE(weather_storage_compact_json("{ this is not json }", out, sizeof(out)));
}

static void test_measure_invalid_returns_zero(void)
{
    TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)weather_storage_measure_compact_json("{ this is not json }"));
}

/*
    test runners
*/

void run_test_storage_weather_storage_validate_json(void)
{
    UnityPrint("=== storage/weather_storage : weather_storage_validate_json() ===");
    UNITY_OUTPUT_CHAR('\n');
    UNITY_OUTPUT_CHAR('\n');

    RUN_TEST(test_validate_json_object_success);
    RUN_TEST(test_validate_json_array_success);
    RUN_TEST(test_validate_json_number_success);
    RUN_TEST(test_validate_json_invalid_fails);
    RUN_TEST(test_validate_json_null_fails);

    UNITY_OUTPUT_CHAR('\n');
}

void run_test_storage_weather_storage_compact_json_and_measure_json(void)
{
    UnityPrint("=== storage/weather_storage : weather_storage_compact_json ===");
    UNITY_OUTPUT_CHAR('\n');
    UnityPrint("=== storage/weather_storage : weather_storage_measure_compact_json ===");
    UNITY_OUTPUT_CHAR('\n');
    UNITY_OUTPUT_CHAR('\n');

    RUN_TEST(test_measure_and_compact_json_success);
    RUN_TEST(test_compact_json_buffer_too_small_fails);
    RUN_TEST(test_compact_json_invalid_input_fails);
    RUN_TEST(test_measure_invalid_returns_zero);

    UNITY_OUTPUT_CHAR('\n');
}