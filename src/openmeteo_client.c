#include "openmeteo_client.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

#include <ctype.h>
#include <stdio.h>

static const char *TAG = "openmeteo";

typedef struct
{
    char *buf;
    size_t len;
    size_t cap;
} http_buf_t;

typedef struct
{
    char *buf;
    size_t len;
    size_t cap;
    size_t max_cap;
} http_acc_t;

static esp_err_t http_acc_grow(http_acc_t *a, size_t add)
{
    if (!a)
        return ESP_FAIL;

    size_t need = a->len + add + 1; // +1 for '\0'
    if (need > a->max_cap)
        return ESP_ERR_NO_MEM;

    if (need <= a->cap)
        return ESP_OK;

    size_t new_cap = (a->cap == 0) ? 512 : a->cap;
    while (new_cap < need)
        new_cap *= 2;
    if (new_cap > a->max_cap)
        new_cap = a->max_cap;

    char *p = realloc(a->buf, new_cap);
    if (!p)
        return ESP_ERR_NO_MEM;

    a->buf = p;
    a->cap = new_cap;
    return ESP_OK;
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    if (!evt)
        return ESP_FAIL;

    http_acc_t *a = (http_acc_t *)evt->user_data;
    if (!a)
        return ESP_OK;

    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        if (evt->data && evt->data_len > 0)
        {
            esp_err_t e = http_acc_grow(a, (size_t)evt->data_len);
            if (e != ESP_OK)
                return e;

            memcpy(a->buf + a->len, evt->data, (size_t)evt->data_len);
            a->len += (size_t)evt->data_len;
            a->buf[a->len] = '\0';
        }
        break;

    default:
        break;
    }
    return ESP_OK;
}

/**
 * Performs HTTPS GET and returns body in malloc'ed buffer (caller must free()).
 */
static esp_err_t openmeteo_http_get_malloc(const char *url, char **out_body)
{
    if (!url || !out_body)
        return ESP_ERR_INVALID_ARG;
    *out_body = NULL;

    http_acc_t acc = {
        .buf = NULL,
        .len = 0,
        .cap = 0,
        .max_cap = 16384, // 16 KB cap (reicht für current + kleine forecasts)
    };

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 10000,
        .event_handler = http_event_handler,
        .user_data = &acc,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c)
        return ESP_FAIL;

    esp_err_t err = esp_http_client_perform(c);
    int status = esp_http_client_get_status_code(c);

    esp_http_client_cleanup(c);

    if (err != ESP_OK)
    {
        free(acc.buf);
        return err;
    }

    if (status < 200 || status >= 300)
    {
        free(acc.buf);
        return ESP_FAIL;
    }

    if (!acc.buf)
    {
        // valid but empty response
        acc.buf = malloc(1);
        if (!acc.buf)
            return ESP_ERR_NO_MEM;
        acc.buf[0] = '\0';
    }

    *out_body = acc.buf;
    return ESP_OK;
}

static esp_err_t http_evt(esp_http_client_event_t *evt)
{
    http_buf_t *b = (http_buf_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA && evt->data && evt->data_len > 0)
    {
        if (b->len + evt->data_len + 1 > b->cap)
            return ESP_FAIL;
        memcpy(b->buf + b->len, evt->data, evt->data_len);
        b->len += evt->data_len;
        b->buf[b->len] = 0;
    }
    return ESP_OK;
}

static esp_err_t http_get_json(const char *url, char *dst, size_t dst_size, int *out_status)
{
    http_buf_t b = {.buf = dst, .len = 0, .cap = dst_size};

    ESP_LOGI(TAG, "HTTP GET: %s", url);

    esp_http_client_config_t cfg = {
        .url = url,
        .event_handler = http_evt,
        .user_data = &b,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000,
        .keep_alive_enable = false,
    };

    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c)
        return ESP_ERR_NO_MEM;

    esp_err_t err = esp_http_client_perform(c);
    int status = esp_http_client_get_status_code(c);
    if (out_status)
        *out_status = status;

    ESP_LOGI(TAG, "HTTP status=%d err=%s body_len=%u", status, esp_err_to_name(err), (unsigned)b.len);
    if (b.len > 0)
    {
        // Log first 300 bytes max (safe)
        size_t n = b.len < 300 ? b.len : 300;
        char tmp[301];
        memcpy(tmp, dst, n);
        tmp[n] = 0;
        ESP_LOGI(TAG, "HTTP body head: %s", tmp);
    }

    esp_http_client_cleanup(c);

    if (err != ESP_OK)
        return err;
    if (status != 200)
        return ESP_FAIL;
    return ESP_OK;
}

static size_t url_encode(char *out, size_t out_size, const char *in)
{
    // Percent-encode UTF-8 bytes. Safe for umlauts etc.
    size_t oi = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p; p++)
    {
        unsigned char c = *p;

        // Unreserved per RFC3986: ALPHA / DIGIT / "-" / "." / "_" / "~"
        if (isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~')
        {
            if (oi + 1 >= out_size)
                break;
            out[oi++] = (char)c;
        }
        else
        {
            if (oi + 3 >= out_size)
                break;
            snprintf(out + oi, out_size - oi, "%%%02X", c);
            oi += 3;
        }
    }
    if (out_size > 0)
        out[oi < out_size ? oi : (out_size - 1)] = '\0';
    return oi;
}

