#include "http_server.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <stdlib.h>

#include "esp_log.h"
#include "esp_http_server.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "core_config.h"
#include "wifi_ap.h"
#include "wifi_sta.h"

#include "esp_system.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_wifi.h"

#include "cJSON.h"

#include "openmeteo_client.h"
#include "storage_locations.h"

static const char *TAG = "http_srv";

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

/* --------- Minimal HTML UI --------- */
static const char *INDEX_HTML =
    "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>ESP32 Setup</title></head><body>"
    "<h2>WiFi Setup</h2>"
    "<form method='POST' action='/save'>"
    "<label>SSID</label><br><input name='ssid' maxlength='31' style='width:100%'><br><br>"
    "<label>Password</label><br><input name='pass' maxlength='63' type='password' style='width:100%'><br><br>"
    "<button type='submit'>Save</button>"
    "</form>"
    "</body></html>";

/* --------- Helpers: parse x-www-form-urlencoded --------- */
static int find_kv(const char *body, const char *key, char *out, size_t out_len)
{
    // Looks for "key=value" in the body, very small/simple parser
    // Assumes ASCII, no percent-decoding for now (good enough for phase 0.6.2)
    const size_t key_len = strlen(key);

    const char *p = body;
    while (p && *p)
    {
        // Start of token
        if (strncmp(p, key, key_len) == 0 && p[key_len] == '=')
        {
            const char *v = p + key_len + 1;
            const char *end = strchr(v, '&');
            size_t len = end ? (size_t)(end - v) : strlen(v);

            if (len >= out_len)
                len = out_len - 1;
            memcpy(out, v, len);
            out[len] = '\0';

            // Convert '+' to space (common in urlencoded)
            for (size_t i = 0; i < len; i++)
                if (out[i] == '+')
                    out[i] = ' ';

            return 1;
        }

        // Next token
        p = strchr(p, '&');
        if (p)
            p++; // skip '&'
    }
    return 0;
}

static void make_location_id(char out[16], float lat, float lon)
{
    // MVP: halbwegs stabiler "Hash" aus lat/lon (nicht kryptographisch)
    int a = (int)(lat * 100.0f);
    int b = (int)(lon * 100.0f);
    snprintf(out, 16, "loc_%d_%d", a, b);
}

static void api_send_json(httpd_req_t *req, const char *status, cJSON *root)
{
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json_str)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON encode failed");
        return;
    }

    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "Connection", "close");

    httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
    cJSON_free(json_str);
}

static esp_err_t api_read_body(httpd_req_t *req, char *buf, size_t buf_len)
{
    if (req->content_len <= 0 || (size_t)req->content_len >= buf_len)
    {
        return ESP_FAIL;
    }

    int received = 0;
    while (received < req->content_len)
    {
        int r = httpd_req_recv(req, buf + received, req->content_len - received);
        if (r <= 0)
            return ESP_FAIL;
        received += r;
    }
    buf[received] = '\0';
    return ESP_OK;
}

static cJSON *api_parse_json_body(httpd_req_t *req, size_t max_len)
{
    // max_len is a hard limit to avoid memory abuse
    if (req->content_len <= 0 || (size_t)req->content_len > max_len)
    {
        return NULL;
    }

    char *buf = calloc(1, req->content_len + 1);
    if (!buf)
        return NULL;

    int received = 0;
    while (received < req->content_len)
    {
        int r = httpd_req_recv(req, buf + received, req->content_len - received);
        if (r <= 0)
        {
            free(buf);
            return NULL;
        }
        received += r;
    }
    buf[req->content_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    return root;
}

static esp_err_t index_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t save_post_handler(httpd_req_t *req)
{
    // Read request body
    int total = req->content_len;
    if (total <= 0 || total > 512)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid body length");
        return ESP_FAIL;
    }

    char *buf = (char *)calloc(1, (size_t)total + 1);
    if (!buf)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }

    int received = 0;
    while (received < total)
    {
        int r = httpd_req_recv(req, buf + received, total - received);
        if (r <= 0)
        {
            free(buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Recv failed");
            return ESP_FAIL;
        }
        received += r;
    }
    buf[total] = '\0';

    char ssid[32] = {0};
    char pass[64] = {0};

    int has_ssid = find_kv(buf, "ssid", ssid, sizeof(ssid));
    int has_pass = find_kv(buf, "pass", pass, sizeof(pass));
    free(buf);

    if (!has_ssid || strlen(ssid) == 0)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing SSID");
        return ESP_FAIL;
    }

    // Save to NVS
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("cfg", NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS open failed");
        return ESP_FAIL;
    }

    err = nvs_set_str(nvs, "sta_ssid", ssid);
    if (err == ESP_OK)
        err = nvs_set_str(nvs, "sta_pass", has_pass ? pass : "");
    if (err == ESP_OK)
        err = nvs_commit(nvs);

    nvs_close(nvs);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS save failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS save failed");
        return ESP_FAIL;
    }

    (void)wifi_sta_connect(ssid, has_pass ? pass : "");

    ESP_LOGI(TAG, "Saved config: ssid='%s' pass_len=%u", ssid, (unsigned)strlen(pass));

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_sendstr(req, "<html><body><h3>Saved.</h3>You can close this page.</body></html>");
}

