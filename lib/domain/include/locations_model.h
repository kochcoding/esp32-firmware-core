#pragma once

#include <stdbool.h>
#include <stddef.h>

#define LOCATIONS_MODEL_MAX_NUMBER 8

typedef struct
{
    char name[32];
    double latitude;
    double longitude;
    bool is_active;
} location_t;

typedef struct
{
    location_t items[LOCATIONS_MODEL_MAX_NUMBER];
    size_t count;
} locations_model_t;

bool locations_model_add(locations_model_t *model, const location_t *loc);
bool locations_model_remove(locations_model_t *model, const char *name);
const location_t *locations_model_get_active(const locations_model_t *model);
