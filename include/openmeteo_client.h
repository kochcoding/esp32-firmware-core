/**
 * @file openmeteo_client.h
 * @brief HTTP client for the Open-Meteo weather API.
 *
 * Provides functions to fetch current weather conditions and multi-day
 * forecasts from the Open-Meteo public API. Responses are returned as
 * raw JSON strings in caller-supplied buffers.
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

//------------------------------------------------------------------------------
// public includes
//------------------------------------------------------------------------------

#include <stddef.h>

//------------------------------------------------------------------------------
// public defines
//------------------------------------------------------------------------------

/** @brief Recommended output buffer size for Open-Meteo API responses. */
#define OPENMETEO_BUF_SIZE (8192U)

//------------------------------------------------------------------------------
// public typedefs
//------------------------------------------------------------------------------

/**
 * @brief Return status codes for Open-Meteo client functions.
 */
typedef enum
{
    OPENMETEO_OK = 0,           /**< Request completed successfully. */
    OPENMETEO_ERR_HTTP,         /**< HTTP request or connection error. */
    OPENMETEO_ERR_OOM,          /**< HTTP client initialisation failed (out of memory). */
    OPENMETEO_ERR_INVALID_ARG,  /**< Invalid argument (NULL buffer or zero length). */
    OPENMETEO_ERR_BUF_TOO_SMALL /**< Response exceeded the output buffer capacity. */
} openmeteo_status_t;

//------------------------------------------------------------------------------
// public API
//------------------------------------------------------------------------------

/**
 * @brief Fetch current weather conditions for a given location.
 *
 * Requests temperature, apparent temperature, humidity, weather code,
 * wind speed, and wind direction from the Open-Meteo API.
 *
 * @param[in]  lat        Latitude in decimal degrees.
 * @param[in]  lon        Longitude in decimal degrees.
 * @param[out] out_buffer Destination buffer for the JSON response. Must not be NULL.
 * @param[in]  out_length Size of @p out_buffer in bytes. Must be >= 1.
 * @return openmeteo_status_t result code.
 */
openmeteo_status_t openmeteo_fetch_current(double lat, double lon, char *out_buffer,
                                           size_t out_length);

/**
 * @brief Fetch a multi-day weather forecast for a given location.
 *
 * Requests daily weather code, max/min temperature, and precipitation sum.
 * The @p days parameter is clamped to [1, 16] internally.
 *
 * @param[in]  lat        Latitude in decimal degrees.
 * @param[in]  lon        Longitude in decimal degrees.
 * @param[in]  days       Number of forecast days (clamped to 1–16).
 * @param[out] out_buffer Destination buffer for the JSON response. Must not be NULL.
 * @param[in]  out_length Size of @p out_buffer in bytes. Must be >= 1.
 * @return openmeteo_status_t result code.
 */
openmeteo_status_t openmeteo_fetch_forecast(double lat, double lon, int days, char *out_buffer,
                                            size_t out_length);

#ifdef __cplusplus
} /* extern "C" */
#endif