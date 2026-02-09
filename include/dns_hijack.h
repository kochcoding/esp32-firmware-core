// dns_hijack.h  (nur die relevanten Zeilen)
#pragma once
#include <stdint.h>
#include "esp_err.h"

esp_err_t dns_hijack_start(uint32_t ipv4_addr_be);
esp_err_t dns_hijack_stop(void);
