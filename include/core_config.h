/**
 * @file core_config.h
 * @brief Stable compile-time configuration macros derived from Kconfig.
 *
 * @details
 *  Wraps CONFIG_* Kconfig symbols into stable, always-defined macros:
 *  - Boolean options are normalised to 0/1 via #ifdef guards.
 *  - Integer and string options are forwarded directly.
 *
 *  Consumers should use the CORE_* macros instead of CONFIG_* symbols
 *  directly to remain decoupled from the Kconfig naming scheme.
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "sdkconfig.h"
#include <stdbool.h>

/*
 * Core configuration abstraction.
 * - For bool Kconfig options we create stable 0/1 macros via #ifdef.
 * - For int/string options we forward the CONFIG_* values directly.
 */

/*------------------------------------------------------------------
 * Boolean options — always defined as 0 or 1
 *------------------------------------------------------------------*/
#ifdef CONFIG_CORE_CAPTIVE_PORTAL_ENABLE
#define CORE_CAPTIVE_PORTAL_ENABLED 1
#else
#define CORE_CAPTIVE_PORTAL_ENABLED 0
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

/*------------------------------------------------------------------
 * Value options — forwarded directly from Kconfig
 *------------------------------------------------------------------*/
#define CORE_AP_SSID CONFIG_CORE_AP_SSID

#ifdef __cplusplus
} /* extern "C" */
#endif