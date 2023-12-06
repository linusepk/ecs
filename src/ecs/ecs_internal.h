#pragma once

#include "ecs.h"

typedef u32_t component_id_t;
typedef u32_t archetype_id_t;

typedef re_dyn_arr_t(component_id_t) type_t;

typedef struct system_info_t system_info_t;
struct system_info_t {
    system_t func;
    type_t component_type;
    type_t unordered_type;
};

typedef struct component_t component_t;
struct component_t {
    u64_t size;
    component_id_t id;
};

typedef struct archetype_t archetype_t;

typedef struct archetype_edge_t archetype_edge_t;
struct archetype_edge_t {
    archetype_t *add;
    archetype_t *remove;
};

struct archetype_t {
    archetype_id_t id;
    type_t type;
    re_hash_map_t(component_id_t, archetype_edge_t) edges;
    re_dyn_arr_t(re_dyn_arr_t(void)) components;
    re_hash_map_t(component_id_t, u32_t) component_map;
    re_dyn_arr_t(entity_id_t) entities;
    u32_t last_run_touch;
};

typedef struct entity_record_t entity_record_t;
struct entity_record_t {
    archetype_t *archetype;
    u32_t column;
};

struct ecs_t {
    u32_t entity_index;
    re_dyn_arr_t(entity_id_t) free_entity_ids;
    re_hash_map_t(entity_id_t, entity_record_t) entity_map;
    re_hash_map_t(entity_id_t, u32_t) generation_map;

    re_dyn_arr_t(archetype_t) archetype_list;
    re_hash_map_t(type_t, archetype_t *) archetype_map;

    re_dyn_arr_t(component_t) component_list;
    re_hash_map_t(re_str_t, component_t *) component_map;

    re_dyn_arr_t(re_dyn_arr_t(system_info_t)) system_groups;

    u32_t run_frame;
};

static inline i32_t type_sort_compare(const void *a, const void *b) {
    const component_id_t *_a = a;
    const component_id_t *_b = b;
    return *_a - *_b;
}
