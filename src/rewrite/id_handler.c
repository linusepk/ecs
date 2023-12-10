#include "core.h"

// ID layout:
//     32 bits - data
//     16 bits - generation
//     8 bits - nothing
//     8 bits - flags

void id_handler_free(id_handler_t *handler) {
    re_hash_map_free(handler->gen_map);
    re_dyn_arr_free(handler->free_ids);
    *handler = (id_handler_t) {0};
}

void id_handler_set_range(id_handler_t *handler, u32_t lower_bound, u32_t upper_bound) {
    if (lower_bound > upper_bound) {
        re_log_error("Lower bound can't be bigger than upper bound.");
        return;
    }

    handler->range_lower = lower_bound;
    handler->range_upper = upper_bound;
    handler->range_offest = 0;

    // Invalidate all id's within the bound.
    for (re_hash_map_iter_t iter = re_hash_map_iter_get(handler->gen_map);
        re_hash_map_iter_valid(iter);
        iter = re_hash_map_iter_next(handler->gen_map, iter)) {
        u32_t id = re_hash_map_get_index_key(handler->gen_map, iter);
        if (id >= lower_bound && id <= upper_bound) {
            u16_t *gen = re_hash_map_get_index_value_ptr(handler->gen_map, iter);
            (*gen)++;
        }
    }
}

static ecs_id_t id_compose(u32_t data, u16_t gen) {
    return (ecs_id_t) data | ((ecs_id_t) gen << 32);
}

ecs_id_t id_handler_new(id_handler_t *handler) {
    // Recycle id's.
    if (re_dyn_arr_count(handler->free_ids) != 0) {
        return re_dyn_arr_pop(handler->free_ids);
    }

    // Generate a new id.
    if (handler->range_upper != 0 && handler->range_lower + handler->range_offest > handler->range_upper) {
        re_log_warn("All id's within range has been used up.");
        return U64_MAX;
    }

    u32_t data = handler->range_lower + handler->range_offest;
    handler->range_offest++;
    re_hash_map_set(handler->gen_map, data, 0);

    return id_compose(data, 0);
}

void id_handler_dispose(id_handler_t *handler, ecs_id_t id) {
    if (!id_valid(handler, id)) {
        return;
    }

    u32_t data = id_get_data(id);

    // Id outside of bounds, discard.
    if (data < handler->range_lower || data > handler->range_upper) {
        re_hash_map_remove(handler->gen_map, data);
        return;
    }

    u16_t gen = id_get_gen(id);
    re_hash_map_set(handler->gen_map, data, gen + 1);

    // Push a new valid id for id recycling.
    ecs_id_t new_id = id_compose(data, gen + 1);
    re_dyn_arr_push(handler->free_ids, new_id);
}

b8_t id_valid(id_handler_t *handler, ecs_id_t id) {
    u32_t data = id_get_data(id);
    u32_t gen = id_get_gen(id);

    // Id hasn't been registered.
    if (!re_hash_map_has(handler->gen_map, data)) {
        return false;
    }

    // Don't do a bounds check because id's aquired before
    // range has been set are still valid until disposal.

    return re_hash_map_get(handler->gen_map, data) == gen;
}

u32_t id_get_data(ecs_id_t id) {
    return (u32_t) id;
}

u16_t id_get_gen(ecs_id_t id) {
    return (u16_t) (id >> 32);
}
