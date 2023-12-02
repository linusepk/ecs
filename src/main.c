#include <rebound.h>

#define re_str_arg(STR) (i32_t) (STR).len, (STR).str

typedef u32_t component_id_t;
typedef u32_t archetype_id_t;
typedef u32_t entity_id_t;
typedef re_dyn_arr_t(component_id_t) type_t;
typedef u32_t system_group_t;

typedef struct ecs_iter_t ecs_iter_t;

typedef void (*system_t)(ecs_iter_t iter);

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
};

typedef struct entity_record_t entity_record_t;
struct entity_record_t {
    archetype_t *archetype;
    u32_t column;
};

typedef struct system_info_t system_info_t;
struct system_info_t {
    system_t func;
    type_t component_type;
    type_t unordered_type;
};

typedef struct ecs_t ecs_t;
struct ecs_t {
    u32_t entity_index;

    re_dyn_arr_t(archetype_t) archetype_list;
    re_hash_map_t(type_t, archetype_t *) archetype_map;

    re_dyn_arr_t(component_t) component_list;
    re_hash_map_t(re_str_t, component_t *) component_map;

    re_hash_map_t(entity_id_t, entity_record_t) entity_map;

    re_dyn_arr_t(re_dyn_arr_t(system_info_t)) system_groups;
};

struct ecs_iter_t {
    const ecs_t *ecs;
    re_hash_map_t(u32_t, re_dyn_arr_t(void)) components;
    const u32_t count;
};

typedef struct entity_t entity_t;
struct entity_t {
    ecs_t *ecs;
    entity_id_t id;
};

static u64_t str_hash(const void *key, u64_t size) {
    (void) size;
    const re_str_t *_key = key;
    return re_fvn1a_hash(_key->str, _key->len);
}
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

    re_hash_map_init(ecs->component_map, re_str_null, NULL, str_hash, str_eq);
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

static i32_t type_sort_compare(const void *a, const void *b) {
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
#define ecs_register_component(ECS, COMPONENT) _ecs_register_component_impl((ECS), re_str_lit(#COMPONENT), sizeof(COMPONENT))

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
            re_log_error("Component '%.*s' not registered.", re_str_arg(comp_name));
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
#define ecs_register_system(ECS, GROUP, SYSTEM, COMPONENTS...) _ecs_register_system_impl((ECS), (GROUP), (SYSTEM), re_str_lit(#COMPONENTS));

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
        /* re_dyn_arr_push(iter.components, comp_array); */
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
            re_log_debug("Can't find archetype.");
            for (u32_t j = 0; j < re_dyn_arr_count(info.component_type); j++) {
                re_log_debug("%d", info.component_type[j]);
            }

            return;
        }

        archetype_t *base = re_hash_map_get(ecs->archetype_map, info.component_type);
        ecs_run_archetype(ecs, base, info);
    }
}

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
        re_hash_map_set(archetype->component_map, type[i], i);
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
        u32_t new_row = re_hash_map_get(new->component_map, old->type[i]);
        void *old_comp_pos = old->components[i] + column * comp_size;
        void *new_comp_pos = new->components[new_row] + new_column * comp_size;
        memcpy(new_comp_pos, old_comp_pos, comp_size);
    }

    entity_record_t record = {
        .archetype = new,
        .column = new_column
    };

    return record;
}

void _entity_add_component_impl(entity_t ent, re_str_t component, const void *data) {
    ecs_t *ecs = ent.ecs;
    if (!re_hash_map_has(ecs->component_map, component)) {
        re_log_error("Component '%.*s' not registered.", re_str_arg(component));
        return;
    }
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
    u32_t index = re_hash_map_get(new->component_map, comp->id);
    void *comp_pos = new->components[index] + new_record.column * comp->size;
    memcpy(comp_pos, data, comp->size);

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

    if (!re_hash_map_has(record.archetype->component_map, comp->id)) {
        return NULL;
    }

    u32_t index = re_hash_map_get(record.archetype->component_map, comp->id);
    return record.archetype->components[index] + record.column * comp->size;
}
#define entity_get_component(ENTITY, COMPONENT) _entity_get_component_impl((ENTITY), re_str_lit(#COMPONENT))

system_group_t ecs_system_group(ecs_t *ecs) {
    re_dyn_arr_push(ecs->system_groups, NULL);
    return re_dyn_arr_count(ecs->system_groups) - 1;
}

void *ecs_iter(ecs_iter_t iter, u32_t index) {
    return re_hash_map_get(iter.components, index);
}

/*=========================*/
// User application
/*=========================*/

typedef re_vec2_t position_t;
typedef re_vec2_t velocity_t;
typedef re_vec2_t scale_t;

void move_system(ecs_iter_t iter) {
    // Get them in the order they were requested
    // when regestering the system.
    velocity_t *vel = ecs_iter(iter, 0);
    position_t *pos = ecs_iter(iter, 1);

    for (u32_t i = 0; i < iter.count; i++) {
        pos[i] = re_vec2_add(pos[i], vel[i]);
    }
}

void print_system(ecs_iter_t iter) {
    position_t *pos = ecs_iter(iter, 0);
    for (u32_t i = 0; i < iter.count; i++) {
        re_log_debug("%d: %f, %f", i, pos[i].x, pos[i].y);
    }
}

i32_t main(void) {
    re_init();
    re_arena_t *arena = re_arena_create(GB(8));

    // TODO: Add slot map so we don't keep dead entities in archetype array.
    // TODO: Edge case where system doesn't take any components.
    ecs_t *ecs = ecs_init();

    ecs_register_component(ecs, position_t);
    ecs_register_component(ecs, velocity_t);
    ecs_register_component(ecs, scale_t);

    system_group_t update_group = ecs_system_group(ecs);
    ecs_register_system(ecs, update_group, move_system, velocity_t, position_t);

    system_group_t other = ecs_system_group(ecs);
    ecs_register_system(ecs, other, print_system, position_t);

    f32_t entity_create_time = re_os_get_time();
    for (u32_t i = 0; i < 8; i++) {
        entity_t bob = ecs_entity(ecs);
        entity_add_component(bob, position_t, {{0, 0}});
        entity_add_component(bob, velocity_t, {{i, i * 2}});
    }
    entity_create_time = re_os_get_time() - entity_create_time;

    f32_t run_time = re_os_get_time();
    ecs_run(ecs, update_group);
    run_time = re_os_get_time() - run_time;

    ecs_run(ecs, other);

    re_log_debug("Entity create time: %f ms", entity_create_time * 1000.0f);
    re_log_debug("Run time: %f ms", run_time * 1000.0f);

    ecs_free(&ecs);

    re_arena_destroy(&arena);
    re_terminate();
    return 0;
}
