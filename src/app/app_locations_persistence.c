/**
 * @file app_locations_persistence.c
 * @brief Implementation of NVS-backed persistence for the locations model.
 *
 * @details
 *  - Locations are serialised to JSON and stored as a single NVS string entry.
 *  - Dynamic allocation is used only for the intermediate JSON buffer.
 *  - Buffer size is bounded by NVS_JSON_MAX_LEN.
 */

//------------------------------------------------------------------------------
// private includes
//------------------------------------------------------------------------------
#include "app/app_locations_persistence.h"

#include "app/nvs_helpers.h"

#include "locations_storage.h"

#include <stdlib.h>
#include <string.h>

//------------------------------------------------------------------------------
// private defines
//------------------------------------------------------------------------------
#define NVS_KEY_LOCATIONS "locations"
#define NVS_JSON_MAX_LEN (2048U)

//------------------------------------------------------------------------------
// public functions
//------------------------------------------------------------------------------

esp_err_t app_locations_load(locations_model_t *out)
{
    if (out == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));

    char *buffer = calloc(1, NVS_JSON_MAX_LEN);
    if (buffer == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t error = nvs_load_json(NVS_KEY_LOCATIONS, buffer, NVS_JSON_MAX_LEN);
    if (error == ESP_OK)
    {
        error = locations_storage_from_json(buffer, out) ? ESP_OK : ESP_FAIL;
    }

    free(buffer);
    buffer = NULL;
    return error;
}

esp_err_t app_locations_save(const locations_model_t *in)
{
    if (in == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    char *buffer = calloc(1, NVS_JSON_MAX_LEN);
    if (buffer == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t error = ESP_FAIL;
    if (locations_storage_to_json(in, buffer, NVS_JSON_MAX_LEN) == true)
    {
        error = nvs_save_json(NVS_KEY_LOCATIONS, buffer);
    }

    free(buffer);
    buffer = NULL;
    return error;
}

esp_err_t app_locations_clear(void) { return nvs_erase_key_cfg(NVS_KEY_LOCATIONS); }