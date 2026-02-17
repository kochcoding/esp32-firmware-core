#include "http/http_helpers.h"
#include <stdio.h>
#include <string.h>

bool http_read_body(httpd_req_t *req, char *buf, size_t buf_len, size_t *out_len)
{
    if (!req || !buf || buf_len < 2)
    {
        return false;
    }

    int total = req->content_len;
    if (total <= 0 || (size_t)total >= buf_len)
    {
        return false;
    }

    int received = 0;
    while (received < total)
    {
        int r = httpd_req_recv(req, buf + received, total - received);
        if (r <= 0)
        {
            return false;
        }
        received += r;
    }
    buf[received] = '\0';
    if (out_len)
    {
        *out_len = (size_t)received;
    }
    return true;
}

static void set_status(httpd_req_t *req, int code)
{
    if (code == 200)
    {
        httpd_resp_set_status(req, "200 OK");
    }
    else if (code == 201)
    {
        httpd_resp_set_status(req, "201 Created");
    }
    else if (code == 400)
    {
        httpd_resp_set_status(req, "400 Bad Request");
    }
    else if (code == 404)
    {
        httpd_resp_set_status(req, "404 Not Found");
    }
    else if (code == 409)
    {
        httpd_resp_set_status(req, "409 Conflict");
    }
    else
    {
        httpd_resp_set_status(req, "500 Internal Server Error");
    }
}

void http_send_json(httpd_req_t *req, int status_code, const char *json)
{
    set_status(req, status_code);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send(req, json ? json : "{}", HTTPD_RESP_USE_STRLEN);
}

void http_send_err(httpd_req_t *req, int status_code, const char *msg)
{
    char buf[160];
    if (!msg)
    {
        msg = "error";
    }
    snprintf(buf, sizeof(buf), "{\"ok\":false,\"error\":\"%s\"}", msg);
    http_send_json(req, status_code, buf);
}
