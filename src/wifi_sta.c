/**
 * @file wifi_sta.c
 * @brief Implementation of WiFi STA connection management.
 *
 * @details
 *  - Implements a state machine (IDLE → CONNECTING → CONNECTED/RETRYING/FAILED).
 *  - Exponential backoff retry via esp_timer (1s, 2s, ... capped at 10s).
 *  - Designed for APSTA mode; never switches WiFi mode autonomously.
 *  - IP octets are stored in wifi_sta_status_t for JSON serialisation.
 */

//------------------------------------------------------------------------------
// private includes
//------------------------------------------------------------------------------
#include "wifi_sta.h"

#include "app/app_settings_persistence.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include "lwip/inet.h"

//------------------------------------------------------------------------------
// private defines
//------------------------------------------------------------------------------

#define WIFI_STA_MAX_RETRIES (10U)
#define WIFI_STA_RETRY_BASE_MS (1000U)
#define WIFI_STA_RETRY_MAX_MS (10000U)
#define WIFI_STA_IP_STR_LEN (16U)
#define WIFI_STA_US_PER_MS (1000ULL)

//------------------------------------------------------------------------------
// private typedefs
//------------------------------------------------------------------------------

typedef struct
{
    wifi_sta_state_t from;
    wifi_sta_state_t to;
} sta_transition_t;

//------------------------------------------------------------------------------
// private variables
//------------------------------------------------------------------------------

static const char *TAG = "wifi_sta";

static esp_netif_t *s_sta_netif = NULL;

static wifi_sta_status_t s_status = {
    .state = WIFI_STA_STATE_IDLE,
    .ssid = {0},
    .ip = {0},
    .retry_count = 0,
};

static esp_timer_handle_t s_retry_timer = NULL;

static const sta_transition_t s_allowed_transitions[] = {
    {WIFI_STA_STATE_IDLE, WIFI_STA_STATE_CONNECTING},
    {WIFI_STA_STATE_CONNECTING, WIFI_STA_STATE_CONNECTED},
    {WIFI_STA_STATE_CONNECTING, WIFI_STA_STATE_RETRYING},
    {WIFI_STA_STATE_CONNECTING, WIFI_STA_STATE_FAILED},
    {WIFI_STA_STATE_RETRYING, WIFI_STA_STATE_CONNECTING},
    {WIFI_STA_STATE_CONNECTED, WIFI_STA_STATE_RETRYING},
    {WIFI_STA_STATE_FAILED, WIFI_STA_STATE_CONNECTING},
};

//------------------------------------------------------------------------------
// private functions (prototypes)
//------------------------------------------------------------------------------

/**
 * @brief Convert a STA state enum value to a human-readable string.
 *
 * @param[in] state State to convert.
 * @return Null-terminated string literal. Never NULL.
 */
static const char *state_to_str(wifi_sta_state_t state);

/**
 * @brief Attempt a state machine transition.
 *
 * Checks s_allowed_transitions[] and updates s_status.state if the
 * transition is valid. Logs a warning and returns false if not allowed.
 *
 * @param[in] new_state Target state.
 * @retval true  Transition accepted and applied.
 * @retval false Transition rejected (invalid from current state).
 */
static bool sta_transition(wifi_sta_state_t new_state);

/**
 * @brief esp_timer callback that retries the WiFi connection.
 *
 * Only acts if current state is WIFI_STA_STATE_RETRYING.
 *
 * @param[in] arg Unused.
 */
static void retry_timer_cb(void *arg);

/**
 * @brief Schedule a retry connection attempt with exponential backoff.
 *
 * Delay = WIFI_STA_RETRY_BASE_MS * (retry_count + 1), capped at
 * WIFI_STA_RETRY_MAX_MS. Does nothing if s_retry_timer is NULL.
 *
 * @param[in] retry_count Current retry count (used to compute delay).
 */
static void schedule_retry(uint32_t current_retry_count);

