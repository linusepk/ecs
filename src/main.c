#include <rebound.h>

#define re_str_arg(STR) (i32_t) (STR).len, (STR).str

typedef u32_t component_id_t;
typedef u32_t archetype_id_t;
typedef u32_t entity_id_t;
typedef re_dyn_arr_t(component_id_t) type_t;

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
    re_str_t name;
};

typedef struct ecs_t ecs_t;
struct ecs_t {
    re_arena_t *arena;

    u32_t entity_index;
    u32_t component_index;
    re_hash_map_t(re_str_t, component_t) component_map;

    re_dyn_arr_t(archetype_t) archetype_list;
    re_hash_map_t(type_t, archetype_t *) archetype_map;

    re_hash_map_t(entity_id_t, archetype_t *) entity_map;
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

ecs_t *ecs_init(re_arena_t *arena) {
    ecs_t *ecs = re_arena_push_zero(arena, sizeof(ecs_t));

    ecs->arena = arena;
    re_hash_map_init(ecs->component_map, re_str_null, ((component_t) {.id = U32_MAX}), re_fvn1a_hash, str_eq);
    re_hash_map_init(ecs->archetype_map, NULL, NULL, type_hash, type_eq);

    // Null archetype
    re_dyn_arr_reserve(ecs->archetype_list, 1);
    archetype_t *archetype = &re_dyn_arr_last(ecs->archetype_list);
    archetype->id = re_dyn_arr_count(ecs->archetype_list) - 1;

    re_hash_map_set(ecs->archetype_map, archetype->type, archetype);

    return ecs;
}

void ecs_free(ecs_t **ecs) {
    re_hash_map_free((*ecs)->component_map);
    re_dyn_arr_free((*ecs)->archetype_list);
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

    u32_t id = ecs->component_index++;
    re_hash_map_set(ecs->component_map, name, ((component_t) {.size = size, .id = id}));
}
#define ecs_register_component(ECS, COMPONENT, ...) _ecs_register_component_impl((ECS), re_str_lit(#COMPONENT), sizeof(COMPONENT))

entity_t ecs_entity(ecs_t *ecs) {
    entity_id_t id = ecs->entity_index++;

    re_hash_map_set(ecs->entity_map, id, &ecs->archetype_list[0]);

    return (entity_t) {ecs, id};
}

void _entity_add_component_impl(entity_t ent, re_str_t component) {
    ecs_t *ecs = ent.ecs;
    archetype_t *curr = re_hash_map_get(ecs->entity_map, ent.id);
    component_id_t component_id = re_hash_map_get(ecs->component_map, component).id;

    archetype_t *new = re_hash_map_get(curr->edges, component_id).add;
    if (new == NULL) {
        re_dyn_arr_t(component_id_t) new_type = NULL;
        re_dyn_arr_push_arr(new_type, curr->type, re_dyn_arr_count(curr->type));
        re_dyn_arr_push(new_type, component_id);
        qsort(new_type, re_dyn_arr_count(new_type), sizeof(component_id_t), type_sort_compare);
        new = re_hash_map_get(ecs->archetype_map, new_type);

        if (new == NULL) {
            re_dyn_arr_reserve(ecs->archetype_list, 1);
            new = &re_dyn_arr_last(ecs->archetype_list);
            new->id = re_dyn_arr_count(ecs->archetype_list) - 1;
            new->type = new_type;

            u32_t name_len = curr->name.len + component.len;
            u8_t *name = re_arena_push(ecs->arena, name_len);
            for (u32_t i = 0; i < curr->name.len; i++) {
                name[i] = curr->name.str[i];
            }
            for (u32_t i = 0; i < component.len; i++) {
                name[curr->name.len + i] = component.str[i];
            }
            new->name = re_str(name, name_len);
        } else {
            re_dyn_arr_free(new_type);
        }

        archetype_edge_t edge = {
            .add = new,
            .remove = curr
        };
        re_hash_map_set(curr->edges, component_id, edge);
        re_hash_map_set(new->edges, component_id, edge);
    }

    re_hash_map_set(ecs->entity_map, ent.id, new);
}
#define entity_add_component(ENTITY, COMPONENT, ...) _entity_add_component_impl((ENTITY), re_str_lit(#COMPONENT)); (void) sizeof(COMPONENT)

void print_graph(archetype_t *base, u32_t spaces) {
    char buffer[512] = {0};
    for (u32_t i = 0; i < spaces; i++) {
        buffer[i] = ' ';
    }

    re_log_debug("%s%d: %.*s", buffer, base->id, (i32_t) base->name.len, base->name.str);
    for (re_hash_map_iter_t iter = re_hash_map_iter_get(base->edges);
        re_hash_map_iter_valid(iter);
        iter = re_hash_map_iter_next(base->edges, iter)) {
        archetype_edge_t edge = re_hash_map_get_index_value(base->edges, iter);
        if (edge.add->id == base->id) {
            continue;
        }
        print_graph(edge.add, spaces + 2);
    }
}

i32_t main(void) {
    re_init();
    re_arena_t *arena = re_arena_create(GB(8));

    typedef re_vec2_t position_t;
    typedef re_vec2_t scale_t;
    typedef f32_t rotation_t;
    typedef re_vec2_t velocity_t;

    ecs_t *ecs = ecs_init(arena);

    ecs_register_component(ecs, position_t);
    ecs_register_component(ecs, scale_t);
    ecs_register_component(ecs, rotation_t);
    ecs_register_component(ecs, velocity_t);

    entity_t pos = ecs_entity(ecs);
    entity_add_component(pos, position_t);

    entity_t scale = ecs_entity(ecs);
    entity_add_component(scale, scale_t);

    entity_t rot = ecs_entity(ecs);
    entity_add_component(rot, rotation_t);

    entity_t pos_scale = ecs_entity(ecs);
    entity_add_component(pos_scale, position_t);
    entity_add_component(pos_scale, scale_t);

    entity_t pos_rot = ecs_entity(ecs);
    entity_add_component(pos_rot, position_t);
    entity_add_component(pos_rot, rotation_t);

    entity_t scale_rot = ecs_entity(ecs);
    entity_add_component(scale_rot, scale_t);
    entity_add_component(scale_rot, rotation_t);

    entity_t pos_scale_rot = ecs_entity(ecs);
    entity_add_component(pos_scale_rot, position_t);
    entity_add_component(pos_scale_rot, scale_t);
    entity_add_component(pos_scale_rot, rotation_t);

    print_graph(&ecs->archetype_list[0], 0);

    ecs_free(&ecs);

    re_arena_destroy(&arena);
    re_terminate();
    return 0;
}
