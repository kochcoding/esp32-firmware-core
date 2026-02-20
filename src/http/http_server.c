#include "http/http_server.h"

#include "esp_http_server.h"
#include "esp_log.h"

#include "http/routes_portal.h"
#include "http/routes_api_wifi.h"
#include "http/routes_api_locations.h"
#include "http/routes_api_weather.h"
#include "ui/ui_routes.h"

static const char *TAG = "http_server";
static httpd_handle_t s_server = NULL;

esp_err_t http_server_start(void)
{
    ESP_LOGW(TAG, "http_server_start() new wiring");

    if (s_server != NULL)
    {
        return ESP_FAIL;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Deine getunten Settings übernehmen
    config.lru_purge_enable = true;
    config.max_open_sockets = 7;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;
    config.max_uri_handlers = 24;
    config.stack_size = 12288;

    // Für später: /ui/* wildcard routes
    config.uri_match_fn = httpd_uri_match_wildcard;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        s_server = NULL;
        return ESP_FAIL;
    }

    routes_portal_register(s_server);
    ui_routes_register(s_server);
    routes_api_wifi_register(s_server);
    routes_api_locations_register(s_server);
    routes_api_weather_register(s_server);

    ESP_LOGI(TAG, "HTTP server started. Open http://192.168.4.1/");

    return ESP_OK;
}