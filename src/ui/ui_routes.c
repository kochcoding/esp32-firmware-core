#include "ui/ui_routes.h"
#include "ui/ui_assets.h"

#include <string.h>
#include <stdlib.h>

#include "esp_log.h"

#include "app/app_settings_persistence.h"
#include "wifi_sta.h"

static const char *TAG = "ui_routes";

static int find_kv(const char *body, const char *key, char *out, size_t out_len)
{
    const size_t key_len = strlen(key);
    const char *p = body;
    while (p && *p)
    {
        if (strncmp(p, key, key_len) == 0 && p[key_len] == '=')
        {
            const char *v = p + key_len + 1;
            const char *end = strchr(v, '&');
            size_t len = end ? (size_t)(end - v) : strlen(v);

            if (len >= out_len)
            {
                len = out_len - 1;
            }
            (void)memcpy(out, v, len);
            out[len] = '\0';

            for (size_t i = 0; i < len; i++)
            {
                if (out[i] == '+')
                {
                    out[i] = ' ';
                }
            }

            return 1;
        }

        p = strchr(p, '&');
        if (p)
        {
            p++;
        }
    }

    return 0;
}

static esp_err_t index_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, UI_INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t save_post_handler(httpd_req_t *req)
{
    int total = req->content_len;
    if (total <= 0 || total > 512)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid body length");
        return ESP_OK;
    }

    char *buf = calloc(1, (size_t)total + 1);
    if (!buf)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_OK;
    }

    int received = 0;
    while (received < total)
    {
        int r = httpd_req_recv(req, buf + received, total - received);
        if (r <= 0)
        {
            free(buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Recv failed");
            return ESP_OK;
        }
        received += r;
    }
    buf[total] = '\0';

    settings_wifi_t s = {0};
    char ssid[32] = {0};
    char pass[64] = {0};

    int has_ssid = find_kv(buf, "ssid", ssid, sizeof(ssid));
    int has_pass = find_kv(buf, "pass", pass, sizeof(pass));
    free(buf);

    if (!has_ssid || ssid[0] == '\0')
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing SSID");
        return ESP_OK;
    }

    strncpy(s.ssid, ssid, sizeof(s.ssid) - 1);
    strncpy(s.pass, has_pass ? pass : "", sizeof(s.pass) - 1);

    esp_err_t err = app_settings_save_wifi(&s);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "save wifi failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Save failed");
        return ESP_OK;
    }

    (void)wifi_sta_connect(s.ssid, s.pass);

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_sendstr(req, "<html><body><h3>Saved.</h3>You can close this page.</body></html>");
}

static esp_err_t favicon_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t uri_index = {.uri = "/", .method = HTTP_GET, .handler = index_get_handler};
static const httpd_uri_t uri_save = {.uri = "/save", .method = HTTP_POST, .handler = save_post_handler};
static const httpd_uri_t uri_favicon = {.uri = "/favicon.ico", .method = HTTP_GET, .handler = favicon_handler};

void ui_routes_register(httpd_handle_t server)
{
    httpd_register_uri_handler(server, &uri_index);
    httpd_register_uri_handler(server, &uri_save);
    httpd_register_uri_handler(server, &uri_favicon);
}
