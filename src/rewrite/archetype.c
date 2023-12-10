#include "core.h"

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
}

static void archetype_print_edges(archetype_t *archetype) {
    re_log_info("%d", archetype->index);
    for (re_hash_map_iter_t iter = re_hash_map_iter_get(archetype->edge_map);
        re_hash_map_iter_valid(iter);
        iter = re_hash_map_iter_next(archetype->edge_map, iter)) {
        archetype_edge_t edge = re_hash_map_get_index_value(archetype->edge_map, iter);
        if (edge.add == archetype) {
            continue;
        }
        archetype_print_edges(edge.add);
    }
}

void archetype_graph_print(archetype_graph_t graph) {
    archetype_print_edges(&graph.archetypes[0]);
}

void archetype_graph_print_all(archetype_graph_t graph) {
    for (u32_t i = 0; i < re_dyn_arr_count(graph.archetypes); i++) {
        re_log_debug("%u: %u", i, graph.archetypes[i].index);
    }
}
