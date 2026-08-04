#include "config.h"

#ifdef IPOD_6G

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "../../../crazypod_miniapp_asset_font.h"
#include "../../../crazypod_miniapps.h"
#include "crazypod_miniapp_scene.h"
#include "crazypod_miniapp_scene_internal.h"

#define SCENE_ANIMATION_MAX 64u

struct scene_animation {
    uint32_t handle;
    uint32_t completion;
    uint16_t property;
    bool active;
};

static struct {
    struct crazypod_miniapp_scene_node nodes[CP_UI_HANDLE_MAX];
    struct scene_animation animations[SCENE_ANIMATION_MAX];
    lv_obj_t *parent;
    uint32_t accent;
    uint32_t focused_handle;
    uint32_t handle_high_water;
    uint16_t focus_ordinal;
    bool focus_dirty;
} scene;

static bool focus_candidate(
    const struct crazypod_miniapp_scene_node *node)
{
    return node->state == CRAZYPOD_MINIAPP_SCENE_SLOT_ALIVE &&
        node->object != NULL &&
        node->values[CP_UI_PROP_VISIBLE] != 0 &&
        node->values[CP_UI_PROP_DISABLED] == 0 &&
        (node->values[CP_UI_PROP_FOCUSABLE] != 0 ||
         node->listeners[
             CP_UI_EVENT_SELECT - CP_UI_EVENT_SELECT] !=
             CP_UI_EVENT_NONE ||
         node->listeners[
             CP_UI_EVENT_CHANGE - CP_UI_EVENT_SELECT] !=
             CP_UI_EVENT_NONE);
}

static uint16_t focus_count(void)
{
    uint16_t count = 0;
    uint32_t index;

    for(index = 0; index < CP_UI_HANDLE_MAX; ++index)
        if(focus_candidate(&scene.nodes[index]))
            ++count;
    return count;
}

static struct crazypod_miniapp_scene_node *focus_at(
    uint16_t ordinal, uint32_t *handle)
{
    uint16_t current = 0;
    uint32_t index;

    for(index = 0; index < CP_UI_HANDLE_MAX; ++index) {
        struct crazypod_miniapp_scene_node *node =
            &scene.nodes[index];

        if(!focus_candidate(node))
            continue;
        if(current++ == ordinal) {
            if(handle != NULL)
                *handle = CP_UI_HANDLE_MAKE(
                    index, node->generation);
            return node;
        }
    }
    return NULL;
}

static void focus_apply(void)
{
    struct crazypod_miniapp_scene_node *focused;
    uint16_t count = focus_count();
    uint32_t index;

    for(index = 0; index < CP_UI_HANDLE_MAX; ++index) {
        struct crazypod_miniapp_scene_node *node =
            &scene.nodes[index];

        if(node->object != NULL &&
           node->state == CRAZYPOD_MINIAPP_SCENE_SLOT_ALIVE) {
            node->values[CP_UI_PROP_FOCUSED] = 0;
            lv_obj_remove_state(
                node->object, LV_STATE_FOCUSED);
        }
    }
    scene.focused_handle = CP_UI_HANDLE_NONE;
    if(count == 0)
        return;
    scene.focus_ordinal %= count;
    focused = focus_at(
        scene.focus_ordinal, &scene.focused_handle);
    if(focused != NULL) {
        focused->values[CP_UI_PROP_FOCUSED] = 1;
        lv_obj_add_state(
            focused->object, LV_STATE_FOCUSED);
        lv_obj_scroll_to_view_recursive(
            focused->object, LV_ANIM_OFF);
    }
}

static struct crazypod_miniapp_scene_node *node_for_handle(
    uint32_t handle)
{
    uint32_t index = CP_UI_HANDLE_INDEX(handle);
    struct crazypod_miniapp_scene_node *node;

    if(handle == CP_UI_HANDLE_NONE || index >= CP_UI_HANDLE_MAX)
        return NULL;
    node = &scene.nodes[index];
    if(node->generation != CP_UI_HANDLE_GENERATION(handle))
        return NULL;
    return node;
}

static void property_mark(
    struct crazypod_miniapp_scene_node *node, uint16_t property)
{
    node->property_mask[property >> 6] |=
        (uint64_t)1u << (property & 63u);
}

static bool property_is_set(
    const struct crazypod_miniapp_scene_node *node,
    uint16_t property)
{
    return (node->property_mask[property >> 6] &
            ((uint64_t)1u << (property & 63u))) != 0;
}

static void animation_deleted(lv_anim_t *animation)
{
    struct scene_animation *slot =
        lv_anim_get_user_data(animation);

    if(slot != NULL)
        slot->active = false;
}

