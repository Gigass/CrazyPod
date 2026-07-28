#ifndef CRAZYPOD_NOW_CAPSULE_H
#define CRAZYPOD_NOW_CAPSULE_H

#include <stdint.h>

#include "lvgl.h"

#include "../../crazypod_music.h"

void crazypod_now_capsule_create(
    lv_obj_t *parent, const lv_font_t *metadata_font);
void crazypod_now_capsule_refresh_material(void);
void crazypod_now_capsule_refresh_appearance(void);
void crazypod_now_capsule_update(
    const struct crazypod_track *track,
    uint32_t elapsed_ms, uint32_t length_ms);
void crazypod_now_capsule_update_artwork(
    const struct crazypod_track *track);
void crazypod_now_capsule_initialize_artwork(void);
void crazypod_now_capsule_poll_artwork(
    const struct crazypod_track *track);
void crazypod_now_capsule_reset_motion(long now);
void crazypod_now_capsule_tick(long now, bool home_active);

#endif
