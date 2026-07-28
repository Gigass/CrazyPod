#ifndef CRAZYPOD_MENU_PREVIEW_MOTION_H
#define CRAZYPOD_MENU_PREVIEW_MOTION_H

#include <stdbool.h>

#include "lvgl.h"

enum crazypod_menu_preview_profile {
    CRAZYPOD_MENU_PREVIEW_PROFILE_DEFAULT = 0,
    CRAZYPOD_MENU_PREVIEW_PROFILE_MUSIC,
    CRAZYPOD_MENU_PREVIEW_PROFILE_PHOTOS,
    CRAZYPOD_MENU_PREVIEW_PROFILE_NOTES,
    CRAZYPOD_MENU_PREVIEW_PROFILE_BOOKS,
};

enum crazypod_menu_preview_motion_event {
    CRAZYPOD_MENU_PREVIEW_ENTERED = 0,
    CRAZYPOD_MENU_PREVIEW_EXITED,
};

typedef void (*crazypod_menu_preview_motion_callback)(
    enum crazypod_menu_preview_motion_event event, void *context);

void crazypod_menu_preview_motion_init(
    crazypod_menu_preview_motion_callback callback, void *context);
void crazypod_menu_preview_motion_invalidate(void);
void crazypod_menu_preview_motion_reset(
    lv_obj_t *parent, int panel_width, int status_height);

lv_obj_t *crazypod_menu_preview_motion_content(void);
bool crazypod_menu_preview_motion_has_content(void);
bool crazypod_menu_preview_motion_active(void);
void crazypod_menu_preview_motion_settle(void);
void crazypod_menu_preview_motion_set_profile(
    enum crazypod_menu_preview_profile profile);
void crazypod_menu_preview_motion_set_direction(int direction);

void crazypod_menu_preview_motion_register(
    lv_obj_t *object,
    int enter_dx, int enter_dy, int enter_scale,
    int enter_rotation, int enter_opacity,
    int enter_delay, int enter_duration,
    int exit_dx, int exit_dy, int exit_scale, int exit_rotation);

int crazypod_menu_preview_motion_start_entrance(bool reduced);
int crazypod_menu_preview_motion_start_exit(bool reduced);

#endif
