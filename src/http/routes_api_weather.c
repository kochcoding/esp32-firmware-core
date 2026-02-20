#include "http/routes_api_weather.h"
#include "http/http_helpers.h"

#include "esp_log.h"
#include "esp_http_server.h"

#include "app/app_locations_persistence.h"
#include "locations_model.h"
#include "openmeteo_client.h"

static const char *TAG = "routes_api_weather";

static const location_t *get_active_location(locations_model_t *model)
{
    esp_err_t err = app_locations_load(model);
    if (err != ESP_OK)
        return NULL;
    return locations_model_get_active(model);
}

// GET /api/weather/current
static esp_err_t api_weather_current(httpd_req_t *req)
{
    locations_model_t model = {0};
    const location_t *loc = get_active_location(&model);

    if (loc == NULL)
    {
        http_send_err(req, 404, "no_active_location");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "GET current weather for '%s' (%.4f, %.4f)",
             loc->name, loc->latitude, loc->longitude);

    static char buf[OPENMETEO_BUF_SIZE];
    openmeteo_status_t status = openmeteo_fetch_current(loc->latitude, loc->longitude, buf, sizeof(buf));

    if (status == OPENMETEO_ERR_BUF_TOO_SMALL)
    {
        http_send_err(req, 500, "response_too_large");
        return ESP_OK;
    }
    if (status != OPENMETEO_OK)
    {
        http_send_err(req, 502, "upstream_error");
        return ESP_OK;
    }

    http_send_json(req, 200, buf);
    return ESP_OK;
}

// GET /api/weather/forecast
static esp_err_t api_weather_forecast(httpd_req_t *req)
{
    locations_model_t model = {0};
    const location_t *loc = get_active_location(&model);

    if (loc == NULL)
    {
        http_send_err(req, 404, "no_active_location");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "GET forecast weather for '%s' (%.4f, %.4f)",
             loc->name, loc->latitude, loc->longitude);

    static char buf[OPENMETEO_BUF_SIZE];
    openmeteo_status_t status = openmeteo_fetch_forecast(loc->latitude, loc->longitude, 7, buf, sizeof(buf));

    if (status == OPENMETEO_ERR_BUF_TOO_SMALL)
    {
        http_send_err(req, 500, "response_too_large");
        return ESP_OK;
    }
    if (status != OPENMETEO_OK)
    {
        http_send_err(req, 502, "upstream_error");
        return ESP_OK;
    }

    http_send_json(req, 200, buf);
    return ESP_OK;
}

static const httpd_uri_t uri_current = {.uri = "/api/weather/current", .method = HTTP_GET, .handler = api_weather_current};
static const httpd_uri_t uri_forecast = {.uri = "/api/weather/forecast", .method = HTTP_GET, .handler = api_weather_forecast};

void routes_api_weather_register(httpd_handle_t server)
{
    ESP_LOGI(TAG, "register weather API routes");
    httpd_register_uri_handler(server, &uri_current);
    httpd_register_uri_handler(server, &uri_forecast);
}