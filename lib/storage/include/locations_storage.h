#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "locations_model.h"

bool locations_storage_from_json(const char *json, locations_model_t *out_model);
bool locations_storage_to_json(const locations_model_t *model, char *out_json, size_t out_len);
size_t locations_storage_measure_json(const locations_model_t *model);