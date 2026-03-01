/**
 * @file routes_api_geocode.c
 * @brief Implementation of the geocoding API route handler.
 *
 * @details
 *  - Proxies GET /api/geocode?name=<query> to the Open-Meteo geocoding API.
 *  - URL-decodes the incoming name parameter before forwarding.
 *  - Returns a compact JSON array of up to 5 matching locations.
 *  - Uses dynamic allocation for the HTTP response buffer only.
 */

//------------------------------------------------------------------------------
// private includes
//------------------------------------------------------------------------------

#include "http/routes_api_geocode.h"

#include "http/http_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_log.h"

#include "cJSON.h"

//------------------------------------------------------------------------------
// private defines
//------------------------------------------------------------------------------

#define GEOCODE_BUF_SIZE (4096U)
#define GEOCODE_MAX_RESULTS (5)
#define GEOCODE_TIMEOUT_MS (8000U)
#define GEOCODE_URL_ENC_MARGIN (4U)

//------------------------------------------------------------------------------
// private typedefs
//------------------------------------------------------------------------------

typedef struct
{
    char *buffer;
    size_t length;
    size_t cap;
    bool overflow;
} http_acc_buffer_t;

//------------------------------------------------------------------------------
// private variables
//------------------------------------------------------------------------------

static const char *TAG = "routes_api_geocode";

//------------------------------------------------------------------------------
// private functions (prototypes)
//------------------------------------------------------------------------------

/**
 * @brief ESP-HTTP-Client event callback — accumulates response body chunks.
 *
 * Appends each incoming data chunk to the caller-supplied @c http_acc_buffer_t.
 * Sets the @c overflow flag and discards further data if the buffer capacity
 * would be exceeded.
 *
 * @param[in] event ESP-HTTP-Client event handle. Must not be NULL.
 * @retval ESP_OK always (required by ESP-IDF callback contract).
 */
static esp_err_t on_data(esp_http_client_event_t *event);

/**
 * @brief HTTP handler for GET /api/geocode?name=<query>.
 *
 * Decodes the query parameter, proxies the request to the Open-Meteo
 * geocoding API, and returns a filtered JSON array of up to
 * GEOCODE_MAX_RESULTS locations.
 *
 * @param[in] request HTTP request handle. Must not be NULL.
 * @retval ESP_OK always (errors are reported via HTTP response).
 */
static esp_err_t api_geocode_get(httpd_req_t *request);

//------------------------------------------------------------------------------
// private functions (implementation)
//------------------------------------------------------------------------------
static esp_err_t on_data(esp_http_client_event_t *event)
{
    if (event->event_id != HTTP_EVENT_ON_DATA)
    {
        return ESP_OK;
    }
    if (event->data == NULL || event->data_len <= 0)
    {
        return ESP_OK;
    }

    http_acc_buffer_t *a = (http_acc_buffer_t *)event->user_data;
    if (a->overflow == true)
    {
        return ESP_OK;
    }

    size_t needed = a->length + (size_t)event->data_len + 1;
    if (needed > a->cap)
    {
        a->overflow = true;
        return ESP_OK;
    }

    memcpy(a->buffer + a->length, event->data, (size_t)event->data_len);
    a->length += (size_t)event->data_len;
    a->buffer[a->length] = '\0';
    return ESP_OK;
}

/* GET /api/geocode?name=<query>
 * Returns up to GEOCODE_MAX_RESULTS matching locations as a JSON array. */
