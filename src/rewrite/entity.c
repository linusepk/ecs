#include "core.h"
#include "rebound.h"

ecs_entity_t ecs_entity_new(ecs_t *ecs) {
    ecs_entity_t ent = id_handler_new(&ecs->id_handler);
    return ent;
}

void ecs_entity_destroy(ecs_t *ecs, ecs_entity_t entity) {
    re_hash_map_remove(ecs->id_name_map, entity);
    id_handler_dispose(&ecs->id_handler, entity);
}

b8_t ecs_entity_alive(ecs_t *ecs, ecs_entity_t entity) {
    return id_valid(&ecs->id_handler, entity);
}

void ecs_entity_name_set(ecs_t *ecs, ecs_entity_t entity, re_str_t name) {
    if (!id_valid(&ecs->id_handler, entity)) {
        re_log_error("Can't set the name of a dead entity.");
        return;
    }

    re_hash_map_set(ecs->id_name_map, entity, name);
}

re_str_t ecs_entity_name_get(ecs_t *ecs, ecs_entity_t entity) {
    if (!id_valid(&ecs->id_handler, entity)) {
        re_log_error("Can't get the name of a dead entity.");
        return re_str_null;
    }

    return re_hash_map_get(ecs->id_name_map, entity);
}

void ecs_entity_add(ecs_t *ecs, ecs_entity_t entity, ecs_entity_t id) {
    archetype_record_t record = archetype_graph_get_id(&ecs->archetype_graph, entity);
    archetype_graph_record_add(&ecs->archetype_graph, record, id);
}

void ecs_entity_remove(ecs_t *ecs, ecs_entity_t entity, ecs_entity_t id) {
    archetype_record_t record = archetype_graph_get_id(&ecs->archetype_graph, entity);
    archetype_graph_record_remove(&ecs->archetype_graph, record, id);
}

void ecs_entity_storage(ecs_t *ecs, ecs_entity_t entity, u64_t size) {
    archetype_add_storage_id(&ecs->archetype_graph, entity, size);
}

void *ecs_entity_storage_get(ecs_t *ecs, ecs_entity_t entity, ecs_id_t id) {
    archetype_record_t record = archetype_graph_get_id(&ecs->archetype_graph, entity);
    void *result = archetype_get_storage_id(ecs->archetype_graph, record, id);
    return result;
}
