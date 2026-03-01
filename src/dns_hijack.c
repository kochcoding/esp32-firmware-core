/**
 * @file dns_hijack.c
 * @brief Implementation of the captive portal DNS hijack server.
 *
 * @details
 *  - Listens on UDP port 53 and responds to all DNS A queries with a
 *    fixed IPv4 address to redirect clients to the captive portal.
 *  - Non-A queries receive a valid NoError response with zero answers.
 *  - Malformed or truncated queries are silently discarded.
 *  - The server runs in a dedicated FreeRTOS task.
 */

//------------------------------------------------------------------------------
// private includes
//------------------------------------------------------------------------------
#include "dns_hijack.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"

#include "lwip/inet.h"
#include "lwip/sockets.h"

//------------------------------------------------------------------------------
// private defines
//------------------------------------------------------------------------------

#define DNS_RX_BUF_SIZE (512U)
#define DNS_TX_BUF_SIZE (544U)
#define DNS_PORT (53U)
#define DNS_TASK_STACK_SIZE (4096U)
#define DNS_TASK_PRIORITY (5U)
#define DNS_TTL_SECONDS (60U)
#define DNS_MIN_REPLY_MARGIN (16U)
#define DNS_ANSWER_RR_SIZE (16U)
#define DNS_FLAGS_NOERROR (0x8180U)
#define DNS_TYPE_A (1U)
#define DNS_CLASS_IN (1U)
#define DNS_POINTER_QNAME (0xC00CU)
#define DNS_RDLENGTH_A (4U)
#define DNS_RETRY_DELAY_MS (200U)
#define DNS_QTYPE_QCLASS_SIZE (4U)
#define DNS_QDCOUNT_ONE (1U)
#define DNS_ANCOUNT_ZERO (0U)
#define DNS_ANCOUNT_ONE (1U)
#define SOCKET_REUSE_ADDR_ENABLE (1)

//------------------------------------------------------------------------------
// private typedefs
//------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------
// private variables
//------------------------------------------------------------------------------

static const char *TAG = "dns_hijack";

static TaskHandle_t s_task = NULL;
static int s_sock = -1;

/* IPv4 reply address in network byte order (big-endian) */
static uint32_t s_reply_ip_be = 0;

//------------------------------------------------------------------------------
// private functions (prototypes)
//------------------------------------------------------------------------------

/**
 * @brief Read a 16-bit big-endian value from a byte buffer.
 *
 * @param[in] p Pointer to two bytes in network byte order. Must not be NULL.
 * @return The 16-bit value in host byte order.
 */
static uint16_t rd16(const uint8_t *p);

/**
 * @brief Write a 16-bit value to a byte buffer in big-endian order.
 *
 * @param[out] p Destination buffer (at least 2 bytes). Must not be NULL.
 * @param[in]  v Value to write in host byte order.
 */
static void wr16(uint8_t *p, uint16_t v);

/**
 * @brief Build a DNS A response from an incoming DNS query.
 *
 * Copies the question section into the response buffer and appends
 * a single A record pointing to @c s_reply_ip_be for IN A queries.
 * Non-A or non-IN queries receive a NoError response with zero answers.
 *
 * @param[in]  request        Incoming DNS query buffer. Must not be NULL.
 * @param[in]  request_length Length of @p request in bytes.
 * @param[out] resp           Output buffer for the DNS response. Must not be NULL.
 * @param[in]  resp_max       Size of @p resp in bytes.
 * @return Length of the response in bytes, or -1 on error.
 */
static int build_dns_a_reply(const uint8_t *request, int request_length, uint8_t *resp,
                             int resp_max);

/**
 * @brief FreeRTOS task that receives DNS queries and dispatches responses.
 *
 * Runs in a loop calling recvfrom() on @c s_sock, builds replies via
 * build_dns_a_reply(), and sends them back to the originating client.
 * Malformed queries are silently discarded.
 *
 * @param[in] arg Unused task argument.
 */
static void dns_hijack_task(void *arg);

//------------------------------------------------------------------------------
// private functions (implementation)
//------------------------------------------------------------------------------

static uint16_t rd16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }

static void wr16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)((v >> 8) & 0xFF);
    p[1] = (uint8_t)(v & 0xFF);
}

