#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_mac.h"

#include "wifi_ap.h"
#include "http_server.h"
#include "core_config.h"

#if CORE_CAPTIVE_PORTAL_ENABLED
#include "dns_hijack.h"
#include "lwip/inet.h" // inet_addr()
#endif

#include "nvs.h"

#include "wifi_sta.h"

static const char *TAG = "main";

static void get_device_ids(char mac_colon[18], char mac_compact[13])
{
    uint8_t mac[6] = {0};

    esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (err != ESP_OK)
    {
        snprintf(mac_colon, 18, "00:00:00:00:00:00");
        snprintf(mac_compact, 13, "000000000000");
        ESP_LOGE(TAG, "esp_read_mac(STA) failed: %s", esp_err_to_name(err));
        return;
    }

    snprintf(mac_colon, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    snprintf(mac_compact, 13, "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("HTTP_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls-mbedtls", ESP_LOG_VERBOSE);
    esp_log_level_set("mbedtls", ESP_LOG_VERBOSE);

    ESP_LOGI(TAG, "Boot OK - starting services...");

    char device_id_mac[18];
    char device_id[13];
    get_device_ids(device_id_mac, device_id);

    ESP_LOGI(TAG, "device_id (STA MAC) = %s", device_id_mac);
    ESP_LOGI(TAG, "device_id (compact) = %s", device_id);

#if CORE_BACKEND_POLL_ENABLED
    char url[256];
    int n = snprintf(url, sizeof(url),
                     "%s/v1/devices/%s/desired",
                     CORE_BACKEND_BASE_URL,
                     device_id);

    if (n < 0 || n >= (int)sizeof(url))
    {
        ESP_LOGE(TAG, "Backend URL build failed (buffer too small?)");
    }
    else
    {
        ESP_LOGI(TAG, "Backend poll URL = %s", url);
    }
#endif

    // WiFi AP
    ESP_ERROR_CHECK(wifi_init_ap());

    // STA subsystem (AP stays active)
    ESP_ERROR_CHECK(wifi_sta_init());

    esp_err_t err = wifi_sta_connect_from_nvs();
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW("main", "No WiFi creds in NVS yet -> staying in captive portal mode");
        // NICHT aborten. Einfach weiterlaufen lassen: SoftAP + captive portal bleibt aktiv.
    }
    else
    {
        ESP_ERROR_CHECK(err);
    }

    // HTTP server (Web UI / endpoints)
    http_server_start();

#if CORE_CAPTIVE_PORTAL_ENABLED
    // Default ESP-IDF softAP IP is typically 192.168.4.1
    uint32_t ap_ip_be = inet_addr("192.168.4.1"); // returns network-byte-order
    ESP_ERROR_CHECK(dns_hijack_start(ap_ip_be));
    ESP_LOGI(TAG, "Captive portal: DNS hijack is ON");
#else
    ESP_LOGI(TAG, "Captive portal: OFF");
#endif

    ESP_LOGI(TAG, "Services started. Entering main loop.");

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
