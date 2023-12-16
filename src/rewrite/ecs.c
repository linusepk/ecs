#include "core.h"
#include "rebound.h"

static u64_t str_hash(const void *key, u64_t size) {
    (void) size;
    const re_str_t *str = key;
    return re_fvn1a_hash(str->str, str->len);
}

static b8_t str_eq(const void *a, const void *b, u32_t size) {
    (void) size;
    const re_str_t *_a = a;
    const re_str_t *_b = b;
    return re_str_cmp(*_a, *_b) == 0;
}

ecs_t *ecs_init(void) {
    ecs_t *ecs = re_malloc(sizeof(ecs_t));
    *ecs = (ecs_t) {0};

    component_t null_comp = {U64_MAX, 0};
    re_hash_map_init(ecs->component_map, re_str_null, null_comp, str_hash, str_eq);

    ecs->archetype_graph = archetype_graph_init();

    return ecs;
}

void ecs_free(ecs_t *ecs) {
    id_handler_free(&ecs->id_handler);
    re_hash_map_free(ecs->id_name_map);
    re_hash_map_free(ecs->component_map);

    re_free(ecs);
}

void _ecs_register_component_impl(ecs_t *ecs, u64_t size, re_str_t name) {
    ecs_entity_t ent = ecs_entity_new(ecs);
    ecs_entity_name_set(ecs, ent, name);
    ecs_entity_storage(ecs, ent, size);

    component_t comp = {ent, size};
    re_hash_map_set(ecs->component_map, name, comp);
}
