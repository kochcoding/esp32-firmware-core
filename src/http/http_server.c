/**
 * @file http_server.c
 * @brief HTTP server initialisation and route registration.
 */

//------------------------------------------------------------------------------
// private includes
//------------------------------------------------------------------------------
#include "http/http_server.h"

#include "esp_http_server.h"
#include "esp_log.h"

#include "http/routes_api_geocode.h"
#include "http/routes_api_locations.h"
#include "http/routes_api_weather.h"
#include "http/routes_api_wifi.h"
#include "http/routes_portal.h"

#include "ui/ui_routes.h"

//------------------------------------------------------------------------------
// private defines
//------------------------------------------------------------------------------
#define HTTP_SERVER_MAX_OPEN_SOCKETS (7U)
#define HTTP_SERVER_RECV_TIMEOUT_S (5U)
#define HTTP_SERVER_SEND_TIMEOUT_S (5U)
#define HTTP_SERVER_MAX_URI_HANDLERS (24U)
#define HTTP_SERVER_STACK_SIZE (12288U)

//------------------------------------------------------------------------------
// private variables
//------------------------------------------------------------------------------
static const char *TAG = "http_server";
static httpd_handle_t s_server = NULL;

//------------------------------------------------------------------------------
// public functions
//------------------------------------------------------------------------------
esp_err_t http_server_start(void)
{
    ESP_LOGI(TAG, "Starting HTTP server...");

    if (s_server != NULL)
    {
        return ESP_FAIL;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    /* Tuned settings for captive portal workload */
    config.lru_purge_enable = true;
    config.max_open_sockets = HTTP_SERVER_MAX_OPEN_SOCKETS;
    config.recv_wait_timeout = HTTP_SERVER_RECV_TIMEOUT_S;
    config.send_wait_timeout = HTTP_SERVER_SEND_TIMEOUT_S;
    config.max_uri_handlers = HTTP_SERVER_MAX_URI_HANDLERS;
    config.stack_size = HTTP_SERVER_STACK_SIZE;

    /* Required for /ui/* wildcard route matching */
    config.uri_match_fn = httpd_uri_match_wildcard;

    esp_err_t error = httpd_start(&s_server, &config);
    if (error != ESP_OK)
    {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(error));
        s_server = NULL;
        return ESP_FAIL;
    }

    routes_portal_register(s_server);
    ui_routes_register(s_server);
    routes_api_wifi_register(s_server);
    routes_api_locations_register(s_server);
    routes_api_weather_register(s_server);
    routes_api_geocode_register(s_server);

    ESP_LOGI(TAG, "HTTP server started. Open http://192.168.4.1/");

    return ESP_OK;
}