/* --------- endpoint handler --------- */

static const char *sta_state_to_str(wifi_sta_state_t s)
{
    switch (s)
    {
    case WIFI_STA_STATE_IDLE:
        return "idle";
    case WIFI_STA_STATE_CONNECTING:
        return "connecting";
    case WIFI_STA_STATE_CONNECTED:
        return "connected";
    case WIFI_STA_STATE_FAILED:
        return "failed";
    default:
        return "unknown";
    }
}

static const char *authmode_to_str(wifi_auth_mode_t a)
{
    switch (a)
    {
    case WIFI_AUTH_OPEN:
        return "OPEN";
    case WIFI_AUTH_WEP:
        return "WEP";
    case WIFI_AUTH_WPA_PSK:
        return "WPA_PSK";
    case WIFI_AUTH_WPA2_PSK:
        return "WPA2_PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:
        return "WPA_WPA2_PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE:
        return "WPA2_ENT";
    case WIFI_AUTH_WPA3_PSK:
        return "WPA3_PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK:
        return "WPA2_WPA3_PSK";
    default:
        return "UNKNOWN";
    }
}

static esp_err_t api_status_get(httpd_req_t *req)
{
    uint32_t uptime_s = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    uint32_t heap_free = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    uint16_t clients = wifi_ap_get_client_count();
    wifi_sta_status_t sta = wifi_sta_get_status();

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "uptime_s", uptime_s);
    cJSON_AddNumberToObject(root, "heap_free", heap_free);

    cJSON *ap = cJSON_AddObjectToObject(root, "ap");
    cJSON_AddStringToObject(ap, "ssid", CORE_AP_SSID);
    cJSON_AddStringToObject(ap, "ip", "192.168.4.1");
    cJSON_AddNumberToObject(ap, "clients", clients);

    cJSON *sta_obj = cJSON_AddObjectToObject(root, "sta");
    cJSON_AddStringToObject(sta_obj, "state", sta_state_to_str(sta.state));
    cJSON_AddStringToObject(sta_obj, "ssid", sta.ssid);
    cJSON_AddNumberToObject(sta_obj, "retries", (int)sta.retry_count);

    if (sta.state == WIFI_STA_STATE_CONNECTED &&
        (sta.ip[0] | sta.ip[1] | sta.ip[2] | sta.ip[3]) != 0)
    {
        char ip_str[16];
        snprintf(ip_str, sizeof(ip_str), "%u.%u.%u.%u",
                 (unsigned)sta.ip[0], (unsigned)sta.ip[1],
                 (unsigned)sta.ip[2], (unsigned)sta.ip[3]);
        cJSON_AddStringToObject(sta_obj, "ip", ip_str);
    }
    else
    {
        cJSON_AddNullToObject(sta_obj, "ip");
    }

    cJSON_AddBoolToObject(root, "captive_portal", CORE_CAPTIVE_PORTAL_ENABLED ? true : false);

    api_send_json(req, "200 OK", root);
    return ESP_OK;
}

static esp_err_t api_config_wifi_get(httpd_req_t *req)
{
    char ssid[32] = {0};
    char pass[64] = {0};

    nvs_handle_t nvs;
    esp_err_t err = nvs_open("cfg", NVS_READONLY, &nvs);
    if (err == ESP_OK)
    {
        size_t ssid_len = sizeof(ssid);
        size_t pass_len = sizeof(pass);
        nvs_get_str(nvs, "sta_ssid", ssid, &ssid_len);
        nvs_get_str(nvs, "sta_pass", pass, &pass_len);
        nvs_close(nvs);
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "ssid", ssid);
    cJSON_AddNumberToObject(root, "pass_len", (int)strlen(pass));

    api_send_json(req, "200 OK", root);
    return ESP_OK;
}

