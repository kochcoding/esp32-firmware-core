#include <string.h>

#include "nvs.h"
#include "nvs_flash.h"

#include "app/app_settings_persistence.h"

#define NVS_NS_CFG "cfg"
#define NVS_KEY_SSID "sta_ssid"
#define NVS_KEY_PASS "sta_pass"

esp_err_t app_settings_load_wifi(settings_wifi_t *out)
{
    if (out == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    (void)memset(out, 0, sizeof(*out));

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NS_CFG, NVS_READONLY, &nvs);
    if (err != ESP_OK)
    {
        return err;
    }

    size_t ssid_len = sizeof(out->ssid);
    size_t pass_len = sizeof(out->pass);

    esp_err_t e1 = nvs_get_str(nvs, NVS_KEY_SSID, out->ssid, &ssid_len);
    esp_err_t e2 = nvs_get_str(nvs, NVS_KEY_PASS, out->pass, &pass_len);

    nvs_close(nvs);

    if (e1 == ESP_ERR_NVS_NOT_FOUND)
    {
        out->ssid[0] = '\0';
        out->pass[0] = '\0';
        return ESP_ERR_NOT_FOUND;
    }
    if (e1 != ESP_OK)
    {
        out->ssid[0] = '\0';
        out->pass[0] = '\0';
        return e1;
    }

    if (e2 != ESP_ERR_NVS_NOT_FOUND)
    {
        out->pass[0] = '\0';
    }
    else if (e2 != ESP_OK)
    {
        out->pass[0] = '\0';
        return e2;
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
    esp_err_t err = nvs_open(NVS_NS_CFG, NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_set_str(nvs, NVS_KEY_SSID, in->ssid);
    if (err == ESP_OK)
    {
        err = nvs_set_str(nvs, NVS_KEY_PASS, in->pass);
    }

    if (err == ESP_OK)
    {
        err = nvs_commit(nvs);
    }

    nvs_close(nvs);
    return err;
}

esp_err_t app_settings_clear_wifi(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NS_CFG, NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        return err;
    }

    (void)nvs_erase_key(nvs, NVS_KEY_SSID);
    (void)nvs_erase_key(nvs, NVS_KEY_PASS);

    err = nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}