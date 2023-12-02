#include <rebound.h>

#define re_str_arg(STR) (i32_t) (STR).len, (STR).str

typedef u32_t component_id_t;
typedef u32_t archetype_id_t;
typedef u32_t entity_id_t;
typedef re_dyn_arr_t(component_id_t) type_t;
typedef void T;

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
    re_dyn_arr_t(re_dyn_arr_t(T)) components;
};

typedef struct entity_record_t entity_record_t;
struct entity_record_t {
    archetype_t *archetype;
    u32_t column;
};

typedef struct ecs_t ecs_t;
struct ecs_t {
    u32_t entity_index;

    re_dyn_arr_t(archetype_t) archetype_list;
    re_hash_map_t(type_t, archetype_t *) archetype_map;

    re_dyn_arr_t(component_t) component_list;
    re_hash_map_t(re_str_t, component_t *) component_map;

    re_hash_map_t(entity_id_t, entity_record_t) entity_map;
};

typedef struct entity_t entity_t;
struct entity_t {
    ecs_t *ecs;
    entity_id_t id;
};

static b8_t str_eq(const void *a, const void *b, u32_t size) {
    (void) size;
    const re_str_t *_a = a;
    const re_str_t *_b = b;
    return re_str_cmp(*_a, *_b) == 0;
}

static u64_t type_hash(const void *key, u64_t size) {
    (void) size;
    const type_t *_key = key;
    if (re_dyn_arr_count(*_key) == 0) {
        return 0;
    }
    return re_fvn1a_hash(*_key, re_dyn_arr_count(*_key) * sizeof(**_key));
}
static b8_t type_eq(const void *a, const void *b, u32_t size) {
    (void) size;
    const type_t *_a = a;
    const type_t *_b = b;
    if (re_dyn_arr_count(*_a) != re_dyn_arr_count(*_b)) {
        return false;
    }
    for (u32_t i = 0; i < re_dyn_arr_count(*_a); i++) {
        if ((*_a)[i] != (*_b)[i]) {
            return false;
        }
    }
    return true;
}

ecs_t *ecs_init(void) {
    ecs_t *ecs = re_malloc(sizeof(ecs_t));
    *ecs = (ecs_t) {0};

    re_hash_map_init(ecs->component_map, re_str_null, NULL, re_fvn1a_hash, str_eq);
    re_hash_map_init(ecs->archetype_map, NULL, NULL, type_hash, type_eq);

    // Null archetype
    re_dyn_arr_reserve(ecs->archetype_list, 1);
    archetype_t *archetype = &re_dyn_arr_last(ecs->archetype_list);
    archetype->id = re_dyn_arr_count(ecs->archetype_list) - 1;

    re_hash_map_set(ecs->archetype_map, archetype->type, archetype);

    return ecs;
}

static void archetype_free(archetype_t *archetype) {
    re_dyn_arr_free(archetype->type);
    re_hash_map_free(archetype->edges);
    for (u32_t i = 0; i < re_dyn_arr_count(archetype->components); i++) {
        re_dyn_arr_free(archetype->components[i]);
    }
    re_dyn_arr_free(archetype->components);
}

void ecs_free(ecs_t **ecs) {
    // Free archetypes.
    for (u32_t i = 0; i < re_dyn_arr_count((*ecs)->archetype_list); i++) {
        archetype_free(&(*ecs)->archetype_list[i]);
    }
    re_dyn_arr_free((*ecs)->archetype_list);
    re_hash_map_free((*ecs)->archetype_map);

    re_hash_map_free((*ecs)->component_map);
    re_hash_map_free((*ecs)->entity_map);

    re_free(*ecs);
    *ecs = NULL;
}

i32_t type_sort_compare(const void *a, const void *b) {
    const component_id_t *_a = a;
    const component_id_t *_b = b;
    return *_a - *_b;
}

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
#define ecs_register_component(ECS, COMPONENT, ...) _ecs_register_component_impl((ECS), re_str_lit(#COMPONENT), sizeof(COMPONENT))

entity_t ecs_entity(ecs_t *ecs) {
    entity_id_t id = ecs->entity_index++;

    entity_record_t record = {
        .archetype = &ecs->archetype_list[0],
        .column = 0
    };
    re_hash_map_set(ecs->entity_map, id, record);

    return (entity_t) {ecs, id};
}

static archetype_t *archetype_new(ecs_t *ecs, type_t type) {
    re_dyn_arr_reserve(ecs->archetype_list, 1);
    archetype_t *archetype = &re_dyn_arr_last(ecs->archetype_list);
    archetype->id = re_dyn_arr_count(ecs->archetype_list) - 1;
    archetype->type = type;

    for (u32_t i = 0; i < re_dyn_arr_count(type); i++) {
        component_t comp = ecs->component_list[i];
        re_dyn_arr_push(archetype->components, NULL);
        re_dyn_arr_new(archetype->components[i], comp.size);
    }

    re_hash_map_set(ecs->archetype_map, type, archetype);

    return archetype;
}

static void archetype_make_edges(ecs_t *ecs, archetype_t *archetype) {
    for (u32_t i = 0; i < re_dyn_arr_count(archetype->type); i++) {
        type_t previous_type = NULL;
        re_dyn_arr_push_arr(previous_type, archetype->type, re_dyn_arr_count(archetype->type));
        u32_t change = re_dyn_arr_remove(previous_type, i);

        archetype_t *previous = re_hash_map_get(ecs->archetype_map, previous_type);
        if (previous == NULL) {
            /* re_log_debug("Here"); */
            previous = archetype_new(ecs, previous_type);
            archetype_make_edges(ecs, previous);
        } else {
            re_dyn_arr_free(previous_type);
        }

        archetype_edge_t edge = {
            .add = archetype,
            .remove = previous,
        };
        re_hash_map_set(archetype->edges, change, edge);
        re_hash_map_set(previous->edges, change, edge);
    }
}