static bool write_utf8_2(char *out, size_t out_size, size_t *oi, unsigned char b1, unsigned char b2)
{
    if (*oi + 2 >= out_size)
        return false;
    out[(*oi)++] = (char)b1;
    out[(*oi)++] = (char)b2;
    return true;
}

static bool make_umlaut_variant(const char *in, char *out, size_t out_size)
{
    size_t oi = 0;
    bool changed = false;

    for (size_t i = 0; in[i] != '\0';)
    {
        char c0 = in[i];
        char c1 = in[i + 1];

        if ((c0 == 'a' || c0 == 'A') && (c1 == 'e' || c1 == 'E'))
        {
            if (!write_utf8_2(out, out_size, &oi, 0xC3, (c0 == 'A') ? 0x84 : 0xA4))
                return false; // Ä/ä
            i += 2;
            changed = true;
            continue;
        }
        if ((c0 == 'o' || c0 == 'O') && (c1 == 'e' || c1 == 'E'))
        {
            if (!write_utf8_2(out, out_size, &oi, 0xC3, (c0 == 'O') ? 0x96 : 0xB6))
                return false; // Ö/ö
            i += 2;
            changed = true;
            continue;
        }
        if ((c0 == 'u' || c0 == 'U') && (c1 == 'e' || c1 == 'E'))
        {
            if (!write_utf8_2(out, out_size, &oi, 0xC3, (c0 == 'U') ? 0x9C : 0xBC))
                return false; // Ü/ü
            i += 2;
            changed = true;
            continue;
        }

        if (oi + 1 >= out_size)
            return false;
        out[oi++] = c0;
        i += 1;
    }

    if (oi >= out_size)
        return false;
    out[oi] = '\0';
    return changed;
}

esp_err_t openmeteo_geocode_first(const char *query, openmeteo_geocode_result_t *out)
{
    if (!query || !out)
        return ESP_ERR_INVALID_ARG;

    char query_alt[128];
    bool has_alt = make_umlaut_variant(query, query_alt, sizeof(query_alt));

    // wir versuchen 1) original, 2) umlaut-variante (falls erzeugt)
    for (int attempt = 0; attempt < 2; attempt++)
    {
        const char *q_in = (attempt == 0) ? query : query_alt;
        if (attempt == 1 && !has_alt)
            break;

        char q_enc[128];

        size_t enc_len = url_encode(q_enc, sizeof(q_enc), q_in);
        if (enc_len == 0)
        {
            // encoding failed (z.B. out buffer zu klein)
            return ESP_ERR_INVALID_SIZE; // oder dein Fehlerhandling
        }

        char url[320];
        // dann q_enc in deine URL einsetzen
        snprintf(url, sizeof(url),
                 "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=1&language=en&format=json",
                 q_enc);

        char json[2048];
        int status = 0;
        esp_err_t err = http_get_json(url, json, sizeof(json), &status);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "geocode http failed: err=%s status=%d", esp_err_to_name(err), status);
            return err;
        }

        cJSON *root = cJSON_Parse(json);
        if (!root)
            return ESP_ERR_INVALID_RESPONSE;

        cJSON *results = cJSON_GetObjectItem(root, "results");
        if (!results || !cJSON_IsArray(results) || cJSON_GetArraySize(results) == 0)
        {
            cJSON_Delete(root);
            // Versuch 2? dann weiter, sonst NOT_FOUND
            if (attempt == 0 && has_alt)
                continue;
            return ESP_ERR_NOT_FOUND;
        }

        cJSON *r0 = cJSON_GetArrayItem(results, 0);

        const cJSON *name = cJSON_GetObjectItem(r0, "name");
        const cJSON *lat = cJSON_GetObjectItem(r0, "lat");
        const cJSON *lon = cJSON_GetObjectItem(r0, "lon");
        const cJSON *tz = cJSON_GetObjectItem(r0, "timezone");
        const cJSON *cc = cJSON_GetObjectItem(r0, "country_code");

        if (!cJSON_IsString(name) || !cJSON_IsNumber(lat) || !cJSON_IsNumber(lon))
        {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_RESPONSE;
        }

        memset(out, 0, sizeof(*out));
        strncpy(out->name, name->valuestring, sizeof(out->name) - 1);
        out->lat = (float)lat->valuedouble;
        out->lon = (float)lon->valuedouble;
        if (cJSON_IsString(tz))
            strncpy(out->timezone, tz->valuestring, sizeof(out->timezone) - 1);
        if (cJSON_IsString(cc))
            strncpy(out->country_code, cc->valuestring, sizeof(out->country_code) - 1);

        cJSON_Delete(root);
        return ESP_OK;
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t openmeteo_get_current_json(float lat, float lon, const char *timezone, char **out_json)
{
    if (!out_json)
        return ESP_ERR_INVALID_ARG;
    *out_json = NULL;

    char url[512];
    if (timezone && timezone[0])
    {
        snprintf(url, sizeof(url),
                 "https://api.open-meteo.com/v1/forecast?"
                 "latitude=%.5f&longitude=%.5f"
                 "&current=temperature_2m,relative_humidity_2m,apparent_temperature,precipitation,weather_code,wind_speed_10m,wind_direction_10m"
                 "&timezone=%s",
                 lat, lon, timezone);
    }
    else
    {
        snprintf(url, sizeof(url),
                 "https://api.open-meteo.com/v1/forecast?"
                 "latitude=%.5f&longitude=%.5f"
                 "&current=temperature_2m,relative_humidity_2m,apparent_temperature,precipitation,weather_code,wind_speed_10m,wind_direction_10m",
                 lat, lon);
    }

    // Reuse deinen bestehenden HTTP-GET Mechanismus (so wie beim Geocode),
    // nur dass du den Body in malloc Buffer schreibst.
    // -> Wenn du aktuell schon eine Funktion hast, die "dst buffer" befüllt:
    //    dann mach: erst HEAD/len holen oder mit realloc wachsen.
    //
    // Wenn du magst, sag kurz wie du im Geocode die Response sammelst
    // (callback / buffer), dann gebe ich dir exakt die passende Minimal-Variante.
    return openmeteo_http_get_malloc(url, out_json); // <- so eine Hilfsfunktion brauchst du (siehe Schritt 1b)
}

