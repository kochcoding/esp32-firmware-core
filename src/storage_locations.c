#include "storage_locations.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>

#define NVS_KEY_ACTIVE_ID "active_loc_id"

#ifndef NVS_NAMESPACE
#define NVS_NAMESPACE "storage_locations"
#endif

static const char *TAG = "storage_locations";
static const char *NVS_NS = "app";
static const char *NVS_KEY = "locations";

static esp_err_t nvs_get_str_alloc(nvs_handle_t h, const char *key, char **out)
{
    size_t len = 0;
    esp_err_t err = nvs_get_str(h, key, NULL, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        *out = NULL;
        return ESP_OK;
    }
    if (err != ESP_OK)
        return err;

    char *buf = calloc(1, len);
    if (!buf)
        return ESP_ERR_NO_MEM;

    err = nvs_get_str(h, key, buf, &len);
    if (err != ESP_OK)
    {
        free(buf);
        return err;
    }

    *out = buf;
    return ESP_OK;
}

static esp_err_t nvs_set_str_commit(nvs_handle_t h, const char *key, const char *val)
{
    esp_err_t err = nvs_set_str(h, key, val);
    if (err != ESP_OK)
        return err;
    return nvs_commit(h);
}

esp_err_t storage_locations_init(void)
{
    // falls du NVS global schon initst, dann kann das hier auch leer bleiben.
    return ESP_OK;
}

esp_err_t storage_locations_add(const location_t *loc, int max_locations)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK)
        return err;

    char *json = NULL;
    err = nvs_get_str_alloc(h, NVS_KEY, &json);
    if (err != ESP_OK)
    {
        nvs_close(h);
        return err;
    }

    cJSON *arr = NULL;
    if (!json)
    {
        arr = cJSON_CreateArray();
    }
    else
    {
        arr = cJSON_Parse(json);
        free(json);
        if (!arr || !cJSON_IsArray(arr))
        {
            cJSON_Delete(arr);
            arr = cJSON_CreateArray();
        }
    }

    // 1) Dedup: wenn ID schon existiert -> update statt add
    int count = cJSON_GetArraySize(arr);
    for (int i = 0; i < count; i++)
    {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        if (!cJSON_IsObject(item))
            continue;

        cJSON *id = cJSON_GetObjectItemCaseSensitive(item, "id");
        if (cJSON_IsString(id) && id->valuestring && strcmp(id->valuestring, loc->id) == 0)
        {
            // Update existing fields
            cJSON_ReplaceItemInObjectCaseSensitive(item, "name", cJSON_CreateString(loc->name));
            cJSON_ReplaceItemInObjectCaseSensitive(item, "lat", cJSON_CreateNumber(loc->lat));
            cJSON_ReplaceItemInObjectCaseSensitive(item, "lon", cJSON_CreateNumber(loc->lon));
            cJSON_ReplaceItemInObjectCaseSensitive(item, "timezone", cJSON_CreateString(loc->timezone));
            cJSON_ReplaceItemInObjectCaseSensitive(item, "country", cJSON_CreateString(loc->country));

            // Save array back to NVS
            char *out = cJSON_PrintUnformatted(arr);
            cJSON_Delete(arr);
            if (!out)
            {
                nvs_close(h);
                return ESP_ERR_NO_MEM;
            }

            err = nvs_set_str_commit(h, NVS_KEY, out);
            free(out);
            nvs_close(h);
            return err; // fertig: kein Duplikat angelegt
        }
    }

    // 2) Max-Limit check nur wenn wir wirklich neu hinzufügen müssen
    if (count >= max_locations)
    {
        cJSON_Delete(arr);
        nvs_close(h);
        return ESP_ERR_INVALID_SIZE;
    }

    // 3) Add new entry
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "id", loc->id);
    cJSON_AddStringToObject(o, "name", loc->name);
    cJSON_AddNumberToObject(o, "lat", loc->lat);
    cJSON_AddNumberToObject(o, "lon", loc->lon);
    cJSON_AddStringToObject(o, "timezone", loc->timezone);
    cJSON_AddStringToObject(o, "country", loc->country);

    cJSON_AddItemToArray(arr, o);

    // Save
    char *out = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    if (!out)
    {
        nvs_close(h);
        return ESP_ERR_NO_MEM;
    }

    err = nvs_set_str_commit(h, NVS_KEY, out);
    free(out);
    nvs_close(h);
    return err;
}

esp_err_t storage_locations_get_all_json(char **out_json) // Rückgabe: JSON string, caller frees
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err != ESP_OK)
        return err;

    char *json = NULL;
    err = nvs_get_str_alloc(h, NVS_KEY, &json);
    nvs_close(h);
    if (err != ESP_OK)
        return err;

    if (!json)
    {
        // immer ein Array liefern
        *out_json = strdup("[]");
        return (*out_json) ? ESP_OK : ESP_ERR_NO_MEM;
    }

    *out_json = json;
    return ESP_OK;
}

