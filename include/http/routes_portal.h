/**
 * @file routes_portal.h
 * @brief HTTP route handlers for the captive portal detection endpoints.
 *
 * Registers OS-specific captive portal probe URLs and a global 404 handler
 * that redirects unknown paths to the root UI, keeping API errors as JSON.
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
 * @brief Register all captive portal routes and the 404 error handler.
 *
 * Registers GET and HEAD handlers for common OS captive portal probe URLs
 * (Android, iOS, Windows) and installs a global 404 handler that redirects
 * unknown paths to "/" while preserving JSON error responses for /api/... routes.
 *
 * @param[in] server HTTP server handle. Must not be NULL.
 */
void routes_portal_register(httpd_handle_t server);

#ifdef __cplusplus
} /* extern "C" */
#endif