static esp_err_t api_config_wifi_post(httpd_req_t *req)
{
    cJSON *root = api_parse_json_body(req, 512);
    if (!root)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_OK;
    }

    const cJSON *ssid = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    const cJSON *pass = cJSON_GetObjectItemCaseSensitive(root, "pass");

    if (!cJSON_IsString(ssid) || ssid->valuestring[0] == '\0')
    {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing ssid");
        return ESP_OK;
    }

    const char *pass_str = (cJSON_IsString(pass) ? pass->valuestring : "");

    nvs_handle_t nvs;
    esp_err_t err = nvs_open("cfg", NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS open failed");
        return ESP_OK;
    }

    err = nvs_set_str(nvs, "sta_ssid", ssid->valuestring);
    if (err == ESP_OK)
        err = nvs_set_str(nvs, "sta_pass", pass_str);
    if (err == ESP_OK)
        err = nvs_commit(nvs);
    nvs_close(nvs);

    // cJSON_Delete(root);

    // if (err != ESP_OK)
    // {
    //     httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS save failed");
    //     return ESP_OK;
    // }

    // cJSON *resp = cJSON_CreateObject();
    // cJSON_AddBoolToObject(resp, "ok", true);
    // api_send_json(req, "201 Created", resp);

    if (err != ESP_OK)
    {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS save failed");
        return ESP_OK;
    }

    // NVS commit ok ...
    esp_err_t conn_err = wifi_sta_connect(ssid->valuestring, pass_str);

    // erst jetzt root freigeben
    cJSON_Delete(root);

    // Response bauen
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON_AddBoolToObject(resp, "connect_started", (conn_err == ESP_OK));
    if (conn_err == ESP_OK)
    {
        cJSON_AddStringToObject(resp, "state", "connecting");
    }
    else
    {
        cJSON_AddStringToObject(resp, "connect_error", esp_err_to_name(conn_err));
    }
    api_send_json(req, "201 Created", resp);

    return ESP_OK;
}

static esp_err_t api_config_wifi_delete(httpd_req_t *req)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("cfg", NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS open failed");
        return ESP_OK;
    }

    nvs_erase_key(nvs, "sta_ssid");
    nvs_erase_key(nvs, "sta_pass");
    err = nvs_commit(nvs);
    nvs_close(nvs);

    if (err != ESP_OK)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS commit failed");
        return ESP_OK;
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    api_send_json(req, "200 OK", resp);
    return ESP_OK;
}

static esp_err_t api_wifi_clients_get(httpd_req_t *req)
{
    uint16_t clients = wifi_ap_get_client_count();

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "clients", clients);

    api_send_json(req, "200 OK", root);
    return ESP_OK;
}

static esp_err_t api_loglevel_put(httpd_req_t *req)
{
    cJSON *root = api_parse_json_body(req, 256);
    if (!root)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_OK;
    }

    const cJSON *level = cJSON_GetObjectItemCaseSensitive(root, "level");
    if (!cJSON_IsNumber(level) || level->valueint < 0 || level->valueint > 5)
    {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid level");
        return ESP_OK;
    }

    esp_log_level_set("*", (esp_log_level_t)level->valueint);
    cJSON_Delete(root);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    api_send_json(req, "200 OK", resp);
    return ESP_OK;
}

static esp_err_t api_loglevel_get(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "level", CORE_LOG_LEVEL_DEFAULT);
    api_send_json(req, "200 OK", root);
    return ESP_OK;
}

static esp_err_t api_reboot_post(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "ok", true);
    api_send_json(req, "200 OK", root);

    vTaskDelay(pdMS_TO_TICKS(100)); // allow response flush
    esp_restart();
    return ESP_OK;
}

