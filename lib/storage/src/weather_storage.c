#include <string.h>

#include <cJSON.h>

#include "weather_storage.h"

bool weather_storage_validate_json(const char *json)
{
    if (json == NULL)
    {
        return false;
    }

    cJSON *root = cJSON_Parse(json);
    if (root == NULL)
    {
        return false;
    }

    cJSON_Delete(root);
    return true;
}

size_t weather_storage_measure_compact_json(const char *json)
{
    size_t needed = 0U;

    if (json == NULL)
    {
        return 0U;
    }

    cJSON *root = cJSON_Parse(json);
    if (root == NULL)
    {
        return 0U;
    }

    char *printed = cJSON_PrintUnformatted(root);
    if (printed != NULL)
    {
        needed = strlen(printed) + 1U;
        cJSON_free(printed);
    }

    cJSON_Delete(root);
    return needed;
}

bool weather_storage_compact_json(const char *json, char *out_json, size_t out_len)
{
    bool ok = false;

    if ((json == NULL) || (out_json == NULL) || (out_len == 0U))
    {
        return false;
    }

    cJSON *root = cJSON_Parse(json);
    if (root == NULL)
    {
        return false;
    }

    char *printed = cJSON_PrintUnformatted(root);
    if (printed != NULL)
    {
        const size_t needed = strlen(printed) + 1U;
        if (needed <= out_len)
        {
            (void)memcpy(out_json, printed, needed);
            ok = true;
        }

        cJSON_free(printed);
    }

    cJSON_Delete(root);
    return ok;
}
