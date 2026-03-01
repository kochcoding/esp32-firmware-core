/**
 * @file ui_routes.h
 * @brief HTTP route handlers for serving the embedded UI pages.
 *
 * Registers GET handlers for the main portal page, the locations
 * management page, and a no-content favicon response.
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
 * @brief Register all UI page routes with the HTTP server.
 *
 * Registers the following endpoints:
 *  - GET /           — main WiFi setup portal (UI_INDEX_HTML)
 *  - GET /locations  — location management page (UI_LOCATIONS_HTML)
 *  - GET /favicon.ico — 204 No Content response
 *
 * @param[in] server HTTP server handle. Must not be NULL.
 */
void ui_routes_register(httpd_handle_t server);

#ifdef __cplusplus
} /* extern "C" */
#endif