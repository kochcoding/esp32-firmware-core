#include "app/nvs_helpers.h"

#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG = "nvs_helpers";

esp_err_t nvs_load_json(const char *key, char *out_buf, size_t out_len)
{
    if (!key || !out_buf || out_len == 0)
        return ESP_ERR_INVALID_ARG;

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NS_CFG, NVS_READONLY, &nvs);
    if (err != ESP_OK)
        return err;

    size_t len = out_len;
    err = nvs_get_str(nvs, key, out_buf, &len);
    nvs_close(nvs);

    if (err == ESP_ERR_NVS_NOT_FOUND)
        return ESP_ERR_NOT_FOUND;

    return err;
}

esp_err_t nvs_save_json(const char *key, const char *json)
{
    if (!key || !json)
        return ESP_ERR_INVALID_ARG;

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NS_CFG, NVS_READWRITE, &nvs);
    if (err != ESP_OK)
        return err;

    err = nvs_set_str(nvs, key, json);
    if (err == ESP_OK)
        err = nvs_commit(nvs);

    nvs_close(nvs);

    if (err != ESP_OK)
        ESP_LOGE(TAG, "nvs_save_json key='%s' failed: %s", key, esp_err_to_name(err));

    return err;
}

esp_err_t nvs_erase_key_cfg(const char *key)
{
    if (!key)
        return ESP_ERR_INVALID_ARG;

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NS_CFG, NVS_READWRITE, &nvs);
    if (err != ESP_OK)
        return err;

    (void)nvs_erase_key(nvs, key);
    err = nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}