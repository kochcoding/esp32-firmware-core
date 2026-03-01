/**
 * @file wifi_sta.h
 * @brief WiFi Station (STA) connection management interface.
 *
 * Manages STA connections with automatic retry/backoff, state machine
 * transitions, and NVS credential loading. Designed to run alongside
 * the AP interface (APSTA mode).
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

//------------------------------------------------------------------------------
// public includes
//------------------------------------------------------------------------------

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

//------------------------------------------------------------------------------
// public defines
//------------------------------------------------------------------------------

/** @brief Minimum output buffer size for wifi_sta_status_to_json(). */
#define WIFI_STA_STATUS_JSON_BUF_SIZE (128U)

/** @brief Maximum SSID length including null terminator. */
#define WIFI_STA_SSID_MAX_LEN (33U)

/** @brief IPv4 address byte count. */
#define WIFI_STA_IP_LEN (4U)

//------------------------------------------------------------------------------
// public typedefs
//------------------------------------------------------------------------------

/**
 * @brief STA connection state machine states.
 */
typedef enum
{
    WIFI_STA_STATE_IDLE = 0,   /**< No connection attempt active. */
    WIFI_STA_STATE_CONNECTING, /**< Connection attempt in progress. */
    WIFI_STA_STATE_RETRYING,   /**< Waiting before next retry. */
    WIFI_STA_STATE_CONNECTED,  /**< Connected and IP assigned. */
    WIFI_STA_STATE_FAILED,     /**< Max retries exceeded. */
} wifi_sta_state_t;

/**
 * @brief Snapshot of the current STA connection status.
 */
typedef struct
{
    wifi_sta_state_t state;           /**< Current state machine state. */
    char ssid[WIFI_STA_SSID_MAX_LEN]; /**< SSID of the target network. */
    uint8_t ip[WIFI_STA_IP_LEN];      /**< Assigned IPv4 address (octets). */
    uint32_t retry_count;             /**< Number of connection retries so far. */
} wifi_sta_status_t;

//------------------------------------------------------------------------------
// public API
//------------------------------------------------------------------------------

/**
 * @brief Initialise the STA subsystem.
 *
 * Registers WiFi and IP event handlers, acquires or creates the STA netif,
 * and creates the retry timer. Must be called after wifi_init_ap().
 *
 * @retval ESP_OK   Initialisation successful.
 * @retval ESP_FAIL STA netif creation failed.
 */
esp_err_t wifi_sta_init(void);

/**
 * @brief Load credentials from NVS and attempt a STA connection.
 *
 * Reads SSID and password from NVS namespace "cfg". If no SSID is stored,
 * the function returns ESP_ERR_NOT_FOUND and the device stays in AP-only mode.
 *
 * @retval ESP_OK           Connection attempt started.
 * @retval ESP_ERR_NOT_FOUND No SSID stored in NVS.
 * @retval ESP_FAIL         Settings load or connection error.
 */
esp_err_t wifi_sta_connect_from_nvs(void);

/**
 * @brief Start a STA connection attempt with the given credentials.
 *
 * Stops any pending retry timer, updates the SSID in the status snapshot,
 * and triggers an asynchronous connection attempt. Does not write to NVS.
 *
 * @param[in] ssid Null-terminated SSID string. Must not be NULL or empty.
 * @param[in] pass Null-terminated password string. NULL is treated as empty (open network).
 * @retval ESP_OK              Connection attempt started.
 * @retval ESP_ERR_INVALID_ARG SSID is NULL or empty.
 */
esp_err_t wifi_sta_connect(const char *ssid, const char *pass);

/**
 * @brief Return a snapshot of the current STA status.
 *
 * @return wifi_sta_status_t Copy of the internal status structure.
 */
wifi_sta_status_t wifi_sta_get_status(void);

/**
 * @brief Check whether the STA interface is currently connected.
 *
 * @retval true  State is WIFI_STA_STATE_CONNECTED (IP assigned).
 * @retval false Any other state.
 */
bool wifi_sta_is_connected(void);

/**
 * @brief Serialise a STA status snapshot to a JSON string.
 *
 * Output format: {"sta_state":"<state>","ip":"<x.x.x.x>"}
 * The function writes nothing if @p status or @p out_buffer is NULL
 * or @p out_length is zero.
 *
 * @param[in]  status     Pointer to the status snapshot. Must not be NULL.
 * @param[out] out_buffer Destination buffer. Must not be NULL.
 * @param[in]  out_length Size of @p out_buffer in bytes.
 */
void wifi_sta_status_to_json(const wifi_sta_status_t *status, char *out_buffer, size_t out_length);

#ifdef __cplusplus
} /* extern "C" */
#endif