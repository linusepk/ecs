#include "core.h"
#include "rebound.h"

static u64_t hash_type(const void *key, u64_t size) {
    (void) size;
    const type_t *t = key;
    return re_fvn1a_hash(*t, re_dyn_arr_count(*t) * re_dyn_arr_size(*t));
}

static b8_t eq_type(const void *a, const void *b, u32_t size) {
    (void) size;

    const type_t *_a = a;
    const type_t *_b = b;

    return type_eq(*_a, *_b);
}

archetype_graph_t archetype_graph_init(void) {
    archetype_graph_t graph = {0};

    re_hash_map_init(graph.archetype_map, NULL, NULL, hash_type, eq_type);

    re_dyn_arr_push(graph.archetypes, (archetype_t) {0});
    archetype_t *archetype = &re_dyn_arr_last(graph.archetypes);
    re_hash_map_set(graph.archetype_map, NULL, archetype);

    return graph;
}

void archetype_free(archetype_t *archetype) {
    type_free(&archetype->type);
    re_hash_map_free(archetype->edge_map);
    *archetype = (archetype_t) {0};
}

void archetype_graph_free(archetype_graph_t *graph) {
    for (u32_t i = 0; i < re_dyn_arr_count(graph->archetypes); i++) {
        archetype_free(&graph->archetypes[i]);
    }
    re_dyn_arr_free(graph->archetypes);
    re_hash_map_free(graph->archetype_map);
    *graph = (archetype_graph_t) {0};
}

static ecs_id_t type_diff(const type_t a, const type_t b) {
    RE_ASSERT(re_dyn_arr_count(a) > re_dyn_arr_count(b), "Type a must have more id's than b.");

    if (re_dyn_arr_count(b) == 0) {
        return re_dyn_arr_last(a);
    }

    for (u32_t i = 0; i < re_dyn_arr_count(a); i++) {
        if (a[i] != b[i]) {
            return a[i];
        }
    }

    return re_dyn_arr_last(a);
}

static void archetype_make_edges(archetype_graph_t *graph, archetype_t *archetype) {
    for (u32_t i = 0; i < re_dyn_arr_count(graph->archetypes); i++) {
        archetype_t *sub = &graph->archetypes[i];
        archetype_edge_t edge = {0};

        if (re_dyn_arr_count(sub->type) == re_dyn_arr_count(archetype->type) - 1) {
            if (type_is_subtype(archetype->type, sub->type)) {
                ecs_id_t diff = type_diff(archetype->type, sub->type);

                edge.add = archetype;
                edge.remove = sub;

                re_hash_map_set(archetype->edge_map, diff, edge);
                re_hash_map_set(sub->edge_map, diff, edge);
            }
            continue;
        }

        if (re_dyn_arr_count(sub->type) == re_dyn_arr_count(archetype->type) + 1) {
            if (type_is_subtype(sub->type, archetype->type)) {
                ecs_id_t diff = type_diff(sub->type, archetype->type);

                edge.add = sub;
                edge.remove = archetype;

                re_hash_map_set(archetype->edge_map, diff, edge);
                re_hash_map_set(sub->edge_map, diff, edge);
            }
            continue;
        }
    }
}

static archetype_t *archetype_graph_add(archetype_graph_t *graph, const type_t type) {
    archetype_t *archetype = re_hash_map_get(graph->archetype_map, type);
    if (archetype != NULL) {
        return archetype;
    }

    re_dyn_arr_push(graph->archetypes, (archetype_t) {0});
    archetype = &re_dyn_arr_last(graph->archetypes);

    archetype->index = re_dyn_arr_count(graph->archetypes) - 1;
    archetype->type = type_copy(type);

    for (u32_t i = 0; i < re_dyn_arr_count(archetype->type); i++) {
        if (!re_hash_map_has(graph->storage_map, archetype->type[i])) {
            continue;
        }

        u64_t size = re_hash_map_get(graph->storage_map, archetype->type[i]);
        re_dyn_arr_push(archetype->storage, NULL);
        re_dyn_arr_new(archetype->storage[i], size);
    }

    re_hash_map_set(graph->archetype_map, archetype->type, archetype);

    archetype_make_edges(graph, archetype);

    for (u32_t i = 0; i < re_dyn_arr_count(archetype->type); i++) {
    }

    return archetype;
}

