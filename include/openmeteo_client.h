#pragma once

#include <stddef.h>

typedef enum
{
    OPENMETEO_OK = 0,
    OPENMETEO_ERR_HTTP,
    OPENMETEO_ERR_OOM,
    OPENMETEO_ERR_INVALID_ARG,
    OPENMETEO_ERR_BUF_TOO_SMALL,
} openmeteo_status_t;

#define OPENMETEO_BUF_SIZE 8192

openmeteo_status_t openmeteo_fetch_current(double lat, double lon, char *out_buf, size_t out_len);
openmeteo_status_t openmeteo_fetch_forecast(double lat, double lon, int days, char *out_buf, size_t out_len);