#include <string.h>
#include <stdbool.h>
#include <stddef.h>

#include "locations_model.h"

bool locations_model_add(locations_model_t *model, const location_t *loc)
{
    bool return_value = false;

    if ((model == NULL) || (loc == NULL))
    {
        return_value = false;
    }
    else if (model->count >= LOCATIONS_MODEL_MAX_NUMBER)
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

bool locations_model_remove(locations_model_t *model, const char *name)
{
    bool return_value = false;

    size_t index = 0U;
    bool found = false;
    bool removed_was_active = false;

    if ((model == NULL) || (name == NULL) || (name[0] == '\0'))
    {
        return_value = false;
    }
    else
    {
        /* find index by name */
        for (size_t i = 0U; i < model->count; i++)
        {
            if (strcmp(model->items[i].name, name) == 0)
            {
                index = i;
                found = true;
                break;
            }
        }

        if (found == false)
        {
            return_value = false;
        }
        else
        {
            removed_was_active = model->items[index].is_active;

            /* shift left */
            for (size_t i = index; (i + 1U) < model->count; i++)
            {
                model->items[i] = model->items[i + 1U];
            }

            /* decrease count */
            model->count--;

            (void)memset(&model->items[model->count], 0, sizeof(model->items[model->count]));

            /* if the removed was active, enforce "no active" afterwards */
            if (removed_was_active == true)
            {
                for (size_t i = 0U; i < model->count; i++)
                {
                    model->items[i].is_active = false;
                }
            }

            return_value = true;
        }
    }

    return return_value;
}

const location_t *locations_model_get_active(const locations_model_t *model)
{
    const location_t *active = NULL;

    if (model != NULL)
    {
        for (size_t i = 0U; i < model->count; i++)
        {
            if (model->items[i].is_active == true)
            {
                active = &model->items[i];
                break;
            }
        }
    }

    return active;
}