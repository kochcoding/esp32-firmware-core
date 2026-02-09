#pragma once
#include "esp_err.h"

typedef struct
{
    char name[48];
    float lat;
    float lon;
    char timezone[48];
    char country_code[8];
} openmeteo_geocode_result_t;

esp_err_t openmeteo_geocode_first(const char *query, openmeteo_geocode_result_t *out);

esp_err_t openmeteo_get_current_json(float lat, float lon, const char *timezone, char **out_json);
esp_err_t openmeteo_get_forecast_json(float lat, float lon, const char *timezone, int forecast_days, char **out_json);

// Fetch weather data from Open-Meteo forecast API.
// Returned JSON strings are heap-allocated (free() by caller).
// If timezone is NULL/empty, Open-Meteo may default to UTC.

typedef enum
{
    OPENMETEO_OK = 0,
    OPENMETEO_ERR_HTTP = 1,
    OPENMETEO_ERR_PARSE = 2,
    OPENMETEO_ERR_OOM = 3,
    OPENMETEO_ERR_INVALID_ARG = 4,
} openmeteo_status_t;

openmeteo_status_t openmeteo_fetch_current_json(double latitude, double longitude,
                                                const char *timezone, char **out_json);

openmeteo_status_t openmeteo_fetch_forecast_json(double latitude, double longitude,
                                                 const char *timezone, int days, char **out_json);
