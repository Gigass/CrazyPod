#ifndef CRAZYPOD_PREVIEW_MOTION_H
#define CRAZYPOD_PREVIEW_MOTION_H

#include <stdbool.h>

#include "lvgl.h"

enum crazypod_preview_motion_profile {
    CRAZYPOD_PREVIEW_PROFILE_DEFAULT = 0,
    CRAZYPOD_PREVIEW_PROFILE_MUSIC,
    CRAZYPOD_PREVIEW_PROFILE_PHOTOS,
    CRAZYPOD_PREVIEW_PROFILE_NOTES,
    CRAZYPOD_PREVIEW_PROFILE_BOOKS,
};

struct crazypod_preview_motion_host {
    long (*now)(void);
    bool (*reduced_motion)(void);
    bool (*can_render)(void);
    void (*render)(bool animated);
    void (*boost)(int ticks);
};

void crazypod_preview_motion_configure(
    const struct crazypod_preview_motion_host *host);
void crazypod_preview_motion_register(
    lv_obj_t *object,
    int enter_dx, int enter_dy, int enter_scale,
    int enter_rotation, int enter_opacity,
    int enter_delay, int enter_duration,
    int exit_dx, int exit_dy, int exit_scale, int exit_rotation);
void crazypod_preview_motion_set_profile(
    enum crazypod_preview_motion_profile profile);
void crazypod_preview_motion_set_direction(int direction);
void crazypod_preview_motion_reset_root(lv_obj_t *parent);
lv_obj_t *crazypod_preview_motion_parent(lv_obj_t *fallback);
void crazypod_preview_motion_start_entrance(void);
void crazypod_preview_motion_start_exit(void);
bool crazypod_preview_motion_active(void);
void crazypod_preview_motion_settle(void);
bool *crazypod_preview_motion_media_deferred_flag(void);
bool crazypod_preview_motion_media_refresh_pending(void);
long crazypod_preview_motion_media_due(void);
void crazypod_preview_motion_clear_media_refresh(void);
bool crazypod_preview_motion_has_content(void);
void crazypod_preview_motion_forget(void);

#endif
