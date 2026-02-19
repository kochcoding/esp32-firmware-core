#include "ui/ui_routes.h"
#include "ui/ui_assets.h"

#include "esp_log.h"
#include "esp_http_server.h"

static const char *TAG = "ui_routes";

static esp_err_t index_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, UI_INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t favicon_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t uri_index = {.uri = "/", .method = HTTP_GET, .handler = index_get_handler};
static const httpd_uri_t uri_favicon = {.uri = "/favicon.ico", .method = HTTP_GET, .handler = favicon_handler};

void ui_routes_register(httpd_handle_t server)
{
    ESP_LOGI(TAG, "register UI routes");
    httpd_register_uri_handler(server, &uri_index);
    httpd_register_uri_handler(server, &uri_favicon);
}
