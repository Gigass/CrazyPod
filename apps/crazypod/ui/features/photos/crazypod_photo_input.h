#ifndef CRAZYPOD_PHOTO_INPUT_H
#define CRAZYPOD_PHOTO_INPUT_H

#include <stdbool.h>

#include "../../navigation/crazypod_input_event.h"

struct crazypod_photo_input_context {
    bool detail;
    bool selected_photo_available;
    long now;
    int pan_step;
};

struct crazypod_photo_input_actions {
    void (*note_direction)(long now);
    void (*adjust_zoom)(int direction, int steps);
    void (*move_selection)(int direction);
    void (*queue_pan)(int delta_x, int delta_y);
    void (*activate)(void);
    void (*select_feedback_removed)(void);
    void (*leave_detail)(void);
    void (*set_home_wallpaper)(void);
};

bool crazypod_photo_input_handle(
    const struct crazypod_input_event *event,
    const struct crazypod_photo_input_context *context,
    const struct crazypod_photo_input_actions *actions);

#endif
