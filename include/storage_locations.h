#pragma once
#include "esp_err.h"

typedef struct
{
    char id[16];
    char name[48];
    float lat;
    float lon;
    char timezone[48];
    char country[8];
} location_t;

esp_err_t storage_locations_init(void);
esp_err_t storage_locations_add(const location_t *loc, int max_locations);
esp_err_t storage_locations_get_all_json(char **out_json);
esp_err_t storage_locations_delete_by_id(const char *id);
esp_err_t storage_locations_clear_all(void);
esp_err_t storage_locations_remove_by_id(const char *id);

esp_err_t storage_locations_get_by_id(const char *id, location_t *out_loc);
esp_err_t storage_locations_get_first(location_t *out_loc);

esp_err_t storage_locations_set_active_id(const char *id);
esp_err_t storage_locations_get_active_id(char *out_id, size_t out_len);
esp_err_t storage_locations_clear_active_id(void);
