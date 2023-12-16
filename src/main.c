#include <rebound.h>

#include "rewrite/core.h"

typedef re_vec2_t position_t;
typedef re_vec2_t velocity_t;

i32_t main(void) {
    re_init();
    re_arena_t *arena = re_arena_create(GB(8));

    ecs_t *ecs = ecs_init();

    ecs_register_component(ecs, position_t);
    ecs_register_component(ecs, velocity_t);

    ecs_entity_t foo = ecs_entity_new(ecs);
    ecs_entity_storage(ecs, foo, sizeof(i32_t));
    ecs_entity_name_set(ecs, foo, re_str_lit("Foo"));
    ecs_entity_t bar = ecs_entity_new(ecs);
    ecs_entity_name_set(ecs, bar, re_str_lit("Bar"));
    ecs_entity_t baz = ecs_entity_new(ecs);
    ecs_entity_name_set(ecs, baz, re_str_lit("Baz"));
    ecs_entity_t qux = ecs_entity_new(ecs);
    ecs_entity_name_set(ecs, qux, re_str_lit("Qux"));

    ecs_entity_t bob = ecs_entity_new(ecs);
    ecs_entity_name_set(ecs, bob, re_str_lit("Bob"));
    ecs_entity_add(ecs, bob, foo);

    i32_t *foo_data = ecs_entity_storage_get(ecs, bob, foo);
    *foo_data = 42;

    ecs_entity_add(ecs, bob, bar);
    ecs_entity_add(ecs, bob, baz);
    ecs_entity_add(ecs, bob, qux);

    foo_data = ecs_entity_storage_get(ecs, bob, foo);
    re_log_debug("%d", *foo_data);

    ecs_entity_remove(ecs, bob, bar);

    foo_data = ecs_entity_storage_get(ecs, bob, foo);
    re_log_debug("%d", *foo_data);

    ecs_entity_t steve = ecs_entity_new(ecs);
    ecs_entity_name_set(ecs, steve, re_str_lit("Steve"));
    ecs_entity_add(ecs, steve, foo);
    ecs_entity_add(ecs, steve, bar);
    ecs_entity_add(ecs, steve, baz);
    ecs_entity_add(ecs, steve, qux);

    ecs_entity_t jerry = ecs_entity_new(ecs);
    ecs_entity_add(ecs, jerry, foo);
    ecs_entity_add(ecs, jerry, baz);

    archetype_graph_print(ecs, ecs->archetype_graph);

    ecs_free(ecs);

    re_arena_destroy(&arena);
    re_terminate();
    return 0;
}
