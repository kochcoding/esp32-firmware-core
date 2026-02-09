
#include <string.h>

#include "wifi_ap.h"
#include "core_config.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_err.h"

#include "lwip/inet.h"
#include "esp_mac.h"

static const char *TAG = "wifi_ap";

/* -------------------------------------------------------------------------- */
/* Temporary stub – replaced later by NVS-backed config store                  */
/* -------------------------------------------------------------------------- */
static bool get_provisioned_ap_psk(char *out_psk, size_t out_len)
{
    (void)out_psk;
    (void)out_len;
    return false;
}

/* -------------------------------------------------------------------------- */
/* Event handlers                                                              */
/* -------------------------------------------------------------------------- */
static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
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
            const wifi_event_ap_staconnected_t *e = event_data;
            ESP_LOGI(TAG, "Client connected: " MACSTR ", AID=%d",
                     MAC2STR(e->mac), e->aid);
            break;
        }

        case WIFI_EVENT_AP_STADISCONNECTED:
        {
            const wifi_event_ap_stadisconnected_t *e = event_data;
            ESP_LOGI(TAG, "Client disconnected: " MACSTR ", AID=%d, reason=%d",
                     MAC2STR(e->mac), e->aid, e->reason);
            break;
        }

        default:
            break;
        }
    }
}

static void ip_event_handler(void *arg,
                             esp_event_base_t event_base,
                             int32_t event_id,
                             void *event_data)
{
    (void)arg;

    if (event_base == IP_EVENT)
    {
        if (event_id == IP_EVENT_AP_STAIPASSIGNED)
        {
            const ip_event_ap_staipassigned_t *e = event_data;

            char ip_str[16] = {0};
            inet_ntoa_r(e->ip, ip_str, sizeof(ip_str));

            ESP_LOGI(TAG, "DHCP lease assigned: %s", ip_str);
        }
    }
}

/* -------------------------------------------------------------------------- */
/* WiFi AP init                                                                */
/* -------------------------------------------------------------------------- */
esp_err_t wifi_init_ap(void)
{
    /* 1) NVS init */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "NVS init failed (%s), erasing...", esp_err_to_name(ret));
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    else
    {
        ESP_ERROR_CHECK(ret);
    }

    /* 2) Netif + event loop */
    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        ESP_ERROR_CHECK(ret);
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        ESP_ERROR_CHECK(ret);
    }

    /* 3) Register handlers */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler, NULL, NULL));

    /* 4) Create AP netif */
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    if (!ap_netif)
    {
        ESP_LOGE(TAG, "Failed to create AP netif");
        return ESP_FAIL;
    }

    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    if (!sta_netif)
    {
        ESP_LOGE(TAG, "Failed to create STA netif");
        return ESP_FAIL;
    }

    /* 4.1) DHCP control */
    if (!CORE_AP_DHCP_ENABLED)
    {
        esp_err_t err = esp_netif_dhcps_stop(ap_netif);
        if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED)
        {
            ESP_LOGW(TAG, "Failed to stop DHCP server: %s", esp_err_to_name(err));
        }
        else
        {
            ESP_LOGI(TAG, "DHCP server stopped");
        }
    }

    /* 5) WiFi init */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* 6) AP configuration */
    wifi_config_t wifi_config = {0};

    strncpy((char *)wifi_config.ap.ssid, CORE_AP_SSID, sizeof(wifi_config.ap.ssid));
    wifi_config.ap.ssid[sizeof(wifi_config.ap.ssid) - 1] = '\0';
    wifi_config.ap.ssid_len = strlen((char *)wifi_config.ap.ssid);

    wifi_config.ap.channel = 1;
    wifi_config.ap.max_connection = 4;

    if (CORE_AP_OPEN_DEFAULT)
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
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA)); // -->neu

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
