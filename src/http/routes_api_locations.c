#include "http/routes_api_locations.h"
#include "http/http_helpers.h"

#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "esp_http_server.h"
#include "esp_log.h"

#include "locations_model.h"
#include "locations_storage.h"
#include "app/app_locations_persistence.h"

static const char *TAG = "routes_api_locations";

// GET /api/locations — alle Locations zurückgeben
static esp_err_t api_locations_get(httpd_req_t *req)
{
    locations_model_t model = {0};
    esp_err_t err = app_locations_load(&model);

    if (err == ESP_ERR_NOT_FOUND)
    {
        http_send_json(req, 200, "{\"locations\":[]}");
        return ESP_OK;
    }
    if (err != ESP_OK)
    {
        http_send_err(req, 500, "load_failed");
        return ESP_OK;
    }

    char *buf = calloc(1, 2048);
    if (buf == NULL)
    {
        http_send_err(req, 500, "oom");
        return ESP_OK;
    }

    if (!locations_storage_to_json(&model, buf, 2048))
    {
        free(buf);
        http_send_err(req, 500, "json_failed");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "GET locations: count=%u", (unsigned)model.count);
    http_send_json(req, 200, buf);
    free(buf);
    return ESP_OK;
}

// POST /api/locations — neue Location hinzufügen
// Body: {"name":"Berlin","latitude":52.52,"longitude":13.405}
static esp_err_t api_locations_post(httpd_req_t *req)
{
    char body[512];
    size_t n = 0;
    if (!http_read_body(req, body, sizeof(body), &n))
    {
        http_send_err(req, 400, "invalid_body");
        return ESP_OK;
    }

    // JSON parsen
    cJSON *root = cJSON_Parse(body);
    if (root == NULL)
    {
        http_send_err(req, 400, "invalid_json");
        return ESP_OK;
    }

    const cJSON *j_name = cJSON_GetObjectItemCaseSensitive(root, "name");
    const cJSON *j_lat = cJSON_GetObjectItemCaseSensitive(root, "latitude");
    const cJSON *j_lon = cJSON_GetObjectItemCaseSensitive(root, "longitude");

    if (!cJSON_IsString(j_name) || !cJSON_IsNumber(j_lat) || !cJSON_IsNumber(j_lon))
    {
        cJSON_Delete(root);
        http_send_err(req, 400, "missing_fields");
        return ESP_OK;
    }

    location_t loc = {0};
    snprintf(loc.name, sizeof(loc.name), "%s", j_name->valuestring);
    loc.latitude = j_lat->valuedouble;
    loc.longitude = j_lon->valuedouble;
    loc.is_active = false;
    cJSON_Delete(root);

    // Bestehende Locations laden
    locations_model_t model = {0};
    esp_err_t err = app_locations_load(&model);
    if (err != ESP_OK && err != ESP_ERR_NOT_FOUND)
    {
        http_send_err(req, 500, "load_failed");
        return ESP_OK;
    }

    // Hinzufügen — locations_model_add prüft auf Duplikate und Max
    if (!locations_model_add(&model, &loc))
    {
        http_send_err(req, 409, "duplicate_or_full");
        return ESP_OK;
    }

    err = app_locations_save(&model);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "save failed: %s", esp_err_to_name(err));
        http_send_err(req, 500, "save_failed");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "POST location added: '%s' (%.4f, %.4f)", loc.name, loc.latitude, loc.longitude);
    http_send_json(req, 201, "{\"ok\":true}");
    return ESP_OK;
}

// DELETE /api/locations?name=Berlin
static esp_err_t api_locations_delete(httpd_req_t *req)
{
    char name[32] = {0};
    if (httpd_req_get_url_query_str(req, name, sizeof(name)) != ESP_OK)
    {
        http_send_err(req, 400, "missing_query");
        return ESP_OK;
    }

    char name_val[32] = {0};
    if (httpd_query_key_value(name, "name", name_val, sizeof(name_val)) != ESP_OK)
    {
        http_send_err(req, 400, "missing_name_param");
        return ESP_OK;
    }

    locations_model_t model = {0};
    esp_err_t err = app_locations_load(&model);
    if (err == ESP_ERR_NOT_FOUND)
    {
        http_send_err(req, 404, "not_found");
        return ESP_OK;
    }
    if (err != ESP_OK)
    {
        http_send_err(req, 500, "load_failed");
        return ESP_OK;
    }

    if (!locations_model_remove(&model, name_val))
    {
        http_send_err(req, 404, "not_found");
        return ESP_OK;
    }

    err = app_locations_save(&model);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "save failed: %s", esp_err_to_name(err));
        http_send_err(req, 500, "save_failed");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "DELETE location: '%s'", name_val);
    http_send_json(req, 200, "{\"ok\":true}");
    return ESP_OK;
}

// PUT /api/locations/active?name=Berlin
static esp_err_t api_locations_set_active(httpd_req_t *req)
{
    char query[32] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK)
    {
        http_send_err(req, 400, "missing_query");
        return ESP_OK;
    }

    char name_val[32] = {0};
    if (httpd_query_key_value(query, "name", name_val, sizeof(name_val)) != ESP_OK)
    {
        http_send_err(req, 400, "missing_name_param");
        return ESP_OK;
    }

    locations_model_t model = {0};
    esp_err_t err = app_locations_load(&model);
    if (err == ESP_ERR_NOT_FOUND)
    {
        http_send_err(req, 404, "not_found");
        return ESP_OK;
    }
    if (err != ESP_OK)
    {
        http_send_err(req, 500, "load_failed");
        return ESP_OK;
    }

    // Alle deaktivieren, gesuchte aktivieren
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

    if (!found)
    {
        http_send_err(req, 404, "not_found");
        return ESP_OK;
    }

    err = app_locations_save(&model);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "save failed: %s", esp_err_to_name(err));
        http_send_err(req, 500, "save_failed");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "PUT active location: '%s'", name_val);
    http_send_json(req, 200, "{\"ok\":true}");
    return ESP_OK;
}

static const httpd_uri_t uri_get = {.uri = "/api/locations", .method = HTTP_GET, .handler = api_locations_get};
static const httpd_uri_t uri_post = {.uri = "/api/locations", .method = HTTP_POST, .handler = api_locations_post};
static const httpd_uri_t uri_delete = {.uri = "/api/locations", .method = HTTP_DELETE, .handler = api_locations_delete};
static const httpd_uri_t uri_active = {.uri = "/api/locations/active", .method = HTTP_PUT, .handler = api_locations_set_active};

void routes_api_locations_register(httpd_handle_t server)
{
    ESP_LOGI(TAG, "register locations API routes");
    httpd_register_uri_handler(server, &uri_get);
    httpd_register_uri_handler(server, &uri_post);
    httpd_register_uri_handler(server, &uri_delete);
    httpd_register_uri_handler(server, &uri_active);
}