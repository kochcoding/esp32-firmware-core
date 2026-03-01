/**
 * @file http_helpers.h
 * @brief Reusable helper utilities for ESP-IDF's HTTP server (esp_http_server).
 *
 * Provides three building blocks used across all HTTP route handlers:
 *  - Safe request-body reading into a caller-supplied buffer
 *  - URL percent-decoding (%XX encoding and '+' as space)
 *  - Standardised JSON response helpers with hardening headers
 *
 * Design constraints:
 *  - No dynamic memory allocation
 *  - All buffer operations are bounded and null-terminated
 *  - All functions are safe to call with NULL arguments (defensive)
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

//------------------------------------------------------------------------------
// public includes
//------------------------------------------------------------------------------

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_http_server.h"

//------------------------------------------------------------------------------
// public API
//------------------------------------------------------------------------------

/**
 * @brief Read the complete HTTP request body into a caller-supplied buffer.
 *
 * Reads exactly @c req->content_len bytes using @c httpd_req_recv() in a loop
 * and appends a null terminator, making the result directly usable as a C string.
 *
 * @param[in]  request       HTTP request handle. Must not be NULL.
 * @param[out] buffer        Destination buffer. Must not be NULL.
 * @param[in]  buffer_length Size of @p buffer in bytes. Must be >= 2.
 * @param[out] out_length    Optional. If not NULL, receives the number of bytes
 *                           read, excluding the null terminator.
 *
 * @retval true  Complete body was read and null-terminated successfully.
 * @retval false Invalid arguments, content_len exceeds buffer, or receive error.
 */
bool http_read_body(httpd_req_t *request, char *buffer, size_t buffer_length, size_t *out_length);

/**
 * @brief Decode a percent-encoded (URL-encoded) string in-place into a buffer.
 *
 * Decoding rules applied in order:
 *  - @c %XX sequences are decoded into a single byte value
 *  - @c + characters are converted to a space (0x20)
 *  - Any incomplete or invalid @c % sequence is copied verbatim
 *
 * @param[in]  in         Null-terminated input string. Must not be NULL.
 * @param[out] out        Output buffer. Must not be NULL.
 * @param[in]  out_length Size of @p out in bytes. Must be >= 1.
 *
 * @retval true  Decoding completed. @p out is always null-terminated on success.
 * @retval false Invalid arguments. @p out is not modified.
 *
 * @note If the decoded output would exceed @p out_length, writing stops at the
 *       boundary. @p out is still null-terminated but the result is truncated.
 */
bool http_url_decode(const char *in, char *out, size_t out_length);

/**
 * @brief Send a JSON response with standard hardening headers.
 *
 * Sets the following headers on every response:
 *  - @c Content-Type: @c application/json
 *  - @c Cache-Control: @c no-store
 *  - @c Connection: @c close
 *
 * Supported status codes: 200, 201, 400, 404, 409, 502.
 * Any unrecognised code falls back to 500 Internal Server Error.
 *
 * @param[in] request     HTTP request handle. Must not be NULL.
 * @param[in] status_code HTTP status code (e.g. 200, 400, 404).
 * @param[in] json        JSON payload string. If NULL, @c "{}" is sent.
 */
void http_send_json(httpd_req_t *request, int status_code, const char *json);

/**
 * @brief Send a fixed-format JSON error response.
 *
 * Sends a response body of the form:
 * @code
 * {"ok":false,"error":"<msg>"}
 * @endcode
 *
 * @param[in] request     HTTP request handle. Must not be NULL.
 * @param[in] status_code HTTP status code (e.g. 400, 404, 500).
 * @param[in] message     Error string to embed. If NULL, @c "error" is used.
 *
 * @warning @p message is embedded directly without JSON escaping.
 *          Callers must ensure it contains only safe ASCII characters
 *          (no quotes, backslashes, or control characters).
 */
void http_send_err(httpd_req_t *request, int status_code, const char *message);

#ifdef __cplusplus
} // extern "C"
#endif