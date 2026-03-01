/**
 * @file routes_api_locations.c
 * @brief Implementation of the locations API route handlers.
 *
 * @details
 *  - GET    /api/locations           — loads and serialises all stored locations.
 *  - POST   /api/locations           — validates, appends, and persists a new location.
 *  - DELETE /api/locations?name=<n>  — removes a location by URL-decoded name.
 *  - PUT    /api/locations/active?name=<n> — sets exactly one location as active.
 */

//------------------------------------------------------------------------------
// private includes
//------------------------------------------------------------------------------

#include "http/routes_api_locations.h"

#include "http/http_helpers.h"

#include "app/app_locations_persistence.h"

#include "locations_model.h"
#include "locations_storage.h"

#include <stdlib.h>
#include <string.h>

#include "esp_http_server.h"
#include "esp_log.h"

#include <cJSON.h>

//------------------------------------------------------------------------------
// private defines
//------------------------------------------------------------------------------

#define LOCATIONS_JSON_BUF_SIZE (2048U)

//------------------------------------------------------------------------------
// private variables
//------------------------------------------------------------------------------

static const char *TAG = "routes_api_locations";

//------------------------------------------------------------------------------
// private function (prototypes)
//------------------------------------------------------------------------------

/**
 * @brief HTTP handler for GET /api/locations.
 *
 * Loads all stored locations and returns them as a JSON array.
 *
 * @param[in] request HTTP request handle. Must not be NULL.
 * @retval ESP_OK always (errors reported via HTTP response).
 */
static esp_err_t api_locations_get(httpd_req_t *request);

/**
 * @brief HTTP handler for POST /api/locations.
 *
 * Parses a JSON body, validates required fields, and appends a new
 * location to persistent storage.
 *
 * @param[in] request HTTP request handle. Must not be NULL.
 * @retval ESP_OK always (errors reported via HTTP response).
 */
static esp_err_t api_locations_post(httpd_req_t *request);

/**
 * @brief HTTP handler for DELETE /api/locations?name=<n>.
 *
 * URL-decodes the name query parameter and removes the matching
 * location from persistent storage.
 *
 * @param[in] request HTTP request handle. Must not be NULL.
 * @retval ESP_OK always (errors reported via HTTP response).
 */
static esp_err_t api_locations_delete(httpd_req_t *request);

/**
 * @brief HTTP handler for PUT /api/locations/active?name=<n>.
 *
 * Sets exactly one location as active and deactivates all others.
 *
 * @param[in] request HTTP request handle. Must not be NULL.
 * @retval ESP_OK always (errors reported via HTTP response).
 */
static esp_err_t api_locations_set_active(httpd_req_t *request);

//------------------------------------------------------------------------------
// public functions
//------------------------------------------------------------------------------

/* Defined here (not in private variables) because they reference the
 * static handler functions declared above. */
static const httpd_uri_t uri_get = {
    .uri = "/api/locations", .method = HTTP_GET, .handler = api_locations_get};
static const httpd_uri_t uri_post = {
    .uri = "/api/locations", .method = HTTP_POST, .handler = api_locations_post};
static const httpd_uri_t uri_delete = {
    .uri = "/api/locations", .method = HTTP_DELETE, .handler = api_locations_delete};
static const httpd_uri_t uri_active = {
    .uri = "/api/locations/active", .method = HTTP_PUT, .handler = api_locations_set_active};

void routes_api_locations_register(httpd_handle_t server)
{
    ESP_LOGI(TAG, "register locations API routes");
    httpd_register_uri_handler(server, &uri_get);
    httpd_register_uri_handler(server, &uri_post);
    httpd_register_uri_handler(server, &uri_delete);
    httpd_register_uri_handler(server, &uri_active);
}

//------------------------------------------------------------------------------
// private function (implementation)
//------------------------------------------------------------------------------

/* GET /api/locations — return all stored locations */
static esp_err_t api_locations_get(httpd_req_t *request)
{
    locations_model_t model = {0};
    esp_err_t error = app_locations_load(&model);

    if (error == ESP_ERR_NOT_FOUND)
    {
        http_send_json(request, 200, "{\"locations\":[]}");
        return ESP_OK;
    }
    if (error != ESP_OK)
    {
        http_send_err(request, 500, "load_failed");
        return ESP_OK;
    }

    char *buffer = calloc(1, LOCATIONS_JSON_BUF_SIZE);
    if (buffer == NULL)
    {
        http_send_err(request, 500, "oom");
        return ESP_OK;
    }

    if (locations_storage_to_json(&model, buffer, LOCATIONS_JSON_BUF_SIZE) == false)
    {
        free(buffer);
        buffer = NULL;
        http_send_err(request, 500, "json_failed");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "GET locations: count=%u", (unsigned)model.count);
    http_send_json(request, 200, buffer);
    free(buffer);
    buffer = NULL;
    return ESP_OK;
}

/* POST /api/locations — add a new location
   Body: {"name":"Berlin","latitude":52.52,"longitude":13.405} */
