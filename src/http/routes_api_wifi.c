/**
 * @file routes_api_wifi.c
 * @brief Implementation of the WiFi configuration API route handlers.
 *
 * @details
 *  - GET    /api/config/wifi — loads stored SSID and merges live STA status into response.
 *  - POST   /api/config/wifi — validates, persists credentials, and triggers STA connect.
 *  - DELETE /api/config/wifi — erases stored WiFi credentials from NVS.
 */

//------------------------------------------------------------------------------
// private includes
//------------------------------------------------------------------------------

#include "http/routes_api_wifi.h"

#include "http/http_helpers.h"

#include "app/app_settings_persistence.h"

#include "settings_storage.h"

#include "wifi_sta.h"

#include <stdlib.h>
#include <string.h>

#include "esp_http_server.h"
#include "esp_log.h"

//------------------------------------------------------------------------------
// private defines
//------------------------------------------------------------------------------
#define WIFI_RESPONSE_BUF_SIZE (320U)

//------------------------------------------------------------------------------
// private variables
//------------------------------------------------------------------------------

static const char *TAG = "routes_api_wifi";

//------------------------------------------------------------------------------
// private functions (prototypes)
//------------------------------------------------------------------------------

/**
 * @brief HTTP handler for GET /api/config/wifi.
 *
 * Loads stored WiFi credentials and merges the live STA connection
 * status into a single JSON response. The password is never returned —
 * only its length is included.
 *
 * @param[in] request HTTP request handle. Must not be NULL.
 * @retval ESP_OK always (errors reported via HTTP response).
 */
static esp_err_t api_wifi_get(httpd_req_t *request);

/**
 * @brief HTTP handler for POST /api/config/wifi.
 *
 * Parses a JSON body containing SSID and password, persists the
 * credentials to NVS, and triggers a WiFi station connection attempt.
 *
 * @param[in] request HTTP request handle. Must not be NULL.
 * @retval ESP_OK always (errors reported via HTTP response).
 */
static esp_err_t api_wifi_post(httpd_req_t *request);

/**
 * @brief HTTP handler for DELETE /api/config/wifi.
 *
 * Erases stored WiFi credentials from NVS.
 *
 * @param[in] request HTTP request handle. Must not be NULL.
 * @retval ESP_OK always (errors reported via HTTP response).
 */
static esp_err_t api_wifi_delete(httpd_req_t *request);

//------------------------------------------------------------------------------
// private functions (implementation)
//------------------------------------------------------------------------------

static esp_err_t api_wifi_get(httpd_req_t *request)
{
    settings_wifi_t settings = {0};
    esp_err_t error = app_settings_load_wifi(&settings);

    if (error == ESP_ERR_NOT_FOUND)
    {
        memset(&settings, 0, sizeof(settings));
    }
    else if (error != ESP_OK)
    {
        http_send_err(request, 500, "load_failed");
        return ESP_OK;
    }

    wifi_sta_status_t sta = wifi_sta_get_status();

    char sta_buf[WIFI_STA_STATUS_JSON_BUF_SIZE];
    wifi_sta_status_to_json(&sta, sta_buf, sizeof(sta_buf));

    /* sta_buf now contains {"sta_state":"connected","ip":"192.168.x.x"}
       Manually assemble the final JSON response */
    char buffer[WIFI_RESPONSE_BUF_SIZE];
    size_t sta_len = strlen(sta_buf);
    if (sta_len > 0U)
    {
        sta_buf[sta_len - 1] = '\0'; /* strip trailing '}' */
    }

    snprintf(buffer, sizeof(buffer), "{\"ssid\":\"%s\",\"pass_len\":%u,%s}", settings.ssid,
             (unsigned)strlen(settings.pass), sta_buf + 1);

    ESP_LOGI(TAG, "GET wifi config: SSID='%s' (password withheld)", settings.ssid);

    http_send_json(request, 200, buffer);
    return ESP_OK;
}

static esp_err_t api_wifi_post(httpd_req_t *request)
{
    char body[512];
    size_t n = 0;
    if (http_read_body(request, body, sizeof(body), &n) == false)
    {
        http_send_err(request, 400, "invalid_body");
        return ESP_OK;
    }

    settings_wifi_t settings = {0};
    if (settings_storage_wifi_from_json(body, &settings) == false)
    {
        http_send_err(request, 400, "invalid_json");
        return ESP_OK;
    }

    esp_err_t error = app_settings_save_wifi(&settings);
    if (error != ESP_OK)
    {
        ESP_LOGE(TAG, "save failed: %s", esp_err_to_name(error));
        http_send_err(request, 500, "save_failed");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "WiFi credentials saved: SSID='%s' (pass_len=%u)", settings.ssid,
             (unsigned)strlen(settings.pass));

    esp_err_t connect_error = wifi_sta_connect(settings.ssid, settings.pass);

    if (connect_error == ESP_OK)
    {
        http_send_json(request, 200, "{\"ok\":true,\"connect_started\":true}");
    }
    else
    {
        http_send_json(request, 200, "{\"ok\":true,\"connect_started\":false}");
    }

    return ESP_OK;
}

static esp_err_t api_wifi_delete(httpd_req_t *request)
{
    esp_err_t error = app_settings_clear_wifi();
    if (error != ESP_OK)
    {
        http_send_err(request, 500, "clear_failed");
        return ESP_OK;
    }
    http_send_json(request, 200, "{\"ok\":true}");
    return ESP_OK;
}

//------------------------------------------------------------------------------
// public functions
//------------------------------------------------------------------------------

/* Defined here (not in private variables) because they reference the
 * static handler functions declared above. */
static const httpd_uri_t uri_get = {
    .uri = "/api/config/wifi", .method = HTTP_GET, .handler = api_wifi_get};
static const httpd_uri_t uri_post = {
    .uri = "/api/config/wifi", .method = HTTP_POST, .handler = api_wifi_post};
static const httpd_uri_t uri_del = {
    .uri = "/api/config/wifi", .method = HTTP_DELETE, .handler = api_wifi_delete};

void routes_api_wifi_register(httpd_handle_t server)
{
    ESP_LOGI(TAG, "register wifi config API routes");

    httpd_register_uri_handler(server, &uri_get);
    httpd_register_uri_handler(server, &uri_post);
    httpd_register_uri_handler(server, &uri_del);
}