esp_err_t storage_locations_delete_by_id(const char *id)
{
    if (!id || !id[0])
        return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK)
        return err;

    char *json = NULL;
    err = nvs_get_str_alloc(h, NVS_KEY, &json);
    if (err != ESP_OK)
    {
        nvs_close(h);
        return err;
    }

    if (!json)
    {
        // nichts gespeichert
        nvs_close(h);
        return ESP_ERR_NOT_FOUND;
    }

    cJSON *arr = cJSON_Parse(json);
    free(json);

    if (!arr || !cJSON_IsArray(arr))
    {
        cJSON_Delete(arr);
        nvs_close(h);
        return ESP_ERR_INVALID_STATE;
    }

    int n = cJSON_GetArraySize(arr);
    int removed_index = -1;

    for (int i = 0; i < n; i++)
    {
        cJSON *o = cJSON_GetArrayItem(arr, i);
        cJSON *jid = cJSON_GetObjectItem(o, "id");
        if (jid && cJSON_IsString(jid) && jid->valuestring && strcmp(jid->valuestring, id) == 0)
        {
            removed_index = i;
            break;
        }
    }

    if (removed_index < 0)
    {
        cJSON_Delete(arr);
        nvs_close(h);
        return ESP_ERR_NOT_FOUND;
    }

    // Entfernen
    cJSON_DeleteItemFromArray(arr, removed_index);

    char *out = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);

    if (!out)
    {
        nvs_close(h);
        return ESP_ERR_NO_MEM;
    }

    err = nvs_set_str_commit(h, NVS_KEY, out);
    free(out);
    nvs_close(h);

    // Wenn die gelöschte Location active war -> active löschen
    char active[64] = {0};
    if (storage_locations_get_active_id(active, sizeof(active)) == ESP_OK)
    {
        if (strcmp(active, id) == 0)
        {
            storage_locations_clear_active_id();
        }
    }

    return err;
}

esp_err_t storage_locations_clear_all(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK)
        return err;

    err = nvs_erase_key(h, NVS_KEY);
    if (err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND)
    {
        esp_err_t err2 = nvs_commit(h);
        nvs_close(h);
        return err2;
    }

    nvs_close(h);
    return err;
}

esp_err_t storage_locations_remove_by_id(const char *id)
{
    if (!id || !id[0])
        return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK)
        return err;

    char *json = NULL;
    err = nvs_get_str_alloc(h, NVS_KEY, &json);
    if (err != ESP_OK)
    {
        nvs_close(h);
        return err;
    }

    if (!json)
    {
        nvs_close(h);
        return ESP_ERR_NOT_FOUND;
    }

    cJSON *arr = cJSON_Parse(json);
    free(json);
    if (!arr || !cJSON_IsArray(arr))
    {
        cJSON_Delete(arr);
        nvs_close(h);
        return ESP_FAIL;
    }

    bool removed = false;
    int n = cJSON_GetArraySize(arr);
    for (int i = 0; i < n; i++)
    {
        cJSON *o = cJSON_GetArrayItem(arr, i);
        cJSON *jid = cJSON_GetObjectItem(o, "id");
        if (cJSON_IsString(jid) && jid->valuestring && strcmp(jid->valuestring, id) == 0)
        {
            cJSON_DeleteItemFromArray(arr, i);
            removed = true;
            break;
        }
    }

    if (!removed)
    {
        cJSON_Delete(arr);
        nvs_close(h);
        return ESP_ERR_NOT_FOUND;
    }

    char *out = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    if (!out)
    {
        nvs_close(h);
        return ESP_ERR_NO_MEM;
    }

    err = nvs_set_str_commit(h, NVS_KEY, out);
    free(out);
    nvs_close(h);
    return err;
}