/**
 * @brief WiFi event handler for STA mode events.
 *
 * Handles STA_START, STA_CONNECTED, and STA_DISCONNECTED events.
 * Triggers retry scheduling on disconnect.
 *
 * @param[in] arg        Unused.
 * @param[in] event_base Event base (expected: WIFI_EVENT).
 * @param[in] event_id   WiFi event ID.
 * @param[in] event_data Event-specific data payload.
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                               void *event_data);

/**
 * @brief IP event handler that stores the assigned IP address.
 *
 * On IP_EVENT_STA_GOT_IP, extracts IP octets into s_status.ip
 * and transitions to WIFI_STA_STATE_CONNECTED.
 *
 * @param[in] arg        Unused.
 * @param[in] event_base Event base (expected: IP_EVENT).
 * @param[in] event_id   IP event ID.
 * @param[in] event_data Event-specific data payload.
 */
static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                             void *event_data);

/**
 * @brief Apply STA credentials and start an asynchronous connection.
 *
 * Validates WiFi mode is APSTA, sets STA config, disconnects any
 * previous session, and calls esp_wifi_connect() asynchronously.
 *
 * @param[in] ssid Null-terminated SSID. Must not be NULL or empty.
 * @param[in] pass Null-terminated password. Must not be NULL.
 * @retval ESP_OK              Connection attempt started.
 * @retval ESP_ERR_INVALID_ARG SSID empty or pass NULL.
 * @retval ESP_ERR_INVALID_STATE WiFi not in APSTA mode.
 */
static esp_err_t apply_and_connect_sta(const char *ssid, const char *pass);

//------------------------------------------------------------------------------
// private functions (implementation)
//------------------------------------------------------------------------------

static const char *state_to_str(wifi_sta_state_t state)
{
    switch (state)
    {
    case WIFI_STA_STATE_IDLE:
        return "IDLE";
    case WIFI_STA_STATE_CONNECTING:
        return "CONNECTING";
    case WIFI_STA_STATE_RETRYING:
        return "RETRYING";
    case WIFI_STA_STATE_CONNECTED:
        return "CONNECTED";
    case WIFI_STA_STATE_FAILED:
        return "FAILED";
    default:
        return "UNKNOWN";
    }
}

static bool sta_transition(wifi_sta_state_t new_state)
{
    const size_t transition_count =
        sizeof(s_allowed_transitions) / sizeof(s_allowed_transitions[0]);
    for (size_t i = 0; i < transition_count; i++)
    {
        if (s_allowed_transitions[i].from == s_status.state &&
            s_allowed_transitions[i].to == new_state)
        {
            ESP_LOGI(TAG, "State: %s -> %s", state_to_str(s_status.state), state_to_str(new_state));
            s_status.state = new_state;
            return true;
        }
    }

    ESP_LOGW(TAG, "Invalid transition: %s -> %s (ignored)", state_to_str(s_status.state),
             state_to_str(new_state));
    return false;
}

static void retry_timer_cb(void *arg)
{
    (void)arg;

    if (s_status.state != WIFI_STA_STATE_RETRYING)
    {
        return;
    }

    ESP_LOGW(TAG, "Retrying WiFi connect (retry=%u)", (unsigned)s_status.retry_count);
    sta_transition(WIFI_STA_STATE_CONNECTING);
    esp_wifi_connect();
}

static void schedule_retry(uint32_t current_retry_count)
{
    /* simple backoff: 1s, 2s, 3s... capped at 10s */
    uint32_t delay_ms = WIFI_STA_RETRY_BASE_MS * (current_retry_count + 1U);
    if (delay_ms > WIFI_STA_RETRY_MAX_MS)
    {
        delay_ms = WIFI_STA_RETRY_MAX_MS;
    }

    if (s_retry_timer == NULL)
    {
        return;
    }

    esp_timer_stop(s_retry_timer);
    esp_timer_start_once(s_retry_timer, (uint64_t)delay_ms * WIFI_STA_US_PER_MS);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                               void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base != WIFI_EVENT)
    {
        return;
    }

    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
        /* Connect is triggered by our connect() flow; keep it explicit. */
        break;

    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");
        /* Got IP comes via IP_EVENT_STA_GOT_IP */
        break;

    case WIFI_EVENT_STA_DISCONNECTED:
        ESP_LOGW(TAG, "WIFI_EVENT_STA_DISCONNECTED");
        memset(s_status.ip, 0, sizeof(s_status.ip));

        if (s_status.retry_count < WIFI_STA_MAX_RETRIES)
        {
            s_status.retry_count++;
            if (sta_transition(WIFI_STA_STATE_RETRYING) == true)
            {
                schedule_retry(s_status.retry_count);
            }
        }
        else
        {
            sta_transition(WIFI_STA_STATE_FAILED);
            ESP_LOGE(TAG, "STA connect failed after %u retries", (unsigned)s_status.retry_count);
        }
        break;

    default:
        break;
    }
}

