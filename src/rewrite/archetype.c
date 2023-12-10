#include "core.h"
#include "rebound.h"

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

void archetype_graph_add(archetype_graph_t *graph, const type_t type) {
    if (re_dyn_arr_count(graph->archetypes) == 0) {
        re_dyn_arr_push(graph->archetypes, (archetype_t) {0});
        archetype_t *archetype = &re_dyn_arr_last(graph->archetypes);
        re_hash_map_set(graph->archetype_map, NULL, archetype);
    }

    archetype_t *archetype = re_hash_map_get(graph->archetype_map, type);
    if (archetype != NULL) {
        return;
    }

    re_dyn_arr_push(graph->archetypes, (archetype_t) {0});
    archetype = &re_dyn_arr_last(graph->archetypes);

    archetype->index = re_dyn_arr_count(graph->archetypes) - 1;
    archetype->type = type_copy(type);

    archetype_make_edges(graph, archetype);
}

static void archetype_print_edges(archetype_t *archetype, u32_t spaces) {
    spaces = re_clamp_max(spaces, 255);
    char buffer[256] = {0};
    for (u32_t i = 0; i < spaces; i++) {
        buffer[i] = ' ';
    }

    char archetype_str[256] = {0};
    char *ptr = archetype_str;
    for (u32_t i = 0; i < re_dyn_arr_count(archetype->type); i++) {
        sprintf(ptr, "%llu, ", archetype->type[i]);
        u32_t diff = strlen(ptr);
        ptr += diff;
    }
    ptr[-2] = '\0';

    re_log_info("%s%d: '%s'", buffer, archetype->index, archetype_str);
    for (re_hash_map_iter_t iter = re_hash_map_iter_get(archetype->edge_map);
        re_hash_map_iter_valid(iter);
        iter = re_hash_map_iter_next(archetype->edge_map, iter)) {
        archetype_edge_t edge = re_hash_map_get_index_value(archetype->edge_map, iter);
        if (edge.add == archetype) {
            continue;
        }
        archetype_print_edges(edge.add, spaces + 2);
    }
}

void archetype_graph_print(archetype_graph_t graph) {
    archetype_print_edges(&graph.archetypes[0], 0);
}

void archetype_graph_print_all(archetype_graph_t graph) {
    for (u32_t i = 0; i < re_dyn_arr_count(graph.archetypes); i++) {
        re_log_debug("%u: %u", i, graph.archetypes[i].index);
    }
}
