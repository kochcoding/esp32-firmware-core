/**
 * @file http_helpers.c
 * @brief Implementation of helper utilities for ESP-IDF's HTTP server.
 *
 * @details
 *  - No dynamic allocation.
 *  - All outputs are bounded and null-terminated where applicable.
 *  - Callers are responsible for passing safe ASCII-only error strings.
 */

//------------------------------------------------------------------------------
// private includes
//------------------------------------------------------------------------------
#include "http/http_helpers.h"

#include <stdio.h>
#include <string.h>

//------------------------------------------------------------------------------
// private defines
//------------------------------------------------------------------------------

#define HTTP_STATUS_OK (200U)
#define HTTP_STATUS_CREATED (201U)
#define HTTP_STATUS_BAD_REQUEST (400U)
#define HTTP_STATUS_NOT_FOUND (404U)
#define HTTP_STATUS_CONFLICT (409U)
#define HTTP_STATUS_INTERNAL_ERROR (500U)
#define HTTP_STATUS_BAD_GATEWAY (502U)

//------------------------------------------------------------------------------
// private typedefs
//------------------------------------------------------------------------------

typedef struct
{
    uint16_t code;
    const char *text;
} http_status_entry_t;

//------------------------------------------------------------------------------
// private variables
//------------------------------------------------------------------------------

static const http_status_entry_t http_status_table[] = {
    {HTTP_STATUS_OK, "200 OK"},
    {HTTP_STATUS_CREATED, "201 Created"},
    {HTTP_STATUS_BAD_REQUEST, "400 Bad Request"},
    {HTTP_STATUS_NOT_FOUND, "404 Not Found"},
    {HTTP_STATUS_CONFLICT, "409 Conflict"},
    {HTTP_STATUS_BAD_GATEWAY, "502 Bad Gateway"},

    /* Fallback / default */
    {HTTP_STATUS_INTERNAL_ERROR, "500 Internal Server Error"}};

//------------------------------------------------------------------------------
// private function (prototypes)
//------------------------------------------------------------------------------
static bool hex_character_to_value(uint8_t *out_value, char hex_character);

static void set_status(httpd_req_t *request, uint16_t status_code);

//------------------------------------------------------------------------------
// public functions
//------------------------------------------------------------------------------

bool http_read_body(httpd_req_t *request, char *buffer, size_t buffer_length, size_t *out_length)
{
    if ((request == NULL) || (buffer == NULL) || (buffer_length < 2U))
    {
        return false;
    }

    int total = request->content_len;
    if (total <= 0 || (size_t)total >= buffer_length)
    {
        return false;
    }

    int received = 0;
    while (received < total)
    {
        int r = httpd_req_recv(request, buffer + received, total - received);
        if (r <= 0)
        {
            return false;
        }
        received += r;
    }
    buffer[received] = '\0';
    if (out_length)
    {
        *out_length = (size_t)received;
    }
    return true;
}

bool http_url_decode(const char *in, char *out, size_t out_length)
{
    if ((in == NULL) || (out == NULL) || (out_length == 0U))
    {
        return false;
    }

    size_t i = 0;
    size_t j = 0;

    while (in[i] != '\0' && j < out_length - 1)
    {
        if (in[i] == '%' && in[i + 1] != '\0' && in[i + 2] != '\0')
        {
            uint8_t hi = 0U;
            uint8_t lo = 0U;

            if (hex_character_to_value(&hi, in[i + 1U]) && hex_character_to_value(&lo, in[i + 2U]))
            {
                out[j++] = (char)(((uint8_t)(hi << 4U)) | lo);
                i += 3U;
                continue;
            }
        }
        if (in[i] == '+')
        {
            out[j++] = ' ';
            i++;
            continue;
        }
        out[j++] = in[i++];
    }
    out[j] = '\0';
    return true;
}

void http_send_json(httpd_req_t *request, int status_code, const char *json)
{
    set_status(request, (uint16_t)status_code);
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_hdr(request, "Connection", "close");
    httpd_resp_send(request, json ? json : "{}", HTTPD_RESP_USE_STRLEN);
}

void http_send_err(httpd_req_t *request, int status_code, const char *message)
{
    char buffer[160];
    if (message == NULL)
    {
        message = "error";
    }
    snprintf(buffer, sizeof(buffer), "{\"ok\":false,\"error\":\"%s\"}", message);
    http_send_json(request, status_code, buffer);
}

//------------------------------------------------------------------------------
// private function (implementation)
//------------------------------------------------------------------------------
static bool hex_character_to_value(uint8_t *out_value, char hex_character)
{
    bool is_valid = false;

    if (out_value != NULL)
    {
        if (((uint8_t)hex_character >= (uint8_t)'0') && ((uint8_t)hex_character <= (uint8_t)'9'))
        {
            *out_value = (uint8_t)hex_character - (uint8_t)'0';
            is_valid = true;
        }
        else if (((uint8_t)hex_character >= (uint8_t)'a') &&
                 ((uint8_t)hex_character <= (uint8_t)'f'))
        {
            *out_value = (uint8_t)((uint8_t)hex_character - (uint8_t)'a') + 10U;
            is_valid = true;
        }
        else if (((uint8_t)hex_character >= (uint8_t)'A') &&
                 ((uint8_t)hex_character <= (uint8_t)'F'))
        {
            *out_value = (uint8_t)((uint8_t)hex_character - (uint8_t)'A') + 10U;
            is_valid = true;
        }
        else
        {
            /* hex_character is not a valid hex digit — is_valid remains false */
        }
    }
    else
    {
        /* out_value is a NULL pointer - is_valid remains false */
    }

    return is_valid;
}

static void set_status(httpd_req_t *request, uint16_t status_code)
{
    size_t index;
    const char *status_text = NULL;
    const size_t table_size = sizeof(http_status_table) / sizeof(http_status_table[0]);

    if (request == NULL)
    {
        /* NULL request: skip silently, ESP-IDF will handle the invalid state */
    }
    else
    {
        for (index = 0U; index < table_size; index++)
        {
            if (http_status_table[index].code == status_code)
            {
                status_text = http_status_table[index].text;
                break;
            }
        }

        if (status_text == NULL)
        {
            /* explicit search for 500 entry */
            for (index = 0U; index < table_size; index++)
            {
                if (http_status_table[index].code == HTTP_STATUS_INTERNAL_ERROR)
                {
                    status_text = http_status_table[index].text;
                    break;
                }
            }
        }

        httpd_resp_set_status(request, status_text);
    }
}