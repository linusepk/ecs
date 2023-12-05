#include <rebound.h>

#include "ecs.h"
#include "ecs/ecs_internal.h"

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
        re_log_info("Position (%d): %f, %f", i, pos[i].x, pos[i].y);
    }
}

i32_t main(void) {
    re_init();
    re_arena_t *arena = re_arena_create(GB(8));

    const u32_t entity_count = 1024;
    ecs_t *ecs = ecs_init();

    ecs_register_component(ecs, position_t);
    ecs_register_component(ecs, velocity_t);
    ecs_register_component(ecs, scale_t);

    system_group_t update_group = ecs_system_group(ecs);
    ecs_register_system(ecs, update_group, move_system, velocity_t, position_t);

    system_group_t other = ecs_system_group(ecs);
    ecs_register_system(ecs, other, print_system, position_t);

    re_dyn_arr_t(entity_t) ents = NULL;

    f32_t entity_create_time = re_os_get_time();
    for (u32_t i = 0; i < entity_count; i++) {
        entity_t bob = ecs_entity(ecs);
        entity_add_component(bob, position_t, {{0, 0}});
        entity_add_component(bob, velocity_t, {{i, i * 2}});
        re_dyn_arr_push(ents, bob);
    }
    entity_create_time = re_os_get_time() - entity_create_time;

    f32_t run_time = re_os_get_time();
    ecs_run(ecs, update_group);
    run_time = re_os_get_time() - run_time;

    f32_t add_comp_time = re_os_get_time();
    for (u32_t i = 0; i < entity_count; i++) {
        entity_add_component(ents[i], scale_t, {0});
    }

    /* for (u32_t i = entity_count - 1; i > 0; i--) { */
    /*     entity_add_component(ents[i], scale_t, {0}); */
    /* } */
    add_comp_time = re_os_get_time() - add_comp_time;

    ecs_run(ecs, other);

    re_log_info("Entity create time: %f ms, %f ms/entity", entity_create_time * 1000.0f, entity_create_time * 1000.0f / entity_count);
    re_log_info("Run time: %f ms", run_time * 1000.0f);
    re_log_info("Add comp time: %f ms", add_comp_time * 1000.0f);

    ecs_free(&ecs);

    re_arena_destroy(&arena);
    re_terminate();
    return 0;
}