static void animation_completed(lv_anim_t *animation)
{
    struct scene_animation *slot =
        lv_anim_get_user_data(animation);
    uint32_t completion;
    uint32_t handle;

    if(slot == NULL || !slot->active)
        return;
    completion = slot->completion;
    handle = slot->handle;
    slot->active = false;
    if(completion != CP_UI_EVENT_NONE)
        (void)crazypod_miniapps_ui_event(
            completion, CP_UI_EVENT_ANIMATION_COMPLETE,
            handle, 0);
}

static void animation_apply(void *context, int32_t value)
{
    struct scene_animation *slot = context;
    struct crazypod_miniapp_scene_node *node;

    if(slot == NULL || !slot->active)
        return;
    node = node_for_handle(slot->handle);
    if(node == NULL ||
       node->state != CRAZYPOD_MINIAPP_SCENE_SLOT_ALIVE) {
        slot->active = false;
        return;
    }
    node->values[slot->property] = value;
    property_mark(node, slot->property);
    crazypod_miniapp_scene_property_apply(
        node, slot->property);
    if(node->type == CP_UI_OBJECT_NOW_PLAYING_ARTWORK)
        crazypod_miniapp_scene_now_playing_artwork_transform_flush(node);
}

static void animations_cancel(uint32_t handle)
{
    uint32_t index;

    for(index = 0; index < SCENE_ANIMATION_MAX; ++index) {
        struct scene_animation *slot =
            &scene.animations[index];

        if(slot->active && slot->handle == handle) {
            (void)lv_anim_delete(slot, NULL);
            slot->active = false;
        }
    }
}

static bool animation_property_valid(uint16_t property)
{
    switch(property) {
    case CP_UI_PROP_X:
    case CP_UI_PROP_Y:
    case CP_UI_PROP_WIDTH:
    case CP_UI_PROP_HEIGHT:
    case CP_UI_PROP_BACKGROUND_COLOR:
    case CP_UI_PROP_BACKGROUND_OPACITY:
    case CP_UI_PROP_BORDER_COLOR:
    case CP_UI_PROP_BORDER_WIDTH:
    case CP_UI_PROP_BORDER_OPACITY:
    case CP_UI_PROP_RADIUS:
    case CP_UI_PROP_OPACITY:
    case CP_UI_PROP_SHADOW_COLOR:
    case CP_UI_PROP_SHADOW_WIDTH:
    case CP_UI_PROP_SHADOW_OPACITY:
    case CP_UI_PROP_TEXT_COLOR:
    case CP_UI_PROP_VALUE:
    case CP_UI_PROP_SCROLL_X:
    case CP_UI_PROP_SCROLL_Y:
    case CP_UI_PROP_TRANSLATE_X:
    case CP_UI_PROP_TRANSLATE_Y:
    case CP_UI_PROP_SCALE_X:
    case CP_UI_PROP_SCALE_Y:
    case CP_UI_PROP_ROTATION:
        return true;
    default:
        return false;
    }
}

static bool start_animation_descriptor(
    uint32_t target, uint16_t property,
    const struct cp_native_ui_animation *descriptor)
{
    struct scene_animation *slot = NULL;
    lv_anim_path_cb_t path = lv_anim_path_linear;
    lv_anim_t animation;
    uint32_t index;

    if(descriptor == NULL ||
       !animation_property_valid(property))
        return false;
    if(descriptor->duration_ms == 0 ||
       descriptor->duration_ms > 60000u ||
       descriptor->delay_ms > 60000u ||
       descriptor->easing > CP_UI_EASING_BOUNCE ||
       descriptor->flags != 0)
        return false;
    for(index = 0; index < SCENE_ANIMATION_MAX; ++index) {
        struct scene_animation *candidate =
            &scene.animations[index];

        if(candidate->active &&
           candidate->handle == target &&
           candidate->property == property) {
            (void)lv_anim_delete(candidate, NULL);
            candidate->active = false;
        }
        if(slot == NULL && !candidate->active)
            slot = candidate;
    }
    if(slot == NULL)
        return false;
    switch(descriptor->easing) {
    case CP_UI_EASING_EASE_IN:
        path = lv_anim_path_ease_in;
        break;
    case CP_UI_EASING_EASE_OUT:
        path = lv_anim_path_ease_out;
        break;
    case CP_UI_EASING_EASE_IN_OUT:
        path = lv_anim_path_ease_in_out;
        break;
    case CP_UI_EASING_OVERSHOOT:
        path = lv_anim_path_overshoot;
        break;
    case CP_UI_EASING_BOUNCE:
        path = lv_anim_path_bounce;
        break;
    default:
        break;
    }
    slot->handle = target;
    slot->completion = descriptor->completion_handler;
    slot->property = property;
    slot->active = true;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, slot);
    lv_anim_set_exec_cb(&animation, animation_apply);
    lv_anim_set_values(
        &animation, descriptor->from, descriptor->to);
    lv_anim_set_duration(&animation, descriptor->duration_ms);
    lv_anim_set_delay(&animation, descriptor->delay_ms);
    lv_anim_set_path_cb(&animation, path);
    lv_anim_set_user_data(&animation, slot);
    lv_anim_set_completed_cb(&animation, animation_completed);
    lv_anim_set_deleted_cb(&animation, animation_deleted);
    lv_anim_start(&animation);
    return true;
}

