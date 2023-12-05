#include "ecs/ecs_internal.h"

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
    re_dyn_arr_free(archetype->entities);
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
