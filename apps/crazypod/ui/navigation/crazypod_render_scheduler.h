#ifndef CRAZYPOD_RENDER_SCHEDULER_H
#define CRAZYPOD_RENDER_SCHEDULER_H

#include "crazypod_ui_routes.h"

struct crazypod_render_scheduler_host {
    bool (*route_available)(void);
    struct route_state *(*current_route)(void);
    void (*render_route)(bool transition);
};

void crazypod_render_scheduler_configure(
    const struct crazypod_render_scheduler_host *host);
void crazypod_render_scheduler_reset(void);
void crazypod_render_scheduler_schedule_route(long due);
void crazypod_render_scheduler_schedule_preview(long due);
void crazypod_render_scheduler_cancel_preview(void);
bool crazypod_render_scheduler_route_pending(void);
bool crazypod_render_scheduler_preview_pending(void);
bool crazypod_render_scheduler_blocked(void);
int crazypod_render_scheduler_wait_ticks(long now);
void crazypod_render_scheduler_service(long now);

#endif