esp_err_t openmeteo_get_forecast_json(float lat, float lon, const char *timezone, int forecast_days, char **out_json)
{
    if (!out_json)
        return ESP_ERR_INVALID_ARG;
    *out_json = NULL;

    if (forecast_days <= 0)
        forecast_days = 7;
    if (forecast_days > 16)
        forecast_days = 16; // Open-Meteo typical max

    char url[768];
    if (timezone && timezone[0])
    {
        snprintf(url, sizeof(url),
                 "https://api.open-meteo.com/v1/forecast?"
                 "latitude=%.5f&longitude=%.5f"
                 "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_sum,wind_speed_10m_max"
                 "&timezone=%s&forecast_days=%d",
                 lat, lon, timezone, forecast_days);
    }
    else
    {
        snprintf(url, sizeof(url),
                 "https://api.open-meteo.com/v1/forecast?"
                 "latitude=%.5f&longitude=%.5f"
                 "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_sum,wind_speed_10m_max"
                 "&forecast_days=%d",
                 lat, lon, forecast_days);
    }

    return openmeteo_http_get_malloc(url, out_json);
}

static openmeteo_status_t http_get_json_alloc(const char *url, size_t cap, char **out_json, int *out_status)
{
    if (!url || !out_json)
        return OPENMETEO_ERR_INVALID_ARG;

    char *buf = (char *)calloc(1, cap);
    if (!buf)
        return OPENMETEO_ERR_OOM;

    int status = 0;
    esp_err_t err = http_get_json(url, buf, cap, &status);

    if (out_status)
        *out_status = status;

    if (err != ESP_OK || status != 200)
    {
        free(buf);
        return OPENMETEO_ERR_HTTP;
    }

    *out_json = buf;
    return OPENMETEO_OK;
}

openmeteo_status_t openmeteo_fetch_current_json(double latitude, double longitude,
                                                const char *timezone, char **out_json)
{
    if (!out_json)
        return OPENMETEO_ERR_INVALID_ARG;

    char tz_enc[96] = {0};
    if (timezone && timezone[0] != '\0')
    {
        url_encode(tz_enc, sizeof(tz_enc), timezone);
    }
    else
    {
        snprintf(tz_enc, sizeof(tz_enc), "UTC");
    }

    char url[512];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast"
             "?latitude=%.5f&longitude=%.5f"
             "&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,wind_speed_10m"
             "&timezone=%s",
             latitude, longitude, tz_enc);

    ESP_LOGI(TAG, "HTTP GET: %s", url);
    return http_get_json_alloc(url, 4096, out_json, NULL);
}

openmeteo_status_t openmeteo_fetch_forecast_json(double latitude, double longitude,
                                                 const char *timezone, int days, char **out_json)
{
    if (!out_json)
        return OPENMETEO_ERR_INVALID_ARG;
    if (days <= 0)
        days = 7;
    if (days > 16)
        days = 16; // Open-Meteo daily forecast limit is typically up to 16 days

    char tz_enc[96] = {0};
    if (timezone && timezone[0] != '\0')
    {
        url_encode(tz_enc, sizeof(tz_enc), timezone);
    }
    else
    {
        snprintf(tz_enc, sizeof(tz_enc), "UTC");
    }

    char url[640];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast"
             "?latitude=%.5f&longitude=%.5f"
             "&daily=weather_code,temperature_2m_max,temperature_2m_min"
             "&forecast_days=%d"
             "&timezone=%s",
             latitude, longitude, days, tz_enc);

    ESP_LOGI(TAG, "HTTP GET: %s", url);
    return http_get_json_alloc(url, 8192, out_json, NULL);
}
