#include "core.h"

static i32_t id_cmp(const void *a, const void *b) {
    const ecs_id_t *_a = a;
    const ecs_id_t *_b = b;

    u32_t data_a = id_get_data(*_a);
    u32_t data_b = id_get_data(*_b);

    return (i32_t) (data_a - data_b);
}

void type_free(type_t *type) {
    re_dyn_arr_free(*type);
}

void type_add(type_t *type, ecs_id_t id) {
    if (type_has(*type, id)) {
        return;
    }

    if (*type == NULL) {
        re_dyn_arr_push(*type, id);
        return;
    }

    if (id < (*type)[0]) {
        re_dyn_arr_insert(*type, id, 0);
        return;
    }

    re_dyn_arr_push(*type, id);

    // Don't sort if it's bigger than the last one
    // keepign it sorted when pushing.
    if (id > re_dyn_arr_last(*type)) {
        return;
    }

    qsort(*type, re_dyn_arr_count(*type), sizeof(ecs_id_t), id_cmp);
}

static i32_t type_id_index(const type_t type, ecs_id_t id) {
    void *ptr = bsearch(&id, type, re_dyn_arr_count(type), sizeof(ecs_id_t), id_cmp);
    if (ptr == NULL) {
        return -1;
    }

    return ((ptr_t) ptr - (ptr_t) type) / sizeof(ecs_id_t);
}

void type_remove(type_t *type, ecs_id_t id) {
    i32_t index = type_id_index(*type, id);
    if (index == -1) {
        return;
    }

    re_dyn_arr_remove(*type, index);
}

b8_t type_has(const type_t type, ecs_id_t id) {
    return type_id_index(type, id) != -1;
}

b8_t type_is_subtype(const type_t base, const type_t sub) {
    if (type_eq(base, sub)) {
        return true;
    }

    if (re_dyn_arr_count(base) <= re_dyn_arr_count(sub)) {
        return false;
    }

    for (u32_t i = 0; i < re_dyn_arr_count(sub); i++) {
        if (!type_has(base, sub[i])) {
            return false;
        }
    }

    return true;
}

b8_t type_eq(const type_t a, const type_t b) {
    if (re_dyn_arr_count(a) != re_dyn_arr_count(b)) {
        return false;
    }

    for (u32_t i = 0; i < re_dyn_arr_count(a); i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }

    return true;
}

type_t type_copy(const type_t type) {
    type_t new = NULL;
    re_dyn_arr_push_arr(new, type, re_dyn_arr_count(type));
    return new;
}
