#pragma once

#include "esp_err.h"
#include "locations_model.h"

esp_err_t app_locations_load(locations_model_t *out);
esp_err_t app_locations_save(const locations_model_t *in);
esp_err_t app_locations_clear(void);