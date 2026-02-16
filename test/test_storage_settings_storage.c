#include <unity.h>

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "test_api.h"
#include "settings_storage.h"

/*
    settings_storage_wifi_from_json
*/

static void test_wifi_from_json_success(void)
{
    const char *json = "{\"ssid\":\"MyWiFi\",\"pass\":\"secret\"}";

    settings_wifi_t out;
    (void)memset(&out, 0, sizeof(out));

    TEST_ASSERT_TRUE(settings_storage_wifi_from_json(json, &out));
    TEST_ASSERT_EQUAL_STRING("MyWiFi", out.ssid);
    TEST_ASSERT_EQUAL_STRING("secret", out.pass);
}

static void test_wifi_from_json_missing_ssid_fails(void)
{
    const char *json = "{\"pass\":\"secret\"}";

    settings_wifi_t out;
    (void)memset(&out, 0, sizeof(out));

    TEST_ASSERT_FALSE(settings_storage_wifi_from_json(json, &out));
}

static void test_wifi_from_json_empty_ssid_fails(void)
{
    const char *json = "{\"ssid\":\"\",\"pass\":\"secret\"}";

    settings_wifi_t out;
    (void)memset(&out, 0, sizeof(out));

    TEST_ASSERT_FALSE(settings_storage_wifi_from_json(json, &out));
}

static void test_wifi_from_json_missing_pass_sets_empty(void)
{
    const char *json = "{\"ssid\":\"MyWiFi\"}";

    settings_wifi_t out;
    (void)memset(&out, 0xAA, sizeof(out)); /* ensure function clears */

    TEST_ASSERT_TRUE(settings_storage_wifi_from_json(json, &out));
    TEST_ASSERT_EQUAL_STRING("MyWiFi", out.ssid);
    TEST_ASSERT_EQUAL_STRING("", out.pass);
}

static void test_wifi_from_json_invalid_json_fails(void)
{
    const char *json = "{ this is not json }";

    settings_wifi_t out;
    (void)memset(&out, 0, sizeof(out));

    TEST_ASSERT_FALSE(settings_storage_wifi_from_json(json, &out));
}

/*
    settings_storage_wifi_to_json
    settings_storage_wifi_measure_json
*/

static void test_wifi_measure_and_to_json_include_pass_success(void)
{
    settings_wifi_t s;
    (void)memset(&s, 0, sizeof(s));
    (void)snprintf(s.ssid, sizeof(s.ssid), "%s", "MyWiFi");
    (void)snprintf(s.pass, sizeof(s.pass), "%s", "secret");

    const size_t needed = settings_storage_wifi_measure_json(&s, true);
    TEST_ASSERT_TRUE(needed > 0U);

    char *buf = (char *)malloc(needed);
    TEST_ASSERT_NOT_NULL(buf);

    TEST_ASSERT_TRUE(settings_storage_wifi_to_json(&s, true, buf, needed));
    TEST_ASSERT_TRUE(strstr(buf, "\"ssid\"") != NULL);
    TEST_ASSERT_TRUE(strstr(buf, "MyWiFi") != NULL);
    TEST_ASSERT_TRUE(strstr(buf, "\"pass\"") != NULL);
    TEST_ASSERT_TRUE(strstr(buf, "secret") != NULL);

    free(buf);
}

static void test_wifi_measure_and_to_json_exclude_pass_uses_pass_len(void)
{
    settings_wifi_t s;
    (void)memset(&s, 0, sizeof(s));
    (void)snprintf(s.ssid, sizeof(s.ssid), "%s", "MyWiFi");
    (void)snprintf(s.pass, sizeof(s.pass), "%s", "secret");

    const size_t needed = settings_storage_wifi_measure_json(&s, false);
    TEST_ASSERT_TRUE(needed > 0U);

    char *buf = (char *)malloc(needed);
    TEST_ASSERT_NOT_NULL(buf);

    TEST_ASSERT_TRUE(settings_storage_wifi_to_json(&s, false, buf, needed));
    TEST_ASSERT_TRUE(strstr(buf, "\"ssid\"") != NULL);
    TEST_ASSERT_TRUE(strstr(buf, "MyWiFi") != NULL);
    TEST_ASSERT_TRUE(strstr(buf, "\"pass_len\"") != NULL);
    TEST_ASSERT_TRUE(strstr(buf, "\"pass\"") == NULL);

    free(buf);
}

static void test_wifi_to_json_buffer_too_small_fails(void)
{
    settings_wifi_t s;
    (void)memset(&s, 0, sizeof(s));
    (void)snprintf(s.ssid, sizeof(s.ssid), "%s", "MyWiFi");
    (void)snprintf(s.pass, sizeof(s.pass), "%s", "secret");

    char out[8];
    TEST_ASSERT_FALSE(settings_storage_wifi_to_json(&s, true, out, sizeof(out)));
}

/*
    test runners
*/

void run_test_storage_settings_storage_wifi_from_json(void)
{
    UnityPrint("=== storage/settings_storage : settings_storage_wifi_from_json() ===");
    UNITY_OUTPUT_CHAR('\n');
    UNITY_OUTPUT_CHAR('\n');

    RUN_TEST(test_wifi_from_json_success);
    RUN_TEST(test_wifi_from_json_missing_ssid_fails);
    RUN_TEST(test_wifi_from_json_empty_ssid_fails);
    RUN_TEST(test_wifi_from_json_missing_pass_sets_empty);
    RUN_TEST(test_wifi_from_json_invalid_json_fails);

    UNITY_OUTPUT_CHAR('\n');
}

void run_test_storage_settings_storage_wifi_to_json_and_measure_json(void)
{
    UnityPrint("=== storage/settings_storage : settings_storage_wifi_to_json ===");
    UNITY_OUTPUT_CHAR('\n');
    UnityPrint("=== storage/settings_storage : settings_storage_wifi_measure_json ===");
    UNITY_OUTPUT_CHAR('\n');
    UNITY_OUTPUT_CHAR('\n');

    RUN_TEST(test_wifi_measure_and_to_json_include_pass_success);
    RUN_TEST(test_wifi_measure_and_to_json_exclude_pass_uses_pass_len);
    RUN_TEST(test_wifi_to_json_buffer_too_small_fails);

    UNITY_OUTPUT_CHAR('\n');
}