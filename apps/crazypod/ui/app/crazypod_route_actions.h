#ifndef CRAZYPOD_ROUTE_ACTIONS_H
#define CRAZYPOD_ROUTE_ACTIONS_H

#include <stdint.h>

#include "../navigation/crazypod_ui_routes.h"

struct crazypod_route_actions_host {
    void (*render)(bool transition);
    void (*close_product)(void);
    void (*refresh_menu_rows)(
        const struct route_state *state);
    int (*item_count)(const struct route_state *state);
    void (*boost)(int ticks);
    int (*initial_album_index)(void);
};

void crazypod_route_actions_configure(
    const struct crazypod_route_actions_host *host);
void crazypod_route_actions_push(
    enum crazypod_route route, int group);
void crazypod_route_actions_push_selected(
    enum crazypod_route route, int group, int selected);
void crazypod_route_actions_pop(void);
void crazypod_route_actions_request_now_playing(void);
void crazypod_route_actions_activate(long now);
void crazypod_route_actions_move(int direction, long now);
bool crazypod_route_actions_alpha_jump(
    int direction, long now);
void crazypod_route_actions_begin_note(
    uint32_t id, bool resume_draft);
void crazypod_route_actions_show_calendar_day(int date);
bool crazypod_route_actions_note_dirty(void);
void crazypod_route_actions_service_notes(void);

#endif
