#pragma once

#include <rebound.h>

typedef struct ecs_t ecs_t;

typedef struct ecs_iter_t ecs_iter_t;
struct ecs_iter_t {
    const ecs_t *ecs;
    re_hash_map_t(u32_t, re_dyn_arr_t(void)) components;
    const u32_t count;
};

typedef void (*system_t)(ecs_iter_t iter);

typedef u32_t system_group_t;

typedef u32_t entity_id_t;

typedef struct entity_t entity_t;
struct entity_t {
    ecs_t *ecs;
    entity_id_t id;
};

extern ecs_t *ecs_init(void);
extern void ecs_free(ecs_t **ecs);

extern system_group_t ecs_system_group(ecs_t *ecs);
extern void *ecs_iter(ecs_iter_t iter, u32_t index);

#define ecs_register_component(ECS, COMPONENT) _ecs_register_component_impl((ECS), re_str_lit(#COMPONENT), sizeof(COMPONENT))
#define ecs_register_system(ECS, GROUP, SYSTEM, COMPONENTS...) _ecs_register_system_impl((ECS), (GROUP), (SYSTEM), re_str_lit(#COMPONENTS));

extern void ecs_run(ecs_t *ecs, system_group_t group);

extern entity_t ecs_entity(ecs_t *ecs);
#define entity_add_component(ENTITY, COMPONENT, ...) ({ \
        COMPONENT temp_comp = (COMPONENT) __VA_ARGS__; \
        _entity_add_component_impl((ENTITY), re_str_lit(#COMPONENT), &temp_comp); \
    })
#define entity_get_component(ENTITY, COMPONENT) _entity_get_component_impl((ENTITY), re_str_lit(#COMPONENT))

extern void _ecs_register_component_impl(ecs_t *ecs, re_str_t name, u64_t size);
extern void _ecs_register_system_impl(ecs_t *ecs, system_group_t group, system_t system, re_str_t components);
extern void _entity_add_component_impl(entity_t ent, re_str_t component, const void *data);
extern const void *_entity_get_component_impl(entity_t ent, re_str_t component);
