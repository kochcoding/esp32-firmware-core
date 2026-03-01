/**
 * @file ui_routes.c
 * @brief Implementation of HTTP handlers for embedded UI pages.
 *
 * @details
 *  - GET /            — serves UI_INDEX_HTML with Cache-Control: no-store.
 *  - GET /locations   — serves UI_LOCATIONS_HTML with Cache-Control: no-store.
 *  - GET /favicon.ico — returns 204 No Content to suppress browser requests.
 */

//------------------------------------------------------------------------------
// private includes
//------------------------------------------------------------------------------
#include "ui/ui_routes.h"

#include "ui/ui_assets.h"

#include "esp_http_server.h"
#include "esp_log.h"

//------------------------------------------------------------------------------
// private variables
//------------------------------------------------------------------------------

static const char *TAG = "ui_routes";

//------------------------------------------------------------------------------
// private functions (prototypes)
//------------------------------------------------------------------------------

/**
 * @brief HTTP handler for GET /.
 *
 * Serves the embedded WiFi setup portal HTML with cache disabled.
 *
 * @param[in] request HTTP request handle. Must not be NULL.
 * @retval ESP_OK always.
 */
static esp_err_t index_get_handler(httpd_req_t *request);

/**
 * @brief HTTP handler for GET /locations.
 *
 * Serves the embedded location management HTML with cache disabled.
 *
 * @param[in] request HTTP request handle. Must not be NULL.
 * @retval ESP_OK always.
 */
static esp_err_t locations_get_handler(httpd_req_t *request);

/**
 * @brief HTTP handler for GET /favicon.ico.
 *
 * Returns 204 No Content to suppress repeated browser favicon requests
 * without serving an actual icon file.
 *
 * @param[in] request HTTP request handle. Must not be NULL.
 * @retval ESP_OK always.
 */
static esp_err_t favicon_handler(httpd_req_t *request);

//------------------------------------------------------------------------------
// private functions (implementation)
//------------------------------------------------------------------------------

static esp_err_t index_get_handler(httpd_req_t *request)
{
    httpd_resp_set_type(request, "text/html");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, UI_INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t locations_get_handler(httpd_req_t *request)
{
    httpd_resp_set_type(request, "text/html");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, UI_LOCATIONS_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t favicon_handler(httpd_req_t *request)
{
    httpd_resp_set_status(request, "204 No Content");
    httpd_resp_send(request, NULL, 0);
    return ESP_OK;
}

//------------------------------------------------------------------------------
// public functions
//------------------------------------------------------------------------------

/* Defined here (not in private variables) because they reference the
 * static handler functions declared above. */
static const httpd_uri_t uri_index = {.uri = "/", .method = HTTP_GET, .handler = index_get_handler};
static const httpd_uri_t uri_locations = {
    .uri = "/locations", .method = HTTP_GET, .handler = locations_get_handler};
static const httpd_uri_t uri_favicon = {
    .uri = "/favicon.ico", .method = HTTP_GET, .handler = favicon_handler};

void ui_routes_register(httpd_handle_t server)
{
    ESP_LOGI(TAG, "register UI routes");
    httpd_register_uri_handler(server, &uri_index);
    httpd_register_uri_handler(server, &uri_locations);
    httpd_register_uri_handler(server, &uri_favicon);
}
