#include <string.h>

#include "locations_model.h"

bool locations_model_add(locations_model_t *model, const location_t *loc)
{
    bool return_value = false;

    if ((model == NULL) || (loc == NULL))
    {
        return_value = false;
    }
    else if (model->count >= LOCATIONS_MAX)
    {
        return_value = false;
    }
    else
    {
        return_value = true;

        for (size_t i = 0U; i < model->count; i++)
        {
            if (strcmp(model->items[i].name, loc->name) == 0)
            {
                return_value = false;
                break;
            }
        }

        if (return_value == true)
        {
            model->items[model->count] = *loc;
            model->count++;
        }
    }

    return return_value;
}