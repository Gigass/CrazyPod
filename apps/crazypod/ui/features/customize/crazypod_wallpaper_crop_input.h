#ifndef CRAZYPOD_WALLPAPER_CROP_INPUT_H
#define CRAZYPOD_WALLPAPER_CROP_INPUT_H

#include <stdbool.h>

#include "../../navigation/crazypod_input_event.h"

struct crazypod_wallpaper_crop_input_actions {
    void (*note_direction)(long now);
    void (*adjust_zoom)(int direction, int steps);
    void (*move)(int direction_x, int direction_y);
    void (*apply)(void);
    void (*cancel)(void);
};

bool crazypod_wallpaper_crop_input_handle(
    const struct crazypod_input_event *event, long now,
    const struct crazypod_wallpaper_crop_input_actions *actions);

#endif
