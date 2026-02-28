/**
 * @file ui_assets.h
 * @brief Embedded UI assets served directly from flash memory.
 *
 * HTML pages are stored as null-terminated C string literals and linked
 * into the firmware image. No filesystem or SPIFFS is required.
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Main WiFi setup portal page (GET /). */
extern const char UI_INDEX_HTML[];

/** @brief Location management and weather comparison page (GET /locations). */
extern const char UI_LOCATIONS_HTML[];

#ifdef __cplusplus
} /* extern "C" */
#endif