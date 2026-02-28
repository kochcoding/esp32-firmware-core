/**
 * @file routes_api_locations.h
 * @brief HTTP route handlers for the locations API endpoints.
 *
 * Exposes CRUD endpoints for managing stored locations:
 *  - GET    /api/locations           — retrieve all stored locations
 *  - POST   /api/locations           — add a new location
 *  - DELETE /api/locations?name=<n>  — remove a location by name
 *  - PUT    /api/locations/active?name=<n> — set the active location
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
 * @brief Register all locations API routes with the HTTP server.
 *
 * Registers the following endpoints:
 *  - GET    /api/locations
 *  - POST   /api/locations
 *  - DELETE /api/locations
 *  - PUT    /api/locations/active
 *
 * @param[in] server HTTP server handle. Must not be NULL.
 */
void routes_api_locations_register(httpd_handle_t server);

#ifdef __cplusplus
} /* extern "C" */
#endif