static esp_err_t api_wifi_scan_get(httpd_req_t *req)
{
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) != ESP_OK || mode != WIFI_MODE_APSTA)
    {
        // 503 oder 409
    }

    wifi_sta_status_t sta = wifi_sta_get_status();
    if (sta.state == WIFI_STA_STATE_CONNECTING)
    {
        // Connect has priority over scanning in ESP-IDF.
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Retry-After", "2");
        httpd_resp_set_hdr(req, "Cache-Control", "no-store");

        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "error", "busy_connecting");
        cJSON_AddStringToObject(root, "details", "STA is connecting; retry scan after connect completes");
        api_send_json(req, "409 Conflict", root);
        return ESP_OK;
    }

    // Optional: max results via query ?max=10
    int max_results = 10;
    char q[32];
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK)
    {
        char v[8];
        if (httpd_query_key_value(q, "max", v, sizeof(v)) == ESP_OK)
        {
            int m = atoi(v);
            if (m >= 1 && m <= 20)
                max_results = m;
        }
    }

    // try scan
    wifi_scan_config_t scan_cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 300,
    };

    // If a previous scan is still running, stop it first.
    esp_wifi_scan_stop();

    esp_err_t err = esp_wifi_scan_start(&scan_cfg, true /*block*/);
    if (err == ESP_ERR_WIFI_MODE || err == ESP_ERR_WIFI_NOT_STARTED)
    {
        err = esp_wifi_scan_start(&scan_cfg, true);
    }

    if (err == ESP_ERR_WIFI_STATE)
    {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "error", "busy");
        cJSON_AddStringToObject(root, "details", "WiFi busy (connecting or scan in progress). Retry.");
        api_send_json(req, "409 Conflict", root);
        return ESP_OK;
    }

    if (err != ESP_OK)
    {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "error", "scan_failed");
        cJSON_AddStringToObject(root, "details", esp_err_to_name(err));
        api_send_json(req, "500 Internal Server Error", root);
        return ESP_OK;
    }

    uint16_t ap_num = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_num));
    if (ap_num > (uint16_t)max_results)
        ap_num = (uint16_t)max_results;

    wifi_ap_record_t *recs = calloc(ap_num, sizeof(wifi_ap_record_t));
    if (!recs)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_OK;
    }

    uint16_t n = ap_num;
    err = esp_wifi_scan_get_ap_records(&n, recs);
    if (err != ESP_OK)
    {
        free(recs);
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "error", "get_records_failed");
        cJSON_AddStringToObject(root, "details", esp_err_to_name(err));
        api_send_json(req, "500 Internal Server Error", root);
        return ESP_OK;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *arr = cJSON_AddArrayToObject(root, "results");
    cJSON_AddNumberToObject(root, "count", (int)n);

    for (int i = 0; i < (int)n; i++)
    {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "ssid", (const char *)recs[i].ssid);
        cJSON_AddNumberToObject(o, "rssi", recs[i].rssi);
        cJSON_AddStringToObject(o, "auth", authmode_to_str(recs[i].authmode));
        cJSON_AddNumberToObject(o, "chan", recs[i].primary);
        cJSON_AddItemToArray(arr, o);
    }

    free(recs);

    api_send_json(req, "200 OK", root);
    return ESP_OK;
}

static esp_err_t api_locations_post(httpd_req_t *req)
{
    if (!wifi_sta_is_connected())
    {
        wifi_sta_status_t st = wifi_sta_get_status(); // copy

        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "ok", false);
        cJSON_AddStringToObject(resp, "error", "sta_not_connected");
        cJSON_AddStringToObject(resp, "hint", "Connect STA first via /api/config/wifi");

        // optional: Status mitsenden (hilft beim Debuggen)
        cJSON *s = cJSON_AddObjectToObject(resp, "sta");
        cJSON_AddNumberToObject(s, "state", (int)st.state);
        cJSON_AddStringToObject(s, "ssid", st.ssid);

        char ip_str[16] = {0};

        snprintf(ip_str, sizeof(ip_str), "%u.%u.%u.%u",
                 (unsigned)st.ip[0], (unsigned)st.ip[1],
                 (unsigned)st.ip[2], (unsigned)st.ip[3]);
        cJSON_AddStringToObject(s, "ip", ip_str);

        cJSON_AddStringToObject(s, "ip", ip_str);

        api_send_json(req, "409 Conflict", resp);
        return ESP_OK;
    }

    cJSON *root = api_parse_json_body(req, 256);
    if (!root)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_OK;
    }

    const cJSON *q = cJSON_GetObjectItemCaseSensitive(root, "query");
    if (!cJSON_IsString(q) || q->valuestring[0] == '\0')
    {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing query");
        return ESP_OK;
    }

    openmeteo_geocode_result_t geo = {0};
    esp_err_t err = openmeteo_geocode_first(q->valuestring, &geo);

    cJSON_Delete(root);

    if (err == ESP_ERR_NOT_FOUND)
    {
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "ok", false);
        cJSON_AddStringToObject(resp, "error", "location_not_found");
        api_send_json(req, "404 Not Found", resp);
        return ESP_OK;
    }
    if (err != ESP_OK)
    {
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "ok", false);
        cJSON_AddStringToObject(resp, "error", "geocode_failed");
        cJSON_AddStringToObject(resp, "details", esp_err_to_name(err));
        api_send_json(req, "502 Bad Gateway", resp);
        return ESP_OK;
    }

    location_t loc = {0};
    make_location_id(loc.id, geo.lat, geo.lon);
    strncpy(loc.name, geo.name, sizeof(loc.name) - 1);
    loc.lat = geo.lat;
    loc.lon = geo.lon;
    strncpy(loc.timezone, geo.timezone, sizeof(loc.timezone) - 1);
    strncpy(loc.country, geo.country_code, sizeof(loc.country) - 1);

    err = storage_locations_add(&loc, 5 /*max*/);
    if (err != ESP_OK)
    {
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "ok", false);
        cJSON_AddStringToObject(resp, "error", "store_failed");
        cJSON_AddStringToObject(resp, "details", esp_err_to_name(err));
        api_send_json(req, "500 Internal Server Error", resp);
        return ESP_OK;
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);

    cJSON *saved = cJSON_AddObjectToObject(resp, "location");
    cJSON_AddStringToObject(saved, "id", loc.id);
    cJSON_AddStringToObject(saved, "name", loc.name);
    cJSON_AddNumberToObject(saved, "lat", loc.lat);
    cJSON_AddNumberToObject(saved, "lon", loc.lon);
    cJSON_AddStringToObject(saved, "timezone", loc.timezone);
    cJSON_AddStringToObject(saved, "country", loc.country);

    api_send_json(req, "201 Created", resp);
    return ESP_OK;
}

