/**
 * @file dns_hijack.h
 * @brief Captive portal DNS hijack server.
 *
 * Starts a UDP DNS server on port 53 that responds to all A queries
 * with a configurable IPv4 address, redirecting clients to the captive
 * portal regardless of the hostname they requested.
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
 * @brief Start the DNS hijack server.
 *
 * Binds UDP port 53 on all interfaces and spawns a FreeRTOS task
 * that responds to all incoming DNS A queries with @p ipv4_addr_be.
 * Calling this function while the server is already running is a no-op.
 *
 * @param[in] ipv4_addr_be Target IPv4 address in network byte order (big-endian).
 * @retval ESP_OK    Server started successfully.
 * @retval ESP_FAIL  Socket creation, bind, or task creation failed.
 */
esp_err_t dns_hijack_start(uint32_t ipv4_addr_be);

/**
 * @brief Stop the DNS hijack server.
 *
 * Closes the UDP socket and deletes the FreeRTOS task.
 * Calling this function while the server is not running is a no-op.
 *
 * @retval ESP_OK always.
 */
esp_err_t dns_hijack_stop(void);

#ifdef __cplusplus
} /* extern "C" */
#endif