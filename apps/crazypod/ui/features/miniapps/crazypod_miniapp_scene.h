#ifndef CRAZYPOD_MINIAPP_SCENE_H
#define CRAZYPOD_MINIAPP_SCENE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lvgl.h"

struct cp_input_event;

void crazypod_miniapp_scene_reset(void);
void crazypod_miniapp_scene_attach(
    lv_obj_t *parent, uint32_t accent);
bool crazypod_miniapp_scene_attached(lv_obj_t *parent);
bool crazypod_miniapp_scene_has_content(void);
bool crazypod_miniapp_scene_modal_visible(void);
bool crazypod_miniapp_scene_refresh_now_playing_artwork(void);
int crazypod_miniapp_scene_begin_update(void);
uint32_t crazypod_miniapp_scene_create(uint8_t object_type);
int crazypod_miniapp_scene_insert(
    uint32_t child, uint32_t parent, uint32_t before);
int crazypod_miniapp_scene_set_i32(
    uint32_t target, uint16_t property, int32_t value);
int crazypod_miniapp_scene_set_color(
    uint32_t target, uint16_t property, uint32_t rgb);
int crazypod_miniapp_scene_set_string(
    uint32_t target, uint16_t property, const char *value);
int crazypod_miniapp_scene_set_bytes(
    uint32_t target, uint16_t property,
    const void *data, size_t size);
int crazypod_miniapp_scene_listen(
    uint32_t target, uint8_t event_type, uint32_t handler);
int crazypod_miniapp_scene_animate(
    uint32_t target, uint16_t property,
    int32_t from, int32_t to,
    uint32_t duration_ms, uint32_t delay_ms,
    uint16_t easing, uint32_t completion_handler);
int crazypod_miniapp_scene_commit_drawing(
    uint32_t target, const void *data, size_t size);
int crazypod_miniapp_scene_remove(uint32_t target);
int crazypod_miniapp_scene_end_update(void);
bool crazypod_miniapp_scene_input(
    const struct cp_input_event *event);
uint32_t crazypod_miniapp_scene_listener(
    uint32_t handle, uint8_t event_type);

#endif
