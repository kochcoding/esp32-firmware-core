#include <string.h>
#include <stdio.h>

#include <cJSON.h>

#include "settings_storage.h"

static const char *KEY_SSID = "ssid";
static const char *KEY_PASS = "pass";
static const char *KEY_PASS_LEN = "pass_len";

static void clear_wifi(settings_wifi_t *s)
{
    if (s != NULL)
    {
        (void)memset(s, 0, sizeof(*s));
    }
}

bool settings_storage_wifi_from_json(const char *json, settings_wifi_t *out)
{
    bool ok = false;

    if ((json == NULL) || (out == NULL))
    {
        return false;
    }

    cJSON *root = cJSON_Parse(json);
    if (root == NULL)
    {
        return false;
    }

    const cJSON *j_ssid = cJSON_GetObjectItemCaseSensitive(root, KEY_SSID);
    const cJSON *j_pass = cJSON_GetObjectItemCaseSensitive(root, KEY_PASS);

    if (cJSON_IsString(j_ssid) && (j_ssid->valuestring != NULL) && (j_ssid->valuestring[0] != '\0'))
    {
        clear_wifi(out);

        /* Bounded copy */
        (void)snprintf(out->ssid, sizeof(out->ssid), "%s", j_ssid->valuestring);

        if (cJSON_IsString(j_pass) && (j_pass->valuestring != NULL))
        {
            (void)snprintf(out->pass, sizeof(out->pass), "%s", j_pass->valuestring);
        }
        else
        {
            out->pass[0] = '\0';
        }

        ok = true;
    }

    cJSON_Delete(root);
    return ok;
}

size_t settings_storage_wifi_measure_json(const settings_wifi_t *s, bool include_pass)
{
    size_t needed = 0U;

    if (s == NULL)
    {
        return 0U;
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
    {
        return 0U;
    }

    (void)cJSON_AddStringToObject(root, KEY_SSID, s->ssid);
    if (include_pass)
    {
        (void)cJSON_AddStringToObject(root, KEY_PASS, s->pass);
    }
    else
    {
        (void)cJSON_AddNumberToObject(root, KEY_PASS_LEN, (int)strlen(s->pass));
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

bool settings_storage_wifi_to_json(const settings_wifi_t *s, bool include_pass, char *out_json, size_t out_len)
{
    bool ok = false;

    if ((s == NULL) || (out_json == NULL) || (out_len == 0U))
    {
        return false;
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
    {
        return false;
    }

    (void)cJSON_AddStringToObject(root, KEY_SSID, s->ssid);
    if (include_pass)
    {
        (void)cJSON_AddStringToObject(root, KEY_PASS, s->pass);
    }
    else
    {
        (void)cJSON_AddNumberToObject(root, KEY_PASS_LEN, (int)strlen(s->pass));
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