#include <rebound.h>

#include "rewrite/core.h"

i32_t main(void) {
    re_init();
    re_arena_t *arena = re_arena_create(GB(8));

    id_handler_t handler = {0};
    ecs_id_t pos_id = id_handler_new(&handler);
    ecs_id_t vel_id = id_handler_new(&handler);
    ecs_id_t rot_id = id_handler_new(&handler);

    type_t bob = NULL;
    type_add(&bob, pos_id);
    type_add(&bob, vel_id);

    type_t jerry = NULL;
    type_add(&jerry, pos_id);

    type_t steve = NULL;
    type_add(&steve, vel_id);

    type_t terry = NULL;
    type_add(&terry, pos_id);
    type_add(&terry, rot_id);
    type_add(&terry, vel_id);

    archetype_graph_t graph = {0};
    archetype_graph_add(&graph, jerry);
    archetype_graph_add(&graph, bob);
    archetype_graph_add(&graph, steve);
    archetype_graph_add(&graph, terry);

    archetype_graph_print(graph);
    re_log_debug("==========");
    archetype_graph_print_all(graph);

    type_free(&bob);

    archetype_graph_free(&graph);
    id_handler_free(&handler);

    re_arena_destroy(&arena);
    re_terminate();
    return 0;
}