static esp_err_t api_locations_get(httpd_req_t *req)
{
    char *arr_json = NULL;
    esp_err_t err = storage_locations_get_all_json(&arr_json);
    if (err != ESP_OK)
    {
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "ok", false);
        cJSON_AddStringToObject(resp, "error", "read_failed");
        cJSON_AddStringToObject(resp, "details", esp_err_to_name(err));
        api_send_json(req, "500 Internal Server Error", resp);
        return ESP_OK;
    }

    cJSON *arr = cJSON_Parse(arr_json ? arr_json : "[]");
    free(arr_json);

    if (!arr || !cJSON_IsArray(arr))
    {
        cJSON_Delete(arr);
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "ok", false);
        cJSON_AddStringToObject(resp, "error", "json_parse_failed");
        api_send_json(req, "500 Internal Server Error", resp);
        return ESP_OK;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "ok", true);
    cJSON_AddNumberToObject(root, "count", cJSON_GetArraySize(arr));
    cJSON_AddItemToObject(root, "locations", arr); // root übernimmt ownership von arr

    api_send_json(req, "200 OK", root);
    return ESP_OK;
}

static esp_err_t api_locations_delete_handler(httpd_req_t *req)
{
    // Query auslesen
    char query[128] = {0};
    char id[64] = {0};

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK)
    {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"missing query\"}");
        return ESP_OK;
    }

    if (httpd_query_key_value(query, "id", id, sizeof(id)) != ESP_OK)
    {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"missing id\"}");
        return ESP_OK;
    }

    esp_err_t err = storage_locations_delete_by_id(id);

    if (err == ESP_ERR_NOT_FOUND)
    {
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"not found\"}");
        return ESP_OK;
    }

    if (err != ESP_OK)
    {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"delete failed\"}");
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t api_locations_delete(httpd_req_t *req)
{
    char query[128] = {0};
    char id[64] = {0};

    // Query vorhanden?
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
        httpd_query_key_value(query, "id", id, sizeof(id)) == ESP_OK &&
        id[0] != '\0')
    {
        esp_err_t err = storage_locations_remove_by_id(id);
        if (err == ESP_ERR_NOT_FOUND)
        {
            httpd_resp_set_status(req, "404 Not Found");
            httpd_resp_sendstr(req, "not found");
            return ESP_OK;
        }
        if (err != ESP_OK)
        {
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_sendstr(req, "delete failed");
            return ESP_OK;
        }
        httpd_resp_sendstr(req, "deleted");
        return ESP_OK;
    }

    // Kein id=... => alles löschen
    esp_err_t err = storage_locations_clear_all();
    if (err != ESP_OK)
    {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "clear failed");
        return ESP_OK;
    }
    httpd_resp_sendstr(req, "cleared");
    return ESP_OK;
}

static esp_err_t api_locations_active_get(httpd_req_t *req)
{
    char active_id[64] = {0};

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "ok", true);

    if (storage_locations_get_active_id(active_id, sizeof(active_id)) == ESP_OK)
    {
        cJSON_AddStringToObject(root, "active_id", active_id);

        location_t loc = {0};
        if (storage_locations_get_by_id(active_id, &loc) == ESP_OK)
        {
            cJSON *o = cJSON_CreateObject();
            cJSON_AddStringToObject(o, "id", loc.id);
            cJSON_AddStringToObject(o, "name", loc.name);
            cJSON_AddNumberToObject(o, "lat", loc.lat);
            cJSON_AddNumberToObject(o, "lon", loc.lon);
            cJSON_AddStringToObject(o, "timezone", loc.timezone);
            cJSON_AddStringToObject(o, "country", loc.country);
            cJSON_AddItemToObject(root, "location", o);
        }
    }
    else
    {
        cJSON_AddNullToObject(root, "active_id");
    }

    api_send_json(req, "200 OK", root);
    return ESP_OK;
}

