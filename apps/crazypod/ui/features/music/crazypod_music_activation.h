#ifndef CRAZYPOD_MUSIC_ACTIVATION_H
#define CRAZYPOD_MUSIC_ACTIVATION_H

#include "../../navigation/crazypod_ui_routes.h"

enum crazypod_music_activation_kind {
    CRAZYPOD_MUSIC_ACTIVATION_UNHANDLED = 0,
    CRAZYPOD_MUSIC_ACTIVATION_NONE,
    CRAZYPOD_MUSIC_ACTIVATION_RENDER,
    CRAZYPOD_MUSIC_ACTIVATION_PUSH,
    CRAZYPOD_MUSIC_ACTIVATION_OPEN_ALBUM_FLOW,
    CRAZYPOD_MUSIC_ACTIVATION_REQUEST_NOW_PLAYING,
    CRAZYPOD_MUSIC_ACTIVATION_SHOW_NOW_ACTIONS,
};

struct crazypod_music_activation_result {
    enum crazypod_music_activation_kind kind;
    enum crazypod_route route;
    int group;
};

const char *crazypod_music_search_query(void);
void crazypod_music_search_backspace(void);
int crazypod_music_podcast_track_index(int position);

struct crazypod_music_activation_result
crazypod_music_activation_execute(const struct route_state *state);

#endif
