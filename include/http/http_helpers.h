#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_http_server.h"

bool http_read_body(httpd_req_t *req, char *buf, size_t buf_len, size_t *out_len);
void http_send_json(httpd_req_t *req, int status_code, const char *json);
void http_send_err(httpd_req_t *req, int status_code, const char *msg);