static esp_err_t api_locations_active_post(httpd_req_t *req)
{
    char body[256] = {0};
    if (api_read_body(req, body, sizeof(body)) != ESP_OK)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid body");
        return ESP_OK;
    }

    cJSON *json = cJSON_Parse(body);
    if (!json)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_OK;
    }

    cJSON *id = cJSON_GetObjectItem(json, "id");
    if (!cJSON_IsString(id) || !id->valuestring || id->valuestring[0] == '\0')
    {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing id");
        return ESP_OK;
    }

    esp_err_t e = storage_locations_set_active_id(id->valuestring);
    cJSON_Delete(json);

    if (e == ESP_ERR_NOT_FOUND)
    {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Location not found");
        return ESP_OK;
    }
    if (e != ESP_OK)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to set active");
        return ESP_OK;
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON_AddStringToObject(resp, "active_id", id->valuestring);
    api_send_json(req, "200 OK", resp);
    return ESP_OK;
}

// --------- Open-Meteo weather endpoints ---------

static esp_err_t api_weather_current_get(httpd_req_t *req)
{
    char query[128] = {0};
    char id[64] = {0};

    location_t loc = {0};

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
        httpd_query_key_value(query, "id", id, sizeof(id)) == ESP_OK &&
        id[0] != '\0')
    {
        esp_err_t err = storage_locations_get_by_id(id, &loc);
        if (err != ESP_OK)
        {
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Location not found");
            return ESP_OK;
        }
    }
    else
    {
        location_t loc = {0};
        bool have_loc = false;

        // 1) id aus Query?
        char query[128] = {0};
        char id[64] = {0};
        if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
            httpd_query_key_value(query, "id", id, sizeof(id)) == ESP_OK)
        {
            if (storage_locations_get_by_id(id, &loc) == ESP_OK)
            {
                have_loc = true;
            }
        }

        // 2) sonst active?
        if (!have_loc)
        {
            char active_id[64] = {0};
            if (storage_locations_get_active_id(active_id, sizeof(active_id)) == ESP_OK &&
                storage_locations_get_by_id(active_id, &loc) == ESP_OK)
            {
                have_loc = true;
            }
        }

        // 3) sonst first
        if (!have_loc)
        {
            if (storage_locations_get_first(&loc) == ESP_OK)
            {
                have_loc = true;
            }
        }

        if (!have_loc)
        {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No location available");
            return ESP_OK;
        }
    }

    char *body = NULL;
    openmeteo_status_t st = openmeteo_fetch_current_json(loc.lat, loc.lon, loc.timezone, &body);
    if (st != OPENMETEO_OK || !body)
    {
        httpd_resp_set_status(req, "502 Bad Gateway");
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_send(req, "Open-Meteo request failed", HTTPD_RESP_USE_STRLEN);

        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
    free(body);
    return ESP_OK;
}

static esp_err_t api_weather_forecast_get(httpd_req_t *req)
{
    char query[128] = {0};
    char id[64] = {0};
    char days_s[16] = {0};

    int days = 7;
    location_t loc = {0};

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
    {
        if (httpd_query_key_value(query, "days", days_s, sizeof(days_s)) == ESP_OK)
        {
            days = atoi(days_s);
            if (days <= 0)
                days = 7;
        }
        if (httpd_query_key_value(query, "id", id, sizeof(id)) == ESP_OK && id[0] != '\0')
        {
            esp_err_t err = storage_locations_get_by_id(id, &loc);
            if (err != ESP_OK)
            {
                httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Location not found");
                return ESP_OK;
            }
        }
    }

    if (loc.id[0] == '\0')
    {
        location_t loc = {0};
        bool have_loc = false;

        // 1) id aus Query?
        char query[128] = {0};
        char id[64] = {0};
        if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
            httpd_query_key_value(query, "id", id, sizeof(id)) == ESP_OK)
        {
            if (storage_locations_get_by_id(id, &loc) == ESP_OK)
            {
                have_loc = true;
            }
        }

        // 2) sonst active?
        if (!have_loc)
        {
            char active_id[64] = {0};
            if (storage_locations_get_active_id(active_id, sizeof(active_id)) == ESP_OK &&
                storage_locations_get_by_id(active_id, &loc) == ESP_OK)
            {
                have_loc = true;
            }
        }

        // 3) sonst first
        if (!have_loc)
        {
            if (storage_locations_get_first(&loc) == ESP_OK)
            {
                have_loc = true;
            }
        }

        if (!have_loc)
        {
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "No location available");
            return ESP_OK;
        }
    }

    char *body = NULL;
    openmeteo_status_t st = openmeteo_fetch_forecast_json(loc.lat, loc.lon, loc.timezone, days, &body);
    if (st != OPENMETEO_OK || !body)
    {
        httpd_resp_set_status(req, "502 Bad Gateway");
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_send(req, "Open-Meteo request failed", HTTPD_RESP_USE_STRLEN);

        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
    free(body);
    return ESP_OK;
}

