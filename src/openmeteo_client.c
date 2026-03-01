/**
 * @file openmeteo_client.c
 * @brief Implementation of the Open-Meteo weather API HTTP client.
 *
 * @details
 *  - Uses esp_http_client with a streaming event handler to accumulate
 *    the response body into a caller-supplied buffer.
 *  - Retries up to HTTP_MAX_RETRIES times on connection failures.
 *  - Buffer overflow is detected in the event handler and reported via
 *    OPENMETEO_ERR_BUF_TOO_SMALL.
 */

//------------------------------------------------------------------------------
// private includes
//------------------------------------------------------------------------------
#include "openmeteo_client.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_http_client.h"
#include "esp_log.h"

//------------------------------------------------------------------------------
// private defines
//------------------------------------------------------------------------------

#define HTTP_MAX_RETRIES (3)
#define HTTP_RETRY_DELAY_MS (2000U)
#define HTTP_TIMEOUT_MS (10000U)
#define OPENMETEO_URL_BUF_SIZE (512U)
#define OPENMETEO_FORECAST_URL_BUF_SIZE (640U)
#define OPENMETEO_MIN_DAYS (1)
#define OPENMETEO_MAX_DAYS (16)
#define OPENMETEO_DEFAULT_DAYS (7)

//------------------------------------------------------------------------------
// private typedefs
//------------------------------------------------------------------------------

typedef struct
{
    char *buffer;
    size_t length;
    size_t capacity;
    bool overflow;
} openmeteo_acc_buffer_t;

//------------------------------------------------------------------------------
// private variables
//------------------------------------------------------------------------------

static const char *TAG = "openmeteo_client";

//------------------------------------------------------------------------------
// private functions (prototypes)
//------------------------------------------------------------------------------

/**
 * @brief esp_http_client event handler that accumulates response body data.
 *
 * Appends incoming data chunks to the accumulator buffer. Sets the overflow
 * flag if the response exceeds the buffer capacity.
 *
 * @param[in] event HTTP client event. user_data must point to an openmeteo_acc_buffer_t.
 * @retval ESP_OK always.
 */
static esp_err_t on_data(esp_http_client_event_t *event);

/**
 * @brief Perform a GET request and accumulate the response into a buffer.
 *
 * Retries up to HTTP_MAX_RETRIES times on connection failures.
 *
 * @param[in]  url        Null-terminated URL string. Must not be NULL.
 * @param[out] out_buffer Destination buffer for the response body. Must not be NULL.
 * @param[in]  out_length Size of @p out_buffer in bytes.
 * @return openmeteo_status_t result code.
 */
static openmeteo_status_t http_get(const char *url, char *out_buffer, size_t out_length);

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

    openmeteo_acc_buffer_t *acc = (openmeteo_acc_buffer_t *)event->user_data;

    if (acc->overflow == true)
    {
        return ESP_OK;
    }

    size_t needed = acc->length + (size_t)event->data_len + 1;
    if (needed > acc->capacity)
    {
        ESP_LOGE(TAG, "response exceeds buffer (%u > %u)", (unsigned)needed,
                 (unsigned)acc->capacity);
        acc->overflow = true;
        return ESP_OK;
    }

    memcpy(acc->buffer + acc->length, event->data, (size_t)event->data_len);
    acc->length += (size_t)event->data_len;
    acc->buffer[acc->length] = '\0';
    return ESP_OK;
}

static openmeteo_status_t http_get(const char *url, char *out_buffer, size_t out_length)
{
    for (int attempt = 0; attempt < HTTP_MAX_RETRIES; attempt++)
    {
        if (attempt > 0)
        {
            ESP_LOGW(TAG, "HTTP retry %d/%d", attempt, HTTP_MAX_RETRIES - 1);
            vTaskDelay(pdMS_TO_TICKS(HTTP_RETRY_DELAY_MS));
        }

        openmeteo_acc_buffer_t acc = {
            .buffer = out_buffer,
            .length = 0,
            .capacity = out_length,
            .overflow = false,
        };

        out_buffer[0] = '\0';

        esp_http_client_config_t cfg = {
            .url = url,
            .method = HTTP_METHOD_GET,
            .timeout_ms = HTTP_TIMEOUT_MS,
            .event_handler = on_data,
            .user_data = &acc,
        };

        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        if (client == NULL)
        {
            return OPENMETEO_ERR_OOM;
        }

        esp_err_t error = esp_http_client_perform(client);
        int status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (acc.overflow == true)
        {
            return OPENMETEO_ERR_BUF_TOO_SMALL;
        }

        if (error == ESP_ERR_HTTP_CONNECT)
        {
            ESP_LOGW(TAG, "Connection failed (attempt %d), retrying...", attempt + 1);
            continue;
        }

        if (error != ESP_OK)
        {
            ESP_LOGE(TAG, "HTTP error: %s", esp_err_to_name(error));
            return OPENMETEO_ERR_HTTP;
        }

        if (status < 200 || status >= 300)
        {
            ESP_LOGE(TAG, "HTTP status %d", status);
            return OPENMETEO_ERR_HTTP;
        }

        ESP_LOGI(TAG, "HTTP OK status=%d body_len=%u", status, (unsigned)acc.length);
        return OPENMETEO_OK;
    }

    ESP_LOGE(TAG, "HTTP failed after %d attempts", HTTP_MAX_RETRIES);
    return OPENMETEO_ERR_HTTP;
}

//------------------------------------------------------------------------------
// public functions
//------------------------------------------------------------------------------

openmeteo_status_t openmeteo_fetch_current(double lat, double lon, char *out_buffer,
                                           size_t out_length)
{
    if ((out_buffer == NULL) || (out_length == 0U))
    {
        return OPENMETEO_ERR_INVALID_ARG;
    }

    char url[OPENMETEO_URL_BUF_SIZE];
    snprintf(url, sizeof(url),
             "http://api.open-meteo.com/v1/forecast"
             "?latitude=%.5f&longitude=%.5f"
             "&current=temperature_2m,apparent_temperature,"
             "relative_humidity_2m,weather_code,"
             "wind_speed_10m,wind_direction_10m",
             lat, lon);

    ESP_LOGI(TAG, "fetch current: lat=%.4f lon=%.4f", lat, lon);
    return http_get(url, out_buffer, out_length);
}

openmeteo_status_t openmeteo_fetch_forecast(double lat, double lon, int days, char *out_buffer,
                                            size_t out_length)
{
    if (out_buffer == NULL || out_length == 0)
    {
        return OPENMETEO_ERR_INVALID_ARG;
    }

    if (days < OPENMETEO_MIN_DAYS)
    {
        days = OPENMETEO_DEFAULT_DAYS;
    }
    if (days > OPENMETEO_MAX_DAYS)
    {
        days = OPENMETEO_MAX_DAYS;
    }

    char url[OPENMETEO_FORECAST_URL_BUF_SIZE];
    snprintf(url, sizeof(url),
             "http://api.open-meteo.com/v1/forecast"
             "?latitude=%.5f&longitude=%.5f"
             "&daily=weather_code,temperature_2m_max,"
             "temperature_2m_min,precipitation_sum"
             "&forecast_days=%d",
             lat, lon, days);

    ESP_LOGI(TAG, "fetch forecast: lat=%.4f lon=%.4f days=%d", lat, lon, days);
    return http_get(url, out_buffer, out_length);
}