static esp_err_t api_geocode_get(httpd_req_t *request)
{
    /* Read query-string */
    char query[128] = {0};
    if (httpd_req_get_url_query_str(request, query, sizeof(query)) != ESP_OK)
    {
        http_send_err(request, 400, "missing_query");
        return ESP_OK;
    }

    char name_raw[64] = {0};
    if (httpd_query_key_value(query, "name", name_raw, sizeof(name_raw)) != ESP_OK)
    {
        http_send_err(request, 400, "missing_name_param");
        return ESP_OK;
    }

    char name[64] = {0};
    http_url_decode(name_raw, name, sizeof(name));

    if (strlen(name) == 0)
    {
        http_send_err(request, 400, "empty_name");
        return ESP_OK;
    }

    /* Percent-encode the name for the external geocoding API */
    char name_enc[192] = {0};
    size_t j = 0;
    for (size_t i = 0; (name[i] != '\0') && (j < (sizeof(name_enc) - GEOCODE_URL_ENC_MARGIN)); i++)
    {
        unsigned char c = (unsigned char)name[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.')
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
             "?name=%s&count=%d&language=en&format=json",
             name_enc, GEOCODE_MAX_RESULTS);

    ESP_LOGI(TAG, "geocode: '%s' → %s", name, url);

    /* Perform HTTP GET request */
    char *raw = calloc(1, GEOCODE_BUF_SIZE);
    if (raw == NULL)
    {
        http_send_err(request, 500, "oom");
        return ESP_OK;
    }

    http_acc_buffer_t a = {.buffer = raw, .length = 0, .cap = GEOCODE_BUF_SIZE, .overflow = false};

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = GEOCODE_TIMEOUT_MS,
        .event_handler = on_data,
        .user_data = &a,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
    {
        free(raw);
        raw = NULL;
        http_send_err(request, 500, "http_init_failed");
        return ESP_OK;
    }

    esp_err_t error = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (error != ESP_OK || status < 200 || status >= 300 || a.overflow)
    {
        free(raw);
        raw = NULL;
        http_send_err(request, 502, "upstream_error");
        return ESP_OK;
    }

    /* Parse response and extract result fields */
    cJSON *root = cJSON_Parse(raw);
    free(raw);
    raw = NULL;

    if (root == NULL)
    {
        http_send_err(request, 502, "parse_error");
        return ESP_OK;
    }

    cJSON *results = cJSON_GetObjectItemCaseSensitive(root, "results");

    if ((cJSON_IsArray(results) == false) || cJSON_GetArraySize(results) == 0)
    {
        cJSON_Delete(root);
        http_send_json(request, 200, "{\"results\":[]}");
        return ESP_OK;
    }

    /* Build compact response JSON with only the fields we need */
    cJSON *out_root = cJSON_CreateObject();
    cJSON *out_results = cJSON_CreateArray();

    int count = cJSON_GetArraySize(results);
    if (count > GEOCODE_MAX_RESULTS)
    {
        count = GEOCODE_MAX_RESULTS;
    }

    for (int i = 0; i < count; i++)
    {
        cJSON *item = cJSON_GetArrayItem(results, i);
        cJSON *j_name = cJSON_GetObjectItemCaseSensitive(item, "name");
        cJSON *j_lat = cJSON_GetObjectItemCaseSensitive(item, "latitude");
        cJSON *j_lon = cJSON_GetObjectItemCaseSensitive(item, "longitude");
        cJSON *j_cc = cJSON_GetObjectItemCaseSensitive(item, "country_code");
        cJSON *j_admin = cJSON_GetObjectItemCaseSensitive(item, "admin1");
        cJSON *j_tz = cJSON_GetObjectItemCaseSensitive(item, "timezone");

        if ((cJSON_IsString(j_name) == false) || (cJSON_IsNumber(j_lat) == false) ||
            (cJSON_IsNumber(j_lon) == false))
        {
            continue;
        }

        cJSON *entry = cJSON_CreateObject();
        cJSON_AddStringToObject(entry, "name", j_name->valuestring);
        cJSON_AddNumberToObject(entry, "latitude", j_lat->valuedouble);
        cJSON_AddNumberToObject(entry, "longitude", j_lon->valuedouble);
        cJSON_AddStringToObject(entry, "country_code",
                                cJSON_IsString(j_cc) ? j_cc->valuestring : "");
        cJSON_AddStringToObject(entry, "admin1",
                                cJSON_IsString(j_admin) ? j_admin->valuestring : "");
        cJSON_AddStringToObject(entry, "timezone",
                                cJSON_IsString(j_tz) ? j_tz->valuestring : "UTC");
        cJSON_AddItemToArray(out_results, entry);
    }

    cJSON_AddItemToObject(out_root, "results", out_results);
    char *out_str = cJSON_PrintUnformatted(out_root);
    cJSON_Delete(root);
    cJSON_Delete(out_root);

    if (out_str == NULL)
    {
        http_send_err(request, 500, "oom");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "geocode '%s': %d results", name, count);
    http_send_json(request, 200, out_str);
    free(out_str);
    out_str = NULL;
    return ESP_OK;
}

//------------------------------------------------------------------------------
// public functions
//------------------------------------------------------------------------------

/* Defined here (not in private variables) because it references api_geocode_get
 * which is defined above. Forward-declaring a static function pointer is not
 * worth the added complexity. */
static const httpd_uri_t uri_geocode = {
    .uri = "/api/geocode", .method = HTTP_GET, .handler = api_geocode_get};

void routes_api_geocode_register(httpd_handle_t server)
{
    ESP_LOGI(TAG, "register geocode API route");
    httpd_register_uri_handler(server, &uri_geocode);
}