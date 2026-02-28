/**
 * @file routes_api_weather.c
 * @brief Implementation of the weather API route handlers.
 *
 * @details
 *  - GET /api/weather/current  — fetches current weather via openmeteo_client.
 *  - GET /api/weather/forecast — fetches a 7-day forecast via openmeteo_client.
 *  - Both endpoints accept an optional name query parameter to select a location.
 *  - If omitted, the active location from persistent storage is used.
 */

//------------------------------------------------------------------------------
// private includes
//------------------------------------------------------------------------------
#include "http/routes_api_weather.h"

#include "http/http_helpers.h"

#include "app/app_locations_persistence.h"

#include "locations_model.h"

#include "openmeteo_client.h"

#include "esp_http_server.h"
#include "esp_log.h"

//------------------------------------------------------------------------------
// private defines
//------------------------------------------------------------------------------
#define WEATHER_FORECAST_DAYS (7U)

//------------------------------------------------------------------------------
// private variables
//------------------------------------------------------------------------------
static const char *TAG = "routes_api_weather";

//------------------------------------------------------------------------------
// private functions (prototypes)
//------------------------------------------------------------------------------

/**
 * @brief Loads the active location from persistent storage.
 *
 * Convenience helper used by both weather handlers when no name
 * query parameter is provided.
 *
 * @param[out] model Caller-supplied model buffer to load into. Must not be NULL.
 * @return Pointer to the active location within @p model, or NULL if none found.
 */
static const location_t *get_active_location(locations_model_t *model);

/**
 * @brief HTTP handler for GET /api/weather/current.
 *
 * Resolves the target location from the optional name query parameter
 * or falls back to the active location, then fetches current weather
 * via the Open-Meteo client.
 *
 * @param[in] request HTTP request handle. Must not be NULL.
 * @retval ESP_OK always (errors reported via HTTP response).
 */
static esp_err_t api_weather_current(httpd_req_t *request);

/**
 * @brief HTTP handler for GET /api/weather/forecast.
 *
 * Resolves the target location from the optional name query parameter
 * or falls back to the active location, then fetches a WEATHER_FORECAST_DAYS
 * day forecast via the Open-Meteo client.
 *
 * @param[in] request HTTP request handle. Must not be NULL.
 * @retval ESP_OK always (errors reported via HTTP response).
 */
static esp_err_t api_weather_forecast(httpd_req_t *request);

//------------------------------------------------------------------------------
// private functions (implementation)
//------------------------------------------------------------------------------

static const location_t *get_active_location(locations_model_t *model)
{
    esp_err_t error = app_locations_load(model);
    if (error != ESP_OK)
    {
        return NULL;
    }
    return locations_model_get_active(model);
}

/* GET /api/weather/current */
static esp_err_t api_weather_current(httpd_req_t *request)
{
    char query[64] = {0};
    char name_val[32] = {0};
    bool has_name = false;

    if (httpd_req_get_url_query_str(request, query, sizeof(query)) == ESP_OK)
    {
        if (httpd_query_key_value(query, "name", name_val, sizeof(name_val)) == ESP_OK)
        {
            has_name = true;
        }
    }

    locations_model_t model = {0};
    const location_t *loc = NULL;

    if (has_name == true)
    {
        esp_err_t error = app_locations_load(&model);
        if (error != ESP_OK)
        {
            http_send_err(request, 500, "load_failed");
            return ESP_OK;
        }
        for (size_t i = 0; i < model.count; i++)
        {
            if (strcmp(model.items[i].name, name_val) == 0)
            {
                loc = &model.items[i];
                break;
            }
        }
        if (loc == NULL)
        {
            http_send_err(request, 404, "location_not_found");
            return ESP_OK;
        }
    }
    else
    {
        loc = get_active_location(&model);
        if (loc == NULL)
        {
            http_send_err(request, 404, "no_active_location");
            return ESP_OK;
        }
    }

    ESP_LOGI(TAG, "GET current weather for '%s' (%.4f, %.4f)", loc->name, loc->latitude,
             loc->longitude);

    static char buffer[OPENMETEO_BUF_SIZE];
    openmeteo_status_t status =
        openmeteo_fetch_current(loc->latitude, loc->longitude, buffer, sizeof(buffer));

    if (status == OPENMETEO_ERR_BUF_TOO_SMALL)
    {
        http_send_err(request, 500, "response_too_large");
        return ESP_OK;
    }
    if (status != OPENMETEO_OK)
    {
        http_send_err(request, 502, "upstream_error");
        return ESP_OK;
    }

    http_send_json(request, 200, buffer);
    return ESP_OK;
}

/* GET /api/weather/forecast */
static esp_err_t api_weather_forecast(httpd_req_t *request)
{
    char query[64] = {0};
    char name_val[32] = {0};
    bool has_name = false;

    if (httpd_req_get_url_query_str(request, query, sizeof(query)) == ESP_OK)
    {
        if (httpd_query_key_value(query, "name", name_val, sizeof(name_val)) == ESP_OK)
        {
            has_name = true;
        }
    }

    locations_model_t model = {0};
    const location_t *loc = NULL;

    if (has_name == true)
    {
        esp_err_t error = app_locations_load(&model);
        if (error != ESP_OK)
        {
            http_send_err(request, 500, "load_failed");
            return ESP_OK;
        }
        for (size_t i = 0; i < model.count; i++)
        {
            if (strcmp(model.items[i].name, name_val) == 0)
            {
                loc = &model.items[i];
                break;
            }
        }
        if (loc == NULL)
        {
            http_send_err(request, 404, "location_not_found");
            return ESP_OK;
        }
    }
    else
    {
        loc = get_active_location(&model);
        if (loc == NULL)
        {
            http_send_err(request, 404, "no_active_location");
            return ESP_OK;
        }
    }

    ESP_LOGI(TAG, "GET forecast weather for '%s' (%.4f, %.4f)", loc->name, loc->latitude,
             loc->longitude);

    static char buffer[OPENMETEO_BUF_SIZE];
    openmeteo_status_t status = openmeteo_fetch_forecast(
        loc->latitude, loc->longitude, WEATHER_FORECAST_DAYS, buffer, sizeof(buffer));

    if (status == OPENMETEO_ERR_BUF_TOO_SMALL)
    {
        http_send_err(request, 500, "response_too_large");
        return ESP_OK;
    }
    if (status != OPENMETEO_OK)
    {
        http_send_err(request, 502, "upstream_error");
        return ESP_OK;
    }

    http_send_json(request, 200, buffer);
    return ESP_OK;
}

//------------------------------------------------------------------------------
// public functions
//------------------------------------------------------------------------------

/* Defined here (not in private variables) because they reference the
 * static handler functions declared above. */
static const httpd_uri_t uri_current = {
    .uri = "/api/weather/current", .method = HTTP_GET, .handler = api_weather_current};
static const httpd_uri_t uri_forecast = {
    .uri = "/api/weather/forecast", .method = HTTP_GET, .handler = api_weather_forecast};

void routes_api_weather_register(httpd_handle_t server)
{
    ESP_LOGI(TAG, "register weather API routes");
    httpd_register_uri_handler(server, &uri_current);
    httpd_register_uri_handler(server, &uri_forecast);
}