#pragma once

#include "esp_err.h"
#include "settings_storage.h" // settings_wifi_t

esp_err_t app_settings_load_wifi(settings_wifi_t *out);
esp_err_t app_settings_save_wifi(const settings_wifi_t *in);
esp_err_t app_settings_clear_wifi(void);