esp_err_t storage_locations_get_by_id(const char *id, location_t *out_loc)
{
    if (!id || !out_loc)
        return ESP_ERR_INVALID_ARG;

    char *json = NULL;
    esp_err_t err = storage_locations_get_all_json(&json);
    if (err != ESP_OK || !json)
    {
        free(json);
        return err != ESP_OK ? err : ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(json);
    free(json);
    if (!root || !cJSON_IsArray(root))
    {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    esp_err_t ret = ESP_ERR_NOT_FOUND;

    const int n = cJSON_GetArraySize(root);
    for (int i = 0; i < n; i++)
    {
        cJSON *it = cJSON_GetArrayItem(root, i);
        if (!cJSON_IsObject(it))
            continue;

        cJSON *jid = cJSON_GetObjectItem(it, "id");
        if (!cJSON_IsString(jid) || !jid->valuestring)
            continue;

        if (strcmp(jid->valuestring, id) == 0)
        {
            cJSON *jname = cJSON_GetObjectItem(it, "name");
            cJSON *jlat = cJSON_GetObjectItem(it, "lat");
            cJSON *jlon = cJSON_GetObjectItem(it, "lon");
            cJSON *jtz = cJSON_GetObjectItem(it, "timezone");

            memset(out_loc, 0, sizeof(*out_loc));
            snprintf(out_loc->id, sizeof(out_loc->id), "%s", jid->valuestring);
            if (cJSON_IsString(jname) && jname->valuestring)
                snprintf(out_loc->name, sizeof(out_loc->name), "%s", jname->valuestring);
            if (cJSON_IsNumber(jlat))
                out_loc->lat = (float)jlat->valuedouble;
            if (cJSON_IsNumber(jlon))
                out_loc->lon = (float)jlon->valuedouble;
            if (cJSON_IsString(jtz) && jtz->valuestring)
                snprintf(out_loc->timezone, sizeof(out_loc->timezone), "%s", jtz->valuestring);

            ret = ESP_OK;
            break;
        }
    }

    cJSON_Delete(root);
    return ret;
}

esp_err_t storage_locations_get_first(location_t *out_loc)
{
    if (!out_loc)
        return ESP_ERR_INVALID_ARG;

    char *json = NULL;
    esp_err_t err = storage_locations_get_all_json(&json);
    if (err != ESP_OK || !json)
    {
        free(json);
        return err != ESP_OK ? err : ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(json);
    free(json);
    if (!root || !cJSON_IsArray(root) || cJSON_GetArraySize(root) <= 0)
    {
        cJSON_Delete(root);
        return ESP_ERR_NOT_FOUND;
    }

    cJSON *it = cJSON_GetArrayItem(root, 0);
    if (!cJSON_IsObject(it))
    {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    cJSON *jid = cJSON_GetObjectItem(it, "id");
    cJSON *jname = cJSON_GetObjectItem(it, "name");
    cJSON *jlat = cJSON_GetObjectItem(it, "lat");
    cJSON *jlon = cJSON_GetObjectItem(it, "lon");
    cJSON *jtz = cJSON_GetObjectItem(it, "timezone");

    if (!cJSON_IsString(jid) || !jid->valuestring)
    {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    memset(out_loc, 0, sizeof(*out_loc));
    snprintf(out_loc->id, sizeof(out_loc->id), "%s", jid->valuestring);
    if (cJSON_IsString(jname) && jname->valuestring)
        snprintf(out_loc->name, sizeof(out_loc->name), "%s", jname->valuestring);
    if (cJSON_IsNumber(jlat))
        out_loc->lat = (float)jlat->valuedouble;
    if (cJSON_IsNumber(jlon))
        out_loc->lon = (float)jlon->valuedouble;
    if (cJSON_IsString(jtz) && jtz->valuestring)
        snprintf(out_loc->timezone, sizeof(out_loc->timezone), "%s", jtz->valuestring);

    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t storage_locations_set_active_id(const char *id)
{
    if (!id || id[0] == '\0')
        return ESP_ERR_INVALID_ARG;

    // Nur setzen, wenn Location existiert (sonst später Ärger)
    location_t tmp = {0};
    esp_err_t e = storage_locations_get_by_id(id, &tmp);
    if (e != ESP_OK)
        return e; // ESP_ERR_NOT_FOUND etc.

    nvs_handle_t nvs;
    e = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (e != ESP_OK)
        return e;

    e = nvs_set_str(nvs, NVS_KEY_ACTIVE_ID, id);
    if (e == ESP_OK)
        e = nvs_commit(nvs);
    nvs_close(nvs);
    return e;
}

esp_err_t storage_locations_get_active_id(char *out_id, size_t out_len)
{
    if (!out_id || out_len == 0)
        return ESP_ERR_INVALID_ARG;

    nvs_handle_t nvs;
    esp_err_t e = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (e != ESP_OK)
        return e;

    size_t needed = 0;
    e = nvs_get_str(nvs, NVS_KEY_ACTIVE_ID, NULL, &needed);
    if (e != ESP_OK)
    {
        nvs_close(nvs);
        return e; // ESP_ERR_NVS_NOT_FOUND wenn nichts gesetzt
    }

    if (needed > out_len)
    {
        nvs_close(nvs);
        return ESP_ERR_NVS_INVALID_LENGTH;
    }

    e = nvs_get_str(nvs, NVS_KEY_ACTIVE_ID, out_id, &needed);
    nvs_close(nvs);
    return e;
}

esp_err_t storage_locations_clear_active_id(void)
{
    nvs_handle_t nvs;
    esp_err_t e = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (e != ESP_OK)
        return e;

    e = nvs_erase_key(nvs, NVS_KEY_ACTIVE_ID);
    if (e == ESP_OK)
        e = nvs_commit(nvs);
    nvs_close(nvs);
    return e;
}
