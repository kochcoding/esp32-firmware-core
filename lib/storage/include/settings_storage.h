#pragma once

#include "stdbool.h"
#include "stddef.h"

#define SETTINGS_WIFI_SSID_MAX_LEN (32)
#define SETTINGS_WIFI_PASS_MAX_LEN (64)

typedef struct
{
    char ssid[SETTINGS_WIFI_SSID_MAX_LEN + 1];
    char pass[SETTINGS_WIFI_PASS_MAX_LEN + 1];
} settings_wifi_t;

bool settings_storage_wifi_from_json(const char *json, settings_wifi_t *out);
size_t settings_storage_wifi_measure_json(const settings_wifi_t *s, bool include_pass);
bool settings_storage_wifi_to_json(const settings_wifi_t *s, bool include_pass, char *out_json, size_t out_len);