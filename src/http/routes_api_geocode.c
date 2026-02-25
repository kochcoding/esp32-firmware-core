#include "http/routes_api_geocode.h"
#include "http/http_helpers.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "routes_api_geocode";

#define GEOCODE_BUF_SIZE 4096

typedef struct
{
    char *buf;
    size_t len;
    size_t cap;
    bool overflow;
} acc_t;

static esp_err_t on_data(esp_http_client_event_t *evt)
{
    if (evt->event_id != HTTP_EVENT_ON_DATA)
        return ESP_OK;
    if (!evt->data || evt->data_len <= 0)
        return ESP_OK;

    acc_t *a = (acc_t *)evt->user_data;
    if (a->overflow)
        return ESP_OK;

    size_t needed = a->len + (size_t)evt->data_len + 1;
    if (needed > a->cap)
    {
        a->overflow = true;
        return ESP_OK;
    }

    memcpy(a->buf + a->len, evt->data, (size_t)evt->data_len);
    a->len += (size_t)evt->data_len;
    a->buf[a->len] = '\0';
    return ESP_OK;
}

// GET /api/geocode?name=<query>
// Returns up to 5 matching locations as a JSON array.
static esp_err_t api_geocode_get(httpd_req_t *req)
{
    // read query-string
    char query[128] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK)
    {
        http_send_err(req, 400, "missing_query");
        return ESP_OK;
    }

    char name_raw[64] = {0};
    if (httpd_query_key_value(query, "name", name_raw, sizeof(name_raw)) != ESP_OK)
    {
        http_send_err(req, 400, "missing_name_param");
        return ESP_OK;
    }

    char name[64] = {0};
    http_url_decode(name_raw, name, sizeof(name));

    if (strlen(name) == 0)
    {
        http_send_err(req, 400, "empty_name");
        return ESP_OK;
    }

    // Percent-encode the name for the external geocoding API
    char name_enc[192] = {0};
    size_t j = 0;
    for (size_t i = 0; name[i] && j < sizeof(name_enc) - 4; i++)
    {
        unsigned char c = (unsigned char)name[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')
        {
            name_enc[j++] = (char)c;
        }
        else
        {
            j += snprintf(name_enc + j, sizeof(name_enc) - j, "%%%02X", c);
        }
    }

    char url[512];
    snprintf(url, sizeof(url),
             "http://geocoding-api.open-meteo.com/v1/search"
             "?name=%s&count=5&language=en&format=json",
             name_enc);

    ESP_LOGI(TAG, "geocode: '%s' → %s", name, url);

    // Perform HTTP GET request
    char *raw = calloc(1, GEOCODE_BUF_SIZE);
    if (!raw)
    {
        http_send_err(req, 500, "oom");
        return ESP_OK;
    }

    acc_t a = {.buf = raw, .len = 0, .cap = GEOCODE_BUF_SIZE, .overflow = false};

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 8000,
        .event_handler = on_data,
        .user_data = &a,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client)
    {
        free(raw);
        http_send_err(req, 500, "http_init_failed");
        return ESP_OK;
    }

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || status < 200 || status >= 300 || a.overflow)
    {
        free(raw);
        http_send_err(req, 502, "upstream_error");
        return ESP_OK;
    }

    // Parse response and extract result fields
    cJSON *root = cJSON_Parse(raw);
    free(raw);

    if (!root)
    {
        http_send_err(req, 502, "parse_error");
        return ESP_OK;
    }

    cJSON *results = cJSON_GetObjectItemCaseSensitive(root, "results");

    if (!cJSON_IsArray(results) || cJSON_GetArraySize(results) == 0)
    {
        cJSON_Delete(root);
        http_send_json(req, 200, "{\"results\":[]}");
        return ESP_OK;
    }

    // Build compact response JSON with only the fields we need
    cJSON *out_root = cJSON_CreateObject();
    cJSON *out_results = cJSON_CreateArray();

    int count = cJSON_GetArraySize(results);
    if (count > 5)
        count = 5;

    for (int i = 0; i < count; i++)
    {
        cJSON *item = cJSON_GetArrayItem(results, i);
        cJSON *j_name = cJSON_GetObjectItemCaseSensitive(item, "name");
        cJSON *j_lat = cJSON_GetObjectItemCaseSensitive(item, "latitude");
        cJSON *j_lon = cJSON_GetObjectItemCaseSensitive(item, "longitude");
        cJSON *j_cc = cJSON_GetObjectItemCaseSensitive(item, "country_code");
        cJSON *j_admin = cJSON_GetObjectItemCaseSensitive(item, "admin1");
        cJSON *j_tz = cJSON_GetObjectItemCaseSensitive(item, "timezone");

        if (!cJSON_IsString(j_name) || !cJSON_IsNumber(j_lat) || !cJSON_IsNumber(j_lon))
            continue;

        cJSON *entry = cJSON_CreateObject();
        cJSON_AddStringToObject(entry, "name", j_name->valuestring);
        cJSON_AddNumberToObject(entry, "latitude", j_lat->valuedouble);
        cJSON_AddNumberToObject(entry, "longitude", j_lon->valuedouble);
        cJSON_AddStringToObject(entry, "country_code", cJSON_IsString(j_cc) ? j_cc->valuestring : "");
        cJSON_AddStringToObject(entry, "admin1", cJSON_IsString(j_admin) ? j_admin->valuestring : "");
        cJSON_AddStringToObject(entry, "timezone", cJSON_IsString(j_tz) ? j_tz->valuestring : "UTC");
        cJSON_AddItemToArray(out_results, entry);
    }

    cJSON_AddItemToObject(out_root, "results", out_results);
    char *out_str = cJSON_PrintUnformatted(out_root);
    cJSON_Delete(root);
    cJSON_Delete(out_root);

    if (!out_str)
    {
        http_send_err(req, 500, "oom");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "geocode '%s': %d results", name, count);
    http_send_json(req, 200, out_str);
    free(out_str);
    return ESP_OK;
}

static const httpd_uri_t uri_geocode = {
    .uri = "/api/geocode",
    .method = HTTP_GET,
    .handler = api_geocode_get};

void routes_api_geocode_register(httpd_handle_t server)
{
    ESP_LOGI(TAG, "register geocode API route");
    httpd_register_uri_handler(server, &uri_geocode);
}
