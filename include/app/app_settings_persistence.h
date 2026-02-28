/**
 * @file app_settings_persistence.h
 * @brief Application-level persistence for WiFi station settings.
 *
 * Provides load, save, and clear operations for WiFi credentials,
 * backed by the ESP-IDF NVS API directly.
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

//------------------------------------------------------------------------------
// public includes
//------------------------------------------------------------------------------

#include "esp_err.h"
#include "settings_storage.h"

//------------------------------------------------------------------------------
// public API
//------------------------------------------------------------------------------

/**
 * @brief Load WiFi credentials from NVS.
 *
 * @param[out] out Caller-supplied struct to populate. Must not be NULL.
 * @retval ESP_OK              Credentials loaded successfully.
 * @retval ESP_ERR_NOT_FOUND   No SSID entry found in NVS.
 * @retval ESP_ERR_INVALID_ARG @p out is NULL.
 * @retval Other               NVS open or read error.
 */
esp_err_t app_settings_load_wifi(settings_wifi_t *out);

/**
 * @brief Save WiFi credentials to NVS.
 *
 * @param[in] in Credentials to persist. Must not be NULL and SSID must not be empty.
 * @retval ESP_OK              Saved and committed successfully.
 * @retval ESP_ERR_INVALID_ARG @p in is NULL or SSID is empty.
 * @retval Other               NVS open, write, or commit error.
 */
esp_err_t app_settings_save_wifi(const settings_wifi_t *in);

/**
 * @brief Erase WiFi credentials from NVS.
 *
 * Erases both SSID and password keys and commits the change.
 * Individual key erase errors are suppressed — only the commit result is returned.
 *
 * @retval ESP_OK   Credentials erased and committed successfully.
 * @retval Other    NVS open or commit error.
 */
esp_err_t app_settings_clear_wifi(void);

#ifdef __cplusplus
} /* extern "C" */
#endif