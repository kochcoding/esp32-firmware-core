/**
 * @file routes_api_weather.h
 * @brief HTTP route handlers for the weather API endpoints.
 *
 * Exposes weather data endpoints backed by the Open-Meteo API:
 *  - GET /api/weather/current?name=<n>  — current weather for a location
 *  - GET /api/weather/forecast?name=<n> — 7-day forecast for a location
 *
 * If the optional name parameter is omitted, the active location is used.
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

//------------------------------------------------------------------------------
// public includes
//------------------------------------------------------------------------------

#include "esp_http_server.h"

//------------------------------------------------------------------------------
// public API
//------------------------------------------------------------------------------

/**
 * @brief Register all weather API routes with the HTTP server.
 *
 * Registers the following endpoints:
 *  - GET /api/weather/current
 *  - GET /api/weather/forecast
 *
 * @param[in] server HTTP server handle. Must not be NULL.
 */
void routes_api_weather_register(httpd_handle_t server);

#ifdef __cplusplus
} /* extern "C" */
#endif