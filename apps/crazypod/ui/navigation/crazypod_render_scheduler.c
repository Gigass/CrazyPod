#include "config.h"

#ifdef IPOD_6G

#include "kernel.h"

#include "../app/crazypod_menu_preview.h"
#include "../presentation/crazypod_menu_list.h"
#include "../presentation/crazypod_preview_motion.h"
#include "crazypod_render_scheduler.h"

static struct {
    struct crazypod_render_scheduler_host host;
    bool route_pending;
    bool preview_pending;
    long route_due;
    long preview_due;
} scheduler;

void crazypod_render_scheduler_configure(
    const struct crazypod_render_scheduler_host *host)
{
    if(host != NULL)
        scheduler.host = *host;
}

void crazypod_render_scheduler_reset(void)
{
    scheduler.route_pending = false;
    scheduler.preview_pending = false;
}

void crazypod_render_scheduler_schedule_route(long due)
{
    scheduler.route_due = due;
    scheduler.route_pending = true;
}

void crazypod_render_scheduler_schedule_preview(long due)
{
    scheduler.preview_due = due;
    scheduler.preview_pending = true;
}

void crazypod_render_scheduler_cancel_preview(void)
{
    scheduler.preview_pending = false;
}

bool crazypod_render_scheduler_route_pending(void)
{
    return scheduler.route_pending;
}

bool crazypod_render_scheduler_preview_pending(void)
{
    return scheduler.preview_pending;
}

bool crazypod_render_scheduler_blocked(void)
{
    return scheduler.route_pending ||
        scheduler.preview_pending ||
        crazypod_preview_motion_media_refresh_pending();
}

static int wait_until(long due, long now)
{
    long remaining = due - now;

    if(remaining <= 0)
        return 1;
    return remaining > HZ ? HZ : (int)remaining;
}

int crazypod_render_scheduler_wait_ticks(long now)
{
    int wait = HZ > 0 ? HZ : 1;
    int pending_wait;

    if(scheduler.route_pending) {
        pending_wait = wait_until(scheduler.route_due, now);
        if(pending_wait < wait)
            wait = pending_wait;
    }
    if(scheduler.preview_pending) {
        pending_wait = wait_until(scheduler.preview_due, now);
        if(pending_wait < wait)
            wait = pending_wait;
    }
    if(crazypod_preview_motion_media_refresh_pending()) {
        pending_wait = wait_until(
            crazypod_preview_motion_media_due(), now);
        if(pending_wait < wait)
            wait = pending_wait;
    }
    return wait;
}

void crazypod_render_scheduler_service(long now)
{
    struct route_state *state;

    if(scheduler.route_pending &&
       !TIME_BEFORE(now, scheduler.route_due)) {
        if(scheduler.host.route_available())
            scheduler.host.render_route(false);
        else
            scheduler.route_pending = false;
    }
    if(scheduler.preview_pending &&
       !TIME_BEFORE(now, scheduler.preview_due)) {
        if(scheduler.host.route_available()) {
            state = scheduler.host.current_route();
            if(crazypod_menu_list_matches(state->route)) {
                scheduler.preview_pending = false;
                if(crazypod_menu_preview_motion_ready() &&
                   crazypod_menu_preview_is_skeuomorphic_route(
                       state->route) &&
                   crazypod_preview_motion_has_content())
                    crazypod_preview_motion_start_exit();
                else
                    crazypod_menu_preview_render(
                        state, false);
            }
            else
                scheduler.preview_pending = false;
        }
        else
            scheduler.preview_pending = false;
    }
    if(!crazypod_preview_motion_media_refresh_pending() ||
       TIME_BEFORE(
           now, crazypod_preview_motion_media_due()))
        return;
    if(scheduler.host.route_available()) {
        state = scheduler.host.current_route();
        if(crazypod_menu_list_matches(state->route) &&
           !scheduler.preview_pending &&
           !crazypod_preview_motion_active()) {
            crazypod_preview_motion_clear_media_refresh();
            crazypod_menu_preview_render(state, false);
        }
        else if(!crazypod_menu_list_matches(state->route))
            crazypod_preview_motion_clear_media_refresh();
    }
    else
        crazypod_preview_motion_clear_media_refresh();
}

#endif
