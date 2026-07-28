#ifndef CRAZYPOD_PHOTO_CONTROLLER_H
#define CRAZYPOD_PHOTO_CONTROLLER_H

#include <stdbool.h>

#include "lvgl.h"

enum crazypod_photo_controller_event {
    CRAZYPOD_PHOTO_EVENT_NONE = 0,
    CRAZYPOD_PHOTO_EVENT_HOLD_PROGRESS,
    CRAZYPOD_PHOTO_EVENT_FAVORITE_CHANGED,
    CRAZYPOD_PHOTO_EVENT_FEEDBACK_EXPIRED,
};

struct crazypod_photo_controller_model {
    int pan_x;
    int pan_y;
    int zoom_percent;
    bool select_long_handled;
    bool select_holding;
    long select_hold_start;
    int select_hold_percent;
    long favorite_feedback_until;
    bool favorite_feedback_added;
    bool favorite_feedback_error;
    bool wheel_touch_active;
    int wheel_touch_start;
    int wheel_touch_max_delta;
    long direction_input_tick;
};

void crazypod_photo_controller_reset(void);
void crazypod_photo_controller_open_detail(int zoom_percent);
const struct crazypod_photo_controller_model *
crazypod_photo_controller_model(void);

const lv_image_dsc_t *crazypod_photo_controller_render_viewport(
    int photo_index);
bool crazypod_photo_controller_adjust_zoom(int direction, int steps);
void crazypod_photo_controller_queue_pan(int delta_x, int delta_y);
bool crazypod_photo_controller_take_pan_render(void);
void crazypod_photo_controller_note_direction(long now);

void crazypod_photo_controller_begin_select(bool valid, long now);
void crazypod_photo_controller_release_select(
    bool *activate, bool *remove_progress);
enum crazypod_photo_controller_event crazypod_photo_controller_tick(
    long now, int photo_index, long progress_delay,
    long hold_duration, long feedback_duration);

void crazypod_photo_controller_wheel_sample(
    int position, long now, long recent_ticks,
    int move_threshold, int pan_step);
void crazypod_photo_controller_cancel_wheel(void);

#endif
