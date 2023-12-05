#include "ecs/ecs_internal.h"

static void ecs_run_archetype(ecs_t *ecs, archetype_t *archetype, system_info_t info) {
    ecs_iter_t iter = {
        .ecs = ecs,
        .components = NULL,
        .count = re_dyn_arr_count(archetype->components[0])
    };
    for (u32_t i = 0; i < re_dyn_arr_count(info.unordered_type); i++) {
        u32_t index = re_hash_map_get(archetype->component_map, info.unordered_type[i]);
        void *comp_array = archetype->components[index];
        re_hash_map_set(iter.components, i, comp_array);
    }

    info.func(iter);

    for (re_hash_map_iter_t iter = re_hash_map_iter_get(archetype->edges);
        re_hash_map_iter_valid(iter);
        iter = re_hash_map_iter_next(archetype->edges, iter)) {
        archetype_edge_t edge = re_hash_map_get_index_value(archetype->edges, iter);
        if (edge.add->id == archetype->id) {
            continue;
        }
        ecs_run_archetype(ecs, edge.add, info);
    }
}

void ecs_run(ecs_t *ecs, system_group_t group) {
    if (group >= re_dyn_arr_count(ecs->system_groups)) {
        re_log_error("Running an invalid system group.");
    }
    for (u32_t i = 0; i < re_dyn_arr_count(ecs->system_groups[group]); i++) {
        system_info_t info = ecs->system_groups[group][i];

        if (!re_hash_map_has(ecs->archetype_map, info.component_type)) {
            return;
        }

        archetype_t *base = re_hash_map_get(ecs->archetype_map, info.component_type);
        ecs_run_archetype(ecs, base, info);
    }
}

