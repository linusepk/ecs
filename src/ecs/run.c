#include "ecs/ecs_internal.h"

re_str_t archetype_name(archetype_t archetype) {
    char buffer[512] = {0};
    i32_t offset = 0;
    for (u32_t i = 0; i < re_dyn_arr_count(archetype.type); i++) {
        switch (archetype.type[i]) {
            case 0:
                sprintf(buffer + offset, "position_t");
                offset += 10;
                break;
            case 1:
                sprintf(buffer + offset, "velocity_t");
                offset += 10;
                break;
            case 2:
                sprintf(buffer + offset, "scale_t");
                offset += 7;
                break;
            default:
                break;
        }
    }

    u8_t *str = re_malloc(offset);
    memcpy(str, buffer, offset);
    return re_str(str, offset);
}

static void ecs_run_archetype(ecs_t *ecs, archetype_t *archetype, system_info_t info) {
    for (re_hash_map_iter_t hm_iter = re_hash_map_iter_get(archetype->edges);
        re_hash_map_iter_valid(hm_iter);
        hm_iter = re_hash_map_iter_next(archetype->edges, hm_iter)) {
        archetype_edge_t edge = re_hash_map_get_index_value(archetype->edges, hm_iter);
        if (edge.add->id == archetype->id) {
            continue;
        }
        re_str_t base_name = archetype_name(*archetype);
        re_str_t edge_name = archetype_name(*edge.add);
        re_log_debug("'%.*s'(%d) running '%.*s'(%d)", (i32_t) base_name.len, base_name.str, archetype->id, (i32_t) edge_name.len, edge_name.str, edge.add->id);
        ecs_run_archetype(ecs, edge.add, info);
    }

    // Archetype has already been run this run call.
    if (archetype->last_run_touch >= ecs->run_frame) {
        return;
    }

    archetype->last_run_touch = ecs->run_frame;

    ecs_iter_t iter = {
        .ecs = ecs,
        .components = NULL,
        .count = re_dyn_arr_count(archetype->entities)
    };

    if (iter.count == 0) {
        return;
    }

    for (u32_t i = 0; i < re_dyn_arr_count(info.unordered_type); i++) {
        u32_t index = re_hash_map_get(archetype->component_map, info.unordered_type[i]);
        void *comp_array = archetype->components[index];
        re_hash_map_set(iter.components, i, comp_array);
    }

    re_log_debug("Archetype id: %u", archetype->id);
    info.func(iter);
}

void ecs_run(ecs_t *ecs, system_group_t group) {
    if (group >= re_dyn_arr_count(ecs->system_groups)) {
        re_log_error("Running an invalid system group.");
    }
    ecs->run_frame++;
    for (u32_t i = 0; i < re_dyn_arr_count(ecs->system_groups[group]); i++) {
        system_info_t info = ecs->system_groups[group][i];

        if (!re_hash_map_has(ecs->archetype_map, info.component_type)) {
            return;
        }

        archetype_t *base = re_hash_map_get(ecs->archetype_map, info.component_type);
        ecs_run_archetype(ecs, base, info);
    }
}

