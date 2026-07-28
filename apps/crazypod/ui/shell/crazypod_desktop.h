#ifndef CRAZYPOD_DESKTOP_H
#define CRAZYPOD_DESKTOP_H

#include <stdbool.h>

#include "lvgl.h"

struct crazypod_desktop_host {
    void (*create_corner_masks)(lv_obj_t *screen, int index);
    void (*refresh_corner_masks)(void);
    void (*refresh_lock_appearance)(void);
    void (*boost)(int ticks);
};

lv_obj_t *crazypod_desktop_create(
    long now, const lv_font_t *metadata_font,
    const struct crazypod_desktop_host *host);
lv_obj_t *crazypod_desktop_screen(void);
int crazypod_desktop_selected(void);
void crazypod_desktop_set_selected(int selected, bool animated);
void crazypod_desktop_move_selection(int direction);
void crazypod_desktop_refresh_appearance(void);
void crazypod_desktop_set_input_enabled(
    long now, bool enabled, bool restore_wheel_events);
void crazypod_desktop_tick(long now);
int crazypod_desktop_take_wheel_feedback(void);
void crazypod_desktop_render_carousel(
    int tile_size, bool blocked);

#endif
