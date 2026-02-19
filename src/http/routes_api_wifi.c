#include "http/routes_api_wifi.h"
#include "http/http_helpers.h"

#include <stdlib.h>
#include <string.h>

#include "esp_http_server.h"
#include "esp_log.h"

#include "settings_storage.h"
#include "app/app_settings_persistence.h"
#include "wifi_sta.h"

static const char *TAG = "routes_api_wifi";

static esp_err_t api_wifi_get(httpd_req_t *req)
{
    settings_wifi_t s = {0};
    esp_err_t err = app_settings_load_wifi(&s);

    if (err == ESP_ERR_NOT_FOUND)
    {
        memset(&s, 0, sizeof(s));
    }
    else if (err != ESP_OK)
    {
        http_send_err(req, 500, "load_failed");
        return ESP_OK;
    }

    // STA runtime status
    // NOTE: JSON is built here as a deliberate pragmatic choice.
    // A cleaner solution (wifi_sta_status_to_json) is planned for the
    // architecture-polish branch (Punkt 7).
    wifi_sta_status_t sta = wifi_sta_get_status();

    const char *sta_state_str = "idle";
    if (sta.state == WIFI_STA_STATE_CONNECTED)
        sta_state_str = "connected";
    else if (sta.state == WIFI_STA_STATE_CONNECTING)
        sta_state_str = "connecting";
    else if (sta.state == WIFI_STA_STATE_FAILED)
        sta_state_str = "failed";

    char ip_str[16] = {0};
    if (sta.state == WIFI_STA_STATE_CONNECTED)
    {
        snprintf(ip_str, sizeof(ip_str), "%u.%u.%u.%u",
                 sta.ip[0], sta.ip[1], sta.ip[2], sta.ip[3]);
    }

    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"ssid\":\"%s\",\"pass_len\":%u,\"sta_state\":\"%s\",\"ip\":\"%s\"}",
             s.ssid,
             (unsigned)strlen(s.pass),
             sta_state_str,
             ip_str);

    ESP_LOGI(TAG, "GET wifi config: SSID='%s' (password withheld), sta=%s",
             s.ssid, sta_state_str);

    http_send_json(req, 200, buf);
    return ESP_OK;
}

static esp_err_t api_wifi_post(httpd_req_t *req)
{
    char body[512];
    size_t n = 0;
    if (!http_read_body(req, body, sizeof(body), &n))
    {
        http_send_err(req, 400, "invalid_body");
        return ESP_OK;
    }

    settings_wifi_t s = {0};
    if (!settings_storage_wifi_from_json(body, &s))
    {
        http_send_err(req, 400, "invalid_json");
        return ESP_OK;
    }

    esp_err_t err = app_settings_save_wifi(&s);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "save failed: %s", esp_err_to_name(err));
        http_send_err(req, 500, "save_failed");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "WiFi credentials saved: SSID='%s' (pass_len=%u)",
             s.ssid, (unsigned)strlen(s.pass));

    esp_err_t conn_err = wifi_sta_connect(s.ssid, s.pass);

    if (conn_err == ESP_OK)
        http_send_json(req, 200, "{\"ok\":true,\"connect_started\":true}");
    else
        http_send_json(req, 200, "{\"ok\":true,\"connect_started\":false}");

    return ESP_OK;
}

static esp_err_t api_wifi_delete(httpd_req_t *req)
{
    esp_err_t err = app_settings_clear_wifi();
    if (err != ESP_OK)
    {
        http_send_err(req, 500, "clear_failed");
        return ESP_OK;
    }
    http_send_json(req, 200, "{\"ok\":true}");
    return ESP_OK;
}

static const httpd_uri_t uri_get = {.uri = "/api/config/wifi", .method = HTTP_GET, .handler = api_wifi_get};
static const httpd_uri_t uri_post = {.uri = "/api/config/wifi", .method = HTTP_POST, .handler = api_wifi_post};
static const httpd_uri_t uri_del = {.uri = "/api/config/wifi", .method = HTTP_DELETE, .handler = api_wifi_delete};

void routes_api_wifi_register(httpd_handle_t server)
{
    httpd_register_uri_handler(server, &uri_get);
    httpd_register_uri_handler(server, &uri_post);
    httpd_register_uri_handler(server, &uri_del);
}
