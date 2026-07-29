#ifndef CRAZYPOD_FEATURE_H
#define CRAZYPOD_FEATURE_H

#include <stdbool.h>

#include "../navigation/crazypod_input_event.h"
#include "../navigation/crazypod_ui_routes.h"

enum crazypod_feature_id {
    CRAZYPOD_FEATURE_MUSIC = 0,
    CRAZYPOD_FEATURE_NOW_PLAYING,
    CRAZYPOD_FEATURE_BOOKS,
    CRAZYPOD_FEATURE_NOTES,
    CRAZYPOD_FEATURE_PHOTOS,
    CRAZYPOD_FEATURE_ORGANIZER,
    CRAZYPOD_FEATURE_CUSTOMIZE,
    CRAZYPOD_FEATURE_SETTINGS,
    CRAZYPOD_FEATURE_MINIAPPS,
    CRAZYPOD_FEATURE_COUNT,
};

struct crazypod_feature {
    enum crazypod_feature_id id;
    const char *name;
};

struct crazypod_feature_input_context {
    long now;
    long ticks_per_second;
    int today_date;
    void (*render)(bool transition);
    void (*boost)(int ticks);
    void (*wake_display)(void);
    void (*move)(int direction);
    void (*activate)(void);
    void (*push)(enum crazypod_route route, int group);
    void (*pop)(void);
};

enum crazypod_feature_media_update {
    CRAZYPOD_FEATURE_MEDIA_NONE = 0,
    CRAZYPOD_FEATURE_MEDIA_ROUTE,
    CRAZYPOD_FEATURE_MEDIA_PREVIEW,
};

const struct crazypod_feature *crazypod_feature_get(
    enum crazypod_feature_id id);

#endif
