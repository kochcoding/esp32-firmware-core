#pragma once

#include "esp_err.h"

esp_err_t wifi_init_ap(void);
uint16_t wifi_ap_get_client_count(void);
