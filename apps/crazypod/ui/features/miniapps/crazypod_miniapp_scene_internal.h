#ifndef CRAZYPOD_MINIAPP_SCENE_INTERNAL_H
#define CRAZYPOD_MINIAPP_SCENE_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lvgl.h"

#include "../../../../../miniapps/sdk/crazypod_miniapp_native.h"

#define CRAZYPOD_MINIAPP_TEXT_CAPACITY 257u
#define CRAZYPOD_MINIAPP_PLACEHOLDER_CAPACITY 129u
#define CRAZYPOD_MINIAPP_SOURCE_CAPACITY CP_NATIVE_RESOURCE_ID_SIZE
#define CRAZYPOD_MINIAPP_EVENT_COUNT \
    (CP_UI_EVENT_ANIMATION_COMPLETE - CP_UI_EVENT_SELECT + 1u)

enum crazypod_miniapp_scene_slot_state {
    CRAZYPOD_MINIAPP_SCENE_SLOT_FREE = 0,
    CRAZYPOD_MINIAPP_SCENE_SLOT_RESERVED,
    CRAZYPOD_MINIAPP_SCENE_SLOT_ALIVE
};

struct crazypod_miniapp_scene_node {
    lv_obj_t *object;
    uint32_t parent;
    uint32_t listeners[CRAZYPOD_MINIAPP_EVENT_COUNT];
    int32_t values[CP_UI_PROP_COUNT];
    uint64_t property_mask[2];
    uint16_t generation;
    uint8_t state;
    uint8_t type;
    int resource_handle;
    uint32_t external_size;
    int secondary_handle;
    uint32_t secondary_size;
    int data_handle;
    uint32_t data_size;
    lv_chart_series_t *chart_series;
    char secondary_source[24];
    uint8_t table_columns;
    int32_t grid_column_descriptors[9];
    int32_t grid_row_descriptors[9];
    lv_image_dsc_t image;
    char text[CRAZYPOD_MINIAPP_TEXT_CAPACITY];
    char placeholder[CRAZYPOD_MINIAPP_PLACEHOLDER_CAPACITY];
    char source[CRAZYPOD_MINIAPP_SOURCE_CAPACITY];
};

lv_obj_t *crazypod_miniapp_scene_object_create(
    uint8_t type, lv_obj_t *parent, lv_obj_t *root);
void crazypod_miniapp_scene_property_apply(
    struct crazypod_miniapp_scene_node *node,
    uint16_t property);
void crazypod_miniapp_scene_node_release(
    struct crazypod_miniapp_scene_node *node);
void crazypod_miniapp_scene_grid_refresh(
    struct crazypod_miniapp_scene_node *node);
bool crazypod_miniapp_scene_canvas_commit(
    struct crazypod_miniapp_scene_node *node,
    const void *data, size_t size);
bool crazypod_miniapp_scene_tilemap_commit(
    struct crazypod_miniapp_scene_node *node,
    const void *data, size_t size);
bool crazypod_miniapp_scene_data_replace(
    struct crazypod_miniapp_scene_node *node,
    const void *data, size_t size);
bool crazypod_miniapp_scene_data_apply(
    struct crazypod_miniapp_scene_node *node);

#endif
