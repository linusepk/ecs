#include "ecs/ecs_internal.h"

void _ecs_register_component_impl(ecs_t *ecs, re_str_t name, u64_t size) {
    if (re_hash_map_has(ecs->component_map, name)) {
        re_log_error("Component '%.*s' already registered.", (i32_t) name.len, name.str);
        return;
    }

    u32_t id = re_dyn_arr_count(ecs->component_list);
    component_t comp = {
        .id = id,
        .size = size
    };
    re_dyn_arr_push(ecs->component_list, comp);
    re_hash_map_set(ecs->component_map, name, &re_dyn_arr_last(ecs->component_list));
}

void _ecs_register_system_impl(ecs_t *ecs, system_group_t group, system_t system, re_str_t components) {
    type_t unordered_type = NULL;
    for (u32_t i = 0; i < components.len; i++) {
        // Skip whitespace.
        while (components.str[i] == ' ' || components.str[i] == '\t' || components.str[i] == '\n') {
            i++;
        }

        u32_t start = i;
        while (components.str[i] != ',' && i < components.len) {
            i++;
        }
        u32_t end = i - 1;
        re_str_t comp_name = re_str_sub(components, start, end);

        if (!re_hash_map_has(ecs->component_map, comp_name)) {
            re_log_error("Component '%.*s' not registered.", (i32_t) comp_name.len, comp_name.str);
            return;
        }

        re_dyn_arr_push(unordered_type, re_hash_map_get(ecs->component_map, comp_name)->id);
    }

    type_t ordererd_type = NULL;
    re_dyn_arr_push_arr(ordererd_type, unordered_type, re_dyn_arr_count(unordered_type));
    qsort(ordererd_type, re_dyn_arr_count(ordererd_type), sizeof(component_id_t), type_sort_compare);

    system_info_t info = {
        .func = system,
        .component_type = ordererd_type,
        .unordered_type = unordered_type
    };

    re_dyn_arr_push(ecs->system_groups[group], info);
}

system_group_t ecs_system_group(ecs_t *ecs) {
    re_dyn_arr_push(ecs->system_groups, NULL);
    return re_dyn_arr_count(ecs->system_groups) - 1;
}

void *ecs_iter(ecs_iter_t iter, u32_t index) {
    return re_hash_map_get(iter.components, index);
}