static void move_record(archetype_graph_t *graph, archetype_record_t record, type_t new_type) {
    archetype_t *curr = record.archetype;

    archetype_t *new = re_hash_map_get(graph->archetype_map, new_type);
    if (new == NULL) {
        new = archetype_graph_add(graph, new_type);
    }

    for (u32_t i = 0; i < re_dyn_arr_count(new->type); i++) {
        if (!re_hash_map_has(graph->storage_map, new->type[i])) {
            continue;
        }

        re_dyn_arr_reserve(new->storage[i], 1);
    }

    // Switch records with the removed record and the last one.
    if (re_dyn_arr_count(curr->ids) > 0) {
        // New - 0 1 2 3 4
        // Old - 0 2 3 4
        if (re_dyn_arr_count(new->type) > re_dyn_arr_count(curr->type)) {
            u32_t curr_i = 0;
            u32_t new_i = 0;
            for (; curr_i < re_dyn_arr_count(curr->type); curr_i++) {
                if (curr->type[curr_i] != new->type[new_i]) {
                    new_i++;
                }

                if (!re_hash_map_has(graph->storage_map, new->type[new_i])) {
                    continue;
                }

                void *old_slot = curr->storage[curr_i] + record.column * re_dyn_arr_count(curr->storage[curr_i]);
                void *new_slot = new->storage[new_i] + record.column * re_dyn_arr_size(new->storage[new_i]);
                _re_dyn_arr_remove_fast_impl((void **) &old_slot, record.column, new_slot);

                new_i++;
            }
        }

        // New - 0 2 3 4
        // Old - 0 1 2 3 4
        if (re_dyn_arr_count(new->type) < re_dyn_arr_count(curr->type)) {
            u32_t curr_i = 0;
            u32_t new_i = 0;
            for (; new_i < re_dyn_arr_count(new->type); new_i++) {
                if (!re_hash_map_has(graph->storage_map, new->type[new_i])) {
                    continue;
                }

                if (curr->type[curr_i] != new->type[new_i]) {
                    void *old_slot = curr->storage[curr_i] + record.column * re_dyn_arr_count(curr->storage[curr_i]);
                    _re_dyn_arr_remove_fast_impl((void **) &old_slot, record.column, NULL);
                    curr_i++;
                }

                void *old_slot = curr->storage[curr_i] + record.column * re_dyn_arr_count(curr->storage[curr_i]);
                void *new_slot = new->storage[new_i] + record.column * re_dyn_arr_size(new->storage[new_i]);
                _re_dyn_arr_remove_fast_impl((void **) &old_slot, record.column, new_slot);

                curr_i++;
            }
        }

        ecs_id_t last_id = re_dyn_arr_last(curr->ids);
        re_hash_map_set(graph->id_map, last_id, record);
        re_dyn_arr_remove_fast(curr->ids, record.column);
    }

    record = (archetype_record_t) {
        .id = record.id,
        .archetype = new,
        .column = re_dyn_arr_count(new->ids),
    };
    re_hash_map_set(graph->id_map, record.id, record);
    re_dyn_arr_push(new->ids, record.id);
}

void archetype_graph_record_add(archetype_graph_t *graph, archetype_record_t record, ecs_id_t id) {
    type_t new_type = type_copy(record.archetype->type);
    type_add(&new_type, id);
    move_record(graph, record, new_type);
    type_free(&new_type);
}

void archetype_graph_record_remove(archetype_graph_t *graph, archetype_record_t record, ecs_id_t id) {
    type_t new_type = type_copy(record.archetype->type);
    type_remove(&new_type, id);
    move_record(graph, record, new_type);
    type_free(&new_type);
}

void archetype_add_storage_id(archetype_graph_t *graph, ecs_id_t id, u64_t size) {
    re_hash_map_set(graph->storage_map, id, size);
}

void *archetype_get_storage_id(archetype_graph_t graph, archetype_record_t record, ecs_id_t id) {
    if (!re_hash_map_has(graph.storage_map, id)) {
        re_log_error("Id '%llu' has no storage attached.", id);
        return NULL;
    }

    archetype_t *archetype = record.archetype;
    for (u32_t i = 0; i < re_dyn_arr_count(archetype->type); i++) {
        if (archetype->type[i] != id) {
            continue;
        }

        return archetype->storage[i] + record.column * re_dyn_arr_size(archetype->storage[i]);
    }

    return NULL;
}

archetype_t *archetype_graph_get(archetype_graph_t *graph, type_t type) {
    return re_hash_map_get(graph->archetype_map, type);
}

archetype_record_t archetype_graph_get_id(archetype_graph_t *graph, ecs_id_t id) {
    archetype_record_t record = re_hash_map_get(graph->id_map, id);
    if (record.archetype == NULL) {
        record = (archetype_record_t) {
            .id = id,
            .archetype = &graph->archetypes[0],
        };
    }
    return record;
}

static void archetype_print_edges(ecs_t *ecs, archetype_t *archetype, u32_t spaces) {
    spaces = re_clamp_max(spaces, 255);
    char buffer[256] = {0};
    for (u32_t i = 0; i < spaces; i++) {
        buffer[i] = ' ';
    }

    char archetype_str[512] = {0};
    char *ptr = archetype_str;
    for (u32_t i = 0; i < re_dyn_arr_count(archetype->type); i++) {
        re_str_t name = ecs_entity_name_get(ecs, archetype->type[i]);
        if (name.str != NULL) {
            for (u32_t i = 0; i < name.len; i++) {
                *ptr++ = name.str[i];
            }
            *ptr++ = ',';
            *ptr++ = ' ';
        } else {
            sprintf(ptr, "%llu, ", archetype->type[i]);
            u32_t diff = strlen(ptr);
            ptr += diff;
        }
    }
    ptr[-2] = '\0';

    re_log_info("%s'%s': %u", buffer, archetype_str, re_dyn_arr_count(archetype->ids));
    for (re_hash_map_iter_t iter = re_hash_map_iter_get(archetype->edge_map);
        re_hash_map_iter_valid(iter);
        iter = re_hash_map_iter_next(archetype->edge_map, iter)) {
        archetype_edge_t edge = re_hash_map_get_index_value(archetype->edge_map, iter);
        if (edge.add == archetype) {
            continue;
        }
        archetype_print_edges(ecs, edge.add, spaces + 4);
    }
}

void archetype_graph_print(ecs_t *ecs, archetype_graph_t graph) {
    archetype_print_edges(ecs, &graph.archetypes[0], 0);
}

void archetype_graph_print_all(archetype_graph_t graph) {
    for (u32_t i = 0; i < re_dyn_arr_count(graph.archetypes); i++) {
        re_log_debug("%u: %u", i, graph.archetypes[i].index);
    }
}