/* --------- Captive portal helpers ---------
 * Goal: be maximally compatible with OS captive portal probes.
 *
 * Key rules:
 * - Never answer captive probes with 404.
 * - Redirect all non-portal paths to the landing page.
 * - Keep responses small and close connections quickly to avoid socket exhaustion.
 */
static esp_err_t captive_redirect_to_root(httpd_req_t *req)
{
    // Use an absolute URL for best cross-platform compatibility.
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");

    // Reduce connection lifetime / resource usage.
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_set_type(req, "text/plain");

    // Empty body is fine.
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t captive_404_handler(httpd_req_t *req, httpd_err_code_t err)
{
    (void)err;

#if CORE_STATUS_API_ENABLED
    if (strncmp(req->uri, "/api/", 5) == 0)
    {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "error", "not_found");
        api_send_json(req, "404 Not Found", root);
        return ESP_OK;
    }
#endif

    return captive_redirect_to_root(req);
}

static esp_err_t favicon_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, NULL, 0);
}

static const httpd_uri_t uri_fav = {
    .uri = "/favicon.ico",
    .method = HTTP_GET,
    .handler = favicon_handler,
    .user_ctx = NULL};

static const httpd_uri_t uri_index = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_get_handler,
    .user_ctx = NULL};

static const httpd_uri_t uri_save = {
    .uri = "/save",
    .method = HTTP_POST,
    .handler = save_post_handler,
    .user_ctx = NULL};

/* Captive minimal probes (GET only) */
static const httpd_uri_t uri_generate_204 = {
    .uri = "/generate_204",
    .method = HTTP_GET,
    .handler = captive_redirect_to_root,
    .user_ctx = NULL};

static const httpd_uri_t uri_hotspot_detect = {
    .uri = "/hotspot-detect.html",
    .method = HTTP_GET,
    .handler = captive_redirect_to_root,
    .user_ctx = NULL};

static const httpd_uri_t uri_connecttest = {
    .uri = "/connecttest.txt",
    .method = HTTP_GET,
    .handler = captive_redirect_to_root,
    .user_ctx = NULL};

static const httpd_uri_t uri_204 = {
    .uri = "/204",
    .method = HTTP_GET,
    .handler = captive_redirect_to_root,
    .user_ctx = NULL};

static const httpd_uri_t uri_ipv6check = {
    .uri = "/ipv6check",
    .method = HTTP_GET,
    .handler = captive_redirect_to_root,
    .user_ctx = NULL};

#if CORE_STATUS_API_ENABLED
static const httpd_uri_t uri_api_status = {
    .uri = "/api/status",
    .method = HTTP_GET,
    .handler = api_status_get};

static const httpd_uri_t uri_api_cfg_get = {
    .uri = "/api/config/wifi",
    .method = HTTP_GET,
    .handler = api_config_wifi_get};

static const httpd_uri_t uri_api_cfg_post = {
    .uri = "/api/config/wifi",
    .method = HTTP_POST,
    .handler = api_config_wifi_post};

static const httpd_uri_t uri_api_cfg_del = {
    .uri = "/api/config/wifi",
    .method = HTTP_DELETE,
    .handler = api_config_wifi_delete};

static const httpd_uri_t uri_api_clients = {
    .uri = "/api/wifi/clients",
    .method = HTTP_GET,
    .handler = api_wifi_clients_get};

static const httpd_uri_t uri_api_ll_get = {
    .uri = "/api/loglevel",
    .method = HTTP_GET,
    .handler = api_loglevel_get};

static const httpd_uri_t uri_api_ll_put = {
    .uri = "/api/loglevel",
    .method = HTTP_PUT,
    .handler = api_loglevel_put};

