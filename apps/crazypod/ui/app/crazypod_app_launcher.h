#ifndef CRAZYPOD_APP_LAUNCHER_H
#define CRAZYPOD_APP_LAUNCHER_H

#include "../../crazypod_apps.h"
#include "../navigation/crazypod_ui_routes.h"

struct crazypod_app_launcher_host {
    void (*boost)(bool enabled);
    void (*render)(bool transition);
    void (*begin_music_scan)(void);
    void (*request_now_playing)(void);
    void (*show_lock)(bool turn_display_off);
};

void crazypod_app_launcher_configure(
    const struct crazypod_app_launcher_host *host);
void crazypod_app_launcher_open(enum crazypod_app_id id);
void crazypod_app_launcher_open_now_playing(void);
void crazypod_app_launcher_open_root(enum crazypod_route route);
void crazypod_app_launcher_open_books(void);
void crazypod_app_launcher_process_pending(void);
void crazypod_app_launcher_cancel_pending(void);

#endif
