#include "ecs/ecs_internal.h"

entity_t ecs_entity(ecs_t *ecs) {
    entity_id_t id = ecs->entity_index++;

    entity_record_t record = {
        .archetype = &ecs->archetype_list[0],
        .column = re_dyn_arr_count(ecs->archetype_list[0].entities)
    };
    re_hash_map_set(ecs->entity_map, id, record);
    re_dyn_arr_push(ecs->archetype_list[0].entities, id);

    return (entity_t) {ecs, id};
}

static archetype_t *archetype_new(ecs_t *ecs, type_t type) {
    re_dyn_arr_reserve(ecs->archetype_list, 1);
    archetype_t *archetype = &re_dyn_arr_last(ecs->archetype_list);
    archetype->id = re_dyn_arr_count(ecs->archetype_list) - 1;
    archetype->type = type;

    for (u32_t i = 0; i < re_dyn_arr_count(type); i++) {
        component_t comp = ecs->component_list[i];
        re_dyn_arr_push(archetype->components, NULL);
        re_dyn_arr_new(archetype->components[i], comp.size);
        re_hash_map_set(archetype->component_map, type[i], i);
    }

    re_hash_map_set(ecs->archetype_map, type, archetype);

    return archetype;
}

static void archetype_make_edges(ecs_t *ecs, archetype_t *archetype) {
    for (u32_t i = 0; i < re_dyn_arr_count(archetype->type); i++) {
        type_t previous_type = NULL;
        re_dyn_arr_push_arr(previous_type, archetype->type, re_dyn_arr_count(archetype->type));
        u32_t change = re_dyn_arr_remove(previous_type, i);

        archetype_t *previous = re_hash_map_get(ecs->archetype_map, previous_type);
        if (previous == NULL) {
            previous = archetype_new(ecs, previous_type);
            archetype_make_edges(ecs, previous);
        } else {
            re_dyn_arr_free(previous_type);
        }

        archetype_edge_t edge = {
            .add = archetype,
            .remove = previous,
        };
        re_hash_map_set(archetype->edges, change, edge);
        re_hash_map_set(previous->edges, change, edge);
    }
}

static entity_record_t move_entity(ecs_t *ecs, u32_t column, archetype_t *old, archetype_t *new) {
    u32_t new_column = re_dyn_arr_count(new->entities);

    for (u32_t i = 0; i < re_dyn_arr_count(new->type); i++) {
        re_dyn_arr_reserve(new->components[i], 1);
    }

    for (u32_t i = 0; i < re_dyn_arr_count(old->type); i++) {
        u32_t comp_id = old->type[i];
        u32_t comp_size = ecs->component_list[comp_id].size;

        u32_t new_row = re_hash_map_get(new->component_map, old->type[i]);
        void *new_comp_pos = new->components[new_row] + new_column * comp_size;

        _re_dyn_arr_remove_fast_impl((void **) &old->components[i], column, new_comp_pos);
    }

    entity_id_t last_ent_id = re_dyn_arr_last(old->entities);
    re_hash_map_set(ecs->entity_map, last_ent_id, ((entity_record_t) {.archetype = old, .column = column}));

    entity_id_t ent_id = re_dyn_arr_remove_fast(old->entities, column);
    re_dyn_arr_push(new->entities, ent_id);

    entity_record_t record = {
        .archetype = new,
        .column = new_column
    };

    return record;
}

void _entity_add_component_impl(entity_t ent, re_str_t component, const void *data) {
    ecs_t *ecs = ent.ecs;
    if (!re_hash_map_has(ecs->component_map, component)) {
        re_log_error("Component '%.*s' not registered.", (i32_t) component.len, component.str);
        return;
    }
    entity_record_t record = re_hash_map_get(ecs->entity_map, ent.id);
    archetype_t *curr = record.archetype;
    component_t *comp = re_hash_map_get(ecs->component_map, component);

    archetype_t *new = re_hash_map_get(curr->edges, comp->id).add;
    if (new == NULL) {
        re_dyn_arr_t(component_id_t) new_type = NULL;
        re_dyn_arr_push_arr(new_type, curr->type, re_dyn_arr_count(curr->type));
        re_dyn_arr_push(new_type, comp->id);
        qsort(new_type, re_dyn_arr_count(new_type), sizeof(component_id_t), type_sort_compare);
        new = re_hash_map_get(ecs->archetype_map, new_type);

        if (new == NULL) {
            new = archetype_new(ecs, new_type);
        } else {
            re_dyn_arr_free(new_type);
        }
    }

    archetype_make_edges(ecs, new);

    entity_record_t new_record = move_entity(ecs, record.column, curr, new);
    u32_t index = re_hash_map_get(new->component_map, comp->id);
    void *comp_pos = new->components[index] + new_record.column * comp->size;
    memcpy(comp_pos, data, comp->size);

    re_hash_map_set(ecs->entity_map, ent.id, new_record);
}

const void *_entity_get_component_impl(entity_t ent, re_str_t component) {
    ecs_t *ecs = ent.ecs;
    if (!re_hash_map_has(ecs->component_map, component)) {
        re_log_error("Component '%.*s' not registered.", (i32_t) component.len, component.str);
        return NULL;
    }
    entity_record_t record = re_hash_map_get(ecs->entity_map, ent.id);
    component_t *comp = re_hash_map_get(ecs->component_map, component);

    if (!re_hash_map_has(record.archetype->component_map, comp->id)) {
        return NULL;
    }

    u32_t index = re_hash_map_get(record.archetype->component_map, comp->id);
    return record.archetype->components[index] + record.column * comp->size;
}

void entity_destroy(entity_t entity) {
    ecs_t *ecs = entity.ecs;

    entity_record_t record = re_hash_map_get(ecs->entity_map, entity.id);
    archetype_t *archetype = record.archetype;

    for (u32_t i = 0; i < re_dyn_arr_count(archetype->type); i++) {
        _re_dyn_arr_remove_fast_impl((void **) &archetype->components[i], record.column, NULL);
    }

    entity_id_t last_ent_id = re_dyn_arr_last(archetype->entities);
    re_hash_map_set(ecs->entity_map, last_ent_id, record);

    re_dyn_arr_remove_fast(archetype->entities, record.column);
}
