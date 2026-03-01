/**
 * @file routes_api_geocode.h
 * @brief HTTP route handler for the geocoding API endpoint.
 *
 * Exposes GET /api/geocode?name=<query>, which proxies requests to the
 * Open-Meteo geocoding API and returns up to 5 matching locations as JSON.
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
 * @brief Register the geocoding API route with the HTTP server.
 *
 * Registers the following endpoint:
 *  - GET /api/geocode?name=<query>
 *
 * @param[in] server HTTP server handle. Must not be NULL.
 */
void routes_api_geocode_register(httpd_handle_t server);

#ifdef __cplusplus
} // extern "C"
#endif