static int build_dns_a_reply(const uint8_t *request, int request_length, uint8_t *resp,
                             int resp_max)
{
    if (request_length < (int)sizeof(dns_header_t))
    {
        return -1;
    }

    /* Ensure we have enough headroom for a minimal answer record */
    if (resp_max < request_length + DNS_MIN_REPLY_MARGIN)
    {
        return -1;
    }

    /* Copy request as base (header + question) */
    memcpy(resp, request, request_length);

    dns_header_t *h = (dns_header_t *)resp;
    const uint16_t qd = ntohs(h->qdcount);
    if (qd < DNS_QDCOUNT_ONE)
    {
        return -1;
    }

    /* Walk question section end (first question only) */
    int off = (int)sizeof(dns_header_t);

    /* QNAME: labels, terminated by 0 */
    while (off < request_length)
    {
        uint8_t lab_len = resp[off++];
        if (lab_len == 0)
        {
            break;
        }
        if (off + lab_len > request_length)
        {
            return -1;
        }
        off += lab_len;
    }

    /* Need QTYPE + QCLASS */
    if (off + DNS_QTYPE_QCLASS_SIZE > request_length)
    {
        return -1;
    }

    const uint16_t qtype = rd16(&resp[off + 0]);
    const uint16_t qclass = rd16(&resp[off + 2]);
    off += (int)DNS_QTYPE_QCLASS_SIZE;

    /* Response header:
     * 0x8180: QR=1, Opcode=0, AA=1? (not set here), TC=0, RD=1 (copied often), RA=0, RCODE=0
     * We'll just use a typical "NoError" response for captive DNS. */
    h->flags = htons(DNS_FLAGS_NOERROR);
    h->qdcount = htons(DNS_QDCOUNT_ONE);
    h->nscount = 0;
    h->arcount = 0;

    int resp_len = off; /* header+question */

    /* Only answer IN A queries; otherwise NoError with 0 answers */
    if (qtype != DNS_TYPE_A || qclass != DNS_CLASS_IN)
    {
        h->ancount = htons(DNS_ANCOUNT_ZERO);
        return resp_len;
    }

    /* Answer RR (16 bytes):
     * NAME: pointer to QNAME at 0x0C => 0xC00C
     * TYPE: A (1)
     * CLASS: IN (1)
     * TTL: 60
     * RDLEN: 4
     * RDATA: ipv4 */
    if (resp_len + DNS_ANSWER_RR_SIZE > resp_max)
    {
        return -1;
    }

    resp[resp_len + 0] = (uint8_t)((DNS_POINTER_QNAME >> 8U) & 0xFFU);
    resp[resp_len + 1] = (uint8_t)(DNS_POINTER_QNAME & 0xFFU);

    wr16(&resp[resp_len + 2], DNS_TYPE_A);   /* TYPE A */
    wr16(&resp[resp_len + 4], DNS_CLASS_IN); /* CLASS IN */

    /* TTL (32-bit) = 60 seconds */
    /* TTL upper 3 bytes are zero (TTL fits in 1 byte = max 255 s) */
    resp[resp_len + 6] = 0x00U;
    resp[resp_len + 7] = 0x00U;
    resp[resp_len + 8] = 0x00U;
    resp[resp_len + 9] = (uint8_t)(DNS_TTL_SECONDS & 0xFFU);

    wr16(&resp[resp_len + 10], DNS_RDLENGTH_A); /* RDLENGTH */

    memcpy(&resp[resp_len + 12], &s_reply_ip_be, DNS_RDLENGTH_A);
    resp_len += DNS_ANSWER_RR_SIZE;
    h->ancount = htons(DNS_ANCOUNT_ONE);

    return resp_len;
}

static void dns_hijack_task(void *arg)
{
    (void)arg;

    uint8_t rx[DNS_RX_BUF_SIZE];
    uint8_t tx[DNS_TX_BUF_SIZE];

    while (1)
    {
        struct sockaddr_in from = {0};
        socklen_t from_length = sizeof(from);

        int bytes_received =
            recvfrom(s_sock, rx, sizeof(rx), 0, (struct sockaddr *)&from, &from_length);
        if (bytes_received < 0)
        {
            int saved_errno = errno;
            if (saved_errno == EINTR)
            {
                continue;
            }

            ESP_LOGE(TAG, "recvfrom failed: errno=%d", saved_errno);
            vTaskDelay(pdMS_TO_TICKS(DNS_RETRY_DELAY_MS));
            continue;
        }

        int resp_length = build_dns_a_reply(rx, bytes_received, tx, sizeof(tx));
        if (resp_length <= 0)
        {
            /* ignore malformed/unsupported queries */
            continue;
        }

        (void)sendto(s_sock, tx, resp_length, 0, (struct sockaddr *)&from, from_length);
    }
}

//------------------------------------------------------------------------------
// public functions
//------------------------------------------------------------------------------

esp_err_t dns_hijack_start(uint32_t ipv4_addr_be)
{
    if (s_task != NULL)
    {
        return ESP_OK; /* already running */
    }

    s_reply_ip_be = ipv4_addr_be;

    s_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_sock < 0)
    {
        ESP_LOGE(TAG, "socket() failed (errno=%d)", errno);
        return ESP_FAIL;
    }

    /* Bind UDP :53 on all interfaces */
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(DNS_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int reuse_addr = SOCKET_REUSE_ADDR_ENABLE;
    (void)setsockopt(s_sock, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));

    if (bind(s_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        ESP_LOGE(TAG, "bind(:53) failed (errno=%d)", errno);
        close(s_sock);
        s_sock = -1;
        return ESP_FAIL;
    }

    BaseType_t task_result = xTaskCreate(dns_hijack_task, "dns_hijack", DNS_TASK_STACK_SIZE, NULL,
                                         DNS_TASK_PRIORITY, &s_task);
    if (task_result != pdPASS)
    {
        ESP_LOGE(TAG, "xTaskCreate failed");
        close(s_sock);
        s_sock = -1;
        s_task = NULL;
        return ESP_FAIL;
    }

    struct in_addr reply_addr = {.s_addr = s_reply_ip_be}; /* inet_ntoa expects network order */
    ESP_LOGI(TAG, "DNS hijack started on UDP :53, replying with %s", inet_ntoa(reply_addr));
    return ESP_OK;
}

esp_err_t dns_hijack_stop(void)
{
    if (s_task == NULL)
    {
        return ESP_OK;
    }

    TaskHandle_t task_to_delete = s_task;
    s_task = NULL;

    if (s_sock >= 0)
    {
        close(s_sock);
        s_sock = -1;
    }

    vTaskDelete(task_to_delete);
    ESP_LOGI(TAG, "DNS hijack stopped");
    return ESP_OK;
}
