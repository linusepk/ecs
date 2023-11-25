#include <rebound.h>

i32_t main(void) {
    re_init();
    re_arena_t *arena = re_arena_create(GB(4));

    re_log_info("Hello, world!");

    re_arena_destroy(&arena);
    re_terminate();
    return 0;
}
