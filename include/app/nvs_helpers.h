/**
 * @file nvs_helpers.h
 * @brief Convenience wrappers for NVS JSON string storage.
 *
 * All functions operate on the shared NVS namespace defined by @c NVS_NS_CFG.
 * They provide a simplified interface over the raw ESP-IDF NVS API for
 * storing and retrieving null-terminated JSON strings.
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

//------------------------------------------------------------------------------
// public includes
//------------------------------------------------------------------------------

#include <stddef.h>

#include "esp_err.h"

//------------------------------------------------------------------------------
// public defines
//------------------------------------------------------------------------------

/** @brief NVS namespace used for all application configuration entries. */
#define NVS_NS_CFG "cfg"

//------------------------------------------------------------------------------
// public API
//------------------------------------------------------------------------------

/**
 * @brief Load a JSON string from NVS into a caller-supplied buffer.
 *
 * @param[in]  key        NVS key string. Must not be NULL.
 * @param[out] out_buffer Destination buffer. Must not be NULL.
 * @param[in]  out_length Size of @p out_buf in bytes. Must be >= 1.
 * @retval ESP_OK            Value loaded successfully.
 * @retval ESP_ERR_NOT_FOUND Key does not exist in NVS.
 * @retval ESP_ERR_INVALID_ARG Invalid arguments.
 * @retval Other             NVS open or read error.
 */
esp_err_t nvs_load_json(const char *key, char *out_buffer, size_t out_length);

/**
 * @brief Save a JSON string to NVS and commit.
 *
 * @param[in] key  NVS key string. Must not be NULL.
 * @param[in] json Null-terminated JSON string to store. Must not be NULL.
 * @retval ESP_OK    Saved and committed successfully.
 * @retval ESP_ERR_INVALID_ARG Invalid arguments.
 * @retval Other     NVS open, write, or commit error.
 */
esp_err_t nvs_save_json(const char *key, const char *json);

/**
 * @brief Erase a single key from the NVS configuration namespace and commit.
 *
 * The individual key erase error is suppressed — only the commit result
 * is returned, consistent with @c app_settings_clear_wifi behaviour.
 *
 * @param[in] key NVS key string. Must not be NULL.
 * @retval ESP_OK    Key erased and committed successfully.
 * @retval ESP_ERR_INVALID_ARG @p key is NULL.
 * @retval Other     NVS open or commit error.
 */
esp_err_t nvs_erase_key_cfg(const char *key);

#ifdef __cplusplus
} /* extern "C" */
#endif