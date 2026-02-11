#include <string.h>
#include <stdio.h>

#include <cJSON.h>

#include "storage_keys.h"
#include "locations_storage.h"

static void clear_model(locations_model_t *model)
{
    if (model != NULL)
    {
        (void)memset(model, 0, sizeof(*model));
    }
}

static bool read_location_object(const cJSON *obj, location_t *out_loc)
{
    bool ok = false;

    if ((obj != NULL) && (out_loc != NULL))
    {
        const cJSON *j_name = cJSON_GetObjectItemCaseSensitive(obj, STORAGE_KEY_NAME);
        const cJSON *j_lat = cJSON_GetObjectItemCaseSensitive(obj, STORAGE_KEY_LATITUDE);
        const cJSON *j_lon = cJSON_GetObjectItemCaseSensitive(obj, STORAGE_KEY_LONGITUDE);
        const cJSON *j_act = cJSON_GetObjectItemCaseSensitive(obj, STORAGE_KEY_IS_ACTIVE);

        if (cJSON_IsString(j_name) && (j_name->valuestring != NULL) &&
            cJSON_IsNumber(j_lat) &&
            cJSON_IsNumber(j_lon) &&
            (cJSON_IsBool(j_act) || (j_act == NULL)))
        {
            (void)memset(out_loc, 0, sizeof(*out_loc));

            /* Name is bounded */
            (void)snprintf(out_loc->name, sizeof(out_loc->name), "%s", j_name->valuestring);

            out_loc->latitude = j_lat->valuedouble;
            out_loc->longitude = j_lon->valuedouble;

            /* is_active optional -> default false */
            out_loc->is_active = (j_act != NULL) ? cJSON_IsTrue(j_act) : false;

            ok = true;
        }
    }

    return ok;
}

bool locations_storage_from_json(const char *json, locations_model_t *out_model)
{
    bool return_value = false;

    if ((json == NULL) || (out_model == NULL))
    {
        return_value = false;
    }
    else
    {
        cJSON *root = cJSON_Parse(json);
        if (root == NULL)
        {
            return_value = false;
        }
        else
        {
            const cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, STORAGE_KEY_LOCATIONS_ARRAY);
            if (!cJSON_IsArray(arr))
            {
                return_value = false;
            }
            else
            {
                locations_model_t tmp;
                bool active_seen = false;

                clear_model(&tmp);
                return_value = true;

                const cJSON *item = NULL;
                cJSON_ArrayForEach(item, arr)
                {
                    if (tmp.count >= LOCATIONS_MODEL_MAX_NUMBER)
                    {
                        return_value = false;
                        break;
                    }

                    location_t loc;
                    if (!read_location_object(item, &loc))
                    {
                        return_value = false;
                        break;
                    }

                    /* Enforce “only one active”: keep first, clear others */
                    if (loc.is_active)
                    {
                        if (active_seen)
                        {
                            loc.is_active = false;
                        }
                        else
                        {
                            active_seen = true;
                        }
                    }

                    /* We intentionally do NOT call locations_model_add here because:
                       - we want to preserve stored order
                       - we want storage to be independent from domain rules if needed
                       If you want strict domain rules, switch to locations_model_add(&tmp,&loc). */
                    tmp.items[tmp.count] = loc;
                    tmp.count++;
                }

                if (return_value)
                {
                    *out_model = tmp;
                }
            }

            cJSON_Delete(root);
        }
    }

    return return_value;
}

size_t locations_storage_measure_json(const locations_model_t *model)
{
    size_t needed = 0U;

    if (model != NULL)
    {
        /* Build JSON and measure length; simplest and reliable */
        cJSON *root = cJSON_CreateObject();
        cJSON *arr = cJSON_CreateArray();

        if ((root != NULL) && (arr != NULL))
        {
            (void)cJSON_AddItemToObject(root, STORAGE_KEY_LOCATIONS_ARRAY, arr);

            for (size_t i = 0U; i < model->count; i++)
            {
                const location_t *loc = &model->items[i];

                cJSON *obj = cJSON_CreateObject();
                if (obj == NULL)
                {
                    needed = 0U;
                    break;
                }

                (void)cJSON_AddStringToObject(obj, STORAGE_KEY_NAME, loc->name);
                (void)cJSON_AddNumberToObject(obj, STORAGE_KEY_LATITUDE, loc->latitude);
                (void)cJSON_AddNumberToObject(obj, STORAGE_KEY_LONGITUDE, loc->longitude);
                (void)cJSON_AddBoolToObject(obj, STORAGE_KEY_IS_ACTIVE, loc->is_active);

                cJSON_AddItemToArray(arr, obj);
            }

            if (needed == 0U)
            {
                /* keep 0 */
            }
            else
            {
                /* not used */
            }

            char *printed = cJSON_PrintUnformatted(root);
            if (printed != NULL)
            {
                needed = strlen(printed) + 1U;
                cJSON_free(printed);
            }
            else
            {
                needed = 0U;
            }
        }

        if (root != NULL)
        {
            cJSON_Delete(root);
        }
    }

    return needed;
}

bool locations_storage_to_json(const locations_model_t *model, char *out_json, size_t out_len)
{
    bool return_value = false;

    if ((model == NULL) || (out_json == NULL) || (out_len == 0U))
    {
        return_value = false;
    }
    else
    {
        cJSON *root = cJSON_CreateObject();
        cJSON *arr = cJSON_CreateArray();

        if ((root == NULL) || (arr == NULL))
        {
            return_value = false;
        }
        else
        {
            (void)cJSON_AddItemToObject(root, STORAGE_KEY_LOCATIONS_ARRAY, arr);

            return_value = true;

            for (size_t i = 0U; i < model->count; i++)
            {
                const location_t *loc = &model->items[i];

                cJSON *obj = cJSON_CreateObject();
                if (obj == NULL)
                {
                    return_value = false;
                    break;
                }

                (void)cJSON_AddStringToObject(obj, STORAGE_KEY_NAME, loc->name);
                (void)cJSON_AddNumberToObject(obj, STORAGE_KEY_LATITUDE, loc->latitude);
                (void)cJSON_AddNumberToObject(obj, STORAGE_KEY_LONGITUDE, loc->longitude);
                (void)cJSON_AddBoolToObject(obj, STORAGE_KEY_IS_ACTIVE, loc->is_active);

                cJSON_AddItemToArray(arr, obj);
            }

            if (return_value)
            {
                char *printed = cJSON_PrintUnformatted(root);
                if (printed == NULL)
                {
                    return_value = false;
                }
                else
                {
                    const size_t needed = strlen(printed) + 1U;
                    if (needed > out_len)
                    {
                        return_value = false;
                    }
                    else
                    {
                        (void)memcpy(out_json, printed, needed);
                        return_value = true;
                    }

                    cJSON_free(printed);
                }
            }
        }

        if (root != NULL)
        {
            cJSON_Delete(root);
        }
    }

    return return_value;
}