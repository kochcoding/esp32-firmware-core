/**
 * @file routes_api_wifi.h
 * @brief HTTP route handlers for the WiFi configuration API endpoints.
 *
 * Exposes endpoints for reading and writing WiFi station credentials:
 *  - GET    /api/config/wifi — retrieve current SSID and connection status
 *  - POST   /api/config/wifi — save new credentials and trigger connection
 *  - DELETE /api/config/wifi — clear stored credentials
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
 * @brief Register all WiFi configuration API routes with the HTTP server.
 *
 * Registers the following endpoints:
 *  - GET    /api/config/wifi
 *  - POST   /api/config/wifi
 *  - DELETE /api/config/wifi
 *
 * @param[in] server HTTP server handle. Must not be NULL.
 */
void routes_api_wifi_register(httpd_handle_t server);

#ifdef __cplusplus
} /* extern "C" */
#endif