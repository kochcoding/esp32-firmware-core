#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

// out_buf must be at least WIFI_STA_STATUS_JSON_BUF_SIZE bytes
#define WIFI_STA_STATUS_JSON_BUF_SIZE 128

typedef enum
{
    WIFI_STA_STATE_IDLE = 0,
    WIFI_STA_STATE_CONNECTING,
    WIFI_STA_STATE_CONNECTED,
    WIFI_STA_STATE_FAILED
} wifi_sta_state_t;

typedef struct
{
    wifi_sta_state_t state;
    char ssid[33]; // 32 + '\0'
    uint8_t ip[4]; // IPv4
    uint32_t retry_count;
} wifi_sta_status_t;

/**
 * Init STA subsystem (registers event handlers, creates STA netif).
 * Call AFTER wifi_init_ap() (because netif + default event loop exist then).
 */
esp_err_t wifi_sta_init(void);

/**
 * Load credentials from NVS ("cfg": "sta_ssid"/"sta_pass") and try connect.
 * Does nothing if no SSID stored.
 */
esp_err_t wifi_sta_connect_from_nvs(void);

/**
 * Start a STA connection attempt immediately using provided credentials.
 * This does NOT store anything to NVS.
 */
esp_err_t wifi_sta_connect(const char *ssid, const char *pass);

/**
 * Get current STA status snapshot.
 */
wifi_sta_status_t wifi_sta_get_status(void);

/**
 * Convenience: whether STA is currently connected (has IP).
 */
bool wifi_sta_is_connected(void);

void wifi_sta_status_to_json(const wifi_sta_status_t *s, char *out_buf, size_t out_len);
