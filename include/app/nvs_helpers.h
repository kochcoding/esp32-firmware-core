#pragma once

#include "esp_err.h"
#include <stddef.h>

#define NVS_NS_CFG "cfg"

esp_err_t nvs_load_json(const char *key, char *out_buf, size_t out_len);
esp_err_t nvs_save_json(const char *key, const char *json);
esp_err_t nvs_erase_key_cfg(const char *key);