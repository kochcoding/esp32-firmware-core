#include <string.h>

#include "nvs.h"
#include "nvs_flash.h"

#include "app/app_locations_persistence.h"
#include "locations_storage.h"

#define NVS_NS_CFG "cfg"
#define NVS_KEY_LOCATIONS "locations"
#define NVS_JSON_MAX_LEN 2048

esp_err_t app_locations_load(locations_model_t *out)
{
    if (out == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NS_CFG, NVS_READONLY, &nvs);
    if (err != ESP_OK)
    {
        return err;
    }

    char *buf = calloc(1, NVS_JSON_MAX_LEN);
    if (buf == NULL)
    {
        nvs_close(nvs);
        return ESP_ERR_NO_MEM;
    }

    size_t len = NVS_JSON_MAX_LEN;
    err = nvs_get_str(nvs, NVS_KEY_LOCATIONS, buf, &len);
    nvs_close(nvs);

    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        free(buf);
        return ESP_ERR_NOT_FOUND;
    }
    if (err != ESP_OK)
    {
        free(buf);
        return err;
    }

    bool ok = locations_storage_from_json(buf, out);
    free(buf);
    return ok ? ESP_OK : ESP_FAIL;
}

esp_err_t app_locations_save(const locations_model_t *in)
{
    if (in == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    char *buf = calloc(1, NVS_JSON_MAX_LEN);
    if (buf == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    bool ok = locations_storage_to_json(in, buf, NVS_JSON_MAX_LEN);
    if (!ok)
    {
        free(buf);
        return ESP_FAIL;
    }

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NS_CFG, NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        free(buf);
        return err;
    }

    err = nvs_set_str(nvs, NVS_KEY_LOCATIONS, buf);
    if (err == ESP_OK)
    {
        err = nvs_commit(nvs);
    }

    nvs_close(nvs);
    free(buf);
    return err;
}

esp_err_t app_locations_clear(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NS_CFG, NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        return err;
    }

    (void)nvs_erase_key(nvs, NVS_KEY_LOCATIONS);
    err = nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}