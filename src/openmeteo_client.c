#include "openmeteo_client.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "esp_http_client.h"
#include "esp_log.h"

static const char *TAG = "openmeteo";

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
        ESP_LOGE(TAG, "response exceeds buffer (%u > %u)",
                 (unsigned)needed, (unsigned)a->cap);
        a->overflow = true;
        return ESP_OK;
    }

    memcpy(a->buf + a->len, evt->data, (size_t)evt->data_len);
    a->len += (size_t)evt->data_len;
    a->buf[a->len] = '\0';
    return ESP_OK;
}

static openmeteo_status_t http_get(const char *url, char *out_buf, size_t out_len)
{
    acc_t a = {
        .buf = out_buf,
        .len = 0,
        .cap = out_len,
        .overflow = false,
    };

    out_buf[0] = '\0';

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 10000,
        .event_handler = on_data,
        .user_data = &a,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client)
        return OPENMETEO_ERR_OOM;

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (a.overflow)
        return OPENMETEO_ERR_BUF_TOO_SMALL;

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "HTTP error: %s", esp_err_to_name(err));
        return OPENMETEO_ERR_HTTP;
    }

    if (status < 200 || status >= 300)
    {
        ESP_LOGE(TAG, "HTTP status %d", status);
        return OPENMETEO_ERR_HTTP;
    }

    ESP_LOGI(TAG, "HTTP OK status=%d body_len=%u", status, (unsigned)a.len);
    return OPENMETEO_OK;
}

openmeteo_status_t openmeteo_fetch_current(double lat, double lon, char *out_buf, size_t out_len)
{
    if (!out_buf || out_len == 0)
        return OPENMETEO_ERR_INVALID_ARG;

    char url[512];
    snprintf(url, sizeof(url),
             "http://api.open-meteo.com/v1/forecast"
             "?latitude=%.5f&longitude=%.5f"
             "&current=temperature_2m,apparent_temperature,"
             "relative_humidity_2m,weather_code,"
             "wind_speed_10m,wind_direction_10m",
             lat, lon);

    ESP_LOGI(TAG, "fetch current: lat=%.4f lon=%.4f", lat, lon);
    return http_get(url, out_buf, out_len);
}

openmeteo_status_t openmeteo_fetch_forecast(double lat, double lon, int days, char *out_buf, size_t out_len)
{
    if (!out_buf || out_len == 0)
        return OPENMETEO_ERR_INVALID_ARG;

    if (days <= 0)
        days = 7;
    if (days > 16)
        days = 16;

    char url[640];
    snprintf(url, sizeof(url),
             "http://api.open-meteo.com/v1/forecast"
             "?latitude=%.5f&longitude=%.5f"
             "&daily=weather_code,temperature_2m_max,"
             "temperature_2m_min,precipitation_sum"
             "&forecast_days=%d",
             lat, lon, days);

    ESP_LOGI(TAG, "fetch forecast: lat=%.4f lon=%.4f days=%d", lat, lon, days);
    return http_get(url, out_buf, out_len);
}