static entity_record_t move_entity(ecs_t *ecs, u32_t column, archetype_t *old, archetype_t *new) {
    u32_t new_column = re_dyn_arr_count(new->components[0]);

    for (u32_t i = 0; i < re_dyn_arr_count(new->type); i++) {
        re_dyn_arr_reserve(new->components[i], 1);
    }

    for (u32_t i = 0; i < re_dyn_arr_count(old->type); i++) {
        u32_t comp_id = old->type[i];
        u32_t comp_size = ecs->component_list[comp_id].size;

        // It's fine to do a linear search since it's such a small list of components.
        for (u32_t j = 0; j < re_dyn_arr_count(new->type); j++) {
            if (old->type[i] != new->type[j]) {
                continue;
            }

            void *old_comp_pos = old->components[i] + column * comp_size;
            void *new_comp_pos = new->components[j] + new_column * comp_size;
            memcpy(new_comp_pos, old_comp_pos, comp_size);
        }
    }

    entity_record_t record = {
        .archetype = new,
        .column = new_column
    };

    return record;
}

void _entity_add_component_impl(entity_t ent, re_str_t component, const void *data) {
    ecs_t *ecs = ent.ecs;
    entity_record_t record = re_hash_map_get(ecs->entity_map, ent.id);
    archetype_t *curr = record.archetype;
    component_t *comp = re_hash_map_get(ecs->component_map, component);

    archetype_t *new = re_hash_map_get(curr->edges, comp->id).add;
    if (new == NULL) {
        re_dyn_arr_t(component_id_t) new_type = NULL;
        re_dyn_arr_push_arr(new_type, curr->type, re_dyn_arr_count(curr->type));
        re_dyn_arr_push(new_type, comp->id);
        qsort(new_type, re_dyn_arr_count(new_type), sizeof(component_id_t), type_sort_compare);
        new = re_hash_map_get(ecs->archetype_map, new_type);

        if (new == NULL) {
            new = archetype_new(ecs, new_type);
        } else {
            re_dyn_arr_free(new_type);
        }
    }

    archetype_make_edges(ecs, new);

    entity_record_t new_record = move_entity(ecs, record.column, curr, new);
    for (u32_t i = 0; i < re_dyn_arr_count(new->type); i++) {
        if (new->type[i] != comp->id) {
            continue;
        }

        void *comp_pos = new->components[i] + new_record.column * comp->size;
        memcpy(comp_pos, data, comp->size);
        break;
    }

    re_hash_map_set(ecs->entity_map, ent.id, new_record);
}
#define entity_add_component(ENTITY, COMPONENT, ...) ({ \
        COMPONENT temp_comp = (COMPONENT) __VA_ARGS__; \
        _entity_add_component_impl((ENTITY), re_str_lit(#COMPONENT), &temp_comp); \
    })

const void *_entity_get_component_impl(entity_t ent, re_str_t component) {
    ecs_t *ecs = ent.ecs;
    if (!re_hash_map_has(ecs->component_map, component)) {
        re_log_error("Component '%.*s' not registered.", re_str_arg(component));
        return NULL;
    }
    entity_record_t record = re_hash_map_get(ecs->entity_map, ent.id);
    component_t *comp = re_hash_map_get(ecs->component_map, component);

    for (u32_t i = 0; i < re_dyn_arr_count(record.archetype->type); i++) {
        if (record.archetype->type[i] != comp->id) {
            continue;
        }

        return record.archetype->components[i] + record.column * comp->size;
    }

    return NULL;
}
#define entity_get_component(ENTITY, COMPONENT) _entity_get_component_impl((ENTITY), re_str_lit(#COMPONENT))

static void print_graph(ecs_t *ecs, archetype_t *base, u32_t spaces) {
    char buffer[512] = {0};
    for (u32_t i = 0; i < spaces; i++) {
        buffer[i] = ' ';
    }

    re_log_debug("%s%d", buffer, base->id);

    for (re_hash_map_iter_t iter = re_hash_map_iter_get(base->edges);
        re_hash_map_iter_valid(iter);
        iter = re_hash_map_iter_next(base->edges, iter)) {
        archetype_edge_t edge = re_hash_map_get_index_value(base->edges, iter);
        if (edge.add->id == base->id) {
            continue;
        }
        print_graph(ecs, edge.add, spaces + 2);
    }
}

i32_t main(void) {
    re_init();
    re_arena_t *arena = re_arena_create(GB(8));

    typedef re_vec2_t position_t;
    typedef re_vec2_t scale_t;
    typedef f32_t rotation_t;
    typedef re_vec2_t velocity_t;

    ecs_t *ecs = ecs_init();

    ecs_register_component(ecs, position_t);
    ecs_register_component(ecs, scale_t);
    ecs_register_component(ecs, rotation_t);
    ecs_register_component(ecs, velocity_t);

    entity_t pos_scale_rot = ecs_entity(ecs);
    entity_add_component(pos_scale_rot, position_t, {{1, 2}});
    entity_add_component(pos_scale_rot, scale_t, {{1, 2}});
    entity_add_component(pos_scale_rot, rotation_t, 22.5f);
    entity_add_component(pos_scale_rot, velocity_t, {{1, 2}});

    print_graph(ecs, &ecs->archetype_list[0], 0);

    ecs_free(&ecs);

    re_arena_destroy(&arena);
    re_terminate();
    return 0;
}
