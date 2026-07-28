#ifndef CRAZYPOD_MEDIA_ACTIVATION_H
#define CRAZYPOD_MEDIA_ACTIVATION_H

#include <stdbool.h>

#include "../../navigation/crazypod_ui_routes.h"

enum crazypod_media_activation_kind {
    CRAZYPOD_MEDIA_ACTIVATION_UNHANDLED = 0,
    CRAZYPOD_MEDIA_ACTIVATION_NONE,
    CRAZYPOD_MEDIA_ACTIVATION_PUSH,
    CRAZYPOD_MEDIA_ACTIVATION_PUSH_SELECTED,
    CRAZYPOD_MEDIA_ACTIVATION_RENDER,
};

struct crazypod_media_activation_result {
    enum crazypod_media_activation_kind kind;
    enum crazypod_route route;
    int group;
    int selected;
    bool video_started;
    unsigned video_generation;
};

struct crazypod_media_activation_result
crazypod_media_activation_execute(struct route_state *state);

#endif