static esp_err_t api_locations_post(httpd_req_t *request)
{
    char body[512];
    size_t n = 0;
    if (http_read_body(request, body, sizeof(body), &n) == false)
    {
        http_send_err(request, 400, "invalid_body");
        return ESP_OK;
    }

    /* Parse request body */
    cJSON *root = cJSON_Parse(body);
    if (root == NULL)
    {
        http_send_err(request, 400, "invalid_json");
        return ESP_OK;
    }

    const cJSON *j_name = cJSON_GetObjectItemCaseSensitive(root, "name");
    const cJSON *j_lat = cJSON_GetObjectItemCaseSensitive(root, "latitude");
    const cJSON *j_lon = cJSON_GetObjectItemCaseSensitive(root, "longitude");

    if (cJSON_IsString(j_name) == false || cJSON_IsNumber(j_lat) == false ||
        cJSON_IsNumber(j_lon) == false)
    {
        cJSON_Delete(root);
        http_send_err(request, 400, "missing_fields");
        return ESP_OK;
    }

    location_t loc = {0};
    snprintf(loc.name, sizeof(loc.name), "%s", j_name->valuestring);
    loc.latitude = j_lat->valuedouble;
    loc.longitude = j_lon->valuedouble;
    loc.is_active = false;
    cJSON_Delete(root);

    /* Load existing locations */
    locations_model_t model = {0};
    esp_err_t error = app_locations_load(&model);
    if (error != ESP_OK && error != ESP_ERR_NOT_FOUND)
    {
        http_send_err(request, 500, "load_failed");
        return ESP_OK;
    }

    /* Add — locations_model_add checks for duplicates and max capacity */
    if (locations_model_add(&model, &loc) == false)
    {
        http_send_err(request, 409, "duplicate_or_full");
        return ESP_OK;
    }

    error = app_locations_save(&model);
    if (error != ESP_OK)
    {
        ESP_LOGE(TAG, "save failed: %s", esp_err_to_name(error));
        http_send_err(request, 500, "save_failed");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "POST location added: '%s' (%.4f, %.4f)", loc.name, loc.latitude, loc.longitude);
    http_send_json(request, 201, "{\"ok\":true}");
    return ESP_OK;
}

/* DELETE /api/locations?name=Berlin */
static esp_err_t api_locations_delete(httpd_req_t *request)
{
    char name[32] = {0};
    if (httpd_req_get_url_query_str(request, name, sizeof(name)) != ESP_OK)
    {
        http_send_err(request, 400, "missing_query");
        return ESP_OK;
    }

    char name_raw[32] = {0};
    if (httpd_query_key_value(name, "name", name_raw, sizeof(name_raw)) != ESP_OK)
    {
        http_send_err(request, 400, "missing_name_param");
        return ESP_OK;
    }

    char name_val[32] = {0};
    http_url_decode(name_raw, name_val, sizeof(name_val));

    locations_model_t model = {0};
    esp_err_t error = app_locations_load(&model);
    if (error == ESP_ERR_NOT_FOUND)
    {
        http_send_err(request, 404, "not_found");
        return ESP_OK;
    }
    if (error != ESP_OK)
    {
        http_send_err(request, 500, "load_failed");
        return ESP_OK;
    }

    if (locations_model_remove(&model, name_val) == false)
    {
        http_send_err(request, 404, "not_found");
        return ESP_OK;
    }

    error = app_locations_save(&model);
    if (error != ESP_OK)
    {
        ESP_LOGE(TAG, "save failed: %s", esp_err_to_name(error));
        http_send_err(request, 500, "save_failed");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "DELETE location: '%s'", name_val);
    http_send_json(request, 200, "{\"ok\":true}");
    return ESP_OK;
}

/* PUT /api/locations/active?name=Berlin */
static esp_err_t api_locations_set_active(httpd_req_t *request)
{
    char query[32] = {0};
    if (httpd_req_get_url_query_str(request, query, sizeof(query)) != ESP_OK)
    {
        http_send_err(request, 400, "missing_query");
        return ESP_OK;
    }

    char name_raw[32] = {0};
    if (httpd_query_key_value(query, "name", name_raw, sizeof(name_raw)) != ESP_OK)
    {
        http_send_err(request, 400, "missing_name_param");
        return ESP_OK;
    }

    char name_val[32] = {0};
    http_url_decode(name_raw, name_val, sizeof(name_val));

    locations_model_t model = {0};
    esp_err_t error = app_locations_load(&model);
    if (error == ESP_ERR_NOT_FOUND)
    {
        http_send_err(request, 404, "not_found");
        return ESP_OK;
    }
    if (error != ESP_OK)
    {
        http_send_err(request, 500, "load_failed");
        return ESP_OK;
    }

    /* Deactivate all, then activate the requested one */
    bool found = false;
    for (size_t i = 0; i < model.count; i++)
    {
        if (strcmp(model.items[i].name, name_val) == 0)
        {
            model.items[i].is_active = true;
            found = true;
        }
        else
        {
            model.items[i].is_active = false;
        }
    }

    if (found == false)
    {
        http_send_err(request, 404, "not_found");
        return ESP_OK;
    }

    error = app_locations_save(&model);
    if (error != ESP_OK)
    {
        ESP_LOGE(TAG, "save failed: %s", esp_err_to_name(error));
        http_send_err(request, 500, "save_failed");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "PUT active location: '%s'", name_val);
    http_send_json(request, 200, "{\"ok\":true}");
    return ESP_OK;
}