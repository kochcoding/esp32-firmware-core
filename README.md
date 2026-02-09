# ESP32 Firmware Core

Minimal, reusable firmware core for ESP32-based projects.

This repository contains a working MVP (~80% functional) that serves as a clean baseline for further development, refactoring, and feature expansion.

---

## Project Status

- MVP / Early stage
- Core functionality working
- Ongoing refactoring and cleanup
- Not production-ready yet

This project is intentionally published early to demonstrate architecture, structure, and development approach.

---

## Motivation

The goal of this project is to build a clean, modular firmware foundation for ESP32 projects that can be reused across multiple applications.

Instead of starting from scratch for each project, this repository aims to provide:
- a solid baseline
- clear separation of concerns
- extensibility for future features

---

## Features

This section will be extended over time.

* WiFi Provisioning (AP + STA)
    * SoftAP for initial setup
    * Parallel APSTA operation
    * Persistent WiFi credentials (NVS)
* Cross-Platform Captive Portal
    * DNS hijacking (UDP/53)
    * OS-specific probe handling
    * Automatic redirect to portal UI
* Embedded HTTP Server
    * ESP-IDF esp_http_server
    * JSON + form data handling
    * Centralized request helpers
* REST API
    * Device status endpoints
    * WiFi configuration endpoints
    * System control (reboot)
* Persistent Storage (NVS)
    * Key-value based persistence
    * Locations and settings stored
    * Safe read/write handling
* Location Management
    * Add / list / delete locations
    * Duplicate prevention & limits
    * Persistent active location
* Public Weather Integration
    * Open-Meteo REST API
    * Current weather & forecast
    * Timezone-aware data
* HTTPS / TLS Networking
    * esp_http_client over HTTPS
    * ESP-IDF certificate bundle
    * No insecure TLS modes
* JSON Processing
    * Parsing with cJSON
    * Explicit validation & errors
    * Robust against partial data
* System Architecture
    * Plain C (ESP-IDF native)
    * Modular, service-oriented design
    * Refactor & tests planned
* Configuration Model
    * Build-time via Kconfig
    * Runtime via REST API
    * No .env or hardcoded secrets

---

## Repository Structure

```
esp32-firmware-core/
├─ src/
├─ include/
├─ components/
├─ CMakeLists.txt
├─ README.md
└─ ...
```

(The structure may change during refactoring.)

---

## Usage

This repository is intended as:
- a reference implementation
- a starting point for ESP32 projects
- a technical portfolio project

Build, flash, and usage instructions will be added once the core structure is stabilized.

---

## License

This project is licensed under the MIT License.

You are free to use, modify, distribute, and use this code in commercial projects.
See the LICENSE file for details.

---

## Portfolio and Commercial Use Notice

This repository is published as part of my personal developer portfolio.

If you are interested in:
- using this code in a commercial product
- commissioning custom firmware development
- extending this project for your specific use case

feel free to get in touch.

---

## Disclaimer

This project is provided as-is, without warranty of any kind.

It is a work in progress and not intended for production use in its current state.
