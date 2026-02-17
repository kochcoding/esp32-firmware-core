#include "esp_log.h"

#include "http/routes_portal.h"

static const char *TAG = "routes_portal";

// Redirect helper
static esp_err_t redirect_to_root(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}

// 404 handler: redirect everything EXCEPT /api/* (keep API 404 as JSON)
static esp_err_t captive_404_handler(httpd_req_t *req, httpd_err_code_t err)
{
    (void)err;

    const char *uri = req->uri;
    if (strncmp(uri, "/api/", 5) == 0)
    {
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"not_found\"}");
        return ESP_OK;
    }

    return redirect_to_root(req);
}

static const httpd_uri_t uri_generate_204 = {.uri = "/generate_204", .method = HTTP_GET, .handler = redirect_to_root};
static const httpd_uri_t uri_hotspot_detect = {.uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = redirect_to_root};
static const httpd_uri_t uri_connecttest = {.uri = "/connecttest.txt", .method = HTTP_GET, .handler = redirect_to_root};
static const httpd_uri_t uri_204 = {.uri = "/204", .method = HTTP_GET, .handler = redirect_to_root};
static const httpd_uri_t uri_ipv6check = {.uri = "/ipv6check", .method = HTTP_GET, .handler = redirect_to_root};

void routes_portal_register(httpd_handle_t server)
{
    ESP_LOGI(TAG, "register captive portal probe routes");

    httpd_register_uri_handler(server, &uri_generate_204);
    httpd_register_uri_handler(server, &uri_hotspot_detect);
    httpd_register_uri_handler(server, &uri_connecttest);
    httpd_register_uri_handler(server, &uri_204);
    httpd_register_uri_handler(server, &uri_ipv6check);

    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, captive_404_handler);
}