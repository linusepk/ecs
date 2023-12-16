#pragma once

#include <rebound.h>

typedef u64_t ecs_id_t;

/*=========================*/
// ID handler
/*=========================*/

typedef struct id_handler_t id_handler_t;
struct id_handler_t {
    // Map id's to a generation for livliness tracking.
    re_hash_map_t(u32_t, u16_t) gen_map;
    // List of disposed id's for recycling.
    re_dyn_arr_t(ecs_id_t) free_ids;

    u32_t range_lower;
    u32_t range_upper;
    u32_t range_offest;
};

// Free resources used by 'id handler'.
extern void id_handler_free(id_handler_t *handler);
// Changing the range will reset the range offset and invalidate all existing ids in that range.
extern void id_handler_set_range(id_handler_t *handler, u32_t lower_bound, u32_t upper_bound);
// Get a new id.
extern ecs_id_t id_handler_new(id_handler_t *handler);
// Dispose of an id
extern void id_handler_dispose(id_handler_t *handler, ecs_id_t id);
// Check fo id livliness.
extern b8_t id_valid(id_handler_t *handler, ecs_id_t id);
// Read the data part of the id.
extern u32_t id_get_data(ecs_id_t id);
// Read the generation part of the id.
extern u16_t id_get_gen(ecs_id_t id);

/*=========================*/
// Type
/*=========================*/

// A sorted list of id's
// Declare: type_t type = NULL;
typedef re_dyn_arr_t(ecs_id_t) type_t;

extern void type_free(type_t *type);
extern void type_add(type_t *type, ecs_id_t id);
extern void type_remove(type_t *type, ecs_id_t id);
extern b8_t type_has(const type_t type, ecs_id_t id);
extern b8_t type_is_subtype(const type_t base, const type_t sub);
extern b8_t type_eq(const type_t a, const type_t b);
extern type_t type_copy(const type_t type);

/*=========================*/
// Archetype
/*=========================*/

typedef struct archetype_t archetype_t;

typedef struct archetype_edge_t archetype_edge_t;
struct archetype_edge_t {
    archetype_t *add;
    archetype_t *remove;
};

struct archetype_t {
    u32_t index;

    type_t type;
    re_hash_map_t(ecs_id_t, archetype_edge_t) edge_map;
    re_dyn_arr_t(ecs_id_t) ids;
    re_dyn_arr_t(re_dyn_arr_t(void)) storage;
};

typedef struct archetype_record_t archetype_record_t;
struct archetype_record_t {
    ecs_id_t id;
    archetype_t *archetype;
    u32_t column;
};

typedef struct archetype_graph_t archetype_graph_t;
struct archetype_graph_t {
    re_dyn_arr_t(archetype_t) archetypes;
    re_hash_map_t(type_t, archetype_t *) archetype_map;
    re_hash_map_t(ecs_id_t, archetype_record_t) id_map;
    re_hash_map_t(ecs_id_t, u64_t) storage_map;
};

extern archetype_graph_t archetype_graph_init(void);
extern void archetype_graph_free(archetype_graph_t *graph);
extern void archetype_free(archetype_t *archetype);

extern void archetype_graph_record_add(archetype_graph_t *graph, archetype_record_t record, ecs_id_t id);
extern void archetype_graph_record_remove(archetype_graph_t *graph, archetype_record_t record, ecs_id_t id);
extern void archetype_add_storage_id(archetype_graph_t *graph, ecs_id_t id, u64_t size);
extern void *archetype_get_storage_id(archetype_graph_t graph, archetype_record_t record, ecs_id_t id);

extern archetype_t *archetype_graph_get(archetype_graph_t *graph, type_t type);
extern archetype_record_t archetype_graph_get_id(archetype_graph_t *graph, ecs_id_t id);

extern void archetype_graph_print_all(archetype_graph_t graph);

/*=========================*/
// ECS
/*=========================*/

typedef ecs_id_t ecs_entity_t;

typedef struct component_t component_t;
struct component_t {
    ecs_id_t id;
    u64_t size;
};

typedef struct ecs_t ecs_t;
struct ecs_t {
    id_handler_t id_handler;
    re_hash_map_t(ecs_id_t, re_str_t) id_name_map;
    re_hash_map_t(re_str_t, component_t) component_map;
    archetype_graph_t archetype_graph;
};

extern ecs_t *ecs_init(void);
extern void ecs_free(ecs_t *ecs);

extern ecs_entity_t ecs_entity_new(ecs_t *ecs);
extern void ecs_entity_destroy(ecs_t *ecs, ecs_entity_t entity);
extern b8_t ecs_entity_alive(ecs_t *ecs, ecs_entity_t entity);
extern void ecs_entity_name_set(ecs_t *ecs, ecs_entity_t entity, re_str_t name);
extern re_str_t ecs_entity_name_get(ecs_t *ecs, ecs_entity_t entity);
extern void ecs_entity_add(ecs_t *ecs, ecs_entity_t entity, ecs_entity_t id);
extern void ecs_entity_remove(ecs_t *ecs, ecs_entity_t entity, ecs_entity_t id);
extern void ecs_entity_storage(ecs_t *ecs, ecs_entity_t entity, u64_t size);
extern void *ecs_entity_storage_get(ecs_t *ecs, ecs_entity_t entity, ecs_id_t id);

#define ecs_register_component(ECS, T) _ecs_register_component_impl((ECS), sizeof(T), re_str_lit(#T))

extern void _ecs_register_component_impl(ecs_t *ecs, u64_t size, re_str_t name);



extern void archetype_graph_print(ecs_t *ecs, archetype_graph_t graph);
