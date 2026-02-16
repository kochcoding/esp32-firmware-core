#pragma once

#include <stdbool.h>
#include <stddef.h>

bool weather_storage_validate_json(const char *json);
size_t weather_storage_measure_compact_json(const char *json);
bool weather_storage_compact_json(const char *json, char *out_json, size_t out_len);
