/**
 * @file app_locations_persistence.h
 * @brief Application-level persistence for the locations model.
 *
 * Provides load, save, and clear operations for the locations model,
 * backed by NVS JSON storage via nvs_helpers.
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

#include "locations_model.h"

//------------------------------------------------------------------------------
// public API
//------------------------------------------------------------------------------

/**
 * @brief Load all stored locations from NVS into a model.
 *
 * @param[out] out Caller-supplied model to populate. Must not be NULL.
 * @retval ESP_OK            Locations loaded and parsed successfully.
 * @retval ESP_ERR_NOT_FOUND No locations entry found in NVS.
 * @retval ESP_ERR_INVALID_ARG @p out is NULL.
 * @retval ESP_ERR_NO_MEM   Internal buffer allocation failed.
 * @retval ESP_FAIL          JSON parsing failed.
 */
esp_err_t app_locations_load(locations_model_t *out);

/**
 * @brief Serialise and save the locations model to NVS.
 *
 * @param[in] in Model to persist. Must not be NULL.
 * @retval ESP_OK            Saved successfully.
 * @retval ESP_ERR_INVALID_ARG @p in is NULL.
 * @retval ESP_ERR_NO_MEM   Internal buffer allocation failed.
 * @retval ESP_FAIL          JSON serialisation or NVS write failed.
 */
esp_err_t app_locations_save(const locations_model_t *in);

/**
 * @brief Erase the locations entry from NVS.
 *
 * @retval ESP_OK    Entry erased successfully.
 * @retval ESP_FAIL  NVS erase operation failed.
 */
esp_err_t app_locations_clear(void);

#ifdef __cplusplus
} /* extern "C" */
#endif