static const httpd_uri_t uri_api_reboot = {
    .uri = "/api/reboot",
    .method = HTTP_POST,
    .handler = api_reboot_post};

static const httpd_uri_t uri_api_wifi_scan = {
    .uri = "/api/wifi/scan",
    .method = HTTP_GET,
    .handler = api_wifi_scan_get};

static const httpd_uri_t uri_api_locations_post = {
    .uri = "/api/locations",
    .method = HTTP_POST,
    .handler = api_locations_post};

static const httpd_uri_t uri_api_locations_get = {
    .uri = "/api/locations",
    .method = HTTP_GET,
    .handler = api_locations_get};

httpd_uri_t uri_api_locations_delete = {
    .uri = "/api/locations",
    .method = HTTP_DELETE,
    .handler = api_locations_delete_handler,
    .user_ctx = NULL};

httpd_uri_t uri_locations_delete = {
    .uri = "/api/locations",
    .method = HTTP_DELETE,
    .handler = api_locations_delete,
    .user_ctx = NULL};

httpd_uri_t uri_api_weather_current = {
    .uri = "/api/weather/current",
    .method = HTTP_GET,
    .handler = api_weather_current_get};

httpd_uri_t uri_api_weather_forecast = {
    .uri = "/api/weather/forecast",
    .method = HTTP_GET,
    .handler = api_weather_forecast_get};

httpd_uri_t uri_api_locations_active_get = {
    .uri = "/api/locations/active",
    .method = HTTP_GET,
    .handler = api_locations_active_get};

httpd_uri_t uri_api_locations_active_post = {
    .uri = "/api/locations/active",
    .method = HTTP_POST,
    .handler = api_locations_active_post};

#endif

void http_server_start(void)
{
    ESP_LOGW(TAG, "http_server_start() from http_server.c: BUILD_MARKER_001");

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Keep your tuned settings
    config.lru_purge_enable = true;
    config.max_open_sockets = 7;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;

    config.max_uri_handlers = 24;

    config.stack_size = 12288; // 12 KB, MVP-stabil. Später evtl. optimieren.

    httpd_handle_t server = NULL;
    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        return;
    }

// Helper macro so we see registration failures (slots, duplicates, etc.)
#define REG(uri_ptr)                                                                                           \
    do                                                                                                         \
    {                                                                                                          \
        esp_err_t __e = httpd_register_uri_handler(server, (uri_ptr));                                         \
        ESP_LOGW(TAG, "register %s method=%d -> %s", (uri_ptr)->uri, (uri_ptr)->method, esp_err_to_name(__e)); \
    } while (0)

    // --- Web UI ---
    // These must be file-scope static const httpd_uri_t: uri_fav, uri_index, uri_save
    REG(&uri_fav);
    REG(&uri_index);
    REG(&uri_save);

    // --- Captive portal probe endpoints (GET only) ---
    // These must be file-scope static const httpd_uri_t:
    // uri_generate_204, uri_hotspot_detect, uri_connecttest
    REG(&uri_generate_204);   // Android
    REG(&uri_hotspot_detect); // iOS/macOS
    REG(&uri_connecttest);    // Windows

    REG(&uri_204);
    REG(&uri_ipv6check);

    // Catch-all for everything else (redirect to root), but keep /api/* as JSON 404
    // captive_404_handler already does the "/api/" exception in your code.
    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, captive_404_handler);

#if CORE_STATUS_API_ENABLED
    // --- API ---
    // These must be file-scope static const httpd_uri_t:
    // uri_api_status, uri_api_cfg_get, uri_api_cfg_post, uri_api_cfg_del,
    // uri_api_clients, uri_api_ll_get, uri_api_ll_put, uri_api_reboot
    REG(&uri_api_status);

    REG(&uri_api_cfg_get);
    REG(&uri_api_cfg_post);
    REG(&uri_api_cfg_del);

    REG(&uri_api_clients);

    REG(&uri_api_ll_get);
    REG(&uri_api_ll_put);

    REG(&uri_api_reboot);

    REG(&uri_api_wifi_scan);

    REG(&uri_api_locations_post);
    REG(&uri_api_locations_get);
    REG(&uri_api_locations_delete);
    REG(&uri_locations_delete);

    REG(&uri_api_weather_current);
    REG(&uri_api_weather_forecast);

    REG(&uri_api_locations_active_get);
    REG(&uri_api_locations_active_post);

#endif

#undef REG

    ESP_LOGI(TAG, "HTTP server started. Open http://192.168.4.1/");
}
