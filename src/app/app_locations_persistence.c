#include "app/app_locations_persistence.h"
#include "app/nvs_helpers.h"
#include "locations_storage.h"

#include <stdlib.h>
#include <string.h>

#define NVS_KEY_LOCATIONS "locations"
#define NVS_JSON_MAX_LEN 2048

esp_err_t app_locations_load(locations_model_t *out)
{
    if (!out)
        return ESP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));

    char *buf = calloc(1, NVS_JSON_MAX_LEN);
    if (!buf)
        return ESP_ERR_NO_MEM;

    esp_err_t err = nvs_load_json(NVS_KEY_LOCATIONS, buf, NVS_JSON_MAX_LEN);
    if (err == ESP_OK)
        err = locations_storage_from_json(buf, out) ? ESP_OK : ESP_FAIL;

    free(buf);
    return err;
}

esp_err_t app_locations_save(const locations_model_t *in)
{
    if (!in)
        return ESP_ERR_INVALID_ARG;

    char *buf = calloc(1, NVS_JSON_MAX_LEN);
    if (!buf)
        return ESP_ERR_NO_MEM;

    esp_err_t err = ESP_FAIL;
    if (locations_storage_to_json(in, buf, NVS_JSON_MAX_LEN))
        err = nvs_save_json(NVS_KEY_LOCATIONS, buf);

    free(buf);
    return err;
}

esp_err_t app_locations_clear(void)
{
    return nvs_erase_key_cfg(NVS_KEY_LOCATIONS);
}