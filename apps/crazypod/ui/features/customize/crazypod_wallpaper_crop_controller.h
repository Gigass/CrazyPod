#ifndef CRAZYPOD_WALLPAPER_CROP_CONTROLLER_H
#define CRAZYPOD_WALLPAPER_CROP_CONTROLLER_H

#include <stdbool.h>

#include "../../../crazypod_appearance.h"
#include "../../../crazypod_wallpaper.h"

enum crazypod_wallpaper_crop_phase {
    CRAZYPOD_WALLPAPER_CROP_EDITING = 0,
    CRAZYPOD_WALLPAPER_CROP_APPLYING,
    CRAZYPOD_WALLPAPER_CROP_APPLIED,
    CRAZYPOD_WALLPAPER_CROP_ERROR,
};

struct crazypod_wallpaper_crop_model {
    int photo_index;
    enum crazypod_appearance_field target;
    int zoom_percent;
    int center_x;
    int center_y;
    enum crazypod_wallpaper_crop_phase phase;
    bool error_loading;
    long feedback_until;
    bool menu_holding;
    bool menu_armed;
    long menu_hold_start;
    bool play_holding;
    bool play_armed;
    long play_hold_start;
    bool select_armed;
    int load_progress_seen;
    int apply_progress;
    enum crazypod_wallpaper_apply_result apply_result;
};

void crazypod_wallpaper_crop_controller_start(
    int photo_index, enum crazypod_appearance_field target);
const struct crazypod_wallpaper_crop_model *
crazypod_wallpaper_crop_controller_model(void);

bool crazypod_wallpaper_crop_controller_rect(
    int source_width, int source_height, int maximum_zoom,
    int *crop_x, int *crop_y, int *crop_width, int *crop_height);
void crazypod_wallpaper_crop_controller_adjust_zoom(
    int source_width, int source_height, int maximum_zoom,
    int direction, int steps);
bool crazypod_wallpaper_crop_controller_move(
    int source_width, int source_height, int maximum_zoom,
    int direction_x, int direction_y);
void crazypod_wallpaper_crop_controller_reset(void);

void crazypod_wallpaper_crop_controller_begin_apply(void);
void crazypod_wallpaper_crop_controller_set_apply_progress(int progress);
void crazypod_wallpaper_crop_controller_fail(
    enum crazypod_wallpaper_apply_result result, bool loading);
void crazypod_wallpaper_crop_controller_finish(long now, long duration);
bool crazypod_wallpaper_crop_controller_feedback_expired(long now);

void crazypod_wallpaper_crop_controller_press_menu(long now);
bool crazypod_wallpaper_crop_controller_release_menu(void);
void crazypod_wallpaper_crop_controller_press_play(long now);
bool crazypod_wallpaper_crop_controller_release_play(void);
void crazypod_wallpaper_crop_controller_press_select(void);
bool crazypod_wallpaper_crop_controller_release_select(void);
void crazypod_wallpaper_crop_controller_clear_holds(void);
bool crazypod_wallpaper_crop_controller_update_holds(
    long now, long threshold);

bool crazypod_wallpaper_crop_controller_note_load_progress(int progress);
void crazypod_wallpaper_crop_controller_request_render(void);
bool crazypod_wallpaper_crop_controller_take_render_request(void);

#endif
