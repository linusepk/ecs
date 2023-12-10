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
};

typedef struct archetype_graph_t archetype_graph_t;
struct archetype_graph_t {
    re_dyn_arr_t(archetype_t) archetypes;
    re_hash_map_t(type_t, archetype_t *) archetype_map;
};

extern void archetype_free(archetype_t *archetype);
extern void archetype_graph_free(archetype_graph_t *graph);
extern void archetype_graph_add(archetype_graph_t *graph, type_t type);

extern void archetype_graph_print(archetype_graph_t graph);
extern void archetype_graph_print_all(archetype_graph_t graph);
