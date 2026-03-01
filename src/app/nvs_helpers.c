/**
 * @file nvs_helpers.c
 * @brief Implementation of NVS JSON string storage helpers.
 *
 * @details
 *  - All functions open and close the NVS handle within the same call.
 *  - NVS handle is always closed on every exit path.
 *  - Errors are propagated to the caller; only save errors are additionally logged.
 */

//------------------------------------------------------------------------------
// private includes
//------------------------------------------------------------------------------
#include "app/nvs_helpers.h"

#include <string.h>

#include "esp_log.h"

#include "nvs.h"

//------------------------------------------------------------------------------
// private variables
//------------------------------------------------------------------------------

static const char *TAG = "nvs_helpers";

//------------------------------------------------------------------------------
// public functions
//------------------------------------------------------------------------------

esp_err_t nvs_load_json(const char *key, char *out_buffer, size_t out_length)
{
    if ((key == NULL) || (out_buffer == NULL) || (out_length == 0U))
    {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs;
    esp_err_t error = nvs_open(NVS_NS_CFG, NVS_READONLY, &nvs);
    if (error != ESP_OK)
    {
        return error;
    }

    size_t len = out_length;
    error = nvs_get_str(nvs, key, out_buffer, &len);
    nvs_close(nvs);

    if (error == ESP_ERR_NVS_NOT_FOUND)
    {
        return ESP_ERR_NOT_FOUND;
    }

    return error;
}

esp_err_t nvs_save_json(const char *key, const char *json)
{
    if ((key == NULL) || (json == NULL))
    {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs;
    esp_err_t error = nvs_open(NVS_NS_CFG, NVS_READWRITE, &nvs);
    if (error != ESP_OK)
    {
        return error;
    }

    error = nvs_set_str(nvs, key, json);
    if (error == ESP_OK)
    {
        error = nvs_commit(nvs);
    }

    nvs_close(nvs);

    if (error != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_save_json key='%s' failed: %s", key, esp_err_to_name(error));
    }

    return error;
}

esp_err_t nvs_erase_key_cfg(const char *key)
{
    if (key == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs;
    esp_err_t error = nvs_open(NVS_NS_CFG, NVS_READWRITE, &nvs);
    if (error != ESP_OK)
    {
        return error;
    }

    (void)nvs_erase_key(nvs, key);
    error = nvs_commit(nvs);
    nvs_close(nvs);
    return error;
}