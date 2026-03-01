/**
 * @file wifi_ap.c
 * @brief Implementation of WiFi Access Point initialisation.
 *
 * @details
 *  - Starts the ESP32 in APSTA mode using configuration from core_config.h.
 *  - SSID and open/WPA2 mode are controlled via Kconfig (CORE_AP_SSID, CORE_AP_OPEN_DEFAULT).
 *  - DHCP server is conditionally disabled based on CORE_AP_DHCP_ENABLED.
 *  - WiFi and IP events are logged for diagnostic purposes.
 */

//------------------------------------------------------------------------------
// private includes
//------------------------------------------------------------------------------
#include "wifi_ap.h"

#include "core_config.h"

#include <string.h>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "nvs_flash.h"

#include "lwip/inet.h"

//------------------------------------------------------------------------------
// private defines
//------------------------------------------------------------------------------

#define WIFI_AP_CHANNEL (1U)
#define WIFI_AP_MAX_CONNECTIONS (4U)
#define WIFI_IP_STR_LEN (16U)

//------------------------------------------------------------------------------
// private variables
//------------------------------------------------------------------------------

static const char *TAG = "wifi_ap";

//------------------------------------------------------------------------------
// private functions (prototypes)
//------------------------------------------------------------------------------

/**
 * @brief WiFi event handler for AP mode events.
 *
 * Logs client connect and disconnect events including MAC address and AID.
 *
 * @param[in] arg        Unused.
 * @param[in] event_base Event base (expected: WIFI_EVENT).
 * @param[in] event_id   WiFi event ID.
 * @param[in] event_data Event-specific data payload.
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                               void *event_data);

/**
 * @brief IP event handler for DHCP lease assignments.
 *
 * Logs the IP address assigned to a newly connected STA client.
 *
 * @param[in] arg        Unused.
 * @param[in] event_base Event base (expected: IP_EVENT).
 * @param[in] event_id   IP event ID.
 * @param[in] event_data Event-specific data payload.
 */
static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                             void *event_data);

//------------------------------------------------------------------------------
// private functions (implementation)
//------------------------------------------------------------------------------

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                               void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_AP_START:
            ESP_LOGI(TAG, "Event: AP_START");
            break;

        case WIFI_EVENT_AP_STOP:
            ESP_LOGI(TAG, "Event: AP_STOP");
            break;

        case WIFI_EVENT_AP_STACONNECTED:
        {
            const wifi_event_ap_staconnected_t *sta_connected = event_data;
            ESP_LOGI(TAG, "Client connected: " MACSTR ", AID=%d", MAC2STR(sta_connected->mac),
                     sta_connected->aid);
            break;
        }

        case WIFI_EVENT_AP_STADISCONNECTED:
        {
            const wifi_event_ap_stadisconnected_t *sta_disconnected = event_data;
            ESP_LOGI(TAG, "Client disconnected: " MACSTR ", AID=%d, reason=%d",
                     MAC2STR(sta_disconnected->mac), sta_disconnected->aid,
                     sta_disconnected->reason);
            break;
        }

        default:
            break;
        }
    }
}

static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                             void *event_data)
{
    (void)arg;

    if (event_base == IP_EVENT)
    {
        if (event_id == IP_EVENT_AP_STAIPASSIGNED)
        {
            const ip_event_ap_staipassigned_t *ip_assigned = event_data;

            char ip_str[WIFI_IP_STR_LEN] = {0};
            inet_ntoa_r(ip_assigned->ip, ip_str, sizeof(ip_str));

            ESP_LOGI(TAG, "DHCP lease assigned: %s", ip_str);
        }
    }
}

//------------------------------------------------------------------------------
// public functions
//------------------------------------------------------------------------------
esp_err_t wifi_init_ap(void)
{
    /* 1) NVS init */
    esp_err_t error = nvs_flash_init();
    if (error == ESP_ERR_NVS_NO_FREE_PAGES || error == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "NVS init failed (%s), erasing...", esp_err_to_name(error));
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    else
    {
        ESP_ERROR_CHECK(error);
    }

    /* 2) Netif + event loop */
    error = esp_netif_init();
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE)
    {
        ESP_ERROR_CHECK(error);
    }

    error = esp_event_loop_create_default();
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE)
    {
        ESP_ERROR_CHECK(error);
    }

    /* 3) Register handlers */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                                        &ip_event_handler, NULL, NULL));

    /* 4) Create AP netif */
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    if (ap_netif == NULL)
    {
        ESP_LOGE(TAG, "Failed to create AP netif");
        return ESP_FAIL;
    }

    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    if (sta_netif == NULL)
    {
        ESP_LOGE(TAG, "Failed to create STA netif");
        return ESP_FAIL;
    }
    (void)sta_netif; /* netif is managed by the stack */

    /* 4.1) DHCP control */
    if (CORE_AP_DHCP_ENABLED == 0)
    {
        esp_err_t dhcp_error = esp_netif_dhcps_stop(ap_netif);
        if (dhcp_error != ESP_OK && dhcp_error != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED)
        {
            ESP_LOGW(TAG, "Failed to stop DHCP server: %s", esp_err_to_name(dhcp_error));
        }
        else
        {
            ESP_LOGI(TAG, "DHCP server stopped");
        }
    }

    /* 5) WiFi init */
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&config));

    /* 6) AP configuration */
    wifi_config_t wifi_config = {0};

    strncpy((char *)wifi_config.ap.ssid, CORE_AP_SSID, sizeof(wifi_config.ap.ssid));
    wifi_config.ap.ssid[sizeof(wifi_config.ap.ssid) - 1] = '\0';
    wifi_config.ap.ssid_len = strlen((char *)wifi_config.ap.ssid);

    wifi_config.ap.channel = WIFI_AP_CHANNEL;
    wifi_config.ap.max_connection = WIFI_AP_MAX_CONNECTIONS;

    if (CORE_AP_OPEN_DEFAULT == 1)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
        ESP_LOGI(TAG, "AP security: OPEN");
    }
    else
    {
        wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
        ESP_LOGI(TAG, "AP security: WPA2 (PSK provision pending)");
    }

    /* 7) Start AP */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "AP started: SSID=%s", wifi_config.ap.ssid);

    return ESP_OK;
}

uint16_t wifi_ap_get_client_count(void)
{
    wifi_sta_list_t list = {0};
    if (esp_wifi_ap_get_sta_list(&list) != ESP_OK)
    {
        return 0;
    }
    return (uint16_t)list.num;
}