static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                             void *event_data)
{
    (void)arg;

    if (event_base != IP_EVENT)
    {
        return;
    }

    if (event_id == IP_EVENT_STA_GOT_IP)
    {
        const ip_event_got_ip_t *ip_event = (const ip_event_got_ip_t *)event_data;

        s_status.ip[0] = ip4_addr1(&ip_event->ip_info.ip);
        s_status.ip[1] = ip4_addr2(&ip_event->ip_info.ip);
        s_status.ip[2] = ip4_addr3(&ip_event->ip_info.ip);
        s_status.ip[3] = ip4_addr4(&ip_event->ip_info.ip);

        s_status.retry_count = 0;
        sta_transition(WIFI_STA_STATE_CONNECTED);

        char ip_str[WIFI_STA_IP_STR_LEN] = {0};
        inet_ntoa_r(ip_event->ip_info.ip, ip_str, sizeof(ip_str));
        ESP_LOGI(TAG, "STA GOT IP: %s", ip_str);
    }
}

static esp_err_t apply_and_connect_sta(const char *ssid, const char *pass)
{
    if ((ssid == NULL) || (ssid[0] == '\0'))
    {
        ESP_LOGE(TAG, "apply_and_connect_sta: SSID missing");
        return ESP_ERR_INVALID_ARG;
    }
    if (pass == NULL)
    {
        ESP_LOGE(TAG, "apply_and_connect_sta: pass is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    /* We require APSTA to be set by wifi_init_ap() once at boot. */
    wifi_mode_t mode = WIFI_MODE_NULL;
    esp_err_t error = esp_wifi_get_mode(&mode);
    if (error != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_get_mode failed: %s", esp_err_to_name(error));
        return error;
    }

    if (mode != WIFI_MODE_APSTA)
    {
        ESP_LOGE(TAG,
                 "WiFi mode is %d, expected APSTA. "
                 "Do NOT switch mode here (would stop AP).",
                 (int)mode);
        return ESP_ERR_INVALID_STATE;
    }

    wifi_config_t sta_config = {0};

    strncpy((char *)sta_config.sta.ssid, ssid, sizeof(sta_config.sta.ssid));
    sta_config.sta.ssid[sizeof(sta_config.sta.ssid) - 1] = '\0';

    strncpy((char *)sta_config.sta.password, pass, sizeof(sta_config.sta.password));
    sta_config.sta.password[sizeof(sta_config.sta.password) - 1] = '\0';

    /* ensure we start from a clean state */
    (void)esp_wifi_disconnect();

    error = esp_wifi_set_config(WIFI_IF_STA, &sta_config);
    if (error != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_set_config(STA) failed: %s", esp_err_to_name(error));
        return error;
    }

    return esp_wifi_connect(); /* async */
}

//------------------------------------------------------------------------------
// public functions
//------------------------------------------------------------------------------

esp_err_t wifi_sta_init(void)
{
    /* Prefer existing default STA netif (created by wifi_init_ap) */
    if (s_sta_netif == NULL)
    {
        s_sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (s_sta_netif == NULL)
        {
            ESP_LOGW(TAG, "STA netif not found, creating default STA netif");
            s_sta_netif = esp_netif_create_default_wifi_sta();
            if (s_sta_netif == NULL)
            {
                ESP_LOGE(TAG, "Failed to create STA netif");
                return ESP_FAIL;
            }
        }
    }

    /* Event handlers (we can register additional ones; AP module already registered its own) */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                                        &ip_event_handler, NULL, NULL));

    /* Retry timer */
    if (s_retry_timer == NULL)
    {
        const esp_timer_create_args_t retry_timer_args = {.callback = &retry_timer_cb,
                                                          .arg = NULL,
                                                          .dispatch_method = ESP_TIMER_TASK,
                                                          .name = "sta_retry"};
        ESP_ERROR_CHECK(esp_timer_create(&retry_timer_args, &s_retry_timer));
    }

    ESP_LOGI(TAG, "STA subsystem initialized");
    return ESP_OK;
}

