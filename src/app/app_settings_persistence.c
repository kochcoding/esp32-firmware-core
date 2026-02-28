/**
 * @file app_settings_persistence.c
 * @brief Implementation of NVS-backed persistence for WiFi station settings.
 *
 * @details
 *  - Credentials are stored as individual NVS string keys under the cfg namespace.
 *  - SSID and password are read and written independently to allow partial updates.
 *  - NVS handle is always closed on every exit path.
 */

//------------------------------------------------------------------------------
// private includes
//------------------------------------------------------------------------------
#include "app/app_settings_persistence.h"

#include "app/nvs_helpers.h"

#include <string.h>

#include "nvs.h"

//------------------------------------------------------------------------------
// private defines
//------------------------------------------------------------------------------

#define NVS_KEY_SSID "sta_ssid"
#define NVS_KEY_PASS "sta_pass"

//------------------------------------------------------------------------------
// public functions
//------------------------------------------------------------------------------

esp_err_t app_settings_load_wifi(settings_wifi_t *out)
{
    if (out == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    (void)memset(out, 0, sizeof(*out));

    nvs_handle_t nvs;
    esp_err_t error = nvs_open(NVS_NS_CFG, NVS_READONLY, &nvs);
    if (error != ESP_OK)
    {
        return error;
    }

    size_t ssid_len = sizeof(out->ssid);
    size_t pass_len = sizeof(out->pass);

    esp_err_t error_ssid = nvs_get_str(nvs, NVS_KEY_SSID, out->ssid, &ssid_len);
    esp_err_t error_pass = nvs_get_str(nvs, NVS_KEY_PASS, out->pass, &pass_len);

    nvs_close(nvs);

    if (error_ssid == ESP_ERR_NVS_NOT_FOUND)
    {
        out->ssid[0] = '\0';
        out->pass[0] = '\0';
        return ESP_ERR_NOT_FOUND;
    }
    if (error_ssid != ESP_OK)
    {
        out->ssid[0] = '\0';
        out->pass[0] = '\0';
        return error_ssid;
    }

    if (error_pass == ESP_ERR_NVS_NOT_FOUND)
    {
        out->pass[0] = '\0';
    }
    else if (error_pass != ESP_OK)
    {
        out->pass[0] = '\0';
        return error_pass;
    }

    return ESP_OK;
}

esp_err_t app_settings_save_wifi(const settings_wifi_t *in)
{
    if (in == NULL || in->ssid[0] == '\0')
    {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs;
    esp_err_t error = nvs_open(NVS_NS_CFG, NVS_READWRITE, &nvs);
    if (error != ESP_OK)
    {
        return error;
    }

    error = nvs_set_str(nvs, NVS_KEY_SSID, in->ssid);
    if (error == ESP_OK)
    {
        error = nvs_set_str(nvs, NVS_KEY_PASS, in->pass);
    }

    if (error == ESP_OK)
    {
        error = nvs_commit(nvs);
    }

    nvs_close(nvs);
    return error;
}

esp_err_t app_settings_clear_wifi(void)
{
    nvs_handle_t nvs;
    esp_err_t error = nvs_open(NVS_NS_CFG, NVS_READWRITE, &nvs);
    if (error != ESP_OK)
    {
        return error;
    }

    (void)nvs_erase_key(nvs, NVS_KEY_SSID);
    (void)nvs_erase_key(nvs, NVS_KEY_PASS);

    error = nvs_commit(nvs);
    nvs_close(nvs);
    return error;
}