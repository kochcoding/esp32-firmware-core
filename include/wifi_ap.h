/**
 * @file wifi_ap.h
 * @brief WiFi Access Point initialisation and status interface.
 *
 * Provides initialisation of the ESP32 in APSTA mode and a helper to
 * query the number of currently connected clients.
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

//------------------------------------------------------------------------------
// public includes
//------------------------------------------------------------------------------

#include <stdint.h>

#include "esp_err.h"

//------------------------------------------------------------------------------
// public API
//------------------------------------------------------------------------------

/**
 * @brief Initialise NVS, network interfaces, and start the WiFi AP.
 *
 * Performs the following steps in order:
 *  1. Initialises NVS flash (erases if necessary).
 *  2. Initialises the TCP/IP stack and default event loop.
 *  3. Registers WiFi and IP event handlers.
 *  4. Creates AP and STA network interfaces.
 *  5. Initialises the WiFi driver and applies AP configuration.
 *  6. Starts WiFi in APSTA mode.
 *
 * @retval ESP_OK    AP started successfully.
 * @retval ESP_FAIL  netif creation or other unrecoverable error.
 */
esp_err_t wifi_init_ap(void);

/**
 * @brief Return the number of clients currently associated with the AP.
 *
 * @return Number of connected STA clients, or 0 on error.
 */
uint16_t wifi_ap_get_client_count(void);

#ifdef __cplusplus
} /* extern "C" */
#endif