/**
 * @file routes_portal.c
 * @brief Implementation of captive portal detection and redirect handlers.
 *
 * @details
 *  - Intercepts OS-specific probe URLs (Android, iOS, Windows) and redirects
 *    them to the root UI via HTTP 302.
 *  - Installs a global 404 handler that redirects unknown paths to "/",
 *    except for /api/* routes which receive a JSON 404 response.
 */

//------------------------------------------------------------------------------
// private includes
//------------------------------------------------------------------------------
#include "http/routes_portal.h"

#include "esp_log.h"

//------------------------------------------------------------------------------
// private defines
//------------------------------------------------------------------------------
#define API_PREFIX "/api/"
#define API_PREFIX_LEN (5U)

//------------------------------------------------------------------------------
// private variables
//------------------------------------------------------------------------------
static const char *TAG = "routes_portal";

//------------------------------------------------------------------------------
// private functions (prototypes)
//------------------------------------------------------------------------------

/**
 * @brief Sends an HTTP 302 redirect response to the root path "/".
 *
 * @param[in] request HTTP request handle. Must not be NULL.
 * @retval ESP_OK always.
 */
static esp_err_t redirect_to_root(httpd_req_t *request);

/**
 * @brief Global 404 error handler for the captive portal.
 *
 * Redirects all unmatched paths to "/" except /api/* routes,
 * which receive a JSON 404 response to preserve API error contracts.
 *
 * @param[in] request HTTP request handle. Must not be NULL.
 * @param[in] error   ESP-IDF HTTP error code (unused, always HTTPD_404_NOT_FOUND).
 * @retval ESP_OK always.
 */
static esp_err_t captive_404_handler(httpd_req_t *request, httpd_err_code_t error);

//------------------------------------------------------------------------------
// private functions (implementation)
//------------------------------------------------------------------------------

/* Redirect helper */
static esp_err_t redirect_to_root(httpd_req_t *request)
{
    httpd_resp_set_status(request, "302 Found");
    httpd_resp_set_hdr(request, "Location", "/");
    httpd_resp_send(request, NULL, 0);

    return ESP_OK;
}

/* 404 handler: redirect everything EXCEPT /api/* (keep API 404 as JSON) */
static esp_err_t captive_404_handler(httpd_req_t *request, httpd_err_code_t error)
{
    (void)error;

    const char *uri = request->uri;
    if (strncmp(uri, API_PREFIX, API_PREFIX_LEN) == 0)
    {
        httpd_resp_set_status(request, "404 Not Found");
        httpd_resp_set_type(request, "application/json");
        httpd_resp_sendstr(request, "{\"ok\":false,\"error\":\"not_found\"}");
        return ESP_OK;
    }

    return redirect_to_root(request);
}

//------------------------------------------------------------------------------
// public functions
//------------------------------------------------------------------------------

/* Defined here (not in private variables) because they reference the
 * static handler functions declared above. */

/* GET routes */
static const httpd_uri_t uri_generate_204 = {
    .uri = "/generate_204", .method = HTTP_GET, .handler = redirect_to_root};
static const httpd_uri_t uri_hotspot_detect = {
    .uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = redirect_to_root};
static const httpd_uri_t uri_connecttest = {
    .uri = "/connecttest.txt", .method = HTTP_GET, .handler = redirect_to_root};
static const httpd_uri_t uri_204 = {.uri = "/204", .method = HTTP_GET, .handler = redirect_to_root};
static const httpd_uri_t uri_ipv6check = {
    .uri = "/ipv6check", .method = HTTP_GET, .handler = redirect_to_root};

/* HEAD routes */
static const httpd_uri_t uri_generate_204_head = {
    .uri = "/generate_204", .method = HTTP_HEAD, .handler = redirect_to_root};
static const httpd_uri_t uri_hotspot_detect_head = {
    .uri = "/hotspot-detect.html", .method = HTTP_HEAD, .handler = redirect_to_root};
static const httpd_uri_t uri_connecttest_head = {
    .uri = "/connecttest.txt", .method = HTTP_HEAD, .handler = redirect_to_root};
static const httpd_uri_t uri_204_head = {
    .uri = "/204", .method = HTTP_HEAD, .handler = redirect_to_root};
static const httpd_uri_t uri_ipv6check_head = {
    .uri = "/ipv6check", .method = HTTP_HEAD, .handler = redirect_to_root};

void routes_portal_register(httpd_handle_t server)
{
    ESP_LOGI(TAG, "register captive portal probe routes");

    /* GET */
    httpd_register_uri_handler(server, &uri_generate_204);
    httpd_register_uri_handler(server, &uri_hotspot_detect);
    httpd_register_uri_handler(server, &uri_connecttest);
    httpd_register_uri_handler(server, &uri_204);
    httpd_register_uri_handler(server, &uri_ipv6check);

    /* HEAD */
    httpd_register_uri_handler(server, &uri_generate_204_head);
    httpd_register_uri_handler(server, &uri_hotspot_detect_head);
    httpd_register_uri_handler(server, &uri_connecttest_head);
    httpd_register_uri_handler(server, &uri_204_head);
    httpd_register_uri_handler(server, &uri_ipv6check_head);

    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, captive_404_handler);
}