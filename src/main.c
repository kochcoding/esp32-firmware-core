/**
 * @file main.c
 * @brief Application entry point for the ESP32 WiFi weather station.
 *
 * @details
 *  - Initialises WiFi in APSTA mode (AP always active).
 *  - Optionally starts the captive portal DNS hijack (CORE_CAPTIVE_PORTAL_ENABLED).
 *  - Starts the HTTP server for the web UI and REST API.
 *  - Attempts STA connection from NVS credentials on boot.
 */

//------------------------------------------------------------------------------
// includes
//------------------------------------------------------------------------------

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"

#include "http/http_server.h"

#include "core_config.h"
#include "wifi_ap.h"
#include "wifi_sta.h"

#if CORE_CAPTIVE_PORTAL_ENABLED
#include "dns_hijack.h"
#include "lwip/inet.h"
#endif

//------------------------------------------------------------------------------
// defines
//------------------------------------------------------------------------------

#define MAIN_LOOP_DELAY_MS (1000U)
#define MAC_COLON_BUF_SIZE (18U)
#define MAC_COMPACT_BUF_SIZE (13U)
#define MAC_BYTE_COUNT (6U)
#define AP_DEFAULT_IP "192.168.4.1"

//------------------------------------------------------------------------------
// variables
//------------------------------------------------------------------------------

static const char *TAG = "main";

//------------------------------------------------------------------------------
// functions (prototypes)
//------------------------------------------------------------------------------

/**
 * @brief Read the STA MAC address and format it as two string representations.
 *
 * On failure, both buffers are filled with zero-strings and an error is logged.
 *
 * @param[out] mac_colon   Buffer for colon-separated MAC (e.g. "AA:BB:CC:DD:EE:FF").
 *                         Must be at least MAC_COLON_BUF_SIZE bytes.
 * @param[out] mac_compact Buffer for compact MAC (e.g. "AABBCCDDEEFF").
 *                         Must be at least MAC_COMPACT_BUF_SIZE bytes.
 */
static void get_device_ids(char mac_colon[MAC_COLON_BUF_SIZE],
                           char mac_compact[MAC_COMPACT_BUF_SIZE]);

//------------------------------------------------------------------------------
// functions (implementation)
//------------------------------------------------------------------------------

static void get_device_ids(char mac_colon[MAC_COLON_BUF_SIZE],
                           char mac_compact[MAC_COMPACT_BUF_SIZE])
{
    uint8_t mac[MAC_BYTE_COUNT] = {0};

    esp_err_t error = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (error != ESP_OK)
    {
        snprintf(mac_colon, MAC_COLON_BUF_SIZE, "00:00:00:00:00:00");
        snprintf(mac_compact, MAC_COMPACT_BUF_SIZE, "000000000000");
        ESP_LOGE(TAG, "esp_read_mac(STA) failed: %s", esp_err_to_name(error));
        return;
    }

    snprintf(mac_colon, MAC_COLON_BUF_SIZE, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
             mac[3], mac[4], mac[5]);

    snprintf(mac_compact, MAC_COMPACT_BUF_SIZE, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2],
             mac[3], mac[4], mac[5]);
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    ESP_LOGI(TAG, "Boot OK - starting services...");

    char device_id_mac[MAC_COLON_BUF_SIZE];
    char device_id[MAC_COMPACT_BUF_SIZE];
    get_device_ids(device_id_mac, device_id);

    ESP_LOGI(TAG, "device_id (STA MAC) = %s", device_id_mac);
    ESP_LOGI(TAG, "device_id (compact) = %s", device_id);

    /* WiFi AP */
    ESP_ERROR_CHECK(wifi_init_ap());

    /* STA subsystem (AP stays active) */
    ESP_ERROR_CHECK(wifi_sta_init());

    /* HTTP server (Web UI / endpoints) */
    http_server_start();

#if CORE_CAPTIVE_PORTAL_ENABLED
    /* Default ESP-IDF softAP IP is typically 192.168.4.1 */
    uint32_t ap_ip_be = inet_addr(AP_DEFAULT_IP); /* returns network-byte-order */
    ESP_ERROR_CHECK(dns_hijack_start(ap_ip_be));
    ESP_LOGI(TAG, "Captive portal: DNS hijack is ON");
#else
    ESP_LOGI(TAG, "Captive portal: OFF");
#endif

    esp_err_t error = wifi_sta_connect_from_nvs();
    if (error == ESP_ERR_NOT_FOUND)
    {
        ESP_LOGW(TAG, "No WiFi creds in NVS yet -> staying in captive portal mode");
        /* Do not abort. Keep running: SoftAP + captive portal remains active. */
    }
    else
    {
        ESP_ERROR_CHECK(error);
    }

    ESP_LOGI(TAG, "Services started. Entering main loop.");

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_DELAY_MS));
    }
}
