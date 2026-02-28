/**
 * @file http_server.h
 * @brief HTTP server lifecycle management.
 *
 * Initialises and starts the ESP-IDF HTTP server with all application
 * route handlers registered (portal, UI, WiFi API, locations API,
 * weather API, geocoding API).
 */

#pragma once

#include "esp_err.h"

/**
 * @brief Start the HTTP server and register all route handlers.
 *
 * Configures the ESP-IDF HTTP server with tuned socket and timeout settings
 * suitable for captive portal workloads, then registers all URI handlers.
 *
 * @retval ESP_OK    Server started and all routes registered successfully.
 * @retval ESP_FAIL  Server already running, or @c httpd_start() failed.
 */
esp_err_t http_server_start(void);