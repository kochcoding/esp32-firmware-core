#pragma once

#include "sdkconfig.h"
#include <stdbool.h>

/*
 * Core configuration abstraction.
 * - For bool Kconfig options we create stable 0/1 macros via #ifdef.
 * - For int/string options we forward the CONFIG_* values directly.
 */

/* ---- bools (stable 0/1) ---- */
#ifdef CONFIG_CORE_CAPTIVE_PORTAL_ENABLE
#define CORE_CAPTIVE_PORTAL_ENABLED 1
#else
#define CORE_CAPTIVE_PORTAL_ENABLED 0
#endif

#ifdef CONFIG_CORE_BLE_SCAN_ENABLE
#define CORE_BLE_SCAN_ENABLED 1
#else
#define CORE_BLE_SCAN_ENABLED 0
#endif

#ifdef CONFIG_CORE_AP_OPEN
#define CORE_AP_OPEN_DEFAULT 1
#else
#define CORE_AP_OPEN_DEFAULT 0
#endif

#ifdef CONFIG_CORE_AP_DHCP_ENABLE
#define CORE_AP_DHCP_ENABLED 1
#else
#define CORE_AP_DHCP_ENABLED 0
#endif

#ifdef CONFIG_CORE_STATUS_API_ENABLE
#define CORE_STATUS_API_ENABLED 1
#else
#define CORE_STATUS_API_ENABLED 0
#endif

/* ---- values (always defined) ---- */
#define CORE_AP_SSID CONFIG_CORE_AP_SSID
#define CORE_LOG_LEVEL_DEFAULT CONFIG_CORE_LOG_LEVEL
#define CORE_BLE_SCAN_INTERVAL_MS CONFIG_CORE_BLE_SCAN_INTERVAL_MS
