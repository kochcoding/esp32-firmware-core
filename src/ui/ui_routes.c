//------------------------------------------------------------------------------
// private includes
//------------------------------------------------------------------------------
#include "ui/ui_routes.h"

#include "ui/ui_assets.h"

#include "esp_http_server.h"
#include "esp_log.h"

//------------------------------------------------------------------------------
// private defines
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// private typedefs
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// private variables
//------------------------------------------------------------------------------

static const char *TAG = "ui_routes";

//------------------------------------------------------------------------------
// private functions (prototypes)
//------------------------------------------------------------------------------

static esp_err_t index_get_handler(httpd_req_t *request);
static esp_err_t locations_get_handler(httpd_req_t *request);
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
