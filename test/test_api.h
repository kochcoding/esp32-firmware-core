#pragma once

/* domain/locations_model */
void run_test_domain_locations_model_add(void);
void run_test_domain_locations_model_remove(void);
void run_test_domain_locations_model_get_active(void);
void run_test_domain_locations_model_invariants(void);

/* storage/locations_storage */
void run_test_storage_locations_storage_from_json(void);
void run_test_storage_locations_storage_to_json_and_measure_json(void);

/* storage/settings_storage */
void run_test_storage_settings_storage_wifi_from_json(void);
void run_test_storage_settings_storage_wifi_to_json_and_measure_json(void);

/* storage/weather_storage */
void run_test_storage_weather_storage_validate_json(void);
void run_test_storage_weather_storage_compact_json_and_measure_json(void);