static void materialize_node(
    struct crazypod_miniapp_scene_node *node)
{
    struct crazypod_miniapp_scene_node *parent_node =
        node_for_handle(node->parent);
    lv_obj_t *parent = parent_node != NULL
        ? parent_node->object : scene.parent;
    uint16_t property;

    if(node->object != NULL || parent == NULL)
        return;
    if(parent_node != NULL &&
       parent_node->type == CP_UI_OBJECT_TILE_VIEW &&
       node->type == CP_UI_OBJECT_VIEW) {
        int32_t column =
            node->values[CP_UI_PROP_TILE_COLUMN];
        int32_t row = node->values[CP_UI_PROP_TILE_ROW];
        int32_t directions =
            node->values[CP_UI_PROP_TILE_DIRECTIONS];

        if(column < 0 || column > 7 ||
           row < 0 || row > 7 ||
           directions <= 0 || directions > 15)
            return;
        node->object = lv_tileview_add_tile(
            parent, (uint8_t)column, (uint8_t)row,
            (lv_dir_t)directions);
    }
    else {
        node->object = crazypod_miniapp_scene_object_create(
            node->type, parent, scene.parent);
    }
    if(node->object == NULL)
        return;
    if(node->type == CP_UI_OBJECT_NOW_PLAYING_ARTWORK)
        node->artwork_generation = 0;
    if(node->type == CP_UI_OBJECT_SCREEN ||
       node->type == CP_UI_OBJECT_VIEW ||
       node->type == CP_UI_OBJECT_MODAL ||
       node->type == CP_UI_OBJECT_BUTTON ||
       node->type == CP_UI_OBJECT_SCROLL_VIEW ||
       node->type == CP_UI_OBJECT_TILEMAP ||
       node->type == CP_UI_OBJECT_NOW_PLAYING_ARTWORK ||
       node->type == CP_UI_OBJECT_SOUND_WAVE)
        lv_obj_remove_style_all(node->object);
    crazypod_miniapp_scene_object_prepare(node);
    if(node->type == CP_UI_OBJECT_SCREEN)
        lv_obj_remove_flag(
            node->object, LV_OBJ_FLAG_SCROLLABLE);
    if(node->type == CP_UI_OBJECT_BUTTON) {
        lv_obj_set_style_outline_color(
            node->object, lv_color_hex(scene.accent),
            LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_outline_width(
            node->object, 2,
            LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_outline_pad(
            node->object, 1,
            LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_outline_opa(
            node->object, LV_OPA_COVER,
            LV_PART_MAIN | LV_STATE_FOCUSED);
    }
    for(property = 1; property < CP_UI_PROP_COUNT; ++property)
        if(property_is_set(node, property))
            crazypod_miniapp_scene_property_apply(node, property);
    if(node->type == CP_UI_OBJECT_NOW_PLAYING_ARTWORK)
        (void)crazypod_miniapp_scene_now_playing_artwork_refresh_node(
            node);
    if(parent_node != NULL) {
        crazypod_miniapp_scene_grid_refresh(parent_node);
        if(parent_node->type == CP_UI_OBJECT_TILE_VIEW &&
           node->type == CP_UI_OBJECT_VIEW &&
           node->values[CP_UI_PROP_TILE_COLUMN] ==
               parent_node->values[
                   CP_UI_PROP_SELECTED_COLUMN] &&
           node->values[CP_UI_PROP_TILE_ROW] ==
               parent_node->values[
                   CP_UI_PROP_SELECTED_ROW])
            lv_tileview_set_tile(
                parent_node->object, node->object,
                LV_ANIM_OFF);
    }
}

static void materialize_all(void)
{
    unsigned int pass;
    uint32_t index;

    for(pass = 0; pass < CP_UI_HANDLE_MAX; ++pass) {
        bool changed = false;

        for(index = 0; index < CP_UI_HANDLE_MAX; ++index) {
            struct crazypod_miniapp_scene_node *node =
                &scene.nodes[index];
            struct crazypod_miniapp_scene_node *parent =
                node_for_handle(node->parent);

            if(node->state != CRAZYPOD_MINIAPP_SCENE_SLOT_ALIVE ||
               node->object != NULL ||
               (node->type != CP_UI_OBJECT_SCREEN &&
                node->parent == CP_UI_HANDLE_NONE) ||
               (node->parent != CP_UI_HANDLE_NONE &&
                (parent == NULL || parent->object == NULL)))
                continue;
            materialize_node(node);
            changed |= node->object != NULL;
        }
        if(!changed)
            break;
    }
}

static void delete_materialized_roots(void)
{
    uint32_t index;

    for(index = 0; index < CP_UI_HANDLE_MAX; ++index) {
        struct crazypod_miniapp_scene_node *node =
            &scene.nodes[index];

        if(node->state == CRAZYPOD_MINIAPP_SCENE_SLOT_ALIVE &&
           node->type == CP_UI_OBJECT_SCREEN &&
           node->object != NULL &&
           node->object != scene.parent &&
           lv_obj_is_valid(node->object))
            lv_obj_delete(node->object);
    }
    for(index = 0; index < CP_UI_HANDLE_MAX; ++index) {
        scene.nodes[index].object = NULL;
        scene.nodes[index].chart_series = NULL;
    }
}

static bool materialized_roots_valid(lv_obj_t *parent)
{
    bool found = false;
    uint32_t index;

    if(parent == NULL || !lv_obj_is_valid(parent))
        return false;
    for(index = 0; index < CP_UI_HANDLE_MAX; ++index) {
        struct crazypod_miniapp_scene_node *node =
            &scene.nodes[index];

        if(node->state != CRAZYPOD_MINIAPP_SCENE_SLOT_ALIVE ||
           node->type != CP_UI_OBJECT_SCREEN)
            continue;
        found = true;
        if(node->object == NULL ||
           !lv_obj_is_valid(node->object) ||
           lv_obj_get_parent(node->object) != parent)
            return false;
    }
    return found;
}

bool crazypod_miniapp_scene_attached(lv_obj_t *parent)
{
    return scene.parent == parent &&
        materialized_roots_valid(parent);
}

void crazypod_miniapp_scene_attach(
    lv_obj_t *parent, uint32_t accent)
{
    if(crazypod_miniapp_scene_attached(parent)) {
        scene.accent = accent;
        return;
    }
    delete_materialized_roots();
    scene.parent = parent;
    scene.accent = accent;
    materialize_all();
    focus_apply();
    scene.focus_dirty = false;
}

void crazypod_miniapp_scene_reset(void)
{
    uint32_t index;

    for(index = 0; index < SCENE_ANIMATION_MAX; ++index)
        if(scene.animations[index].active)
            (void)lv_anim_delete(&scene.animations[index], NULL);
    memset(scene.animations, 0, sizeof(scene.animations));
    for(index = 0; index < CP_UI_HANDLE_MAX; ++index)
        crazypod_miniapp_scene_node_release(&scene.nodes[index]);
    delete_materialized_roots();
    crazypod_miniapp_asset_fonts_reset();
    for(index = 0; index < CP_UI_HANDLE_MAX; ++index) {
        uint16_t generation = scene.nodes[index].generation;

        memset(&scene.nodes[index], 0, sizeof(scene.nodes[index]));
        scene.nodes[index].generation = generation;
    }
    scene.parent = NULL;
    scene.focused_handle = CP_UI_HANDLE_NONE;
    scene.focus_ordinal = 0;
    scene.focus_dirty = false;
}

bool crazypod_miniapp_scene_has_content(void)
{
    uint32_t index;

    for(index = 0; index < CP_UI_HANDLE_MAX; ++index)
        if(scene.nodes[index].state ==
           CRAZYPOD_MINIAPP_SCENE_SLOT_ALIVE)
            return true;
    return false;
}

bool crazypod_miniapp_scene_modal_visible(void)
{
    uint32_t index;

    for(index = 0; index < CP_UI_HANDLE_MAX; ++index)
        if(scene.nodes[index].state ==
               CRAZYPOD_MINIAPP_SCENE_SLOT_ALIVE &&
           scene.nodes[index].type == CP_UI_OBJECT_MODAL)
            return true;
    return false;
}

bool crazypod_miniapp_scene_refresh_now_playing_artwork(void)
{
    bool changed = false;
    uint32_t index;

    for(index = 0; index < CP_UI_HANDLE_MAX; ++index) {
        struct crazypod_miniapp_scene_node *node =
            &scene.nodes[index];

        if(node->state != CRAZYPOD_MINIAPP_SCENE_SLOT_ALIVE ||
           node->object == NULL ||
           !crazypod_miniapp_scene_now_playing_artwork_needs_refresh(
               node))
            continue;
        changed |=
            crazypod_miniapp_scene_now_playing_artwork_refresh_node(
                node);
    }
    return changed;
}

static uint32_t scene_reserve(uint8_t object_type)
{
    uint32_t index;

    if(object_type < CP_UI_OBJECT_SCREEN ||
       object_type >= CP_UI_OBJECT_TYPE_COUNT)
        return CP_UI_HANDLE_NONE;
    for(index = 0; index < CP_UI_HANDLE_MAX; ++index) {
        struct crazypod_miniapp_scene_node *node =
            &scene.nodes[index];
        uint16_t generation;

        if(node->state != CRAZYPOD_MINIAPP_SCENE_SLOT_FREE)
            continue;
        generation = (uint16_t)(node->generation + 1u);
        if(generation == 0)
            generation = 1;
        memset(node, 0, sizeof(*node));
        node->generation = generation;
        node->state = CRAZYPOD_MINIAPP_SCENE_SLOT_RESERVED;
        node->type = object_type;
        node->values[CP_UI_PROP_VISIBLE] = 1;
        node->values[CP_UI_PROP_OPACITY] = LV_OPA_COVER;
        node->values[CP_UI_PROP_BACKGROUND_OPACITY] = LV_OPA_TRANSP;
        node->values[CP_UI_PROP_MAXIMUM] = 100;
        node->values[CP_UI_PROP_SCALE_X] = LV_SCALE_NONE;
        node->values[CP_UI_PROP_SCALE_Y] = LV_SCALE_NONE;
        node->values[CP_UI_PROP_TILE_DIRECTIONS] = 15;
        return CP_UI_HANDLE_MAKE(index, node->generation);
    }
    return CP_UI_HANDLE_NONE;
}

static void remove_node(uint32_t handle)
{
    uint32_t index;
    struct crazypod_miniapp_scene_node *node =
        node_for_handle(handle);
    struct crazypod_miniapp_scene_node *parent;
    bool removes_screen;

    if(node == NULL)
        return;
    removes_screen = node->type == CP_UI_OBJECT_SCREEN;
    parent = node_for_handle(node->parent);
    animations_cancel(handle);
    for(index = 0; index < CP_UI_HANDLE_MAX; ++index)
        if(scene.nodes[index].state ==
               CRAZYPOD_MINIAPP_SCENE_SLOT_ALIVE &&
           scene.nodes[index].parent == handle)
            remove_node(CP_UI_HANDLE_MAKE(
                index, scene.nodes[index].generation));
    crazypod_miniapp_scene_node_release(node);
    if(node->object != NULL && node->object != scene.parent)
        lv_obj_delete(node->object);
    node->object = NULL;
    node->parent = CP_UI_HANDLE_NONE;
    node->state = CRAZYPOD_MINIAPP_SCENE_SLOT_FREE;
    scene.focus_dirty = true;
    if(removes_screen) {
        scene.focused_handle = CP_UI_HANDLE_NONE;
        scene.focus_ordinal = 0;
    }
    crazypod_miniapp_scene_grid_refresh(parent);
}

static struct crazypod_miniapp_scene_node *alive_node(
    uint32_t handle)
{
    struct crazypod_miniapp_scene_node *node =
        node_for_handle(handle);

    return node != NULL &&
        node->state == CRAZYPOD_MINIAPP_SCENE_SLOT_ALIVE
        ? node : NULL;
}

int crazypod_miniapp_scene_begin_update(void)
{
    return 0;
}

uint32_t crazypod_miniapp_scene_create(uint8_t object_type)
{
    uint32_t handle =
        scene_reserve(object_type);
    struct crazypod_miniapp_scene_node *node =
        node_for_handle(handle);

    if(node == NULL)
        return CP_UI_HANDLE_NONE;
    node->state = CRAZYPOD_MINIAPP_SCENE_SLOT_ALIVE;
    {
        uint32_t count = crazypod_miniapp_scene_handle_count();

        if(count > scene.handle_high_water)
            scene.handle_high_water = count;
    }
    if(node->type == CP_UI_OBJECT_SCREEN)
        materialize_node(node);
    return handle;
}

uint32_t crazypod_miniapp_scene_handle_count(void)
{
    uint32_t count = 0;
    uint32_t index;

    for(index = 0; index < CP_UI_HANDLE_MAX; ++index)
        if(scene.nodes[index].state ==
           CRAZYPOD_MINIAPP_SCENE_SLOT_ALIVE)
            ++count;
    return count;
}

uint32_t crazypod_miniapp_scene_handle_high_water(void)
{
    return scene.handle_high_water;
}

void crazypod_miniapp_scene_reset_handle_high_water(void)
{
    scene.handle_high_water =
        crazypod_miniapp_scene_handle_count();
}

static bool parent_cycle(uint32_t child, uint32_t parent)
{
    unsigned int depth;

    for(depth = 0; depth < CP_UI_HANDLE_MAX; ++depth) {
        struct crazypod_miniapp_scene_node *node;

        if(parent == CP_UI_HANDLE_NONE)
            return false;
        if(parent == child)
            return true;
        node = alive_node(parent);
        if(node == NULL)
            return true;
        parent = node->parent;
    }
    return true;
}

int crazypod_miniapp_scene_insert(
    uint32_t child_handle, uint32_t parent_handle,
    uint32_t before_handle)
{
    struct crazypod_miniapp_scene_node *child =
        alive_node(child_handle);
    struct crazypod_miniapp_scene_node *parent =
        alive_node(parent_handle);
    struct crazypod_miniapp_scene_node *before = NULL;
    bool materialized;

    if(child == NULL || parent == NULL ||
       parent_cycle(child_handle, parent_handle))
        return -1;
    if(before_handle != CP_UI_HANDLE_NONE) {
        before = alive_node(before_handle);
        if(before == NULL || before_handle == child_handle ||
           before->parent != parent_handle)
            return -1;
    }
    materialized = child->object != NULL;
    child->parent = parent_handle;
    scene.focus_dirty = true;
    if(!materialized)
        materialize_all();
    if(child->object != NULL && parent->object != NULL) {
        if(materialized)
            lv_obj_set_parent(child->object, parent->object);
        if(before != NULL && before->object != NULL)
            lv_obj_move_to_index(
                child->object, lv_obj_get_index(before->object));
        crazypod_miniapp_scene_grid_refresh(parent);
    }
    return 0;
}

static bool property_numeric(uint16_t property)
{
    if(property == 0 || property >= CP_UI_PROP_COUNT)
        return false;
    switch(property) {
    case CP_UI_PROP_TEXT:
    case CP_UI_PROP_IMAGE_SOURCE:
    case CP_UI_PROP_FONT_SOURCE:
    case CP_UI_PROP_PLACEHOLDER:
    case CP_UI_PROP_DATA:
    case CP_UI_PROP_BACKGROUND_COLOR:
    case CP_UI_PROP_BORDER_COLOR:
    case CP_UI_PROP_SHADOW_COLOR:
    case CP_UI_PROP_TEXT_COLOR:
        return false;
    default:
        return true;
    }
}

int crazypod_miniapp_scene_set_i32(
    uint32_t target, uint16_t property, int32_t value)
{
    struct crazypod_miniapp_scene_node *node =
        alive_node(target);

    if(node == NULL || !property_numeric(property))
        return -1;
    if(property_is_set(node, property) &&
       node->values[property] == value) {
        if(property != CP_UI_PROP_REVISION ||
           !crazypod_miniapp_scene_now_playing_artwork_needs_refresh(node))
            return 0;
    }
    node->values[property] = value;
    property_mark(node, property);
    if(property == CP_UI_PROP_VISIBLE ||
       property == CP_UI_PROP_DISABLED ||
       property == CP_UI_PROP_FOCUSABLE)
        scene.focus_dirty = true;
    if(property == CP_UI_PROP_FONT)
        return crazypod_miniapp_scene_text_font_apply(node, value)
            ? 0 : -1;
    crazypod_miniapp_scene_property_apply(node, property);
    return 0;
}

int crazypod_miniapp_scene_set_color(
    uint32_t target, uint16_t property, uint32_t rgb)
{
    struct crazypod_miniapp_scene_node *node =
        alive_node(target);

    if(node == NULL || rgb > 0xffffffu ||
       (property != CP_UI_PROP_BACKGROUND_COLOR &&
        property != CP_UI_PROP_BORDER_COLOR &&
        property != CP_UI_PROP_SHADOW_COLOR &&
        property != CP_UI_PROP_TEXT_COLOR))
        return -1;
    if(property_is_set(node, property) &&
       node->values[property] == (int32_t)rgb)
        return 0;
    node->values[property] = (int32_t)rgb;
    property_mark(node, property);
    crazypod_miniapp_scene_property_apply(node, property);
    return 0;
}

int crazypod_miniapp_scene_set_string(
    uint32_t target, uint16_t property, const char *value)
{
    struct crazypod_miniapp_scene_node *node =
        alive_node(target);
    char *destination;
    size_t capacity;
    size_t length;

    if(node == NULL || value == NULL)
        return -1;
    if(property == CP_UI_PROP_FONT_SOURCE &&
       crazypod_miniapp_asset_font(value) == NULL)
        return -1;
    if(property == CP_UI_PROP_TEXT) {
        destination = node->text;
        capacity = sizeof(node->text);
    }
    else if(property == CP_UI_PROP_PLACEHOLDER) {
        destination = node->placeholder;
        capacity = sizeof(node->placeholder);
    }
    else if(property == CP_UI_PROP_IMAGE_SOURCE) {
        destination = node->source;
        capacity = sizeof(node->source);
    }
    else if(property == CP_UI_PROP_FONT_SOURCE) {
        destination = node->font_source;
        capacity = sizeof(node->font_source);
    }
    else {
        return -1;
    }
    length = strlen(value);
    if(length >= capacity)
        return -1;
    if(property_is_set(node, property) &&
       strcmp(destination, value) == 0)
        return 0;
    memcpy(destination, value, length + 1);
    property_mark(node, property);
    crazypod_miniapp_scene_property_apply(node, property);
    return 0;
}

int crazypod_miniapp_scene_set_bytes(
    uint32_t target, uint16_t property,
    const void *data, size_t size)
{
    struct crazypod_miniapp_scene_node *node =
        alive_node(target);

    if(node == NULL || property != CP_UI_PROP_DATA ||
       !crazypod_miniapp_scene_data_replace(node, data, size) ||
       !crazypod_miniapp_scene_data_apply(node))
        return -1;
    property_mark(node, property);
    return 0;
}

int crazypod_miniapp_scene_listen(
    uint32_t target, uint8_t event_type, uint32_t handler)
{
    struct crazypod_miniapp_scene_node *node =
        alive_node(target);

    if(node == NULL ||
       event_type < CP_UI_EVENT_SELECT ||
       event_type > CP_UI_EVENT_ANIMATION_COMPLETE)
        return -1;
    if(node->listeners[event_type - CP_UI_EVENT_SELECT] == handler)
        return 0;
    node->listeners[event_type - CP_UI_EVENT_SELECT] = handler;
    if(event_type == CP_UI_EVENT_SELECT ||
       event_type == CP_UI_EVENT_CHANGE)
        scene.focus_dirty = true;
    return 0;
}

int crazypod_miniapp_scene_animate(
    uint32_t target, uint16_t property,
    int32_t from, int32_t to,
    uint32_t duration_ms, uint32_t delay_ms,
    uint16_t easing, uint32_t completion_handler)
{
    struct cp_native_ui_animation descriptor = {
        .from = from,
        .to = to,
        .duration_ms = duration_ms,
        .delay_ms = delay_ms,
        .easing = easing,
        .flags = 0,
        .completion_handler = completion_handler,
    };

    return alive_node(target) != NULL &&
        start_animation_descriptor(target, property, &descriptor)
        ? 0 : -1;
}

int crazypod_miniapp_scene_commit_drawing(
    uint32_t target, const void *data, size_t size)
{
    struct crazypod_miniapp_scene_node *node =
        alive_node(target);

    if(node == NULL)
        return -1;
    if(node->type == CP_UI_OBJECT_TILEMAP)
        return crazypod_miniapp_scene_tilemap_commit(
            node, data, size) ? 0 : -1;
    if(node->type == CP_UI_OBJECT_CANVAS)
        return crazypod_miniapp_scene_canvas_commit(
            node, data, size) ? 0 : -1;
    return -1;
}

int crazypod_miniapp_scene_remove(uint32_t target)
{
    if(alive_node(target) == NULL)
        return -1;
    remove_node(target);
    return 0;
}

int crazypod_miniapp_scene_end_update(void)
{
    uint32_t index;

    materialize_all();
    for(index = 0; index < CP_UI_HANDLE_MAX; ++index) {
        struct crazypod_miniapp_scene_node *node =
            &scene.nodes[index];

        if(node->state == CRAZYPOD_MINIAPP_SCENE_SLOT_ALIVE &&
           node->type == CP_UI_OBJECT_NOW_PLAYING_ARTWORK)
            crazypod_miniapp_scene_now_playing_artwork_transform_flush(node);
    }
    if(scene.focus_dirty) {
        focus_apply();
        scene.focus_dirty = false;
    }
    return 0;
}

static bool dispatch_focused(
    uint8_t event_type, int32_t value)
{
    struct crazypod_miniapp_scene_node *node =
        alive_node(scene.focused_handle);
    uint32_t handler;
    uint32_t target = scene.focused_handle;

    if(node == NULL)
        return false;
    handler = node->listeners[event_type - CP_UI_EVENT_SELECT];
    if(handler == CP_UI_EVENT_NONE)
        return false;
    return crazypod_miniapps_ui_event(
        handler, event_type, target, value);
}

bool crazypod_miniapp_scene_input(
    const struct cp_input_event *event)
{
    struct crazypod_miniapp_scene_node *node;
    uint16_t count;
    int32_t value;

    if(event == NULL || event->struct_size < sizeof(*event))
        return false;
    count = focus_count();
    if(event->type == CP_INPUT_WHEEL_CLOCKWISE ||
       event->type == CP_INPUT_WHEEL_COUNTERCLOCKWISE) {
        uint32_t previous_handle = scene.focused_handle;
        struct crazypod_miniapp_scene_node *previous =
            alive_node(previous_handle);
        uint32_t blur_handler = previous != NULL
            ? previous->listeners[
                CP_UI_EVENT_BLUR - CP_UI_EVENT_SELECT]
            : CP_UI_EVENT_NONE;
        uint32_t focus_handler;
        uint32_t focused_handle;
        struct crazypod_miniapp_scene_node *focused;
        int delta;

        if(count == 0)
            return false;
        delta = event->steps;
        if(event->type == CP_INPUT_WHEEL_COUNTERCLOCKWISE)
            delta = -delta;
        scene.focus_ordinal = (uint16_t)(
            (scene.focus_ordinal + count +
             delta % count) % count);
        focus_apply();
        focused_handle = scene.focused_handle;
        focused = alive_node(focused_handle);
        focus_handler = focused != NULL
            ? focused->listeners[
                CP_UI_EVENT_FOCUS - CP_UI_EVENT_SELECT]
            : CP_UI_EVENT_NONE;
        if(previous_handle != focused_handle &&
           blur_handler != CP_UI_EVENT_NONE)
            (void)crazypod_miniapps_ui_event(
                blur_handler, CP_UI_EVENT_BLUR,
                previous_handle, 0);
        if(previous_handle != focused_handle &&
           focus_handler != CP_UI_EVENT_NONE &&
           alive_node(focused_handle) != NULL)
            (void)crazypod_miniapps_ui_event(
                focus_handler, CP_UI_EVENT_FOCUS,
                focused_handle, 0);
        return true;
    }
    node = alive_node(scene.focused_handle);
    if(node == NULL)
        return false;
    if(event->type == CP_INPUT_SELECT) {
        uint8_t type = event->repeated
            ? CP_UI_EVENT_LONG_PRESS : CP_UI_EVENT_SELECT;

        if((node->type == CP_UI_OBJECT_SWITCH ||
            node->type == CP_UI_OBJECT_CHECKBOX) &&
           node->listeners[
               CP_UI_EVENT_CHANGE - CP_UI_EVENT_SELECT] !=
               CP_UI_EVENT_NONE) {
            value = node->values[CP_UI_PROP_CHECKED] == 0;
            node->values[CP_UI_PROP_CHECKED] = value;
            crazypod_miniapp_scene_property_apply(
                node, CP_UI_PROP_CHECKED);
            return dispatch_focused(
                CP_UI_EVENT_CHANGE, value);
        }
        return dispatch_focused(type, 0);
    }
    if((event->type == CP_INPUT_LEFT ||
        event->type == CP_INPUT_RIGHT) &&
       node->type == CP_UI_OBJECT_SLIDER &&
       node->listeners[
           CP_UI_EVENT_CHANGE - CP_UI_EVENT_SELECT] !=
           CP_UI_EVENT_NONE) {
        int32_t minimum =
            node->values[CP_UI_PROP_MINIMUM];
        int32_t maximum =
            node->values[CP_UI_PROP_MAXIMUM];

        value = node->values[CP_UI_PROP_VALUE] +
            (event->type == CP_INPUT_LEFT ? -1 : 1);
        if(value < minimum)
            value = minimum;
        if(value > maximum)
            value = maximum;
        node->values[CP_UI_PROP_VALUE] = value;
        crazypod_miniapp_scene_property_apply(
            node, CP_UI_PROP_VALUE);
        return dispatch_focused(
            CP_UI_EVENT_CHANGE, value);
    }
    return false;
}

uint32_t crazypod_miniapp_scene_listener(
    uint32_t handle, uint8_t event_type)
{
    struct crazypod_miniapp_scene_node *node =
        node_for_handle(handle);

    if(node == NULL ||
       node->state != CRAZYPOD_MINIAPP_SCENE_SLOT_ALIVE ||
       event_type < CP_UI_EVENT_SELECT ||
       event_type > CP_UI_EVENT_ANIMATION_COMPLETE)
        return CP_UI_EVENT_NONE;
    return node->listeners[event_type - CP_UI_EVENT_SELECT];
}

#endif