esp_err_t wifi_sta_connect_from_nvs(void)
{
    settings_wifi_t wifi_settings;
    esp_err_t error = app_settings_load_wifi(&wifi_settings);

    if (error == ESP_ERR_NOT_FOUND || wifi_settings.ssid[0] == '\0')
    {
        ESP_LOGI(TAG, "No STA SSID stored; staying in AP-only mode");
        /* Boot initialisation — no state transition triggered. */
        s_status.state = WIFI_STA_STATE_IDLE;
        return ESP_ERR_NOT_FOUND;
    }

    if (error != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to load WiFi settings: %s", esp_err_to_name(error));
        return error;
    }

    return wifi_sta_connect(wifi_settings.ssid, wifi_settings.pass);
}

esp_err_t wifi_sta_connect(const char *ssid, const char *pass)
{
    if ((ssid == NULL) || (ssid[0] == '\0'))
    {
        ESP_LOGW(TAG, "wifi_sta_connect: missing SSID");
        return ESP_ERR_INVALID_ARG;
    }

    const char *password = (pass != NULL) ? pass : "";

    if (password[0] == '\0')
    {
        ESP_LOGW(TAG, "Starting STA connect to SSID='%s' (Open Network / no password)", ssid);
    }
    else
    {
        ESP_LOGI(TAG, "Starting STA connect to SSID='%s' (pass_len=%u)", ssid,
                 (unsigned)strlen(password));
    }

    /* Stop any pending retry from a previous attempt. */
    if (s_retry_timer != NULL)
    {
        esp_timer_stop(s_retry_timer);
    }

    strncpy(s_status.ssid, ssid, sizeof(s_status.ssid));
    s_status.ssid[sizeof(s_status.ssid) - 1] = '\0';
    memset(s_status.ip, 0, sizeof(s_status.ip));
    s_status.retry_count = 0;
    sta_transition(WIFI_STA_STATE_CONNECTING);

    return apply_and_connect_sta(ssid, password);
}

wifi_sta_status_t wifi_sta_get_status(void) { return s_status; }

bool wifi_sta_is_connected(void) { return (s_status.state == WIFI_STA_STATE_CONNECTED); }

void wifi_sta_status_to_json(const wifi_sta_status_t *status, char *out_buffer, size_t out_length)
{
    if ((status == NULL) || (out_buffer == NULL) || (out_length == 0U))
    {
        return;
    }

    const char *state_str = "idle";
    if (status->state == WIFI_STA_STATE_CONNECTED)
    {
        state_str = "connected";
    }
    else if (status->state == WIFI_STA_STATE_CONNECTING)
    {
        state_str = "connecting";
    }
    else if (status->state == WIFI_STA_STATE_FAILED)
    {
        state_str = "failed";
    }
    else if (status->state == WIFI_STA_STATE_RETRYING)
    {
        state_str = "retrying";
    }

    char ip_str[WIFI_STA_IP_STR_LEN] = {0};
    if (status->state == WIFI_STA_STATE_CONNECTED)
    {
        snprintf(ip_str, sizeof(ip_str), "%u.%u.%u.%u", status->ip[0], status->ip[1], status->ip[2],
                 status->ip[3]);
    }

    snprintf(out_buffer, out_length, "{\"sta_state\":\"%s\",\"ip\":\"%s\"}", state_str, ip_str);
}