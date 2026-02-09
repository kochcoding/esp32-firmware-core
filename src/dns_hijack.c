// dns_hijack.c
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"

#include "lwip/sockets.h"
#include "lwip/inet.h"

#include "dns_hijack.h"

static const char *TAG = "dns_hijack";

static TaskHandle_t s_task = NULL;
static int s_sock = -1;

// IPv4 reply address in network byte order (big-endian)
static uint32_t s_reply_ip_be = 0;

#pragma pack(push, 1)
typedef struct
{
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} dns_header_t;
#pragma pack(pop)

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

static void wr16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)((v >> 8) & 0xFF);
    p[1] = (uint8_t)(v & 0xFF);
}

static int build_dns_a_reply(const uint8_t *req, int req_len, uint8_t *resp, int resp_max)
{
    if (req_len < (int)sizeof(dns_header_t))
        return -1;

    // Ensure we have enough headroom for a minimal answer record
    if (resp_max < req_len + 16)
        return -1;

    // Copy request as base (header + question)
    memcpy(resp, req, req_len);

    dns_header_t *h = (dns_header_t *)resp;
    const uint16_t qd = ntohs(h->qdcount);
    if (qd < 1)
        return -1;

    // Walk question section end (first question only)
    int off = (int)sizeof(dns_header_t);

    // QNAME: labels, terminated by 0
    while (off < req_len)
    {
        uint8_t lab_len = resp[off++];
        if (lab_len == 0)
            break;
        if (off + lab_len > req_len)
            return -1;
        off += lab_len;
    }

    // Need QTYPE + QCLASS
    if (off + 4 > req_len)
        return -1;

    const uint16_t qtype = rd16(&resp[off + 0]);
    const uint16_t qclass = rd16(&resp[off + 2]);
    off += 4;

    // Response header:
    // 0x8180: QR=1, Opcode=0, AA=1? (not set here), TC=0, RD=1 (copied often), RA=0, RCODE=0
    // We'll just use a typical "NoError" response for captive DNS.
    h->flags = htons(0x8180);
    h->qdcount = htons(1);
    h->nscount = 0;
    h->arcount = 0;

    int resp_len = off; // header+question

    // Only answer IN A queries; otherwise NoError with 0 answers
    if (qtype != 1 || qclass != 1)
    {
        h->ancount = htons(0);
        return resp_len;
    }

    // Answer RR (16 bytes):
    // NAME: pointer to QNAME at 0x0C => 0xC00C
    // TYPE: A (1)
    // CLASS: IN (1)
    // TTL: 60
    // RDLEN: 4
    // RDATA: ipv4
    if (resp_len + 16 > resp_max)
        return -1;

    resp[resp_len + 0] = 0xC0;
    resp[resp_len + 1] = 0x0C;

    wr16(&resp[resp_len + 2], 1); // TYPE A
    wr16(&resp[resp_len + 4], 1); // CLASS IN

    // TTL (32-bit) = 60 seconds
    resp[resp_len + 6] = 0x00;
    resp[resp_len + 7] = 0x00;
    resp[resp_len + 8] = 0x00;
    resp[resp_len + 9] = 0x3C;

    wr16(&resp[resp_len + 10], 4); // RDLENGTH

    memcpy(&resp[resp_len + 12], &s_reply_ip_be, 4);

    resp_len += 16;
    h->ancount = htons(1);

    return resp_len;
}

static void dns_hijack_task(void *arg)
{
    (void)arg;

    uint8_t rx[512];
    uint8_t tx[544];

    while (1)
    {
        struct sockaddr_in from = {0};
        socklen_t from_len = sizeof(from);

        int r = recvfrom(s_sock, rx, sizeof(rx), 0, (struct sockaddr *)&from, &from_len);
        if (r < 0)
        {
            int e = errno;
            if (e == EINTR)
                continue;

            ESP_LOGE(TAG, "recvfrom failed: errno=%d", e);
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        int resp_len = build_dns_a_reply(rx, r, tx, sizeof(tx));
        if (resp_len <= 0)
        {
            // ignore malformed/unsupported queries
            continue;
        }

        (void)sendto(s_sock, tx, resp_len, 0, (struct sockaddr *)&from, from_len);
    }
}

esp_err_t dns_hijack_start(uint32_t ipv4_addr_be)
{
    if (s_task)
        return ESP_OK; // already running

    s_reply_ip_be = ipv4_addr_be;

    s_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_sock < 0)
    {
        ESP_LOGE(TAG, "socket() failed (errno=%d)", errno);
        return ESP_FAIL;
    }

    // Bind UDP :53 on all interfaces
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(53);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int yes = 1;
    (void)setsockopt(s_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    if (bind(s_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        ESP_LOGE(TAG, "bind(:53) failed (errno=%d)", errno);
        close(s_sock);
        s_sock = -1;
        return ESP_FAIL;
    }

    BaseType_t ok = xTaskCreate(dns_hijack_task, "dns_hijack", 4096, NULL, 5, &s_task);
    if (ok != pdPASS)
    {
        ESP_LOGE(TAG, "xTaskCreate failed");
        close(s_sock);
        s_sock = -1;
        s_task = NULL;
        return ESP_FAIL;
    }

    struct in_addr a = {.s_addr = s_reply_ip_be}; // inet_ntoa expects network order
    ESP_LOGI(TAG, "DNS hijack started on UDP :53, replying with %s", inet_ntoa(a));
    return ESP_OK;
}

esp_err_t dns_hijack_stop(void)
{
    if (!s_task)
        return ESP_OK;

    TaskHandle_t t = s_task;
    s_task = NULL;

    if (s_sock >= 0)
    {
        close(s_sock);
        s_sock = -1;
    }

    vTaskDelete(t);
    ESP_LOGI(TAG, "DNS hijack stopped");
    return ESP_OK;
}
